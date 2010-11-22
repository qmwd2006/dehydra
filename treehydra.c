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
//spidermonkey/gcc conflict
#ifdef TEST_BIT
#undef TEST_BIT
#endif

#include <unistd.h>
#include <stdio.h>

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
#include <cgraph.h>
#include <stdint.h>

#include "xassert.h"
#include "dehydra_builtins.h"
#include "util.h"
#include "dehydra.h"
#include "dehydra_types.h"
#include "treehydra.h"
#include "jsval_map.h"

// mess to figure out what gimple is implemented like
#ifdef GIMPLE_TUPLE_P // 4.3
#define TREEHYDRA_HAS_TSCOMMON(t) (!GIMPLE_TUPLE_P(t))
#else
#define TREEHYDRA_HAS_TSCOMMON(t) 1 //gcc vers other than 4.3
#ifndef __APPLE_CC__ // checking for __APPLE_CC__ just checking for 4.2
//4.4 and onwards
#include "gimple.h"
#define TREEHYDRA_GIMPLE_TUPLES
#endif
#endif

// no precompiler symbol defined for fatvals change (bug 549143)
#ifndef JSID_TO_STRING
#define JSID_TO_STRING(id) JSVAL_TO_STRING(id)
#endif

#if JS_VERSION < 180
#error "Need SpiderMonkey 1.8 or higher. Treehydra does not support older spidermonkeys due to lack of JS_AlreadyHasOwnProperty"
#endif
/* The entries in the map should be transitively rooted by
   this->globalObj's current_function_decl property */
static struct jsval_map *jsvalMap = NULL;

/* this helps correlate js nodes to their C counterparts */
static int global_seq = 0;
static JSObject *dehydraSysObj = NULL;

int treehydra_debug = 0;

typedef struct {
  treehydra_handler handler;
  void *data;
} lazy_handler;

void convert_tree_node_union (struct Dehydra *this, enum tree_node_structure_enum code,
                              union tree_node *var, struct JSObject *obj);
#ifdef TREEHYDRA_GIMPLE_TUPLES
void convert_gimple_statement_d_union (struct Dehydra *this, unsigned int code, union gimple_statement_d *topmost, struct JSObject *obj);
#endif

/* delete me */
JSBool tree_construct(JSContext *cx, JSObject *obj,
                         uintN argc, jsval *argv, jsval *rval)
{
  fprintf(stderr, "tree_construct\n");
  return JS_TRUE;
}

void tree_finalize(JSContext *cx, JSObject *obj)
{
  JS_free(cx, JS_GetPrivate(cx, obj));
}

static const char *SEQUENCE_N = "SEQUENCE_N";

/* JavaScript class resolve hook function for Treehydra lazy objects.
 * The strategy here is to call the lazy handler exactly once, the
 * first time any property is asked for. Thus, the handler must install
 * all lazy properties. */
static JSBool ResolveTreeNode (JSContext *cx, JSObject *obj, jsid id,
                               uintN flags, JSObject **objp) {
  Dehydra *this = JS_GetContextPrivate (cx);
  /* when the going gets tough will be able to implement
   unions using the id field.for now avoiding it at all cost*/
  lazy_handler *lazy = JS_GetPrivate (cx, obj);
  *objp = obj;
  if (!lazy) {
    if (flags & (JSRESOLVE_ASSIGNING | JSRESOLVE_DETECTING)) {
      return JS_TRUE;
    }
    /* The lazy handler has already been called. Standard behavior would
     * be to let the interpreter to continue searching the scope chain.
     * Instead, we're going to check it first so that we can return an
     * error if and only if the property doesn't exist.
     *
     * A better way to do this would be to simply set strict mode, but
     * strict mode doesn't always report this condition (see bug 425066). */
    JSString* str = JSID_TO_STRING(id);
    const char *prop_name = JS_GetStringBytes(str);
    JSBool has_prop;
    JSObject *protoObj = JS_GetPrototype(cx, obj);
    JSBool rv = JS_HasProperty(cx, protoObj, prop_name, &has_prop);
    if (rv && has_prop) {
      *objp = protoObj;
      return JS_TRUE;
    }
    /* Property not found anywhere: produce the error. */
    jsval unhandled_property_handler = dehydra_getToplevelFunction(
        this, "unhandledLazyProperty");
    jsval rval;
    jsval arg = STRING_TO_JSVAL(str);
    return JS_CallFunctionValue (this->cx, this->globalObj, unhandled_property_handler,
                                 1, &arg, &rval);
  }
  JS_SetPrivate (cx, obj, NULL);
  lazy->handler (this, lazy->data, obj);
  free (lazy);
  return JS_TRUE;
}

