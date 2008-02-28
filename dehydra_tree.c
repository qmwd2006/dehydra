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

static jsval convert_enum (Dehydra *this, const char *name, int value) {
  jsval v = dehydra_getToplevelObject (this, name);
  if (v != JSVAL_VOID)
    return v;
  
  char buf[1024];
  int len = sprintf (buf, "this.%s = new EnumValue (\"%s\", %d)",
                     name, name, value);
  xassert (JS_EvaluateScript (this->cx, this->globalObj, buf, len,
                              "", 1, &v));
  return v;
}

#include "treehydra_generated.h"

static jsval convert_tree_node (Dehydra *this, tree t) {
  void **v;
  if (!t) return JSVAL_VOID;
  v = pointer_map_contains (jsobjMap, t);
  if (v) return (jsval) *v;

  JSObject *obj = JS_ConstructObject (this->cx, &js_ObjectClass, NULL, 
                                      this->globalObj);
  *pointer_map_insert (jsobjMap, t) = obj;
  dehydra_rootObject (this, OBJECT_TO_JSVAL (obj));
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
  return OBJECT_TO_JSVAL (obj);
}

static tree
walk_n_test (tree *tp, int *walk_subtrees, void *data)
{
  enum tree_code code = TREE_CODE (*tp);
  fprintf(stderr, "walking tree element: %s \n", tree_code_name[code]);
    //  *walk_subtrees = 0;
  return NULL_TREE;
}

void dehydra_plugin_pass (Dehydra *this) {
  jsval process_tree = dehydra_getToplevelObject(this, "process_tree");
  if (process_tree == JSVAL_VOID) return;

  if (!jsobjMap) {
    jsobjMap = pointer_map_create ();
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
  walk_tree (&body_chain, walk_n_test, NULL, NULL);
  dehydra_unrootObject (this, fnkey);
}
