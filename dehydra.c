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
#include <jsapi.h>
#include <unistd.h>
#include <stdio.h>

#include <config.h>
#include <system.h>
#include <coretypes.h>
#include <tm.h>
#include <tree.h>
#include "cp-tree-jsapi-workaround.h"
#include <cp/cxx-pretty-print.h>
#include <tree-iterator.h>
#include <pointer-set.h>
#include <toplev.h>
#include <langhooks.h>

#include "xassert.h"
#include "dehydra_builtins.h"
#include "util.h"
#include "dehydra.h"
#include "dehydra_types.h"
#include "dehydra_ast.h"

#ifndef JS_HAS_NEW_GLOBAL_OBJECT
#define JS_NewCompartmentAndGlobalObject(cx, cl, pr) JS_NewObject(cx, cl, NULL, NULL)
#endif

const char *NAME = "name";
const char *SHORTNAME = "shortName";
const char *LOC = "loc";
const char *BASES = "bases";
const char *ASSIGN = "assign";
const char *VALUE = "value";
const char *TYPE = "type";
const char *FUNCTION = "isFunction";
const char *RETURN = "isReturn";
const char *FCALL = "isFcall";
const char *ARGUMENTS = "arguments";
const char *DH_CONSTRUCTOR = "isConstructor";
const char *DH_EXPLICIT = "isExplicit";
const char *FIELD_OF = "fieldOf";
const char *MEMBERS = "members";
const char *PARAMETERS = "parameters";
const char *ATTRIBUTES = "attributes";
const char *STATEMENTS = "statements";
const char *BITFIELD = "bitfieldBits";
const char *MEMBER_OF = "memberOf";
const char *UNSIGNED = "isUnsigned";
const char *SIGNED = "isSigned";
const char *MIN_VALUE = "min";
const char *MAX_VALUE = "max";
const char *PRECISION = "precision";
const char *HAS_DEFAULT = "hasDefault";
const char *TEMPLATE = "template";
const char *SYS = "sys";
static const char *STATIC = "isStatic";
static const char *VIRTUAL = "isVirtual";
static const char *INCLUDE_PATH = "include_path";
static const char *STD_INCLUDE = "libs";
static const char *VERSION_STRING = "gcc_version";
static const char *FRONTEND = "frontend";
static const char *EXTERN = "isExtern";
static const char *EXTERN_C = "isExternC";

static char *my_dirname (char *path);

