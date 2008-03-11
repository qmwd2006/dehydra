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

static jsval convert_tree_node (Dehydra *this, tree t);

static struct pointer_map_t *jsobjMap = NULL;
/* this helps correlate js nodes to their C counterparts */
static int global_seq = 0;

static jsval convert_int (Dehydra *this, int i) {
  return INT_TO_JSVAL (i);
}
 
static jsval convert_char_star (Dehydra *this, const char *str) {
  return STRING_TO_JSVAL (JS_NewStringCopyZ (this->cx, str));
}

static jsval get_enum_value (Dehydra *this, const char *name) {
  jsval val = JSVAL_VOID;
  JS_GetProperty(this->cx, this->globalObj, name, &val);
  if (val == JSVAL_VOID) {
    error ("EnumValue '%s' not found. enums.js isn't loaded", name);
  }
  return val;
}

// cleanup some gcc polution in GCC trunk as of Mar 5, 2008
#ifdef in_function_try_handler
#undef in_function_try_handler
#endif

#ifdef in_base_initializer
#undef in_base_initializer
#endif

/* put these into a custom rooted array
   to do region-style deallocation at
   the end of the pass*/
typedef void (*treehydra_handler)(Dehydra *this, void *structure, JSObject *obj);

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
  lazy_handler *lazy = JS_GetPrivate (cx, obj);
  if (!lazy)
    return JS_FALSE;
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

static jsval get_lazy (Dehydra *this, treehydra_handler handler, void *v) {
  JSObject *obj = JS_NewObject (this->cx, &js_tree_class, NULL, 
                                      this->globalObj);
  const jsval jsvalObj = OBJECT_TO_JSVAL (obj);
  dehydra_rootObject (this, jsvalObj);
  lazy_handler *lazy = xmalloc (sizeof (lazy_handler));
  lazy->handler = handler;
  lazy->data = v;
  JS_SetPrivate (this->cx, obj, lazy);
  return jsvalObj;
}

/* This either returnes a cached object or creates a new lazy object */
static jsval get_existing_or_lazy (Dehydra *this, treehydra_handler handler, void *v) {
  if (!v) return JSVAL_VOID;
  void **ret = pointer_map_contains (jsobjMap, v);
  if (ret) return (jsval) *ret;

  jsval jsvalObj = get_lazy (this, handler, v);
 
  *pointer_map_insert (jsobjMap, v) = (void*) jsvalObj;
  return jsvalObj;
}

static void lazy_tree_node (Dehydra *this, void *structure, JSObject *obj);

#include "treehydra_generated.h"

static void lazy_tree_node (Dehydra *this, void *structure, JSObject *obj) {
  tree t = (tree)structure;
  const int myseq = ++global_seq;
  dehydra_defineProperty (this, obj, SEQUENCE_N, INT_TO_JSVAL (myseq));
  enum tree_node_structure_enum i;
  enum tree_code code = TREE_CODE (t);
  i = tree_node_structure(t);
  convert_tree_node_union (this, TS_BASE, t, obj);
  convert_tree_node_union (this, i, t, obj);
  for (i = 0; i < LAST_TS_ENUM;i++) {
    if (tree_contains_struct[code][i]) {
      convert_tree_node_union (this, i, t, obj);
    }
  }
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

void treehydra_plugin_pass (Dehydra *this) {
  jsval process_tree = dehydra_getToplevelObject(this, "process_tree");
  if (process_tree == JSVAL_VOID) return;

  /* the map is per-invocation 
   to cope with gcc mutating things */
  jsobjMap = pointer_map_create ();
  if (dehydra_getToplevelObject(this, "C_walk_tree") == JSVAL_VOID) {
    xassert (JS_DefineFunction (this->cx, this->globalObj, "C_walk_tree", 
                                JS_C_walk_tree, 0, 0));
  }
  int fnkey = dehydra_getArrayLength (this,
                                      this->rootedArgDestArray);
  JSObject *fObj = dehydra_addVar (this, current_function_decl,
                                   this->rootedArgDestArray);
  tree body_chain = DECL_SAVED_TREE (current_function_decl);
  if (body_chain && TREE_CODE (body_chain) == BIND_EXPR) {
    body_chain = BIND_EXPR_BODY (body_chain);
  }
  jsval bodyVal = get_existing_or_lazy (this, lazy_tree_node, (void*)body_chain); //convert_tree_node (this, body_chain);
  jsval rval, argv[2];
  argv[0] = OBJECT_TO_JSVAL (fObj);
  argv[1] = bodyVal;
  xassert (JS_CallFunctionValue (this->cx, this->globalObj, process_tree,
                                 sizeof (argv)/sizeof (argv[0]), argv, &rval));
  dehydra_unrootObject (this, fnkey);
  pointer_map_destroy (jsobjMap);
  jsobjMap = NULL;
}
