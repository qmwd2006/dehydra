/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Dehydra and Treehydra scriptable static analysis tools
 * Copyright (C) 2007-2010 The Mozilla Foundation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "dehydra-config.h"

/*js*/
#include <jsapi.h>
// cgraph.h conflicts with nspr
#undef TEST_BIT

#include "gcc_cp_headers.h"

#include "xassert.h"
#include "dehydra.h"
#include "dehydra_ast.h"
#include "dehydra_types.h"
#include "util.h"
#include "cgraph.h"
#include "tree-pass.h"
#ifdef TREEHYDRA_PLUGIN
#include "treehydra.h"
#include "tree-pass.h"

/* True if initialization has been completed. */
static int init_finished = 0;

/* Plugin pass position. This is kept here so that JS can set it. */
static char *after_gcc_pass = 0;
#endif

#ifndef CFG_PLUGINS_MOZ
#include "gcc-plugin.h"
#include "plugin.h"
#define PLUGIN_HANDLER_RETURN return
#define NOT_MOZ_PLUGINS(x) ,x
#else
#define NOT_MOZ_PLUGINS(x)
#include <version.h>
#define PLUGIN_HANDLER_RETURN return 0
#define MAYBE_CLASS_TYPE_P(tree_type) (TREE_CODE(tree_type) == RECORD_TYPE)
#endif

/* Queue up tree object for latest processing (ie because gcc will fill in more info or 
   loose track of them)
 */
static VEC(tree,heap) *tree_queue_vec = NULL;

static int processed = 0;
static Dehydra dehydra = {0};

static void process(tree);
static void process_type(tree t);
static void dehydra_ehqueue_tree (tree t);

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
    if (TREE_CODE (record_type) == RECORD_TYPE
	|| TREE_CODE (record_type) == UNION_TYPE
	|| TREE_CODE (record_type) == QUAL_UNION_TYPE)
      process_type (record_type);
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
  if (TREE_CODE(f) == TYPE_DECL && DECL_ORIGINAL_TYPE(f)) {
    f = TREE_TYPE(f);
    dehydra_visitType (&dehydra, f);
#ifdef TREEHYDRA_PLUGIN
    treehydra_call_js (&dehydra, "process_tree_type", f);
#endif
  }
  else {
    dehydra_visitDecl (&dehydra, f);
#ifdef TREEHYDRA_PLUGIN
    treehydra_call_js (&dehydra, "process_tree_decl", f);
#endif
  }
  JS_MaybeGC(dehydra.cx);
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

  if (EXCEPTIONAL_CLASS_P(t)) {
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
    process_record_or_union_type (t);
    break;
  case ENUMERAL_TYPE:
    process_enumeral_type (t);
    break;
  default:
    /*    fprintf(stderr, "Unhandled type:%s\n", tree_code_name[TREE_CODE(t)]);*/
    break;
  }

  JS_MaybeGC(dehydra.cx);
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
    && MAYBE_CLASS_TYPE_P (tree_type)
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
  // nothing interesting in using decls
  case USING_DECL:
    return;
  default:
    /*error ( "Dehydra unknown tree element: %s", tree_code_name[TREE_CODE(t)]);*/
    xassert(!DECL_P (t));
    break;
  }
}

#define STRINGIFY(x) #x

int gcc_plugin_init(const char *file, const char* arg, char **pass
                    NOT_MOZ_PLUGINS(const char *version_string)) {
  char *script, *rest;
  const char *arguments = "";

  if (!arg) {
    error ("Use " STRINGIFY(PLUGIN_ARG) "=<scriptname> to specify the dehydra script to run");
    return 1;
  }
  pset = pointer_set_create ();
  type_pset = pointer_set_create ();
  tree_queue_vec = VEC_alloc(tree, heap, 10);
  dehydra_init (&dehydra, file, version_string);
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
    arguments = rest;
  }

  dehydra_defineStringProperty(&dehydra, dehydra.globalObj, "_arguments", arguments);
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

#ifdef CFG_PLUGINS_MOZ
int gcc_plugin_post_parse()
#else
static void gcc_plugin_post_parse(void*_, void*_2) 
#endif
{
  if (processed || errorcount) PLUGIN_HANDLER_RETURN;
  processed = 1;

  int i;
  tree t;
  /* first visit recorded structs */
  for (i = 0; tree_queue_vec && VEC_iterate (tree, tree_queue_vec, i, t); ++i) {
    process_type (t);
#ifdef TREEHYDRA_PLUGIN
    treehydra_call_js (&dehydra, "process_tree_type", t);
#endif
  }

  VEC_free (tree, heap, tree_queue_vec);
  tree_queue_vec = NULL;

  if (global_namespace)
    process(global_namespace);

  //  Bug 532319 -  Fix processing of instantiated template functions
  // TODO: use cgraph_nodes + varpool to get rid of global_namespace and end up with working C support
  struct cgraph_node *n;
  for (n = cgraph_nodes; n; n = n->next) {
    process(n->decl);
  }

  postGlobalNamespace = 1;
  PLUGIN_HANDLER_RETURN;
}

#ifdef CFG_PLUGINS_MOZ
void gcc_plugin_cp_pre_genericize(tree fndecl)
#else
static void gcc_plugin_cp_pre_genericize(tree fndecl, void *_)
#endif
{
  if (errorcount
      || DECL_CLONED_FUNCTION_P (fndecl)
      || DECL_ARTIFICIAL(fndecl)) return;
  
  dehydra_cp_pre_genericize(&dehydra, fndecl, postGlobalNamespace);
#ifdef TREEHYDRA_PLUGIN
  treehydra_call_js (&dehydra, "process_cp_pre_genericize", fndecl);
#endif
}