static JSClass js_tree_class = {
  "GCCNode",  /* name */
  JSCLASS_CONSTRUCT_PROTOTYPE | JSCLASS_HAS_PRIVATE | JSCLASS_NEW_RESOLVE, /* flags */
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
  JS_EnumerateStub,(JSResolveOp) ResolveTreeNode, JS_ConvertStub, tree_finalize,
  NULL, NULL, NULL, tree_construct, NULL, NULL, NULL, NULL
};

/* setups a lazy object. transitively rooted through parent */
jsval get_lazy (Dehydra *this, treehydra_handler handler, void *v,
                       JSObject *parent, const char *propname) {
  JSObject *obj;
  jsval jsvalObj;
  xassert (parent && propname);

  obj =
    definePropertyObject(
        this->cx, parent, propname, &js_tree_class, NULL,
        JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT);
  jsvalObj = OBJECT_TO_JSVAL (obj);

  lazy_handler *lazy = xmalloc (sizeof (lazy_handler));
  lazy->handler = handler;
  lazy->data = v;
  JS_SetPrivate (this->cx, obj, lazy);
  return jsvalObj;
}

/* This either returnes a cached object or creates a new lazy object */
jsval get_existing_or_lazy (Dehydra *this, treehydra_handler handler, void *v,
                                   JSObject *parent, const char *propname) {
  if (!v) {
    dehydra_defineProperty (this, parent, propname, JSVAL_VOID);
    return JSVAL_VOID;
  }

  jsval val;
  bool found = jsval_map_get (jsvalMap, v, &val);
  if (found) {
    dehydra_defineProperty (this, parent, propname, val);
    return val;
  }

  const jsval jsret = get_lazy (this, handler, v, parent, propname);
  jsval_map_put(jsvalMap, v, jsret);
  return jsret;
}

#ifdef TREEHYDRA_GIMPLE_TUPLES

// I would love to implement this as a toplevel gimple_op(gs,i) func
// but currently there is no robust way to go from a js object to the C(once it gets delazified)
void lazy_gimple_ops (Dehydra *this, void *structure, JSObject *obj) {
  gimple stmt = (gimple)structure;
  char buf[32];
  unsigned i;
  for (i = 0; i < gimple_num_ops (stmt); i++) {
    sprintf (buf, "%d", i);
    get_existing_or_lazy (this, lazy_tree_node, gimple_op (stmt, i), obj, buf);
  }
}

// this kind of crap should be generated by convert_tree.js
void lazy_gimple_statement_d (Dehydra *this, void *structure, JSObject *obj) {
  gimple stmt = (gimple)structure;
  convert_gimple_statement_d_union (this, GSS_BASE, stmt, obj);
  convert_gimple_statement_d_union (this, gimple_statement_structure(stmt),
                                    stmt, obj);
  get_lazy (this, lazy_gimple_ops, stmt, obj, "gimple_ops");
}
#endif

void lazy_tree_string (struct Dehydra *this, void* void_var, struct JSObject *obj) {
  int wchar_bytes;
  int num_chars;
  struct tree_string *topmost = (struct tree_string*) void_var;
  tree str = (tree) void_var;
  if (!void_var) return;
  
  // reflect stuff in the struct as treehydra_generated would've
  get_lazy (this, lazy_tree_common, &topmost->common, obj, "common");
  convert_int(this, obj, "length", (HOST_WIDE_INT) topmost->length);

  // now reflect .str, account for unicode magic (bug 526970)
  tree str_type = TREE_TYPE (str);
  if (str_type && TYPE_PRECISION (TREE_TYPE (str_type)) == TYPE_PRECISION (char_type_node)) {
    wchar_bytes = 1;
  } else {
    wchar_bytes = TYPE_PRECISION (wchar_type_node) / BITS_PER_UNIT;
  }
  num_chars = (TREE_STRING_LENGTH (str) / wchar_bytes);
  // TREE_STRING_LENGTH is 0 for certain empty strings
  if (num_chars != 0) {
    // skip trailing null
    --num_chars;
  }

  if (wchar_bytes == 1) {
    jsval v = STRING_TO_JSVAL (JS_NewStringCopyN (this->cx, TREE_STRING_POINTER (str), num_chars));
    dehydra_defineProperty (this, obj, "str", v);
  } else {
    int i;
    jsval v;
    jschar *buf = xmalloc (num_chars * sizeof(jschar));
    for (i = 0; i < num_chars; ++i) {
      if (wchar_bytes == 2)
        buf[i] = ( (const uint16_t*)(TREE_STRING_POINTER (str)) )[i];
      else // assume wchar_bytes == 4
        buf[i] = ( (const uint32_t*)(TREE_STRING_POINTER (str)) )[i];
      // (this assumes something about endianness)
    }
    v = STRING_TO_JSVAL (JS_NewUCStringCopyN (this->cx, buf, num_chars));
    dehydra_defineProperty (this, obj, "str", v);
    free (buf);
  }
}

