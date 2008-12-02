/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
#include "gcc_cp_headers.h"

/*js*/
#include <jsapi.h>
#include "xassert.h"
#include "dehydra.h"
#include "dehydra_ast.h"
#include "dehydra_types.h"
#ifdef TREEHYDRA_PLUGIN
#include "treehydra.h"

/* True if initialization has been completed. */
static int init_finished = 0;

/* Plugin pass position. This is kept here so that JS can set it. */
static char *after_gcc_pass = 0;
#endif

static int processed = 0;
static Dehydra dehydra = {0};

static void process(tree);
static void process_type(tree t);

// do a DFS of a TREE_CHAIN
void dfs_process_chain(tree t) {
  if (!t) return;
  dfs_process_chain(TREE_CHAIN(t));
  process(t);
}

static void process_template_decl (tree td) {
  tree inst;
  // process all of the template instantiations too
  for (inst = DECL_TEMPLATE_INSTANTIATIONS (td); inst; inst = TREE_CHAIN (inst)) {
    tree record_type = TREE_VALUE (inst);
    process_type (RECORD_TYPE_CHECK (record_type));
  }
}

static void process_namespace_decl (tree ns) {
  if (DECL_NAMESPACE_ALIAS (ns)) return;
  
  struct cp_binding_level *level = NAMESPACE_LEVEL (ns);
  dfs_process_chain(level->names);
  for (ns = level->namespaces; ns; ns = TREE_CHAIN(ns)) {
    process_namespace_decl(ns);
  }
}

static void process_decl (tree f) {
  dehydra_visitDecl (&dehydra, f);
#ifdef TREEHYDRA_PLUGIN
  treehydra_call_js (&dehydra, "process_tree_decl", f);
#endif
}

static void process_record_or_union_type (tree c) {
  tree field, func;
  if (!COMPLETE_TYPE_P (c)) return;

  // bug 418170: don't process template specializations
  if (isGPlusPlus() && CLASSTYPE_USE_TEMPLATE(c) == 2) {
    // bug 449428: want all template decls
    process (TYPE_NAME (c));
    return;
  }
  //fprintf(stderr, "class %s\n", type_as_string(c, 0));

  /* iterate over base classes to ensure they are visited first */
  tree binfo = TYPE_BINFO (c);
  int n_baselinks = binfo ? BINFO_N_BASE_BINFOS (binfo) : 0;
  int i;
  for (i = 0; i < n_baselinks; i++)
    {
      tree base_binfo = BINFO_BASE_BINFO (binfo, i);
      process_type (BINFO_TYPE (base_binfo));
    }

  /* Output all the method declarations in the class.  */
  for (func = TYPE_METHODS (c) ; func ; func = TREE_CHAIN (func)) {
    if (DECL_ARTIFICIAL(func)) continue;
    /* Don't output the cloned functions.  */
    if (DECL_CLONED_FUNCTION_P (func)) continue;
    process(func);
  }

  for (field = TYPE_FIELDS (c) ; field ; field = TREE_CHAIN (field)) {
    if (DECL_ARTIFICIAL(field) && !DECL_IMPLICIT_TYPEDEF_P(field)) continue;
    // ignore typedef of self field
    // my theory is that the guard above takes care of this one too
    if (TREE_CODE (field) == TYPE_DECL 
        && TREE_TYPE (field) == c) continue;
     
    process(field);
    //fprintf(stderr, "%s: member %s\n", loc(c), tree_code_name[TREE_CODE(field)]);
  }
  xassert (COMPLETE_TYPE_P (c));
  dehydra_visitType (&dehydra, c);
  //fprintf(stderr, "/class //%s\n", type_as_string(c, 0));
}

static void process_enumeral_type (tree enumeral) {
  dehydra_visitType (&dehydra, enumeral);
}

// guard against duplicate visits
static struct pointer_set_t *pset = NULL;
static struct pointer_set_t *type_pset = NULL;

static void process_type(tree t) {
  if (pointer_set_insert (type_pset, t)) {
    return;
  }

  // dmandelin@mozilla.com -- bug 420299
  // We need to process the original type first, because it will be the target
  // of the typedef field. This is the natural extension of the DFS strategy.
  tree type_decl = TYPE_NAME (t);
  if (type_decl && TREE_CODE (type_decl) == TYPE_DECL) {
    tree original_type = DECL_ORIGINAL_TYPE (type_decl);
    if (original_type) {
      process_type(original_type);
    }
  }

  switch (TREE_CODE(t)) {
  case RECORD_TYPE:
  case UNION_TYPE:
    return process_record_or_union_type (t);
  case ENUMERAL_TYPE:
    return process_enumeral_type (t);
  default:
    /*    fprintf(stderr, "Unhandled type:%s\n", tree_code_name[TREE_CODE(t)]);*/
    break;
  }
}

