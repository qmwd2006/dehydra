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

static jsval tree_convert (Dehydra *this, tree t) {
  void **v = pointer_map_contains(dtrees.treeMap, t);
  if (v) {
    return OBJECT_TO_JSVAL ((JSObject*) *v);
  }
  unsigned int length = dehydra_getArrayLength(this, dtrees.rootedTreesArray);
  JSObject *obj = JS_NewArrayObject(this->cx, 0, NULL);
  JS_DefineElement(this->cx, dtrees.rootedTreesArray, 
                   length++, OBJECT_TO_JSVAL(obj),
                   NULL, NULL, JSPROP_ENUMERATE);

  *pointer_map_insert (dtrees.treeMap, t) = obj;
  
  enum tree_code code = TREE_CODE (t);
  switch (code) {
  case STATEMENT_LIST:
    {
      tree_stmt_iterator i;
      int y = 0;
      for (i = tsi_start (t); !tsi_end_p (i); tsi_next (&i)) {
        JS_DefineElement(this->cx, obj, 
                         y++, tree_convert (this, *tsi_stmt_ptr (i)),
                         NULL, NULL, JSPROP_ENUMERATE);
      }
    }
    break;
  default:
    error ("Unhandled tree node: %s", tree_code_name[code]);
    cp_walk_tree_without_duplicates (&t, tree_walker, this);
    break;
  case GIMPLE_MODIFY_STMT:
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

  }

  if (obj) {
    dehydra_defineProperty (this, obj, "code", 
                            INT_TO_JSVAL (code));
    *pointer_map_insert (dtrees.treeMap, t) = obj;
    return OBJECT_TO_JSVAL (obj);
  }
  return JSVAL_VOID;
}

void dehydra_plugin_pass (Dehydra *this) {
  jsval process_tree = dehydra_getCallback(this, "process_tree");
  if (process_tree == JSVAL_VOID) return;

  if (!dtrees.rootedTreesArray) {
    dtrees.rootedTreesArray = JS_NewArrayObject(this->cx, 0, NULL);
    xassert (JS_AddRoot (this->cx, &dtrees.rootedTreesArray));
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