// work around the fact that DECL_ASSEMBLER_NAME needs to do a callback if the name isn't initialized yet
// TODO: needs testcase
void lazy_decl_as_string (Dehydra *this, void *structure, JSObject *obj) {
  tree t = (tree) structure;
  convert_char_star(this, obj, "str", decl_as_string(t, 0));
}

// this kind of crap should be generated by convert_tree.js
void lazy_decl_assembler_name (Dehydra *this, void *structure, JSObject *obj) {
  tree t = (tree) structure;
  get_lazy (this, lazy_tree_node, HAS_DECL_ASSEMBLER_NAME_P(t) ? DECL_ASSEMBLER_NAME(t) : NULL_TREE,
            obj, "identifier");
}

/* This code is ugly because it deals with low level representation 
 * if gcc's tree union */
void lazy_tree_node (Dehydra *this, void *structure, JSObject *obj) {
  tree t = (tree)structure;
  if (treehydra_debug) {
    const int myseq = ++global_seq;
    dehydra_defineProperty (this, obj, SEQUENCE_N, INT_TO_JSVAL (myseq));
  }
  enum tree_code code = TREE_CODE (t);
  /* special case knowledge of non-decl nodes */
#ifndef __APPLE_CC__
  // no TS_BASE in mac gcc 42 and no easy way to check too
  convert_tree_node_union (this, TS_BASE, t, obj);
#endif
  if (TREEHYDRA_HAS_TSCOMMON (t)) {
    convert_tree_node_union (this, TS_COMMON, t, obj);
  }
  /* do not do tree_node_structure() for non C types */
  /* taras: AFAIK This guard is only needed for gcc <= 4.3 */
  if (code < NUM_TREE_CODES
      || (isGPlusPlus()
          && cp_tree_node_structure ((union lang_tree_node *)t) == TS_CP_GENERIC)) {
    enum tree_node_structure_enum i = tree_node_structure (t);
    convert_tree_node_union (this, i, t, obj);
    /* stuff below is empty for non-decls */
    if (!DECL_P (t)) return;

    get_lazy (this, lazy_decl_as_string, t, obj, "_decl_as_string");
    get_lazy (this, lazy_decl_assembler_name, t, obj, "_decl_assembler_name");

    for (i = 0; i < LAST_TS_ENUM;i++) {
      if (tree_contains_struct[code][i]) {
        convert_tree_node_union (this, i, t, obj);
      }
    }
  } else {
    fprintf(stderr, "%s\n", tree_code_name[code]);
  }
}

/* BEGIN Functions for treehydra_generated */
jsval get_enum_value (struct Dehydra *this, const char *name) {
  jsval val = JSVAL_VOID;
  JS_GetProperty(this->cx, this->globalObj, name, &val);
  if (val == JSVAL_VOID) {
    error ("EnumValue '%s' not found. enums.js isn't loaded", name);
  }
  return val;
}

void convert_char_star (struct Dehydra *this, struct JSObject *parent,
                        const char *propname, const char *str) {
  jsval v = STRING_TO_JSVAL (JS_NewStringCopyZ (this->cx, str));
  dehydra_defineProperty (this, parent, propname, v);
}

void convert_int (struct Dehydra *this, struct JSObject *parent,
                  const char *propname, HOST_WIDE_INT i) {

  jsval v;
  JS_NewNumberValue(this->cx, (jsdouble) i, &v);
  dehydra_defineProperty (this, parent, propname, v);

  /* because 64-bit ints can overflow a jsdouble, do the same representation,
     unsigned, in string form */
  static char buf[32];
  sprintf(buf, "%llx", (unsigned long long) i);

  int len = strlen(propname);
  char *unsignedprop = xmalloc(len + 5);
  strcpy(unsignedprop, propname);
  strcpy(unsignedprop + len, "_str");

  dehydra_defineStringProperty (this, parent, unsignedprop, buf);

  free(unsignedprop);
}

/* END Functions for treehydra_generated */

typedef struct {
  size_t capacity;
  size_t length;
  Dehydra *this;
  char str[1];
} GrowingString;

