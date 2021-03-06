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

#ifndef GCC_COMPAT_H
#define GCC_COMPAT_H

/* not sure if there is a better place to put these */
#pragma weak global_namespace
#pragma weak cp_walk_subtrees

#ifndef TREEHYDRA_GENERATED

#if !defined(TREEHYDRA_CONVERT_JS) && !defined(TREEHYDRA_GENERATED)
#include <jsapi.h>
#endif

#include <config.h>
#include <system.h>
#include <coretypes.h>
#include <tm.h>
#include <tree.h>
#include "cp-tree-jsapi-workaround.h"
#include <cp/cxx-pretty-print.h>
#include <tree-iterator.h>
#include <pointer-set.h>
#include <toplev.h>
#endif

#ifndef cp_walk_tree_without_duplicates
#define cp_walk_tree_without_duplicates walk_tree_without_duplicates
#endif

#ifndef GENERIC_TREE_OPERAND
#define GENERIC_TREE_OPERAND TREE_OPERAND
#endif

#ifndef AGGR_INIT_EXPR_FN
#define AGGR_INIT_EXPR_FN aggr_init_expr_fn
static inline tree aggr_init_expr_fn(tree t) {
  tree fn = TREE_OPERAND (t, 0);
  if (TREE_CODE (fn) == ADDR_EXPR)
    fn = TREE_OPERAND (fn, 0);
  return fn;
}
#endif

#ifndef aggr_init_expr_nargs
#ifdef __APPLE_CC__
#define DEHYDRA_INPLACE_ARGS
#else
#error "inplace arguments are not supported in this environment"
#endif
static inline tree aggr_init_expr_args(tree t) {
  // see c-tree.texi: if AGGR_INIT_VIA_CTOR_P then the first argument in the
  // argument chain is not an argument but a VAR_DECL and equal to the
  // address of the third operand of AGGR_INIT_EXPR, so we'd rather skip it.
  tree args = TREE_OPERAND (t, 1);
  if (AGGR_INIT_VIA_CTOR_P (t))
    args = TREE_CHAIN (args);
  return args;
}
#else
static const int aggr_init_expr_first_param_index = 3;
static inline tree aggr_init_expr_args(tree t) { return t; }
static inline int aggr_init_expr_param_count(tree t) {
  return aggr_init_expr_nargs(t) + 3;
}
#endif

#ifndef CALL_EXPR_FN
#define CALL_EXPR_FN(t) TREE_OPERAND_CHECK_CODE (t, CALL_EXPR, 0)
#ifndef DEHYDRA_INPLACE_ARGS
#error "gcc and os combination you are compiling for is not supported"
#endif
static inline tree call_expr_args(tree t) { return TREE_OPERAND (t, 1); }
static inline tree call_expr_first_arg(tree t) {
  // in apple gcc 42 the first arg is the value on the head of the chain 
  // which is the first operand
  return TREE_VALUE (call_expr_args(t));
}
static inline tree call_expr_rest_args(tree t) {
  return TREE_CHAIN (call_expr_args(t));
}
#else
static const int call_expr_first_param_index = 3;
static inline tree call_expr_first_arg(tree t) {
  return GENERIC_TREE_OPERAND (t, call_expr_first_param_index);
}
static inline tree call_expr_rest_args(tree t) { return t; }
#endif

#ifndef TREE_OPERAND_LENGTH
#define TREE_OPERAND_LENGTH(t) TREE_CODE_LENGTH (TREE_CODE (t))
#endif

#endif
