#include <jsapi.h>
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
#include "dehydra.h"
#include "util.h"

static void dehydra_iterate_statementlist (Dehydra *, tree);
static tree statement_walker (tree *, int *walk_, void *);


/*todo: make this avoid consecutive vars of the same thing */
static JSObject* dehydra_makeVar (Dehydra *this, tree t, 
                                  char const *prop, JSObject *attachToObj) {
  unsigned int length = dehydra_getArrayLength (this, this->destArray);
  this->inExpr++;
  cp_walk_tree_without_duplicates (&t, statement_walker, this);        
  this->inExpr--;
  xassert (length < dehydra_getArrayLength (this, this->destArray));
  jsval v;
  JS_GetElement (this->cx, this->destArray, length, &v);
  JSObject *obj = v == JSVAL_VOID ? NULL : JSVAL_TO_OBJECT (v);
  if (prop && attachToObj && obj) {
    dehydra_defineProperty (this, attachToObj, prop, v);
    JS_SetArrayLength (this->cx, this->destArray, length);
  }
  return obj;
}

static void 
dehydra_fcallDoArgs (Dehydra *this, JSObject *obj, tree expr,
                                 int i, int count) {
  JSObject *tmp = this->destArray;
  this->destArray = JS_NewArrayObject (this->cx, 0, NULL);
  dehydra_defineProperty (this, obj, ARGUMENTS,
                          OBJECT_TO_JSVAL (this->destArray));
  for (; i < count;i++) {
    tree e = GENERIC_TREE_OPERAND(expr, i);
    cp_walk_tree_without_duplicates(&e,
                                    statement_walker, this);        
  }
  this->destArray = tmp;
}

/* messes with dest-array, returns the  */
static jsval dehydra_attachNestedFields(Dehydra *this, JSObject *obj, char const *name, tree t) {
  JSObject *tmp = this->destArray;
  this->destArray = JS_NewArrayObject(this->cx, 0, NULL);
  jsval ret = OBJECT_TO_JSVAL (this->destArray);
  dehydra_defineProperty(this, obj, name, ret);
  cp_walk_tree_without_duplicates(&t, statement_walker, this);
  this->destArray = tmp;
  return ret;
}

/* borrowed from cp/error.c */
static tree
resolve_virtual_fun_from_obj_type_ref (tree ref)
{
  tree obj_type = TREE_TYPE (OBJ_TYPE_REF_OBJECT (ref));
  HOST_WIDE_INT index = tree_low_cst (OBJ_TYPE_REF_TOKEN (ref), 1);
  tree fun = BINFO_VIRTUALS (TYPE_BINFO (TREE_TYPE (obj_type)));
  while (index)
    {
      fun = TREE_CHAIN (fun);
      index -= (TARGET_VTABLE_USES_DESCRIPTORS
		? TARGET_VTABLE_USES_DESCRIPTORS : 1);
    }
  return BV_FN (fun);
}

