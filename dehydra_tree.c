#include <jsapi.h>
#include <jsobj.h>
#include <unistd.h>
#include <stdio.h>

#include <config.h>
#include <system.h>
#include <coretypes.h>
#include <tm.h>
#include <tree.h>
#include <cp-tree.h>
#include <cxx-pretty-print.h>
#include <tree-iterator.h>
#include <pointer-set.h>
#include <toplev.h>

#include "xassert.h"
#include "builtins.h"
#include "util.h"
#include "dehydra.h"
#include "dehydra_types.h"

struct Trees {
  JSObject *rootedTreesArray;
  struct pointer_map_t *treeMap;
};

typedef struct Trees Trees;

static Trees dtrees = {0};

static jsval tree_convert (Dehydra *this, tree tree);

static tree
tree_walker (tree *tp, int *walk_subtrees, void *data) {
  *walk_subtrees = 1;
  return NULL_TREE;
}

static void rootObj (Dehydra *this, JSObject *obj) {
  unsigned int length = dehydra_getArrayLength (this, dtrees.rootedTreesArray);
  JS_DefineElement (this->cx, dtrees.rootedTreesArray, 
                   length++, OBJECT_TO_JSVAL(obj),
                   NULL, NULL, JSPROP_ENUMERATE);
}

static JSObject* compound_convert (Dehydra *this, tree t) {
  JSObject *obj = JS_NewArrayObject(this->cx, 0, NULL);
  rootObj (this, obj);
  enum tree_code code = TREE_CODE (t);
  switch (code) {
  case STATEMENT_LIST:
    {
      tree_stmt_iterator i;
      int y = 0;
      for (i = tsi_start (t); !tsi_end_p (i); tsi_next (&i)) {
        JS_DefineElement (this->cx, obj, 
                          y++, tree_convert (this, *tsi_stmt_ptr (i)),
                          NULL, NULL, JSPROP_ENUMERATE);
      }
    }
    break;
  case CALL_EXPR:
  case ADDR_EXPR:
  case GIMPLE_MODIFY_STMT:
  case RETURN_EXPR:
    if (IS_EXPR_CODE_CLASS (TREE_CODE_CLASS (code))
        || IS_GIMPLE_STMT_CODE_CLASS (TREE_CODE_CLASS (code)))
      {
        int i, len;

        /* Walk over all the sub-trees of this operand.  */
        len = TREE_OPERAND_LENGTH (t);
        /* Go through the subtrees.  We need to do this in forward order so
           that the scope of a FOR_EXPR is handled properly.  */
        for (i = 0; i < len; ++i) {
          JS_DefineElement(this->cx, obj, i,
                           tree_convert (this, 
                                         GENERIC_TREE_OPERAND (t, i)),
                           NULL, NULL, JSPROP_ENUMERATE);
        }
      }
    break;
    /*  {
      int len = host_integerp (GENERIC_TREE_OPERAND (t, 0));
      int i = 0;
      for (; i < len; i++) {
          JS_DefineElement(this->cx, obj, i,
                           tree_convert (this, 
                                         GENERIC_TREE_OPERAND (t, i)),
                           NULL, NULL, JSPROP_ENUMERATE);
      }
    }
    break;*/
  default:
    error ("Unhandled tree node: %s", tree_code_name[code]);
    cp_walk_tree_without_duplicates (&t, tree_walker, this);
    break;
  }
  return obj;
}

static JSObject *newRootedObj (Dehydra *this) {
  JSObject *obj = JS_ConstructObject (this->cx, &js_ObjectClass, NULL, 
                                      this->globalObj);
  rootObj (this, obj);
  return obj;
}

static jsval tree_convert (Dehydra *this, tree t) {
  if (!t) return JSVAL_VOID;
  void **v = pointer_map_contains(dtrees.treeMap, t);
  if (v) {
    return (jsval) *v;
  }
  JSObject *obj = NULL;
  jsval val = JSVAL_VOID;
  enum tree_code code = TREE_CODE (t);
  switch (code) {
  case VAR_DECL:
  case FUNCTION_DECL:
  case RESULT_DECL:
    obj = newRootedObj (this);
    if (DECL_NAME (t))
      dehydra_defineProperty (this, obj, "decl_name", 
                              tree_convert (this, DECL_NAME (t)));
    if (DECL_INITIAL (t) && code != RESULT_DECL)
      dehydra_defineProperty (this, obj, "decl_initial",
                              tree_convert (this, DECL_INITIAL (t)));
    val = OBJECT_TO_JSVAL (obj);
    /* dehydra_defineProperty (this, obj, "tree_type", 
       dehydra_convertType (this, TREE_TYPE (t)));*/
    break;
  case IDENTIFIER_NODE:
    if (IDENTIFIER_POINTER (t)) {
      JSString *str = JS_NewStringCopyZ (this->cx, 
                                         IDENTIFIER_POINTER (t));
      val = STRING_TO_JSVAL (str);
    }
    break;
  case INTEGER_CST:
    val = INT_TO_JSVAL (host_integerp (t, 0));
    break;
  default:
    break;
  }
  if (val == JSVAL_VOID) {
    val = OBJECT_TO_JSVAL (compound_convert (this, t));
  }
  
  if (val != JSVAL_VOID) {
    if (JSVAL_IS_OBJECT (val))
      dehydra_defineProperty (this, JSVAL_TO_OBJECT (val), "tree_code", 
                              INT_TO_JSVAL (code));
    *pointer_map_insert (dtrees.treeMap, t) = (void*) val;
  }
  return val;
}

void dehydra_plugin_pass (Dehydra *this) {
  jsval process_tree = dehydra_getCallback(this, "process_tree");
  if (process_tree == JSVAL_VOID) return;

  if (!dtrees.rootedTreesArray) {
    dtrees.rootedTreesArray = JS_NewArrayObject(this->cx, 0, NULL);
    JS_AddRoot (this->cx, &dtrees.rootedTreesArray);
    dtrees.treeMap = pointer_map_create ();
  }
  JSObject *fObj = dehydra_addVar (this, current_function_decl, 
                                   dtrees.rootedTreesArray);
  tree body_chain = DECL_SAVED_TREE (current_function_decl);
  if (body_chain && TREE_CODE (body_chain) == BIND_EXPR) {
    body_chain = BIND_EXPR_BODY (body_chain);
  }

  jsval bodyVal = tree_convert (this, body_chain);
  jsval rval, argv[2];
  argv[0] = OBJECT_TO_JSVAL (fObj);
  argv[1] = bodyVal;
  xassert (JS_CallFunctionValue (this->cx, this->globalObj, process_tree,
                                 sizeof (argv)/sizeof (argv[0]), argv, &rval));
  /* that's if we start running of out of memory*/
  /* xassert (JS_SetArrayLength (this->cx, dtrees.rootedTreesArray, 0));*/
}