static tree
walk_n_test (tree *tp, int *walk_subtrees, void *data)
{
  enum tree_code code = TREE_CODE (*tp);
  const char *strcode = tree_code_name[code];
  GrowingString **gstr = (GrowingString**) data;
  /* figure out corresponding seq# by peeking at js */
  jsval v;
  bool found = jsval_map_get (jsvalMap, *tp, &v);
  int seq = 0;
  if (found) {
    JSObject *obj = JSVAL_TO_OBJECT (v);
    jsval val = JSVAL_VOID;
    JS_GetProperty(gstr[0]->this->cx, obj, SEQUENCE_N, &val);
    if (val != JSVAL_VOID)
      seq = JSVAL_TO_INT (val);
  }
  /* 30 is a "this should be big enough" size to fit in [space]+pointers */
  while (gstr[0]->length + 30 + strlen(strcode) > gstr[0]->capacity) {
    gstr[0]->capacity *= 2;
    gstr[0] = xrealloc (gstr[0], gstr[0]->capacity + sizeof(GrowingString));
  }
  /* point at the end of the string*/
  gstr[0]->length += sprintf(gstr[0]->str + gstr[0]->length, "%s %d\n", strcode, seq);
  return NULL_TREE;
}

/* Returns a list of walked nodes in the current function body */
JSBool JS_C_walk_tree(JSContext *cx, uintN argc, jsval *vp)
{
  Dehydra *this = JS_GetContextPrivate (cx);
  const size_t capacity = 512;
  GrowingString *gstr = xrealloc (NULL, capacity);
  gstr->capacity = capacity - sizeof(GrowingString);
  gstr->this = this;
  gstr->length = 0;
  gstr->str[0] = 0;
  tree body_chain = DECL_SAVED_TREE (current_function_decl);
  struct pointer_set_t *pset = pointer_set_create ();
  walk_tree (&body_chain, walk_n_test, &gstr, pset);
  pointer_set_destroy (pset);
  /* remove last \n */
  if (gstr->length)
    gstr->str[gstr->length - 1] = 0;
  JS_SET_RVAL(cx, vp, STRING_TO_JSVAL (JS_NewStringCopyZ (this->cx, gstr->str)));
  free(gstr);
  return JS_TRUE;
}

static void lazy_integer_types (Dehydra *this, void *structure, JSObject *obj) {
  int i;
  char buf[8];
  for (i = 0;i < sizeof(integer_types)/sizeof(integer_types[0]);++i) {
    sprintf(buf, "%d", i);
    get_existing_or_lazy (this, lazy_tree_node, integer_types[i], obj, buf);
  }
}

static void lazy_gcc_globals (Dehydra *this, void *structure, JSObject *obj) {
  get_existing_or_lazy (this, lazy_cgraph_node, cgraph_nodes, obj,
                        "cgraph_nodes");
  get_lazy (this, lazy_integer_types, NULL, obj, "integer_types");
}

static void lazy_treehydra_globals (Dehydra *this, void *structure, JSObject *obj) {
  get_lazy (this, lazy_gcc_globals, NULL, obj, "gcc");
}

void treehydra_call_js (struct Dehydra *this, const char *callback, tree treeval) {
  jsval process = dehydra_getToplevelFunction(this, callback);
  if (process == JSVAL_VOID) return;

  jsval rval;
  xassert (!jsvalMap);
  jsvalMap = jsval_map_create();

  get_lazy (this, lazy_treehydra_globals, NULL, dehydraSysObj,
            "treehydra");
  // Ensure that error/warning report errors within a useful context
  tree old_current_function_decl = NULL_TREE;

  if (current_function_decl != treeval && TREE_CODE (treeval) == FUNCTION_DECL) {
    old_current_function_decl = current_function_decl;
    current_function_decl = treeval;
  }
  jsval fnval =
    get_existing_or_lazy (this, lazy_tree_node, treeval,
                          this->globalObj, "__treehydra_top_obj");

  if (old_current_function_decl)
    current_function_decl = old_current_function_decl;
  xassert (JS_CallFunctionValue (this->cx, this->globalObj, process,
                                 1, &fnval, &rval));
  JS_DeleteProperty (this->cx, dehydraSysObj, "treehydra");
  JS_DeleteProperty (this->cx, this->globalObj,
                       "__treehydra_top_obj");

  jsval_map_destroy(jsvalMap);
  jsvalMap = NULL;
  JS_MaybeGC(this->cx);
}

int treehydra_startup (Dehydra *this) {
  jsval sys_val = JSVAL_VOID;

  JS_GetProperty(this->cx, this->globalObj, SYS, &sys_val);
  xassert (sys_val != JSVAL_VOID);
  dehydraSysObj = JSVAL_TO_OBJECT (sys_val);

  xassert (JS_DefineFunction (this->cx, this->globalObj, "C_walk_tree",
                              (JSNative) JS_C_walk_tree, 0, JSFUN_FAST_NATIVE));

  xassert (JS_InitClass(this->cx, this->globalObj, NULL
                        ,&js_tree_class , NULL, 0, NULL, NULL, NULL, NULL));
  xassert (!dehydra_includeScript (this, "treehydra.js"));
  return 0;
}
