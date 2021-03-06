/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Dehydra and Treehydra scriptable static analysis tools
 * Copyright (C) 2007-2010 The Mozilla Foundation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <jsapi.h>
#include <unistd.h>
#include <stdio.h>

#include "gcc_compat.h"

#include "xassert.h"
#include "dehydra.h"
#include "dehydra_types.h"
#include "util.h"

static void dehydra_iterate_statementlist (Dehydra *, tree);
static tree statement_walker (tree *, int *walk_, void *);
static void dehydra_nextStatement(Dehydra *this, location_t loc);

/* Make a Dehydra variable to represent the value of the given
 * AST expression node. 
 * This function does not itself create a Dehydra variable. Instead,
 * it relies on the tree walk of the tree to create a Dehydra variable
 * by calling dehydra_addVar at some point. */
/*todo: make this avoid consecutive vars of the same thing */
JSObject* dehydra_makeVar (Dehydra *this, tree t, 
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

#ifndef DEHYDRA_INPLACE_ARGS
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
#endif

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

/* Does variable initialization and special case logic for stack
   variables being initialized by a constructor.
*/
void dehydra_initVar (Dehydra *this, tree lval, tree init, bool rotate) {
  unsigned int objPos = dehydra_getArrayLength (this, this->destArray);
  JSObject *obj = dehydra_makeVar (this, lval, NULL, NULL);
  xassert (obj);
  /* now add constructor */
  /* note here we are assuming that last addVar as the last declaration */
  /* op 0 is an anonymous temporary..i think..so use last var instead */
  if (!init) return;
  jsval val = dehydra_attachNestedFields (this, obj, ASSIGN, init); 

  JSObject *assignArray = JSVAL_TO_OBJECT (val);
  unsigned int assignArrayLength = 
    dehydra_getArrayLength (this, assignArray);
  /* Ensure the world makes sense and nothing but except for a possible
     constructor is in assignArray */
  if (assignArrayLength != 1) return;

  JS_GetElement (this->cx, assignArray, 0, &val);
  JSObject *objConstructor = JSVAL_TO_OBJECT (val);
  /* verify that we got a constructor */
  JS_GetProperty(this->cx, objConstructor, DH_CONSTRUCTOR, &val);
  if (val == JSVAL_TRUE) {
    /* swap obj<->objConstructor if constructor */
    dehydra_defineProperty (this, objConstructor, FIELD_OF, 
                            OBJECT_TO_JSVAL (obj));
    if (rotate) {
      /* replace obj with objConstructor */
      JS_DefineElement (this->cx, this->destArray, objPos,
                        OBJECT_TO_JSVAL(objConstructor),
                        NULL, NULL, JSPROP_ENUMERATE);
      /* finish up by deleting assign */
      JS_DeleteProperty (this->cx, obj, ASSIGN);
    }
  }
}

JSObject* dehydra_call_or_aggr_init_expr (Dehydra *this, tree t) {
  xassert (TREE_CODE (t) == CALL_EXPR || TREE_CODE (t) == AGGR_INIT_EXPR);
  tree fn = TREE_CODE (t) == CALL_EXPR ? CALL_EXPR_FN (t) : AGGR_INIT_EXPR_FN (t);
  if (TREE_CODE (fn) == ADDR_EXPR)
    fn = TREE_OPERAND (fn, 0);

  int offset = 0;
  JSObject *obj = dehydra_makeVar (this, fn, NULL, NULL);
  dehydra_defineProperty (this, obj, FCALL, JSVAL_TRUE);
  tree args;
  if (TREE_CODE (TREE_TYPE (fn)) == METHOD_TYPE) {
    // need to generalize this arg_init_expr
    tree o = call_expr_first_arg(t);
    ++offset;
    xassert (dehydra_makeVar (this, o, FIELD_OF, obj));
    // same as above
    args = call_expr_rest_args(t);
  }
#ifdef DEHYDRA_INPLACE_ARGS
  else {
    args = call_expr_args(t);
  }
  dehydra_attachNestedFields(this, obj, ARGUMENTS, args);
#else
  dehydra_fcallDoArgs (this, obj, t,
                       call_expr_first_param_index + offset,
                       TREE_OPERAND_LENGTH(t));
#endif
  return obj;
}

static const int enable_ast_debug = 0;
static int statement_walker_depth = 0;
static tree
statement_walker (tree *tp, int *walk_subtrees, void *data) {
  Dehydra *this = data;
  enum tree_code code = TREE_CODE(*tp); 
  if (enable_ast_debug) {
    location_t prior_loc = UNKNOWN_LOCATION;
    statement_walker_depth++;
    char *spaces = xmalloc(statement_walker_depth+1);
    int d;
    for (d = 0; d < statement_walker_depth;d++) {
      spaces[d] = '-';
    }
    spaces[statement_walker_depth] = 0;
    location_t loc = location_of(*tp);
    if (loc_is_unknown(loc))
      loc = prior_loc;
    else
      prior_loc = loc;
    warning(0, "%H%sast: %s %s",&loc,spaces,tree_code_name[TREE_CODE(*tp)], DECL_P(*tp) ? expr_as_string(*tp, 0) : "");
    free(spaces);
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
        dehydra_initVar (this, decl, init, false);      
        /*JSObject *obj = dehydra_addVar(this, decl, NULL);
        dehydra_defineProperty(this, obj, DECL, JSVAL_TRUE);
        if (init) {
          dehydra_attachNestedFields(this, obj, ASSIGN, init);
          }*/
      }
      *walk_subtrees = 0;
      break;
    }
  case INIT_EXPR: 
    {
      tree lval = GENERIC_TREE_OPERAND (*tp, 0);
      tree init = GENERIC_TREE_OPERAND(*tp, 1);
      dehydra_initVar (this, lval, init, true);      
    }
    *walk_subtrees = 0;
    break;
  case TARGET_EXPR:
     {
       /* This is a weird initializer tree node. It seems to be used to
        * decorate regular initializers to add cleanup code, etc. We'll
        * just skip the wrapping. */
       cp_walk_tree_without_duplicates(&TARGET_EXPR_INITIAL(*tp),
                                      statement_walker, this);        
       *walk_subtrees = 0;
       break;
     }
  case AGGR_INIT_EXPR:
    {
      /* C++ constructor invocation. */
      dehydra_call_or_aggr_init_expr (this, *tp);
      *walk_subtrees = 0;
      break;
    }
  case CONSTRUCTOR:
    {
      /* In C, this is used for struct/array initializers. It appears
       * that in C++ it is also used for default constructor invocations. */
      (void) dehydra_addVar(this, *tp, NULL);
      /* This tree type has an argument list, but the arguments should
       * all be default zero values of no interest. */
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
  case CLEANUP_POINT_EXPR:
    // do not walk the cleanup stuff
    cp_walk_tree_without_duplicates (&GENERIC_TREE_OPERAND (*tp, 0),
                                     statement_walker, this);        
    *walk_subtrees = 0;
    break;
  case CALL_EXPR:
    {
      dehydra_call_or_aggr_init_expr (this, *tp);
      *walk_subtrees = 0;
      break;
    }
  case MODIFY_EXPR:
  case PREDECREMENT_EXPR:
  case PREINCREMENT_EXPR:
  case POSTDECREMENT_EXPR:
  case POSTINCREMENT_EXPR:
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
  case REAL_CST:
  case INTEGER_CST: 
#ifdef FIXED_CST_CHECK
  case FIXED_CST:
#endif
  case COMPLEX_CST:
  case VECTOR_CST:
  case STRING_CST:
  case PTRMEM_CST:
    {
      tree type = TREE_TYPE (*tp);
      JSObject *obj = dehydra_addVar (this, NULL_TREE, NULL);
      /* Bug 429362: GCC pretty-printer is broken for unsigned ints. */
      dehydra_defineStringProperty (this, obj, VALUE, code == INTEGER_CST ?
                                    dehydra_intCstToString(*tp) : expr_as_string(*tp, 0));
      if (type) {
        dehydra_defineProperty (this, obj, TYPE,
                                dehydra_convert_type(this, type));
      }
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
  case EXPR_STMT:
    if (!this->inExpr) {
      location_t loc = location_of (*tp);
      if (!loc_is_unknown (loc))
        dehydra_nextStatement (this, loc);
    }
    break;
  case TRY_CATCH_EXPR:
#ifdef POINTER_PLUS_EXPR_CHECK
  case POINTER_PLUS_EXPR:
#endif
  case ADDR_EXPR:
  case INDIRECT_REF:
  case LABEL_DECL:
    // above expressions have a single operand
    break;
  default:
    {
      tree maybe_decl = *tp;
      if (code == PTRMEM_CST) {
        maybe_decl = PTRMEM_CST_MEMBER (maybe_decl);
        code = TREE_CODE (maybe_decl);
      }
      if (code != NAMESPACE_DECL && DECL_P(maybe_decl)) {
        dehydra_addVar (this, maybe_decl, NULL);
      }
    }
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
  convert_location_t(this, obj, LOC, this->loc);
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
    (void*)(intptr_t) key;
  
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