static const int enable_ast_debug = 0;
static int statement_walker_depth = 0;
static tree
statement_walker (tree *tp, int *walk_subtrees, void *data) {
  Dehydra *this = data;
  enum tree_code code = TREE_CODE(*tp); 
  if (enable_ast_debug) {
    statement_walker_depth++;
    int d;
    for (d = 0; d < statement_walker_depth;d++) {
      fprintf(stderr, " ");
    }
    fprintf(stderr, "ast: %s\n",tree_code_name[TREE_CODE(*tp)]);
  }
  switch (code) {
  case STATEMENT_LIST:
    *walk_subtrees = 0;
    dehydra_iterate_statementlist(this, *tp);
    break;
  case DECL_EXPR:
    {
      tree e = *tp;
      tree decl = GENERIC_TREE_OPERAND (e, 0);
      if (TREE_CODE (decl) != USING_DECL) {
        tree init = DECL_INITIAL (decl);
        JSObject *obj = dehydra_addVar(this, decl, NULL);
        dehydra_defineProperty(this, obj, DECL, JSVAL_TRUE);
        if (init) {
          dehydra_attachNestedFields(this, obj, ASSIGN, init);
        }
      }
      *walk_subtrees = 0;
      break;
    }
  case INIT_EXPR: 
    {
      tree lval = GENERIC_TREE_OPERAND (*tp, 0);
      tree init = GENERIC_TREE_OPERAND(*tp, 1);
      /* TODO dehydra_makeVar should check the entry prior to the one it adds
         to see if it's the same thing and then nuke the new one and return old one
         this will avoid gcc annoynace with
         declar foo = boo; beaing broken up into declare foo; foo = boo;
       */
      unsigned int objPos = dehydra_getArrayLength (this, this->destArray);
      JSObject *obj = dehydra_makeVar (this, lval, NULL, NULL);
      xassert (obj);
      /* now add constructor */
      /* note here we are assuming that last addVar as the last declaration */
      /* op 0 is an anonymous temporary..i think..so use last var instead */
      jsval val = dehydra_attachNestedFields (this, obj, ASSIGN, init); 
      /* Now do special case for stack variables being initialized by a constructor*/
      JSObject *assignArray = JSVAL_TO_OBJECT (val);
      unsigned int assignArrayLength = 
        dehydra_getArrayLength (this, assignArray);
      /* Ensure the world makes sense and nothing but except for a possible
         constructor is in assignArray */
      if (assignArrayLength == 1) {
        JS_GetElement (this->cx, assignArray, 0, &val);
        JSObject *objConstructor = JSVAL_TO_OBJECT (val);
        /* verify that we got a constructor */
        JS_GetProperty(this->cx, objConstructor, DH_CONSTRUCTOR, &val);
        if (val == JSVAL_TRUE) {
          /* swap obj<->objConstructor if constructor */
          dehydra_defineProperty (this, objConstructor, FIELD_OF, 
                                  OBJECT_TO_JSVAL (obj));
          /* replace obj with objConstructor */
          JS_DefineElement (this->cx, this->destArray, objPos,
                            OBJECT_TO_JSVAL(objConstructor),
                            NULL, NULL, JSPROP_ENUMERATE);
          /* finish up by deleting assign */
          JS_DeleteProperty (this->cx, obj, ASSIGN);
        }
      }
    }
    *walk_subtrees = 0;
    break;
  case TARGET_EXPR:
     {
       /* this is a weird initializer tree node, however it's usually with INIT_EXPR
          so info in it is redudent */
       cp_walk_tree_without_duplicates(&TARGET_EXPR_INITIAL(*tp),
                                      statement_walker, this);        
       *walk_subtrees = 0;
       break;
     }
  case AGGR_INIT_EXPR:
    {
      /* another weird initializer */
      int paramCount = aggr_init_expr_nargs(*tp) + 3;
      tree fn = AGGR_INIT_EXPR_FN(*tp);
      JSObject *obj = dehydra_makeVar (this, fn, NULL, NULL);
      dehydra_defineProperty (this, obj, FCALL, JSVAL_TRUE);
      dehydra_fcallDoArgs (this, obj, *tp, 4, paramCount);
      *walk_subtrees = 0;
      break;
    }
    /* these wrappers all contain important stuff as first arg */
  case OBJ_TYPE_REF:
    /* resovle virtual calls */
    {
      tree fn = resolve_virtual_fun_from_obj_type_ref (*tp);
      JSObject *obj = dehydra_addVar (this, fn, NULL);
      xassert (dehydra_makeVar (this, OBJ_TYPE_REF_OBJECT (*tp),
                                FIELD_OF, obj));
    }
    *walk_subtrees = 0;
    break;
  case TRY_CATCH_EXPR:
  case POINTER_PLUS_EXPR:
  case ADDR_EXPR:
  case INDIRECT_REF:
  case CLEANUP_POINT_EXPR:
    cp_walk_tree_without_duplicates (&GENERIC_TREE_OPERAND (*tp, 0),
                                     statement_walker, this);        
    *walk_subtrees = 0;
    break;
  case CALL_EXPR:
    {
      tree fn = CALL_EXPR_FN (*tp);
      if (TREE_CODE (fn) == ADDR_EXPR)
        fn = TREE_OPERAND (fn, 0);

      /* index of first param */
      int i = 3;
      JSObject *obj = dehydra_makeVar (this, fn, NULL, NULL);
      dehydra_defineProperty (this, obj, FCALL, JSVAL_TRUE);
      if (TREE_CODE (TREE_TYPE (fn)) == METHOD_TYPE) {
        tree o = GENERIC_TREE_OPERAND(*tp, i);
        ++i;
        xassert (dehydra_makeVar (this, o, FIELD_OF, obj));
      }
      dehydra_fcallDoArgs (this, obj, *tp, i, TREE_OPERAND_LENGTH (*tp));
      *walk_subtrees = 0;
      break;
    }
  case MODIFY_EXPR:
    {
      JSObject *obj = dehydra_makeVar (this, GENERIC_TREE_OPERAND (*tp, 0),
                                       NULL, NULL);
      if (obj) {
        dehydra_attachNestedFields (this, obj,
                                    ASSIGN, GENERIC_TREE_OPERAND (*tp, 1));
      }
      *walk_subtrees = 0;
      break;
    }
  case COMPONENT_REF:
    {
      JSObject *obj = dehydra_addVar (this, GENERIC_TREE_OPERAND(*tp, 1), 
                                      NULL);
      xassert (dehydra_makeVar (this, GENERIC_TREE_OPERAND (*tp, 0),
                                FIELD_OF, obj));
    }
    *walk_subtrees = 0;
    break;
  case INTEGER_CST:
  case REAL_CST:
  case FIXED_CST:
  case COMPLEX_CST:
  case VECTOR_CST:
  case STRING_CST:
    {
      JSObject *obj = dehydra_addVar(this, NULL_TREE, NULL);
      dehydra_defineStringProperty(this, 
                                   obj, VALUE, 
                                   expr_as_string(*tp, 0));
    }
    break;
  case RETURN_EXPR: 
    {
      tree expr = GENERIC_TREE_OPERAND (*tp, 0);
      if (expr && TREE_CODE (expr) != RESULT_DECL) {
        if (TREE_CODE (expr) == INIT_EXPR) {
          expr = GENERIC_TREE_OPERAND (expr, 1);
        } 
        JSObject *obj = dehydra_makeVar (this, expr, NULL, NULL);
        xassert (obj);
        dehydra_defineProperty (this, obj, RETURN, JSVAL_TRUE);
      }
      *walk_subtrees = 0;
      break;
    }
  case VAR_DECL:
  case FUNCTION_DECL:
  case PARM_DECL:
    /* result decl is a funky special case return values*/
  case RESULT_DECL:
    {
      dehydra_addVar(this, *tp, NULL);
      break;
    }
    /* this isn't magic, but breaks pretty-printing */
  case LABEL_DECL:
    break;
  default:
    break;
  }
  if (enable_ast_debug) {
    statement_walker_depth--;
  }
  /*    fprintf(stderr, "%s:", loc_as_string(this->loc));
    fprintf(stderr, "walking tree element: %s. %s\n", tree_code_name[TREE_CODE(*tp)],
    expr_as_string(*tp, 0));*/

  return NULL_TREE;
}

