#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"
#include "rtl.h"
#include "tm_p.h"
#include "hard-reg-set.h"
#include "basic-block.h"
#include "output.h"
#include "errors.h"
#include "flags.h"
#include "function.h"
#include "expr.h"
#include "diagnostic.h"
#include "tree-flow.h"
#include "timevar.h"
#include "tree-dump.h"
#include "tree-pass.h"
#include "toplev.h"
#include "c-common.h"
#include "tree.h"
#include "cgraph.h"
#include "input.h"
/*C++ headers*/
#include "cp-tree.h"
#include "name-lookup.h"
#include "cxx-pretty-print.h"
static int processed = 0;

void process(tree);

// do a DFS of a TREE_CHAIN
void dfs_process_chain(tree t) {
  if (!t) return;
  dfs_process_chain(TREE_CHAIN(t));
  process(t);
}


/* stolen from gcc/cp/error.h */
static location_t
location_of (tree t)
{
  if (TREE_CODE (t) == PARM_DECL && DECL_CONTEXT (t))
    t = DECL_CONTEXT (t);
  else if (TYPE_P (t))
    t = TYPE_MAIN_DECL (t);
  else if (TREE_CODE (t) == OVERLOAD)
    t = OVL_FUNCTION (t);

  return DECL_SOURCE_LOCATION (t);
}

static char *locationbuf = NULL;

char const * loc(tree t) {
  location_t loc = location_of(t);
  if (!loc || loc == UNKNOWN_LOCATION) return NULL;
  size_t n = 0;
  n = locationbuf ? strlen(locationbuf) : 0;
  expanded_location eloc = expand_location(loc);
  size_t en = strlen(eloc.file) + 20;
  if (en > n) {
    free(locationbuf);
    locationbuf = xmalloc(en);
  }
  sprintf(locationbuf, "%s:%d:%d", eloc.file, eloc.line, eloc.column);
  return locationbuf;
}
#define TREE_HANDLER(name, var) static void process_##name(tree var)

TREE_HANDLER(namespace, ns) {
    if (DECL_NAMESPACE_ALIAS (ns)) return;

    fprintf(stderr, "namespace %s\n", DECL_NAME(ns) 
            ? IDENTIFIER_POINTER(DECL_NAME(ns))
            : "<anon>");
    struct cp_binding_level *level = NAMESPACE_LEVEL (ns);
    dfs_process_chain(level->names);
}

TREE_HANDLER(function, f) {
  if (DECL_IS_BUILTIN(f)) return;
  fprintf(stderr, "function %s\n", decl_as_string(f, 0xff));
}

TREE_HANDLER(type, t) {
  if (DECL_IS_BUILTIN(t)) return;
  fprintf(stderr, "%s: %s\n", loc(t), decl_as_string(t, TFF_DECL_SPECIFIERS));
}

void process(tree t) {
  switch(TREE_CODE(t)) {
  case NAMESPACE_DECL:
    return process_namespace(t);
  case FUNCTION_DECL:
    return process_function(t);
  case TYPE_DECL:
    return process_type(t);
  default:
    printf("unknown tree element: %s\n", tree_code_name[TREE_CODE(t)]);
  }
}

int gcc_plugin_main(tree t, const char* arg) {
  if (processed) return 0;
  
  process(global_namespace);
  free(locationbuf);
  return 0;
}