static void process (tree t) {
  xassert (DECL_P (t));
  if (pointer_set_insert (pset, t)) {
    return;
  }
  if  (TREE_CODE (t) != NAMESPACE_DECL
       && DECL_IS_BUILTIN (t)) {
    return;
  }

  tree tree_type = TREE_TYPE (t);
  bool is_template_typedef = tree_type
    && TREE_CODE (tree_type) == RECORD_TYPE
    && TYPE_TEMPLATE_INFO (tree_type);
  bool is_artifical = 
    (TREE_CODE (t) == TYPE_DECL && DECL_IMPLICIT_TYPEDEF_P (t))
    || DECL_ARTIFICIAL (t) || DECL_IS_BUILTIN (t);
  // templates actually have useful info in the typedefs
  if (is_template_typedef || !is_artifical)
    process_decl (t);
  switch (TREE_CODE (t)) {
  case NAMESPACE_DECL:
    return process_namespace_decl (t);
  case TEMPLATE_DECL:
    return process_template_decl (t);
  case FIELD_DECL:
  case CONST_DECL:
  case TYPE_DECL:
  case FUNCTION_DECL:
  case VAR_DECL:
    return process_type (tree_type);
  default:
    xassert(!DECL_P (t));
    /*error ( "unknown tree element: %s", tree_code_name[TREE_CODE(t)]);*/
    break;
  }
}

int gcc_plugin_init(const char *file, const char* arg, char **pass) {
  char *script, *rest;

  if (!arg) {
    error ("Use -fplugin-arg=<scriptname> to specify the dehydra script to run");
    return 1;
  }
  pset = pointer_set_create ();
  type_pset = pointer_set_create ();
  dehydra_init (&dehydra, file);
  int ret = dehydra_startup (&dehydra);
  if (ret) return ret;
#ifdef TREEHYDRA_PLUGIN
  ret = treehydra_startup (&dehydra);
  if (ret) return ret;
#endif

  script = xstrdup(arg);
  rest = strchr(script, ' ');
  if (rest) {
    *rest = '\0';
    ++rest;
  }
  else {
    rest = "";
  }

  dehydra_defineStringProperty(&dehydra, dehydra.globalObj, "_arguments", rest);
  dehydra_appendDirnameToPath (&dehydra, script);
  ret = dehydra_includeScript (&dehydra, script);

  free(script);

#ifdef TREEHYDRA_PLUGIN
  /* This has to come after including the user's script, because once
   * init_finished is set, the user can't set the gcc pass. */
  init_finished = 1;
  if (after_gcc_pass)
    *pass = after_gcc_pass;
#endif
  return ret;
}

#ifdef TREEHYDRA_PLUGIN
/* API function to set the plugin pass position. This should be called
 * only before/during plugin initialization. 
 * Return nonzero on failure. */
int set_after_gcc_pass(const char *pass) {
  if (init_finished) return 1;
  if (after_gcc_pass) free(after_gcc_pass);
  after_gcc_pass = xstrdup(pass);
  return 0;
}

void gcc_plugin_pass (void) {
  // TODO  This is rather painful, but we really don't have any other
  //       place to put it.
  if (after_gcc_pass) {
    free(after_gcc_pass);
    after_gcc_pass = 0;
  }

  treehydra_call_js (&dehydra, "process_tree", current_function_decl);
}
#endif

/* template instations happen late
   and various code gets nuked so we can't hook into them
   the generic way. Thus this hack to make
   dehydra_cp_pre_genericize call dehydra_visitDecl directly */
static bool postGlobalNamespace = 0;

/* The queue stuff is used to get a deterministic order to types */
typedef struct tree_queue {
  tree t;
  struct tree_queue *next;
} tree_queue;

static tree_queue *tree_queue_head = NULL;
static tree_queue *tree_queue_tail = NULL;

int gcc_plugin_post_parse() {
  if (processed || errorcount) return 0;
  processed = 1;
  
  /* first visit recorded structs */
  while(tree_queue_head) {
    tree_queue *q = tree_queue_head;
    process_type (q->t);
#ifdef TREEHYDRA_PLUGIN
    treehydra_call_js (&dehydra, "process_tree_type", q->t);
#endif
    tree_queue_head = q->next;
    free(q);
  }
  tree_queue_tail = NULL;
  if (global_namespace)
    process(global_namespace);

  postGlobalNamespace = 1;
  return 0;
}

void gcc_plugin_cp_pre_genericize(tree fndecl) {
  if (DECL_CLONED_FUNCTION_P (fndecl)) return;
  if (DECL_ARTIFICIAL(fndecl)) return;
  
  dehydra_cp_pre_genericize(&dehydra, fndecl, postGlobalNamespace);
#ifdef TREEHYDRA_PLUGIN
  treehydra_call_js (&dehydra, "process_cp_pre_genericize", fndecl);
#endif
}

void gcc_plugin_finish_struct (tree t) {
  dehydra_finishStruct (&dehydra, t);

  /* It's lame but types are still instantiated after post_parse
    when some of the stuff saved by dehydra_cp_pre_genericize has been 
    freed by GCC */
  if (postGlobalNamespace) {
    process_type (t);
#ifdef TREEHYDRA_PLUGIN
    treehydra_call_js (&dehydra, "process_tree_type", t);
#endif
    return;
  }
  /* Appending stuff to the queue instead of 
     processing immediately is because gcc is overly
     lazy and does some things (like setting anonymous
     struct names) sometime after completing the type */
  xassert(!tree_queue_tail || tree_queue_head);
  tree_queue *q = xmalloc (sizeof (tree_queue));
  q->t = t;
  q->next = NULL;
  if (tree_queue_tail)
    tree_queue_tail->next = q;
  else if (!tree_queue_head)
    tree_queue_head = q;
  tree_queue_tail = q;
}

void gcc_plugin_finish () {
  pointer_set_destroy (pset);
  pset = NULL;
  pointer_set_destroy (type_pset);
  type_pset = NULL;
  dehydra_input_end (&dehydra);
}