/* Creates next array to dump dehydra objects onto */
static void dehydra_nextStatement(Dehydra *this, location_t loc) {
  unsigned int length = dehydra_getArrayLength (this,
                                                this->statementHierarchyArray);
  xassert (!this->inExpr);
  this->loc = loc;
  this->destArray = NULL;
  JSObject *obj = NULL;
  /* Check that the last statement array was used, otherwise reuse it */
  if (length) {
    jsval val;
    JS_GetElement (this->cx, this->statementHierarchyArray, length - 1,
                   &val);
    obj = JSVAL_TO_OBJECT (val);
    JS_GetProperty (this->cx, obj, STATEMENTS, &val);
    this->destArray = JSVAL_TO_OBJECT (val);
    int destLength = dehydra_getArrayLength (this, this->destArray);
    /* last element is already setup & empty, we are done */
    if (destLength != 0) {
      this->destArray = NULL;
    }
  }
  if (!this->destArray) {
    obj = JS_NewObject (this->cx, NULL, 0, 0);
    JS_DefineElement (this->cx, this->statementHierarchyArray, length,
                      OBJECT_TO_JSVAL(obj),
                      NULL, NULL, JSPROP_ENUMERATE);
    this->destArray = JS_NewArrayObject(this->cx, 0, NULL);
    dehydra_defineProperty (this, obj, STATEMENTS, 
                            OBJECT_TO_JSVAL (this->destArray));
  }
  /* always update location */
  if (this->loc) {
    const char *loc_str = loc_as_string (this->loc);
    const char *s = strrchr (loc_str, '/');
    if (s) loc_str = s + 1;
    dehydra_defineStringProperty (this, obj, LOC, loc_str);
  } else {
    dehydra_defineProperty (this, obj, LOC, JSVAL_VOID);
  }
}

static void dehydra_iterate_statementlist (Dehydra *this, tree statement_list) {
  tree_stmt_iterator i = tsi_start (STATEMENT_LIST_CHECK( statement_list));
  for (; !tsi_end_p (i); tsi_next (&i)) {
    tree s = *tsi_stmt_ptr (i);
    /* weird statements within expression shouldn't 
       trigger dehydra_nextStatement */
    if (! this->inExpr)
      dehydra_nextStatement (this, location_of (s));
    cp_walk_tree_without_duplicates (&s, statement_walker, this);
  }
}

void dehydra_cp_pre_genericize(Dehydra *this, tree fndecl, bool callJS) {
  this->statementHierarchyArray = JS_NewArrayObject (this->cx, 0, NULL);
  int key = 
    dehydra_rootObject (this, 
                        OBJECT_TO_JSVAL (this->statementHierarchyArray));
  
  *pointer_map_insert (this->fndeclMap, fndecl) = 
    (void*) key;
  
  dehydra_nextStatement (this, location_of (fndecl));

  tree body_chain = DECL_SAVED_TREE (fndecl);
  if (body_chain && TREE_CODE (body_chain) == BIND_EXPR) {
    body_chain = BIND_EXPR_BODY (body_chain);
  }
  cp_walk_tree_without_duplicates (&body_chain, statement_walker, this);
  this->statementHierarchyArray = NULL;
  if (callJS) {
    dehydra_visitDecl (this, fndecl);
  }
}