#ifdef CFG_PLUGINS_MOZ
int gcc_plugin_finish_struct (tree t)
#else
static void gcc_plugin_finish_struct (tree t, void *_)
#endif
{
  // gcc trunk gives us error_mark for some reason
  if (errorcount || TREE_CODE(t) != RECORD_TYPE)
    PLUGIN_HANDLER_RETURN;
  dehydra_finishStruct (&dehydra, t);

  /* It's lame but types are still instantiated after post_parse
    when some of the stuff saved by dehydra_cp_pre_genericize has been 
    freed by GCC */
  if (postGlobalNamespace) {
    process_type (t);
#ifdef TREEHYDRA_PLUGIN
    treehydra_call_js (&dehydra, "process_tree_type", t);
#endif
    PLUGIN_HANDLER_RETURN;
  }
  /* Appending stuff to the queue instead of 
     processing immediately is because gcc is overly
     lazy and does some things (like setting anonymous
     struct names) sometime after completing the type */
  dehydra_ehqueue_tree(t);
  PLUGIN_HANDLER_RETURN;
}

static void dehydra_ehqueue_tree (tree t) {
  VEC_safe_push(tree, heap, tree_queue_vec, t);
}

#ifdef CFG_PLUGINS_MOZ
int gcc_plugin_finish ()
#else
static void gcc_plugin_finish (void *_, void *_2)
#endif
{
  pointer_set_destroy (pset);
  pset = NULL;
  pointer_set_destroy (type_pset);
  type_pset = NULL;
  if (tree_queue_vec)
    VEC_free(tree, heap, tree_queue_vec);
  tree_queue_vec = NULL;

  if (!errorcount)
    dehydra_input_end (&dehydra);
  PLUGIN_HANDLER_RETURN;
}

#ifndef CFG_PLUGINS_MOZ
#ifdef TREEHYDRA_PLUGIN
static unsigned int execute_treehydra_pass (void)
{
  gcc_plugin_pass();
  return 0;
}

static struct opt_pass treehydra_pass =
{
  .type                 = GIMPLE_PASS,
  .name                 = "treehydra",
  .gate                 = NULL,
  .execute              = execute_treehydra_pass,
  .sub                  = NULL,
  .next                 = NULL,
  .static_pass_number   = 0,
  .tv_id                = 0,
  .properties_required  = PROP_gimple_any,
  .properties_provided  = 0,
  .properties_destroyed = 0,
  .todo_flags_start     = 0,
  .todo_flags_finish    = 0
};
#endif

static tree
handle_user_attribute (tree *node, tree name, tree args,
			int flags, bool *no_add_attrs)
{
  return NULL_TREE;
}

static struct attribute_spec user_attr =
  { "user", 1, 1, false,  false, false, handle_user_attribute };

static void gcc_plugin_attributes(void *_, void *_2) {
  register_attribute (&user_attr);
  // hack, plugin_init is called before aux_base_name is set
  // but it should be set by attribute registration
  dehydra_setFilename(&dehydra);
}

// FSF requires this crap to be put in
int plugin_is_GPL_compatible = 1;

int plugin_init (struct plugin_name_args *plugin_info, struct plugin_gcc_version *version)
{
  char *pass_name = 0;

  if (!plugin_info->argc)
    return 1;

  char *script_name = plugin_info->argv[0].value;
  int ret = gcc_plugin_init (plugin_info->full_name, script_name, &pass_name, version->basever);

  if (!ret) {
    // Look for a pass introduced in GCC 4.5 prunes useful info(class members/etc) & disable it
#define DEFTIMEVAR(identifier__, name__) \
    identifier__,
    enum
    {
      TV_NONE,
#include "timevar.def"
      TIMEVAR_LAST
    };

    struct opt_pass *p;
    for(p = all_small_ipa_passes;p;p=p->next) {
      if (p->tv_id != TV_IPA_FREE_LANG_DATA)
        continue;
      //disable it
      p->execute = NULL;
      break;
    }
    // It's gone now
#ifdef TREEHYDRA_PLUGIN
    struct register_pass_info pass_info;
    pass_info.pass = &treehydra_pass;
    if (pass_name) {
      pass_info.reference_pass_name = pass_name;
    } else {
      pass_info.reference_pass_name = all_lowering_passes->name;
    }
    pass_info.ref_pass_instance_number = 0;
    pass_info.pos_op = PASS_POS_INSERT_AFTER;
    register_callback (plugin_info->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &pass_info);
#endif
    register_callback (plugin_info->base_name, PLUGIN_FINISH_UNIT, gcc_plugin_post_parse, NULL);
    if (isGPlusPlus()) {
      register_callback (plugin_info->base_name, 
#ifdef CFG_PLUGINS_MOZ
                         PLUGIN_CXX_CP_PRE_GENERICIZE, 
#else
                         PLUGIN_PRE_GENERICIZE,
#endif
                         (plugin_callback_func) gcc_plugin_cp_pre_genericize, NULL);
    }
    register_callback (plugin_info->base_name, PLUGIN_FINISH_TYPE, 
                       (plugin_callback_func) gcc_plugin_finish_struct, NULL);
    register_callback (plugin_info->base_name, PLUGIN_FINISH, gcc_plugin_finish, NULL);
    register_callback (plugin_info->base_name, PLUGIN_ATTRIBUTES, gcc_plugin_attributes, NULL);
  }
  return ret;
}
#endif
