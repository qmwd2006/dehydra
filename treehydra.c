/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
#include <jsapi.h>
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
#include <cxx-pretty-print.h>
#include <tree-iterator.h>
#include <pointer-set.h>
#include <toplev.h>
#include <cgraph.h>

#include "xassert.h"
#include "dehydra_builtins.h"
#include "util.h"
#include "dehydra.h"
#include "dehydra_types.h"
#include "treehydra.h"

#if JS_VERSION < 180
#error "Need SpiderMonkey 1.8 or higher. Treehydra does not support older spidermonkeys due to lack of JS_AlreadyHasOwnProperty"
#endif
/* The entries in the map should be transitively rooted by 
   this->globalObj's current_function_decl property */
static struct pointer_map_t *jsobjMap = NULL;
/* this helps correlate js nodes to their C counterparts */
static int global_seq = 0;

typedef struct {
  treehydra_handler handler;
  void *data;
} lazy_handler;

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
static JSBool ResolveTreeNode (JSContext *cx, JSObject *obj, jsval id,
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
    const char *prop_name = JS_GetStringBytes(JSVAL_TO_STRING(id));
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
    return JS_CallFunctionValue (this->cx, this->globalObj, unhandled_property_handler,
                                 1, &id, &rval);
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

  void **ret = pointer_map_contains (jsobjMap, v);
  if (ret) {
    dehydra_defineProperty (this, parent, propname, (jsval) *ret);
    return (jsval) *ret;
  }

  const jsval jsret = get_lazy (this, handler, v, parent, propname);
 
  *pointer_map_insert (jsobjMap, v) = (void*) jsret;
  return jsret;
}

void lazy_tree_node (Dehydra *this, void *structure, JSObject *obj) {
  tree t = (tree)structure;
  const int myseq = ++global_seq;
  dehydra_defineProperty (this, obj, SEQUENCE_N, INT_TO_JSVAL (myseq));
  enum tree_code code = TREE_CODE (t);
  /* special case knowledge of non-decl nodes */
#ifndef __APPLE_CC__
  // no TS_BASE in mac gcc 42 and no easy way to check too
  convert_tree_node_union (this, TS_BASE, t, obj);
  if (!GIMPLE_TUPLE_P (t)) 
#endif
    {
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
                  const char *propname, int i) {
  jsval v = INT_TO_JSVAL (i);
  dehydra_defineProperty (this, parent, propname, v);
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
  void **v = pointer_map_contains (jsobjMap, *tp);
  int seq = 0;
  if (v) {
    JSObject *obj = JSVAL_TO_OBJECT ((jsval) *v);
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
JSBool JS_C_walk_tree(JSContext *cx, JSObject *obj, uintN argc,
                   jsval *argv, jsval *rval) {
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
  *rval = STRING_TO_JSVAL (JS_NewStringCopyZ (this->cx, gstr->str));
  free(gstr);
  return JS_TRUE;
}

void treehydra_plugin_pass (Dehydra *this) {
  xassert (!jsobjMap);
  /* the map is per-invocation 
   to cope with gcc mutating things */
  jsval rval;
  if (current_function_decl) {
    jsval process_tree = dehydra_getToplevelFunction(this, "process_tree");
    if (process_tree == JSVAL_VOID) return;

    jsobjMap = pointer_map_create ();   

    jsval fnval = 
      get_existing_or_lazy (this, lazy_tree_node, current_function_decl,
                            this->globalObj, "current_function_decl");
    xassert (JS_CallFunctionValue (this->cx, this->globalObj, process_tree,
                                   1, &fnval, &rval));
    JS_DeleteProperty (this->cx, this->globalObj, "current_function_decl");
  } else if (cgraph_nodes) {
    jsval process_cgraph = dehydra_getToplevelFunction(this, "process_cgraph");
    if (process_cgraph == JSVAL_VOID) return;
    
    jsobjMap = pointer_map_create ();   

    jsval cgraphval =
      get_existing_or_lazy (this, lazy_cgraph_node, cgraph_nodes,
                            this->globalObj, "cgraph_nodes");
    xassert (JS_CallFunctionValue (this->cx, this->globalObj, process_cgraph,
                                   1, &cgraphval, &rval));

    JS_DeleteProperty (this->cx, this->globalObj, "cgraph_nodes");
  }
  if (jsobjMap) {
    pointer_map_destroy (jsobjMap);
    jsobjMap = NULL;
  }
}

void treehydra_cp_pre_genericize (struct Dehydra *this, tree fndecl) {
  jsval process = dehydra_getToplevelFunction(this, "process_cp_pre_genericize");
  if (process == JSVAL_VOID) return;
  
  jsval rval;
  jsobjMap = pointer_map_create ();   

  jsval fnval =
    get_existing_or_lazy (this, lazy_tree_node, current_function_decl,
                          this->globalObj, "current_function_decl");
  xassert (JS_CallFunctionValue (this->cx, this->globalObj, process,
                                 1, &fnval, &rval));
  JS_DeleteProperty (this->cx, this->globalObj, "current_function_decl");
  pointer_map_destroy (jsobjMap);
  jsobjMap = NULL;

}

int treehydra_startup (Dehydra *this) {
  /* Check conditions that should hold for treehydra_generated.h */
  xassert (NULL == JSVAL_NULL && sizeof (void*) == sizeof (jsval));
  xassert (JS_DefineFunction (this->cx, this->globalObj, "C_walk_tree", 
                              JS_C_walk_tree, 0, 0));

  xassert (JS_InitClass(this->cx, this->globalObj, NULL
                        ,&js_tree_class , NULL, 0, NULL, NULL, NULL, NULL));
  xassert (!dehydra_includeScript (this, "treehydra.js"));
  return 0;
}
