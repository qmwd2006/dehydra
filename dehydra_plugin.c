#include "dehydra.h"
#include "pointer-set.h"
#include "errors.h"
/*C++ headers*/
#include "cp-tree.h"
#include "cxx-pretty-print.h"

static int processed = 0;

static void process(tree);
static void process_type(tree t);

// do a DFS of a TREE_CHAIN
void dfs_process_chain(tree t) {
  if (!t) return;
  dfs_process_chain(TREE_CHAIN(t));
  process(t);
}

/* stolen from gcc/cp/error.h */
location_t
location_of (tree t)
{
  if (TREE_CODE (t) == PARM_DECL && DECL_CONTEXT (t))
    t = DECL_CONTEXT (t);
  else if (TYPE_P (t))
    t = TYPE_MAIN_DECL (t);
  else if (TREE_CODE (t) == OVERLOAD)
    t = OVL_FUNCTION (t);

  if (DECL_P(t))
    return DECL_SOURCE_LOCATION (t);
  else if (EXPR_P(t) && EXPR_HAS_LOCATION(t))
    return EXPR_LOCATION(t);
  else
    return UNKNOWN_LOCATION;
}

static char *locationbuf = NULL;

char const * loc_as_string (location_t loc) {
  if (!loc || loc == UNKNOWN_LOCATION) return NULL;
  size_t n = 0;
  n = locationbuf ? strlen(locationbuf) : 0;
  expanded_location eloc = expand_location(loc);
  size_t en = strlen(eloc.file) + 20;
  if (en > n) {
    free(locationbuf);
    locationbuf = (char*) xmalloc(en);
  }
  sprintf(locationbuf, "%s:%d:%d", eloc.file, eloc.line, eloc.column);
  return locationbuf;
}

#define TREE_HANDLER(name, var) static void process_##name(tree var)

TREE_HANDLER (template_decl, td) {
  tree inst;
  for (inst = DECL_VINDEX (td); inst; inst = TREE_CHAIN (inst)) {
    tree record_type = TREE_VALUE (inst);
    xassert (TREE_CODE (record_type) == RECORD_TYPE);
    process_type (record_type);
  }

}

TREE_HANDLER (namespace_decl, ns) {
  if (DECL_NAMESPACE_ALIAS (ns)) return;
  
  struct cp_binding_level *level = NAMESPACE_LEVEL (ns);
  dfs_process_chain(level->names);
  for (ns = level->namespaces; ns; ns = TREE_CHAIN(ns)) {
    process_namespace_decl(ns);
  }
}

TREE_HANDLER (function_decl, f) {
  tree body_chain = DECL_SAVED_TREE(f);
  if (!body_chain) return;

  /* cp_walk_tree_without_duplicates(&body_chain, decl_walker, NULL);*/
  if (!visitFunction(f)) return;
  /* fprintf(stderr, "%s: function %s\n", loc(f), decl_as_string(f, 0xff));*/
  postvisitFunction(f);
}

TREE_HANDLER (record_type, c) {
  tree field, func;
  if (!COMPLETE_TYPE_P (c)) return;
  //fprintf(stderr, "class %s\n", type_as_string(c, 0));

  /* iterate over base classes to ensure they are visited first */
  tree binfo = TYPE_BINFO (c);
  int n_baselinks = BINFO_N_BASE_BINFOS (binfo);
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
  if (!visitClass(c)) return;
  postvisitClass(c);
  //fprintf(stderr, "/class //%s\n", type_as_string(c, 0));
}

TREE_HANDLER(type_decl, t) {
  /*  fprintf(stderr, "Taras:%s: %s, abstract:%d\n", loc(t),
      decl_as_string(t, TFF_DECL_SPECIFIERS), DECL_ABSTRACT(t));*/
  if (!DECL_ARTIFICIAL (t)) {
    // this is a typedef..i think
  } else {
    process_type(TREE_TYPE(t));
  }
}

TREE_HANDLER(field_decl, f) {
    return process_type(TREE_TYPE(f));
}

// guard against duplicate visits
static struct pointer_set_t *pset = NULL;
static struct pointer_set_t *type_pset = NULL;

static void process_type(tree t) {
  if (pointer_set_insert (type_pset, t)) {
    return;
  }

  switch (TREE_CODE(t)) {
  case RECORD_TYPE:
    return process_record_type(t);
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
    return process_function_decl (t);
  case TYPE_DECL:
    return process_type_decl (t);
  case FIELD_DECL:
    return process_field_decl (t);
  case TEMPLATE_DECL:
    return process_template_decl (t);
  default:
    /*    fprintf(stderr, "unknown tree element: %s\n", tree_code_name[TREE_CODE(t)]);*/
    break;
  }
}

int gcc_plugin_init(const char *file, const char* arg) {
  if (!arg) {
    error ("Use -fplugin-arg=<scriptname> to specify the dehydra script to run");
    return 1;
  }
  initDehydra(file, arg);
  return 0;
}

int gcc_plugin_post_parse() {
  if (processed) return 0;
  processed = 1;

  pset = pointer_set_create ();
  type_pset = pointer_set_create ();
  
  process(global_namespace);

  pointer_set_destroy (pset);
  pointer_set_destroy (type_pset);

  dehydra_input_end();
  free(locationbuf);
  locationbuf = NULL;
  return 0;
}

void gcc_plugin_cp_pre_genericize(tree fndecl) {
  if (DECL_CLONED_FUNCTION_P (fndecl)) return;
  if (DECL_ARTIFICIAL(fndecl)) return;

  dehydra_cp_pre_genericize(fndecl);
}

/* Attach attributes that would otherwise be dropped */
void gcc_plugin_decl_attributes (tree node, tree attributes, int flags) {
  if (TREE_CODE (node) == RECORD_TYPE && !TYPE_ATTRIBUTES (node) && attributes && !strcmp("nsRegion", type_as_string (node, 0))
      && (flags & ATTR_FLAG_TYPE_IN_PLACE)) {
    tree a;
    /* big hack to make gcc happy
       The variants diverge if they aren't kept in sync and then gcc complains angrily 
       see decl_attributes in gcc if something breaks
    */
    for (a = attributes; a; a = TREE_CHAIN (a))
      {
        tree name = TREE_PURPOSE (a);
        tree args = TREE_VALUE (a);
        tree old_attrs = TYPE_ATTRIBUTES (node);
        TYPE_ATTRIBUTES (node) = tree_cons (name, args, old_attrs);
        /* If this is the main variant, also push the attributes
           out to the other variants.  */
        if (node == TYPE_MAIN_VARIANT (node))
          {
            tree variant;
            for (variant = node; variant;
                 variant = TYPE_NEXT_VARIANT (variant))
              {
                if (TYPE_ATTRIBUTES (variant) == old_attrs)
                  TYPE_ATTRIBUTES (variant)
                    = TYPE_ATTRIBUTES (node);
                /*else if (!lookup_attribute
                         (spec->name, TYPE_ATTRIBUTES (variant)))
                  TYPE_ATTRIBUTES (variant) = tree_cons
                  (name, args, TYPE_ATTRIBUTES (variant));*/
              }
          }
      }
  }
}
