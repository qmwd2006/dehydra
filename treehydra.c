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

#include "treehydra_generated.h"

static const char *SEQUENCE_N = "SEQUENCE_N";

static jsval convert_tree_node (Dehydra *this, tree t) {
  void **v;
  if (!t) return JSVAL_VOID;
  v = pointer_map_contains (jsobjMap, t);
  if (v) return (jsval) *v;

  if (cp_tree_node_structure ((union lang_tree_node*) t) != TS_CP_GENERIC) {
    *pointer_map_insert (jsobjMap, t) = (void*) JSVAL_VOID;
    return JSVAL_VOID;
  } 

  JSObject *obj = JS_ConstructObject (this->cx, &js_ObjectClass, NULL, 
                                      this->globalObj);
  const jsval jsvalObj = OBJECT_TO_JSVAL (obj);
  *pointer_map_insert (jsobjMap, t) = (void*) jsvalObj;
  dehydra_rootObject (this, jsvalObj);
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
  return jsvalObj;
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
