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
#include "dehydra_builtins.h"
#include "util.h"
#include "dehydra.h"
#include "dehydra_types.h"
#include "treehydra.h"

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

static JSBool ResolveTreeNode (JSContext *cx, JSObject *obj, jsval id) {
  Dehydra *this = JS_GetContextPrivate (cx);
  /* when the going gets tough will be able to implement
   unions using the id field.for now avoiding it at all cost*/
  lazy_handler *lazy = JS_GetPrivate (cx, obj);
  if (!lazy) {
    /* This exists because spidermonkey occasionally doesn't report errors */
    jsval process_tree = dehydra_getToplevelFunction(this, "unhandledLazyProperty");
    jsval rval;
    return JS_CallFunctionValue (this->cx, this->globalObj, process_tree,
                                 1, &id, &rval);
  }
  // once the materialize the object, no need to keep private data
  JS_SetPrivate (cx, obj, NULL);
  lazy->handler (this, lazy->data, obj);
  free (lazy);
  return JS_TRUE;
}

static JSClass js_tree_class = {
  "tree",  /* name */
  JSCLASS_CONSTRUCT_PROTOTYPE | JSCLASS_HAS_PRIVATE, /* flags */
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
  JS_EnumerateStub, ResolveTreeNode, JS_ConvertStub, tree_finalize,
  NULL, NULL, NULL, tree_construct, NULL, NULL, NULL, NULL
};

/* setups a lazy object. transitively rooted through parent */
jsval get_lazy (Dehydra *this, treehydra_handler handler, void *v,
                       JSObject *parent, const char *propname) {
  JSObject *obj;
  jsval jsvalObj;
  xassert (parent && propname);
    
  obj = 
    JS_DefineObject(this->cx, parent,
                    propname, &js_tree_class, NULL,
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
  inline jsval assign_existing (jsval val) {
    dehydra_defineProperty (this, parent, propname, val);
    return val;
  }

  if (!v)
    return assign_existing (JSVAL_VOID);

  void **ret = pointer_map_contains (jsobjMap, v);
  if (ret)
    return assign_existing ((jsval) *ret);
  
  const jsval jsret = get_lazy (this, handler, v, parent, propname);
 
  *pointer_map_insert (jsobjMap, v) = (void*) jsret;
  return jsret;
}

void lazy_tree_node (Dehydra *this, void *structure, JSObject *obj) {
  tree t = (tree)structure;
  const int myseq = ++global_seq;
  dehydra_defineProperty (this, obj, SEQUENCE_N, INT_TO_JSVAL (myseq));
  enum tree_node_structure_enum i;
  enum tree_code code = TREE_CODE (t);
  i = tree_node_structure(t);
  /* special case knowledge of non-decl nodes */
  convert_tree_node_union (this, TS_BASE, t, obj);
  if (!GIMPLE_TUPLE_P (t)) {
    convert_tree_node_union (this, TS_COMMON, t, obj);
  }
  convert_tree_node_union (this, i, t, obj);
  /* stuff below is empty for non-decls */
  if (!DECL_P (t)) return;
  for (i = 0; i < LAST_TS_ENUM;i++) {
    if (tree_contains_struct[code][i]) {
      convert_tree_node_union (this, i, t, obj);
    }
  }
}

/* Next 3 functions are for treehydra_generated */
jsval get_enum_value (struct Dehydra *this, const char *name) {
  jsval val = JSVAL_VOID;
  JS_GetProperty(this->cx, this->globalObj, name, &val);
  if (val == JSVAL_VOID) {
    error ("EnumValue '%s' not found. enums.js isn't loaded", name);
  }
  return val;
}

jsval convert_char_star (struct Dehydra *this, const char *str) {
  return STRING_TO_JSVAL (JS_NewStringCopyZ (this->cx, str));
}

jsval convert_int (struct Dehydra *this, int i) {
  return INT_TO_JSVAL (i);
}

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
  if (body_chain && TREE_CODE (body_chain) == BIND_EXPR) {
    body_chain = BIND_EXPR_BODY (body_chain);
  }
  struct pointer_set_t *pset = pointer_set_create ();
  walk_tree (&body_chain, walk_n_test, &gstr, pset);
  pointer_set_destroy (pset);
  /* remove last \n */
  if (gstr->length)
    gstr->str[gstr->length - 1] = 0;
  *rval = convert_char_star (this, gstr->str);
  free(gstr);
  return JS_TRUE;
}

struct JSObject *dehydra_defineArrayProperty (struct Dehydra *this,
                                              struct JSObject *obj,
                                              char const *name,
                                              int length) {
  JSObject *destArray = JS_NewArrayObject (this->cx, length, NULL);
  dehydra_defineProperty (this, obj, name, OBJECT_TO_JSVAL (destArray));
  return destArray;  
}

void treehydra_plugin_pass (Dehydra *this) {
  jsval process_tree = dehydra_getToplevelFunction(this, "process_tree");
  if (process_tree == JSVAL_VOID) return;

  /* the map is per-invocation 
   to cope with gcc mutating things */
  jsobjMap = pointer_map_create ();
  if (dehydra_getToplevelFunction(this, "C_walk_tree") == JSVAL_VOID) {
    /* Check conditions that should hold for treehydra_generated.h */
    xassert (NULL == JSVAL_NULL && sizeof (void*) == sizeof (jsval));
    xassert (JS_DefineFunction (this->cx, this->globalObj, "C_walk_tree", 
                                JS_C_walk_tree, 0, 0));
  }

  jsval fnval  = get_existing_or_lazy (this, lazy_tree_node, current_function_decl, this->globalObj, "current_function_decl");
  jsval rval;
  xassert (JS_CallFunctionValue (this->cx, this->globalObj, process_tree,
                                 1, &fnval, &rval));
  JS_DeleteProperty (this->cx, this->globalObj, "current_function_decl");
  pointer_map_destroy (jsobjMap);
  jsobjMap = NULL;
}
