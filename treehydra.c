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
#include "dehydra_builtins.h"
#include "util.h"
#include "dehydra.h"
#include "dehydra_types.h"

static jsval convert_tree_node (Dehydra *this, tree t);

static struct pointer_map_t *jsobjMap = NULL;

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

#include "treehydra_generated.h"

static jsval convert_tree_node (Dehydra *this, tree t) {
  void **v;
  if (!t) return JSVAL_VOID;
  v = pointer_map_contains (jsobjMap, t);
  if (v) return (jsval) *v;

  JSObject *obj = JS_ConstructObject (this->cx, &js_ObjectClass, NULL, 
                                      this->globalObj);
  const jsval jsvalObj = OBJECT_TO_JSVAL (obj);
  *pointer_map_insert (jsobjMap, t) = (void*) jsvalObj;
  dehydra_rootObject (this, jsvalObj);
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
  return jsvalObj;
}

static tree
walk_n_test (tree *tp, int *walk_subtrees, void *data)
{
  enum tree_code code = TREE_CODE (*tp);
  char **buf = (char**)data;
  size_t *capacity = (size_t*) *buf;
  size_t *len = ((size_t*) *buf) + 1;
  if (*len + 100 > *capacity) {
    *capacity *= 2;
    *buf = xrealloc (*buf, *capacity);
  }
  /* point at the end of the string*/
  char *str = *buf + sizeof(size_t) * 2 + *len;
  *len += sprintf(str, "%s\n", tree_code_name[code]);
  fprintf(stderr, "walking tree element: %s %p\n", tree_code_name[code], *tp);
    //  *walk_subtrees = 0;
  return NULL_TREE;
}

JSBool JS_C_walk_tree(JSContext *cx, JSObject *obj, uintN argc,
                   jsval *argv, jsval *rval) {
  size_t capacity = 1024;
  size_t len = 0;
  char *buf = xrealloc (NULL, capacity);
  ((size_t*)buf)[0] = capacity;
  ((size_t*)buf)[1] = len;
  char *str = buf + sizeof(size_t)*2;
  tree body_chain = DECL_SAVED_TREE (current_function_decl);
  if (body_chain && TREE_CODE (body_chain) == BIND_EXPR) {
    body_chain = BIND_EXPR_BODY (body_chain);
  }
  struct pointer_set_t *pset = pointer_set_create ();
  walk_tree (&body_chain, walk_n_test, &buf, pset);
  pointer_set_destroy (pset);
  Dehydra *this = JS_GetContextPrivate (this->cx);
  *rval = convert_char_star (this, str);
  free(buf);
  return JS_TRUE;
}

void treehydra_plugin_pass (Dehydra *this) {
  jsval process_tree = dehydra_getToplevelObject(this, "process_tree");
  if (process_tree == JSVAL_VOID) return;

  if (!jsobjMap) {
    jsobjMap = pointer_map_create ();
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
  jsval bodyVal = convert_tree_node (this, body_chain);
  jsval rval, argv[2];
  argv[0] = OBJECT_TO_JSVAL (fObj);
  argv[1] = bodyVal;
  xassert (JS_CallFunctionValue (this->cx, this->globalObj, process_tree,
                                 sizeof (argv)/sizeof (argv[0]), argv, &rval));
  dehydra_unrootObject (this, fnkey);
}