JSClass js_type_class = {
  "DehydraType",  /* name */
  JSCLASS_CONSTRUCT_PROTOTYPE, /* flags */
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
  JS_EnumerateStub,JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

static JSClass js_decl_class = {
  "DehydraDecl",  /* name */
  JSCLASS_CONSTRUCT_PROTOTYPE, /* flags */
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
  JS_EnumerateStub,JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

static JSClass global_class = {
  "DehydraGlobal", /* name */
  JSCLASS_GLOBAL_FLAGS, /* flags */
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
  JSCLASS_NO_OPTIONAL_MEMBERS
};

void dehydra_init(Dehydra *this, const char *file, const char *version) {
  static JSFunctionSpec shell_functions[] = {
    DH_JS_FN("_print",          Print,          0,    0,    0),
    DH_JS_FN("include",         Include,        1,    0,    0),
    DH_JS_FN("write_file",      WriteFile,      1,    0,    0),
    DH_JS_FN("read_file",       ReadFile,       1,    0,    0),
    DH_JS_FN("diagnostic",      Diagnostic,     0,    0,    0),
    DH_JS_FN("require",         Require,        1,    0,    0),
    DH_JS_FN("hashcode",        Hashcode,       1,    0,    0),
    DH_JS_FN("resolve_path",    ResolvePath,    1,    0,    0),
    JS_FS_END
  };

  this->fndeclMap = pointer_map_create ();
  this->rt = JS_NewRuntime (0x32L * 1024L * 1024L);
  if (this->rt == NULL)
    exit(1);

  this->cx = JS_NewContext (this->rt, 8192);
  if (this->cx == NULL)
    exit(1);

#ifdef JSOPTION_METHODJIT
  JS_SetOptions(this->cx, JS_GetOptions(this->cx) | JSOPTION_JIT | JSOPTION_METHODJIT);
#elif defined(JSOPTION_JIT)
  JS_SetOptions(this->cx, JS_GetOptions(this->cx) | JSOPTION_JIT);
#endif

  JS_SetContextPrivate (this->cx, this);
  
  this->globalObj = JS_NewCompartmentAndGlobalObject (this->cx, &global_class, NULL);
  if (this->globalObj == NULL)
    exit(1);

  JS_InitStandardClasses (this->cx, this->globalObj);

  /* register error handler */
  JS_SetErrorReporter (this->cx, ErrorReporter);

  xassert (JS_DefineFunctions (this->cx, this->globalObj, shell_functions));
  if (dehydra_getToplevelFunction(this, "include") == JSVAL_VOID) {
    fprintf (stderr, "Your version of spidermonkey has broken JS_DefineFunctions, upgrade it or ./configure with another version\n");
    exit(1);
  }
  this->rootedArgDestArray = 
    JS_NewArrayObject (this->cx, 0, NULL);
  JS_AddObjectRoot (this->cx, &this->rootedArgDestArray);
  // this is to be added at function_decl time
  this->rootedFreeArray = JS_NewArrayObject (this->cx, 0, NULL);
  JS_DefineElement (this->cx, this->rootedArgDestArray, 0,
                    OBJECT_TO_JSVAL (this->rootedFreeArray),
                    NULL, NULL, JSPROP_ENUMERATE);
  JS_SetVersion (this->cx, JSVERSION_LATEST);

  /* Initialize namespace for plugin system stuff. */
  JSObject *sys = dehydra_defineObjectProperty (this, this->globalObj, SYS);
  /* Set version info */
  dehydra_defineStringProperty (this, sys, VERSION_STRING, version);
  dehydra_defineStringProperty (this, sys, FRONTEND, lang_hooks.name);
  /* Initialize include path. */
  dehydra_defineArrayProperty (this, sys, INCLUDE_PATH, 0);

  char *filename_copy = xstrdup(file);
  char *dir = my_dirname(filename_copy);
  dehydra_appendToPath(this, dir);
  char *libdir = xmalloc(strlen(dir) + strlen(STD_INCLUDE) + 2);
  sprintf(libdir, "%s/%s", dir, STD_INCLUDE);
  dehydra_appendToPath(this, libdir);
  free(libdir);
  free(filename_copy);

  xassert (JS_InitClass(this->cx, this->globalObj, NULL
                        ,&js_type_class , NULL, 0, NULL, NULL, NULL, NULL));
  xassert (JS_InitClass(this->cx, this->globalObj, NULL
                        ,&js_decl_class , NULL, 0, NULL, NULL, NULL, NULL));
#ifdef CFG_PLUGINS_MOZ
  dehydra_setFilename(this);
#endif
}

void dehydra_setFilename(Dehydra *this) {
  if (aux_base_name) {
    /* provide filename info */
    jsval sys_val;
    JS_GetProperty(this->cx, this->globalObj, SYS, &sys_val);
    dehydra_defineStringProperty (this, JSVAL_TO_OBJECT(sys_val),
                                  "aux_base_name", aux_base_name);
  }

  if (main_input_filename) {
    /* provide filename info */
    jsval sys_val;
    JS_GetProperty(this->cx, this->globalObj, SYS, &sys_val);
    dehydra_defineStringProperty (this, JSVAL_TO_OBJECT(sys_val),
                                  "main_input_filename", main_input_filename);
  }
}

int dehydra_startup (Dehydra *this) {
  return dehydra_includeScript (this, "dehydra.js");
}

int dehydra_includeScript (Dehydra *this, const char *script) {
  jsval strval = STRING_TO_JSVAL (JS_NewStringCopyZ (this->cx, 
                                                     script));
  int key = dehydra_rootObject (this, strval);
  jsval rval;
  int ret = !JS_CallFunctionName(this->cx, this->globalObj, "include", 1, &strval, &rval);
  dehydra_unrootObject (this, key);
  return ret;
}

JSObject *dehydra_getIncludePath (Dehydra *this)
{
  jsval sys_val, path_val;
  JS_GetProperty(this->cx, this->globalObj, SYS, &sys_val);
  JS_GetProperty(this->cx, JSVAL_TO_OBJECT(sys_val), INCLUDE_PATH, &path_val);
  return JSVAL_TO_OBJECT(path_val);
}

/* Append a directory name to the script include path. */
void dehydra_appendToPath (Dehydra *this, const char *dir) 
{
  JSObject *path = dehydra_getIncludePath(this);
  unsigned int length = dehydra_getArrayLength(this, path);
  JSString *dir_str = JS_NewStringCopyZ(this->cx, dir);
  jsval dir_val = STRING_TO_JSVAL(dir_str);
  JS_DefineElement(this->cx, path, length, dir_val, NULL, NULL,
                   JSPROP_ENUMERATE);
}

/* Avoiding bug 431100. Spec from man 2 dirname */
static char *my_dirname (char *path) {
  char *r = strrchr(path, '/');
  if (!r) {
    strcpy (path, ".");
    return path;
  } else if (r == path && r[1] == 0) {
    return path; // '/'
  } else if (r[1] == 0) {
    // /foo/ foo/ cases
    *r = 0;
    return my_dirname (path);
  }
  *r = 0;
  return path;
}

/* Append the dirname of the given file to the script include path. */
void dehydra_appendDirnameToPath (Dehydra *this, const char *filename) 
{
  char *filename_copy = xstrdup(filename);
  char *dir = my_dirname(filename_copy);
  dehydra_appendToPath(this, dir);
  free(filename_copy);
}

/* Search the include path for a file matching the given name. The current
 * directory will be searched last. */
FILE *dehydra_searchPath (Dehydra *this, const char *filename, char **realname)
{
  if (filename && filename[0] != '/') {
    JSObject *path = dehydra_getIncludePath(this);
    int length = dehydra_getArrayLength(this, path);
    int i;
    for (i = 0; i < length; ++i) {
      jsval val;
      JS_GetElement(this->cx, path, i, &val);

      JSString *dir_str = JS_ValueToString(this->cx, val);
      if (!dir_str) continue;
      char *dir = JS_EncodeString(this->cx, dir_str);
      xassert(dir);

      char *buf = xmalloc(strlen(dir) + strlen(filename) + 2);
      /* Doing a little extra work here to get rid of unneeded '/'. */
      const char *sep = dir[strlen(dir)-1] == '/' ? "" : "/";
      sprintf(buf, "%s%s%s", dir, sep, filename);
      JS_free(this->cx, dir);

      FILE *f = fopen(buf, "r");
      if (f) {
        *realname = buf;
        return f;
      } else {
        free(buf);
      }
    }
  }
  
  FILE *f = fopen(filename, "r");
  if (f) {
    *realname = xstrdup(filename);
    return f;
  }

  return NULL;
}

jsuint dehydra_getArrayLength (Dehydra *this, JSObject *array) {
  jsuint length = 0;
  xassert (JS_GetArrayLength (this->cx, array, &length));
  return length;
}

JSObject *definePropertyObject (JSContext *cx, JSObject *obj,
                                const char *name, JSClass *clasp,
                                JSObject *proto, uintN flags) {
  JSObject *nobj = JS_NewObject (cx, clasp, proto, NULL);
  JS_DefineProperty (cx, obj, name, OBJECT_TO_JSVAL(nobj), NULL, NULL, flags);
  return nobj;
}

void dehydra_defineProperty (Dehydra *this, JSObject *obj,
                             char const *name, jsval value)
{
  JS_DefineProperty (this->cx, obj, name, value,
                     NULL, NULL, JSPROP_ENUMERATE);
}

jsval dehydra_defineStringProperty (Dehydra *this, JSObject *obj,
                                   char const *name, char const *value)
{
  JSString *str = JS_NewStringCopyZ (this->cx, value);
  jsval val = STRING_TO_JSVAL(str);
  dehydra_defineProperty (this, obj, name, val);
  return val;
}

JSObject *dehydra_defineArrayProperty (Dehydra *this, JSObject *obj,
                                       char const *name, int length) {
  JSObject *destArray = JS_NewArrayObject (this->cx, length, NULL);
  dehydra_defineProperty (this, obj, name, OBJECT_TO_JSVAL (destArray));
  return destArray;  
}

JSObject *dehydra_defineObjectProperty (Dehydra *this, JSObject *obj,
                                       char const *name) {
  return definePropertyObject(
      this->cx, obj, name, NULL, NULL,
      JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT);
}

/* Load and execute a Javascript file. 
 * Return:    0 on success
 *            1 on failure if a Javascript exception is pending
 *            does not return if a Javascript error is reported
 * The general behavior of (De|Tree)hydra is to print a message and
 * exit if a JS error is reported at the top level. But if this function
 * is called from JS, then the JS_* functions will instead set an
 * exception, which will be propgated back up to the callers for
 * eventual handling or exit. */

jsval dehydra_getToplevelFunction(Dehydra *this, char const *name) {
  jsval val = JSVAL_VOID;
  return (JS_GetProperty(this->cx, this->globalObj, name, &val)
          && val != JSVAL_VOID
          && JS_TypeOfValue(this->cx, val) == JSTYPE_FUNCTION) ? val : JSVAL_VOID;
}

void dehydra_setLoc(Dehydra *this, JSObject *obj, tree t) {
  location_t loc = location_of (t);
  /* Don't attach empty locations */
  if (loc_is_unknown(loc)) return;
  convert_location_t(this, obj, LOC, loc);
}

#ifndef __APPLE__
/* On modern GCCs we use a lazy location system to save memory
 and location lookup*/
static JSBool ResolveLocation (JSContext *cx, JSObject *obj, jsid id,
                               uintN flags, JSObject **objp) {
  *objp = obj;
  JSBool has_prop;
  // check if we already unpacked the location
  JSBool rv = JS_HasProperty(cx, obj, "file", &has_prop);
  if (rv && has_prop) return JS_TRUE;

  jsval v;
  JS_GetProperty (cx, obj, "_source_location", &v);
  location_t loc = JSVAL_TO_INT (v);
  expanded_location eloc = expand_location(loc);
  Dehydra *this = JS_GetContextPrivate(cx);
  dehydra_defineStringProperty (this, obj, "file", eloc.file);
  dehydra_defineProperty (this, obj, "line", INT_TO_JSVAL(eloc.line));
  dehydra_defineProperty (this, obj, "column", INT_TO_JSVAL(eloc.column));
  return JS_TRUE;
}
#endif 

static JSClass js_location_class = {
  "Location",  /* name */
#ifndef __APPLE__
  JSCLASS_NEW_RESOLVE, /* flags */
#else
  0,
#endif
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
  JS_EnumerateStub, 
#ifndef __APPLE__
  (JSResolveOp) ResolveLocation, 
#else
  JS_ResolveStub,
#endif
  JS_ConvertStub, JS_FinalizeStub,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

// Used by both Dehydra and Treehydra.
void convert_location_t (struct Dehydra *this, struct JSObject *parent,
                         const char *propname, location_t loc) {
  if (loc_is_unknown(loc)) {
    dehydra_defineProperty (this, parent, propname, JSVAL_VOID);
    return;
  }
  // Instead of calling ConstructWithArguments, we simply create the 
  // object and set up the properties because GC rooting is a lot
  // easier this way.
  JSObject *obj = definePropertyObject(this->cx, parent, propname, 
                                       &js_location_class, NULL, 
                                       JSPROP_ENUMERATE);
#ifdef __APPLE__
  expanded_location eloc = expand_location(loc);
  dehydra_defineStringProperty (this, obj, "file", eloc.file);
  dehydra_defineProperty (this, obj, "line", INT_TO_JSVAL(eloc.line));
#else
  dehydra_defineProperty (this, obj, "_source_location", INT_TO_JSVAL(loc));
#endif
}

void dehydra_addAttributes (Dehydra *this, JSObject *destArray,
                            tree attributes) {
  int i = 0;
  tree a;
  for (a = attributes; a; a = TREE_CHAIN (a)) {
    tree name = TREE_PURPOSE (a);
    tree args = TREE_VALUE (a);
    JSObject *obj = JS_NewObject(this->cx, NULL, 0, 0);
    JS_DefineElement (this->cx, destArray, i++,
                      OBJECT_TO_JSVAL (obj),
                      NULL, NULL, JSPROP_ENUMERATE);
    dehydra_defineStringProperty (this, obj, NAME, IDENTIFIER_POINTER (name));
    JSObject *array = JS_NewArrayObject (this->cx, 0, NULL);
    dehydra_defineProperty (this, obj, VALUE, OBJECT_TO_JSVAL (array));
    int i = 0;
    for (; args; args = TREE_CHAIN (args)) {
      tree t = TREE_VALUE (args);
      const char *val = TREE_CODE (t) == STRING_CST 
        ? TREE_STRING_POINTER (t)
        : expr_as_string (t, 0);
      JSString *str = 
        JS_NewStringCopyZ(this->cx, val);
      JS_DefineElement(this->cx, array, i++, 
                       STRING_TO_JSVAL(str),
                       NULL, NULL, JSPROP_ENUMERATE);
    }
  }
}

static void dehydra_setName (Dehydra *this, JSObject *obj, tree v) {
  if (DECL_NAME (v)) {
    tree n = DECL_NAME (v);
    jsval jsname = dehydra_defineStringProperty (this, obj, SHORTNAME, decl_as_string (n, 0));
    // turns out calling decL_as_string on template names will cause template instantiation
    // avoid that like the plague as it can cause compiler bugs
    if (TREE_CODE(v) == TEMPLATE_DECL)
      dehydra_defineProperty (this, obj, NAME, jsname);
    else {
      if (HAS_DECL_ASSEMBLER_NAME_P (v))
        dehydra_defineStringProperty (this, obj, "assemblerName", 
                                      IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (v)));
      dehydra_defineStringProperty (this, obj, NAME, decl_as_string (v, 0));
    }
  } else {
    static char buf[128];
    sprintf (buf, " _%d", DECL_UID (v));
    switch (TREE_CODE (v)) {
    case CONST_DECL:
      buf[0] = 'C';
      break;
    case RESULT_DECL:
      buf[0] = 'R';
      break;
    default:
      buf[0] = 'D';
    }
    dehydra_defineStringProperty (this, obj, NAME, buf);
  }
}

// Add the "hasDefault" property to function parameters with default values.
// This information is provided by gcc as part of the function type definition,
// so to add it to each parameter we must walk the parameter array and copy
// the information. However, we must be careful to ignore the |this|
// parameter for nonstatic class functions, which will be present in the
// parameter array but not in the function type definition.
void dehydra_moveDefaults (Dehydra *this, JSObject *obj) {
  jsval val;

  JS_GetProperty(this->cx, obj, TYPE, &val);
  if (val == JSVAL_VOID) return;
  JSObject *type_obj = JSVAL_TO_OBJECT(val);

  JS_GetProperty(this->cx, type_obj, HAS_DEFAULT, &val);
  if (val == JSVAL_VOID) return;
  JSObject *defaults_obj = JSVAL_TO_OBJECT(val);

  JS_GetProperty(this->cx, obj, PARAMETERS, &val);
  if (val == JSVAL_VOID) return;
  JSObject *params_obj = JSVAL_TO_OBJECT(val);

  jsuint defaults_length, params_length;
  JS_GetArrayLength(this->cx, defaults_obj, &defaults_length);
  JS_GetArrayLength(this->cx, params_obj, &params_length);

  // determine if we have a nonstatic member function, which will
  // thus have a |this| parameter. |isStatic| hasn't been defined
  // on |obj| yet, so look at array lengths instead
  JS_GetProperty(this->cx, obj, MEMBER_OF, &val);
  int hasThis = val != JSVAL_VOID && params_length > defaults_length;

  int i;
  for (i = 0; i < defaults_length; ++i) {
    // offset the index into the array by one if |hasThis|
    JS_GetElement(this->cx, params_obj, i + hasThis, &val);
    JSObject *param_obj = JSVAL_TO_OBJECT(val);

    JS_GetElement(this->cx, defaults_obj, i, &val);
    if (val != JSVAL_VOID && JSVAL_TO_BOOLEAN(val))
      dehydra_defineProperty(this, param_obj, HAS_DEFAULT, val);
  }
}

/* Add a Dehydra variable to the given parent array corresponding to
 * the GCC tree v, which must represent a declaration (e.g., v can
 * be a DECL_*. */
JSObject* dehydra_addVar (Dehydra *this, tree v, JSObject *parentArray) {
  if (!parentArray) parentArray = this->destArray;
  unsigned int length = dehydra_getArrayLength (this, parentArray);
  JSObject *obj = JS_NewObject (this->cx,  &js_decl_class, NULL, 
                                this->globalObj);
  //append object to array(rooting it)
  JS_DefineElement (this->cx, parentArray, length,
                    OBJECT_TO_JSVAL (obj),
                    NULL, NULL, JSPROP_ENUMERATE);
  if (!v) return obj;

  /* Common case */
  if (DECL_P(v)) {
    dehydra_setName (this, obj, v);
    tree decl_context = DECL_CONTEXT(v);
    if (decl_context && TREE_CODE(decl_context) == RECORD_TYPE) {
      dehydra_defineStringProperty (this, obj, ACCESS,
                                    TREE_PRIVATE (v) ? PRIVATE :
                                    (TREE_PROTECTED (v) ? PROTECTED
                                    : PUBLIC));
      dehydra_defineProperty (this, obj, MEMBER_OF, 
                              dehydra_convert_type (this, decl_context));
    }

    if (DECL_EXTERNAL(v)) {
      /* for non-static member variables, this attribute doesn't make sense */
      if (TREE_CODE(v) != VAR_DECL ||
          !decl_context ||
          TREE_STATIC(v)) {
        dehydra_defineProperty (this, obj, EXTERN, JSVAL_TRUE);
      }
    }
    
    /* define the C linkage */
    if ((TREE_CODE (v) == FUNCTION_DECL ||
         TREE_CODE (v) == VAR_DECL) &&
        DECL_EXTERN_C_P(v)) {
      dehydra_defineProperty(this, obj, EXTERN_C, JSVAL_TRUE);
    }

    tree typ = TREE_TYPE (v);
    if (TREE_CODE (v) == FUNCTION_DECL ||
        (isGPlusPlus() && DECL_FUNCTION_TEMPLATE_P (v))) {
      dehydra_defineProperty (this, obj, FUNCTION, JSVAL_TRUE);  

      if (isGPlusPlus() && DECL_CONSTRUCTOR_P (v)) {
        dehydra_defineProperty (this, obj, DH_CONSTRUCTOR, JSVAL_TRUE);
        
        if (DECL_NONCONVERTING_P (v)) {
          dehydra_defineProperty (this, obj, DH_EXPLICIT, JSVAL_TRUE);
        }
      }

      if (TREE_CODE(v) == FUNCTION_DECL) {
        JSObject *arglist = JS_NewArrayObject (this->cx, 0, NULL);
        dehydra_defineProperty (this, obj, "parameters",
                                OBJECT_TO_JSVAL (arglist));

        tree args;
        for (args = DECL_ARGUMENTS(v); args; args = TREE_CHAIN (args))
          dehydra_addVar(this, args, arglist);
      }
      
      if (isGPlusPlus()) {
        if (DECL_PURE_VIRTUAL_P (v))
          dehydra_defineStringProperty (this, obj, VIRTUAL, "pure");
        else if (DECL_VIRTUAL_P (v))
          dehydra_defineProperty (this, obj, VIRTUAL, JSVAL_TRUE);
      }

      if (DECL_FUNCTION_TEMPLATE_P (v)) {
        tree args = DECL_INNERMOST_TEMPLATE_PARMS (v);
        int len = TREE_VEC_LENGTH (args);

        JSObject *template = JS_NewArrayObject (this->cx, 0, NULL);
        dehydra_defineProperty (this, obj, TEMPLATE,
                                OBJECT_TO_JSVAL (template));

        int ix;
        for (ix = 0; ix != len; ++ix) {
          dehydra_addVar(this, TREE_VALUE(TREE_VEC_ELT(args, ix)), template);
        }
      }
    }
    dehydra_defineProperty (this, obj, TYPE, 
                            dehydra_convert_type (this, typ));
    tree attributes = DECL_ATTRIBUTES (v);
    if (attributes) {
      JSObject *tmp = JS_NewArrayObject (this->cx, 0, NULL);
      dehydra_defineProperty (this, obj, ATTRIBUTES, OBJECT_TO_JSVAL (tmp));
      dehydra_addAttributes (this, tmp, attributes);
    }

    if (TREE_CODE (v) == FUNCTION_DECL) {
      // now we have the parameter list, pull hasDefault values from the type
      // spec and attach them to each individual parameter, for convenience
      dehydra_moveDefaults(this, obj);
    }
    
    /* Following code handles 3 different meanings of static */
    /* static vars */
    if ((TREE_CODE (v) == VAR_DECL && TREE_STATIC (v)) 
        /* static functions */
        || (TREE_CODE (v) == FUNCTION_DECL && !TREE_PUBLIC (v))
        /* static class members */
        || (TREE_CODE (TREE_TYPE(v)) == FUNCTION_TYPE && decl_context 
            && TREE_CODE (decl_context) == RECORD_TYPE)) 
      dehydra_defineProperty (this, obj, STATIC, JSVAL_TRUE);
  } else if (TREE_CODE(v) == CONSTRUCTOR) {
    /* Special case for this node type */
    tree type = TREE_TYPE(v);
    char const *name = type_as_string(type, 0);
    dehydra_defineStringProperty(this, obj, NAME, 
                                 name);
    dehydra_defineProperty(this, obj, DH_CONSTRUCTOR, JSVAL_TRUE);
    dehydra_defineProperty(this, obj, MEMBER_OF, 
                           dehydra_convert_type(this, type));
  } else {
    /* Invalid argument tree code */
    xassert(0);
  }

  dehydra_setLoc(this, obj, v);
  return obj;
}

int dehydra_visitType (Dehydra *this, tree t) {
  jsval process_type = dehydra_getToplevelFunction(this, "process_type");
  if (process_type == JSVAL_VOID) return true;
  
  jsval rval, argv[1];
  argv[0] = dehydra_convert_type (this, t);
  xassert (JS_CallFunctionValue (this->cx, this->globalObj, process_type,
                                 1, argv, &rval));
  return true;
}

static void dehydra_visitFunctionDecl (Dehydra *this, tree f) {
  jsval process_function = dehydra_getToplevelFunction (this, "process_function");
  if (process_function == JSVAL_VOID) return;

  void **v = pointer_map_contains (this->fndeclMap, f);
  if (!v) {
    return;
  }
  if (!*v) {
    /* mustn't visit this function twice */
    return;
  }
  int key = (int)(intptr_t) *v;
  this->statementHierarchyArray = 
    JSVAL_TO_OBJECT (dehydra_getRootedObject (this, key));
  *v = NULL;
  
  int fnkey = dehydra_getArrayLength (this,
                                      this->rootedArgDestArray);
  JSObject *fobj = dehydra_addVar (this, f, this->rootedArgDestArray);
  jsval rval, argv[2];
  // This ensures that error/warning provide a sensible function name
  tree old_current_decl = current_function_decl;
  argv[0] = OBJECT_TO_JSVAL (fobj);
  argv[1] = OBJECT_TO_JSVAL (this->statementHierarchyArray);
  current_function_decl = f;
  xassert (JS_CallFunctionValue (this->cx, this->globalObj, process_function,
                                 sizeof (argv)/sizeof (argv[0]), argv, &rval));
  current_function_decl = old_current_decl;
  dehydra_unrootObject (this, key);
  dehydra_unrootObject (this, fnkey);
  this->statementHierarchyArray = NULL;
  this->destArray = NULL;
  JS_MaybeGC(this->cx);
}

static void dehydra_visitVarDecl (Dehydra *this, tree d) {
  jsval process_decl = dehydra_getToplevelFunction (this, "process_decl");
  if (process_decl == JSVAL_VOID) return;

  /* this is a hack,basically does dehydra_rootObject manually*/
  int key = dehydra_getArrayLength (this, this->rootedArgDestArray);
  JSObject *obj = dehydra_addVar (this, d, this->rootedArgDestArray);
  jsval rval, argv[1];
  argv[0] = OBJECT_TO_JSVAL (obj);
  xassert (JS_CallFunctionValue (this->cx, this->globalObj, process_decl,
                                 sizeof (argv)/sizeof (argv[0]), argv, &rval));
  dehydra_unrootObject (this, key);
}

int dehydra_rootObject (Dehydra *this, jsval val) {
  /* positions start from 1 since rootedFreeArray is always the first element */
  int pos;
  int length = dehydra_getArrayLength (this, this->rootedFreeArray);
  if (length) {
    jsval val;
    length--;
    JS_GetElement(this->cx, this->rootedFreeArray, length, &val);
    JS_SetArrayLength (this->cx, this->rootedFreeArray, length);
    pos = JSVAL_TO_INT (val);
  } else {
    pos = dehydra_getArrayLength (this, this->rootedArgDestArray);
  }
  xassert (pos != 0); /* don't overwrite the rootedFreeArray root */
  JS_DefineElement (this->cx, this->rootedArgDestArray, pos,
                    val, NULL, NULL, JSPROP_ENUMERATE);
  return pos;
}

void dehydra_unrootObject (Dehydra *this, int pos) {
  xassert (pos != 0); /* don't unroot the rootedFreeArray */
  int length = dehydra_getArrayLength (this, this->rootedFreeArray);
  JS_DefineElement (this->cx, this->rootedFreeArray, length,
                    INT_TO_JSVAL (pos),
                    NULL, NULL, JSPROP_ENUMERATE);
  JS_DefineElement (this->cx, this->rootedArgDestArray, pos,
                    JSVAL_VOID,
                    NULL, NULL, JSPROP_ENUMERATE);

}

jsval dehydra_getRootedObject (Dehydra *this, int pos) {
  jsval v = JSVAL_VOID;
  JS_GetElement(this->cx, this->rootedArgDestArray, pos, &v);
  return v;
}

void dehydra_visitDecl (Dehydra *this, tree d) {
  dehydra_visitVarDecl (this, d);
  if (TREE_CODE (d) == FUNCTION_DECL) { 
    dehydra_visitFunctionDecl (this, d);
  } 
}

void dehydra_print(Dehydra *this, jsval arg) {
  jsval print = dehydra_getToplevelFunction(this, "print");
  if (print == JSVAL_VOID) {
    fprintf(stderr, "function user_print() not defined in JS\n");
    return;
  }
  jsval rval;
  xassert (JS_CallFunctionValue(this->cx, this->globalObj, print,
                               1, &arg, &rval));
}

void dehydra_input_end (Dehydra *this) {
  jsval input_end = dehydra_getToplevelFunction(this, "input_end");
  if (input_end == JSVAL_VOID) return;
  
  jsval rval;
  xassert (JS_CallFunctionValue(this->cx, this->globalObj, input_end,
                               0, NULL, &rval));
  JS_GC(this->cx);
}
