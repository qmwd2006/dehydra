#include "gcc_cp_headers.h"

/*js*/
#include <jsapi.h>
#include "xassert.h"
#include "dehydra.h"
#include "dehydra_ast.h"
#include "dehydra_types.h"

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
  for (inst = DECL_VINDEX (td); inst; inst = TREE_CHAIN (inst)) {
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
}

static void process_record_or_union_type (tree c) {
  tree field, func;
  if (!COMPLETE_TYPE_P (c)) return;

  // bug 418170: don't process template specializations
  if (CLASSTYPE_USE_TEMPLATE(c) == 2) return;
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

static void process_type_decl (tree t) {
  /*  fprintf(stderr, "Taras:%s: %s, abstract:%d\n", loc(t),
      decl_as_string(t, TFF_DECL_SPECIFIERS), DECL_ABSTRACT(t));*/
  if (!DECL_ARTIFICIAL (t)) {
    // this is a typedef..i think
  } else {
    process_type(TREE_TYPE(t));
  }
}

static void process_field_decl(tree f) {
    return process_type(TREE_TYPE(f));
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
  if (type_decl) {
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
  switch (TREE_CODE (t)) {
  case NAMESPACE_DECL:
    return process_namespace_decl (t);
  case FUNCTION_DECL:
  case VAR_DECL:
    return process_decl (t);
  case TYPE_DECL:
    return process_type_decl (t);
  case FIELD_DECL:
    return process_field_decl (t);
  case TEMPLATE_DECL:
    return process_template_decl (t);
  default:
    /*error ( "unknown tree element: %s", tree_code_name[TREE_CODE(t)]);*/
    break;
  }
}

int gcc_plugin_init(const char *file, const char* arg, char **pass) {
  if (!arg) {
    error ("Use -fplugin-arg=<scriptname> to specify the dehydra script to run");
    return 1;
  }
  pset = pointer_set_create ();
  type_pset = pointer_set_create ();
  dehydra_init (&dehydra, file);
  return dehydra_startup (&dehydra, arg);
}

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
    tree_queue_head = q->next;
    free(q);
  }
  tree_queue_tail = NULL;
  process(global_namespace);

  postGlobalNamespace = 1;
  return 0;
}

void gcc_plugin_cp_pre_genericize(tree fndecl) {
  if (DECL_CLONED_FUNCTION_P (fndecl)) return;
  if (DECL_ARTIFICIAL(fndecl)) return;
  
  dehydra_cp_pre_genericize(&dehydra, fndecl, postGlobalNamespace);
}

void gcc_plugin_finish_struct (tree t) {
  dehydra_finishStruct (&dehydra, t);
  /* It's lame but types are still instantiated after post_parse
    when some of the stuff saved by dehydra_cp_pre_genericize has been 
    freed by GCC */
  if (postGlobalNamespace) {
    process_type (t);
    return;
  }
  /* Appending stuff to the queue instead of 
     processing immediately is because gcc is overly
     lazy and does some things (like setting anonymous
     struct names) sometime after completing the type */
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
