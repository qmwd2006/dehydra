/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
#include <jsapi.h>
#include <unistd.h>
#include <stdio.h>

#include <config.h>
#include <system.h>
#include <coretypes.h>
#include <tm.h>
#include <tree.h>
#include "cp-tree-jsapi-workaround.h"
#include <cxx-pretty-print.h>
#include <tree-iterator.h>
#include <pointer-set.h>
#include <toplev.h>

#include "xassert.h"
#include "dehydra_builtins.h"
#include "util.h"
#include "dehydra.h"
#include "dehydra_ast.h"
#include "dehydra_types.h"

static const char *POINTER = "isPointer";
static const char *REFERENCE = "isReference";
static const char *KIND = "kind";
static const char *TYPEDEF = "typedef";
static const char *ARRAY = "isArray";
static const char *SIZE = "size";
static const char *INCOMPLETE = "isIncomplete";
static const char *ACCESS = "access";
static const char *PUBLIC = "public";
static const char *PRIVATE = "private";
static const char *PROTECTED = "protected";
static const char *ISVIRTUAL = "isVirtual";
static const char *ISTYPENAME = "isTypename";
static const char *VARIANT = "variantOf";

static struct pointer_map_t *typeMap = NULL;

static const char *dehydra_typeString(tree type);
static jsval dehydra_convert (Dehydra *this, tree type);
static jsval dehydra_convert2 (Dehydra *this, tree type, JSObject *obj);

void dehydra_attachTypeAttributes (Dehydra *this, JSObject *obj, tree type) {
  JSObject *destArray = JS_NewArrayObject (this->cx, 0, NULL);
  dehydra_defineProperty (this, obj, ATTRIBUTES,
                          OBJECT_TO_JSVAL (destArray));
  /* first add attributes from template */
  tree decl_template_info = (isGPlusPlus() && TREE_CODE (type) == RECORD_TYPE)
    ? TYPE_TEMPLATE_INFO (type) : NULL_TREE;
  
  if (decl_template_info) {
    tree template_decl = TREE_PURPOSE (decl_template_info);
    tree type = TREE_TYPE (template_decl);
    tree attributes = TYPE_ATTRIBUTES (type);
    dehydra_addAttributes (this, destArray, attributes);
  }
  tree attributes = TYPE_ATTRIBUTES (type);
  dehydra_addAttributes (this, destArray, attributes);
  /* drop the attributes array if there are none */
  if (! dehydra_getArrayLength (this, destArray)) {
    JS_DeleteProperty (this->cx, obj, ATTRIBUTES);
  }
}

static void dehydra_attachEnumStuff (Dehydra *this, JSObject *objEnum, tree enumeral_type) {
  JSObject *destArray = JS_NewArrayObject (this->cx, 0, NULL);
  dehydra_defineStringProperty (this, objEnum, KIND, "enum");
  dehydra_defineProperty (this, objEnum, MEMBERS, 
                          OBJECT_TO_JSVAL(destArray));
  tree tv;
  /* Output the list of possible values for the enumeration type.  */
  for (tv = TYPE_VALUES (enumeral_type); tv ; tv = TREE_CHAIN (tv))
    {
      JSObject *obj = dehydra_addVar (this, NULL_TREE, destArray); 
      dehydra_defineStringProperty (this, obj, NAME, 
                                    IDENTIFIER_POINTER (TREE_PURPOSE (tv)));
      tree v = TREE_VALUE (tv);
      int value = TREE_INT_CST_LOW (v);
      dehydra_defineProperty (this, obj, VALUE,
                              INT_TO_JSVAL (value));
    }
}

static void dehydra_addClassMember (Dehydra *this, tree decl, JSObject *memberArray) {
  JSObject *m = dehydra_addVar (this, decl, memberArray);
  dehydra_defineStringProperty (this, m, ACCESS,
                                TREE_PRIVATE (decl) ? PRIVATE :
                                (TREE_PROTECTED (decl) ? PROTECTED
                                 : PUBLIC));
}

/* Note that this handles class|struct|union nodes */
static void dehydra_attachClassStuff (Dehydra *this, JSObject *objClass, tree record_type) {
  JSObject *destArray = JS_NewArrayObject (this->cx, 0, NULL);
  tree binfo = TYPE_BINFO (record_type);
  
  int n_baselinks = binfo ? BINFO_N_BASE_BINFOS (binfo) : 0;
  if (n_baselinks)
    dehydra_defineProperty (this, objClass, BASES, 
                            OBJECT_TO_JSVAL(destArray));

  VEC(tree,gc) *accesses = binfo ? BINFO_BASE_ACCESSES (binfo) : 0;
  int i;
  for (i = 0; i < n_baselinks; i++)
    {
      JSObject *obj = JS_NewObject(this->cx, NULL, 0, 0);
      JS_DefineElement (this->cx, destArray, i, 
                        OBJECT_TO_JSVAL (obj),
                        NULL, NULL, JSPROP_ENUMERATE);
      tree access = VEC_index (tree, accesses, i);
      dehydra_defineStringProperty (this, obj, ACCESS, IDENTIFIER_POINTER(access));
      
      tree base_binfo = BINFO_BASE_BINFO (binfo, i);
      jsval base_type = 
        OBJECT_TO_JSVAL (dehydra_convert(this, BINFO_TYPE(base_binfo)));
      dehydra_defineProperty (this, obj, TYPE, base_type);
      if (BINFO_VIRTUAL_P(base_binfo))
        dehydra_defineProperty (this, obj, ISVIRTUAL, JSVAL_TRUE);
    }
  
  destArray = JS_NewArrayObject(this->cx, 0, NULL);
  dehydra_defineProperty(this, objClass, MEMBERS,
                         OBJECT_TO_JSVAL(destArray));
  tree func;
  /* Output all the method declarations in the class.  */
  for (func = TYPE_METHODS (record_type) ; func ; func = TREE_CHAIN (func)) {
    if (DECL_ARTIFICIAL(func)) continue;
    /* Don't output the cloned functions.  */
    if (DECL_CLONED_FUNCTION_P (func)) continue;
    dehydra_addClassMember (this, func, destArray);
  }

  tree field;
  for (field = TYPE_FIELDS (record_type); field ; field = TREE_CHAIN (field)) {
    if (DECL_ARTIFICIAL(field) && !DECL_IMPLICIT_TYPEDEF_P(field)) continue;
    // ignore typedef of self field
    // my theory is that the guard above takes care of this one too
    if (TREE_CODE (field) == TYPE_DECL 
        && TREE_TYPE (field) == record_type) continue;
    if (TREE_CODE (field) != FIELD_DECL) continue;
    dehydra_addClassMember (this, field, destArray);
  }
  dehydra_defineProperty (this, objClass, "size_of", 
                          INT_TO_JSVAL (host_integerp (TYPE_SIZE_UNIT (record_type), 1)));
}

static bool isAnonymousStruct(tree t) {
  tree name = TYPE_NAME (t);
  if (name && TREE_CODE (name) == TYPE_DECL)
    name = DECL_NAME (name);
  return !name || ANON_AGGRNAME_P (name);
}

static void dehydra_attachTemplateStuff (Dehydra *this, JSObject *parent, tree type) {
  if (!isGPlusPlus()) return;
  /* for reference see dump_aggr_type */
  /* ugliest guard ever */
  tree type_name = TYPE_NAME (type);
  bool decl_artificial = type_name ? DECL_ARTIFICIAL (type_name) : false;
  if (!(decl_artificial && TREE_CODE (type) != ENUMERAL_TYPE
        && TYPE_LANG_SPECIFIC (type) && CLASSTYPE_TEMPLATE_INFO (type)
        && (TREE_CODE (CLASSTYPE_TI_TEMPLATE (type)) != TEMPLATE_DECL
            || PRIMARY_TEMPLATE_P (CLASSTYPE_TI_TEMPLATE (type))))) {
    return;
  }
        
  tree tpl = CLASSTYPE_TI_TEMPLATE (type);
  if (!tpl) return;

  JSObject *obj = 
    definePropertyObject(this->cx, parent, TEMPLATE, NULL, NULL, 
                         JSPROP_ENUMERATE);
  
  while (DECL_TEMPLATE_INFO (tpl))
    tpl = DECL_TI_TEMPLATE (tpl);

  tree name = DECL_NAME (tpl);
  xassert (name);
  dehydra_defineStringProperty (this, obj, NAME, IDENTIFIER_POINTER (name));
  
  tree info = TYPE_TEMPLATE_INFO (type);
  tree args = info ? TI_ARGS (info) : NULL_TREE;
  
  if (!args) return;
  if (TMPL_ARGS_HAVE_MULTIPLE_LEVELS (args))
    args = TREE_VEC_ELT (args, TREE_VEC_LENGTH (args) - 1);

  int len = TREE_VEC_LENGTH (args);
  JSObject *arguments = JS_NewArrayObject (this->cx, len, NULL);
  dehydra_defineProperty (this, obj, ARGUMENTS, OBJECT_TO_JSVAL (arguments));

  int ix;
  for (ix = 0; ix != len; ix++) {
    tree arg = TREE_VEC_ELT (args, ix);
    jsval val = JSVAL_VOID;
    if (TYPE_P (arg)) {
      val = dehydra_convert (this, arg);
    } else {
      JSString *str = JS_NewStringCopyZ (this->cx, expr_as_string (arg, 0));
      val = STRING_TO_JSVAL (str);
    }
    xassert (val != JSVAL_VOID);
    JS_DefineElement(this->cx, arguments, ix,
                     val, NULL, NULL, JSPROP_ENUMERATE);
  }
}

static void dehydra_attachClassName (Dehydra *this, JSObject *obj, tree type) {
  if (isAnonymousStruct (type)) {
    dehydra_defineProperty (this, obj, NAME, JSVAL_VOID);
    return;
  }
  dehydra_defineStringProperty (this, obj, NAME, dehydra_typeString(type));
}

static void dehydra_convertAttachFunctionType (Dehydra *this, JSObject *obj, tree type) {
  tree arg_type = TYPE_ARG_TYPES (type);
  /* Skip "this" argument. */
  if (TREE_CODE(type) == METHOD_TYPE) 
      arg_type = TREE_CHAIN (arg_type);

  /* return type */
  dehydra_defineProperty (this, obj, TYPE, 
                          dehydra_convert (this, TREE_TYPE (type)));  
  JSObject *params = JS_NewArrayObject (this->cx, 0, NULL);
  dehydra_defineProperty (this, obj, PARAMETERS, OBJECT_TO_JSVAL (params));
  int i = 0;
  while (arg_type && (arg_type != void_list_node))
    {
      JS_DefineElement(this->cx, params, i++,
                       dehydra_convert (this, TREE_VALUE (arg_type)),      
                       NULL, NULL, JSPROP_ENUMERATE);
      arg_type = TREE_CHAIN (arg_type);
    }
}

void dehydra_finishStruct (Dehydra *this, tree type) {
  if (!typeMap) return;
  void **v = pointer_map_contains(typeMap, type);
  if (!v) return;
  JSObject *obj = (JSObject*) *v;
  jsval incomplete = JSVAL_VOID;
  JS_GetProperty(this->cx, obj, INCOMPLETE, &incomplete);
  if (incomplete != JSVAL_TRUE) return;
  JS_DeleteProperty (this->cx, obj, INCOMPLETE);
  /* complete the type */
  dehydra_convert2 (this, type, obj);
}

static jsval dehydra_convert (Dehydra *this, tree type) {
  void **v = pointer_map_contains(typeMap, type);
  JSObject *obj = NULL;
  if (v) {
    jsval incomplete = JSVAL_VOID;
    obj = (JSObject*) *v;
    JS_GetProperty(this->cx, obj, INCOMPLETE, &incomplete);
    /* add missing stuff to the type if is now COMPLETE_TYPE_P */
    if (incomplete == JSVAL_TRUE && COMPLETE_TYPE_P (type)) {
      JS_DeleteProperty (this->cx, obj, INCOMPLETE);
    } else {
      return OBJECT_TO_JSVAL (obj);
    }
  } else {
    obj = JS_NewObject (this->cx, &js_type_class, NULL,
                        this->globalObj);
    dehydra_rootObject (this, OBJECT_TO_JSVAL (obj));
    *pointer_map_insert (typeMap, type) = obj;
  }
  return dehydra_convert2 (this, type, obj);
}

static jsval dehydra_convert2 (Dehydra *this, tree type, JSObject *obj) {
  tree context = TYPE_CONTEXT(type);
  if (context && TYPE_P(context)) {
    dehydra_defineProperty (this, obj, MEMBER_OF,
                            dehydra_convertType (this, context));
  }

  tree variant = TYPE_MAIN_VARIANT(type);
  if (variant != type) {
    dehydra_defineProperty (this, obj, VARIANT,
                            dehydra_convertType (this, variant));
  }

  int qualifiers = TYPE_QUALS (type);
  if (qualifiers & TYPE_QUAL_CONST)
    dehydra_defineProperty (this, obj, "isConst", JSVAL_TRUE);
  if (qualifiers & TYPE_QUAL_VOLATILE)
    dehydra_defineProperty (this, obj, "isVolatile", JSVAL_TRUE);
  if (qualifiers & TYPE_QUAL_RESTRICT)
    dehydra_defineProperty (this, obj, 
                            flag_isoc99 ? "restrict" : "__restrict__",
                            JSVAL_TRUE);  

  tree type_decl = TYPE_NAME (type);
  tree next_type = NULL_TREE;
  if (type_decl && TREE_CODE(type_decl) == TYPE_DECL) {
    tree original_type = DECL_ORIGINAL_TYPE (type_decl);
    if (original_type) {
      //dehydra_defineStringProperty (this, obj, NAME, 
      //decl_as_string (type_decl, 0));
      dehydra_defineStringProperty (this, obj, NAME, 
                                    IDENTIFIER_POINTER(DECL_NAME(type_decl)));
      jsval subval = dehydra_convert (this, original_type);
      dehydra_defineProperty (this, obj, TYPEDEF, subval);
      dehydra_setLoc (this, obj, type_decl);
      tree attributes = DECL_ATTRIBUTES (type_decl);
      if (attributes) {
        JSObject *tmp = dehydra_defineArrayProperty (this, obj, ATTRIBUTES, 0);
        dehydra_addAttributes (this, tmp, attributes);
      }
      return OBJECT_TO_JSVAL (obj);
    }
  }
  switch (TREE_CODE (type)) {
  case POINTER_TYPE:
  case OFFSET_TYPE:
  case REFERENCE_TYPE:
    dehydra_defineProperty (this, obj,
                            TREE_CODE (type) == REFERENCE_TYPE ? REFERENCE : POINTER,
                            JSVAL_TRUE);
    next_type = TREE_TYPE (type);
    dehydra_defineProperty(this, obj, PRECISION,
                           INT_TO_JSVAL (TYPE_PRECISION (type)));
    break;
  case RECORD_TYPE:
  case UNION_TYPE:
  case ENUMERAL_TYPE:
    dehydra_defineStringProperty (this, obj, KIND, 
                                  class_key_or_enum_as_string (type));
    dehydra_attachClassName (this, obj, type);

    if (!COMPLETE_TYPE_P (type)) {
      /* need to do specal treatment for incomplete types
         Those need to be looked up later and finished
      */
      dehydra_defineProperty (this, obj, INCOMPLETE, JSVAL_TRUE);
    } else if (TREE_CODE (type) == ENUMERAL_TYPE) {
      dehydra_attachEnumStuff (this, obj, type);
      dehydra_defineProperty(this, obj, UNSIGNED,
                             TYPE_UNSIGNED (type) ? JSVAL_TRUE : JSVAL_FALSE);
      dehydra_defineProperty(this, obj, PRECISION,
                             INT_TO_JSVAL (TYPE_PRECISION (type)));

    } else
      dehydra_attachClassStuff (this, obj, type);

    dehydra_attachTemplateStuff (this, obj, type);
    dehydra_setLoc (this, obj, type);
    break;
  case INTEGER_TYPE:
  case BOOLEAN_TYPE:
    {
      JSObject *tmp = this->destArray;
      this->destArray = JS_NewArrayObject (this->cx, 0, NULL);
      int key = dehydra_rootObject(this, OBJECT_TO_JSVAL (this->destArray));
      tree type_min = TYPE_MIN_VALUE (type);
      // min/max may be missing in typecast expressions
      if (type_min)
        dehydra_makeVar(this, type_min, MIN_VALUE, obj);
      tree type_max = TYPE_MAX_VALUE (type);
      if (type_max)
        dehydra_makeVar(this, type_max, MAX_VALUE, obj);
      dehydra_unrootObject (this, key);
      this->destArray = tmp;
    }
  case REAL_TYPE:
    if (TYPE_UNSIGNED (type))
      dehydra_defineProperty(this, obj, UNSIGNED, JSVAL_TRUE);
    else
      dehydra_defineProperty(this, obj, SIGNED, JSVAL_TRUE);

    dehydra_defineProperty(this, obj, PRECISION,
                           INT_TO_JSVAL (TYPE_PRECISION (type)));
  case VOID_TYPE:
#ifdef FIXED_POINT_TYPE_CHECK
  case FIXED_POINT_TYPE:
#endif
    if (!type_decl)
      {
        int prec = TYPE_PRECISION (type);
        dehydra_defineProperty (this, obj, BITFIELD, INT_TO_JSVAL (prec));
#ifdef ALL_FIXED_POINT_MODE_P
        if (ALL_FIXED_POINT_MODE_P (TYPE_MODE (type)))
          type = c_common_type_for_mode (TYPE_MODE (type), TYPE_SATURATING (type));
        else
#endif
          type = c_common_type_for_mode (TYPE_MODE (type), TYPE_UNSIGNED (type));
        type_decl = TYPE_NAME (type);
      }
    char const *realName;
    if (type_decl)
      realName = IDENTIFIER_POINTER(TREE_CODE (type_decl) == TYPE_DECL ?
                                    DECL_NAME(type_decl) : type_decl);
    else
      realName = type_as_string(type, 0xff);
    dehydra_defineStringProperty (this, obj, NAME, realName);
    break;
  case COMPLEX_TYPE:
  case VECTOR_TYPE:
    /* maybe should add an isTemplateParam? */
  case TEMPLATE_TYPE_PARM:
  case TYPENAME_TYPE:
    dehydra_defineStringProperty (this, obj, NAME, dehydra_typeString(type));
    dehydra_defineProperty (this, obj, ISTYPENAME, JSVAL_TRUE);
    break;
  case FUNCTION_TYPE:
  case METHOD_TYPE:
    dehydra_convertAttachFunctionType (this, obj, type);
    break;
  case ARRAY_TYPE:
    dehydra_defineProperty (this, obj, ARRAY, JSVAL_TRUE);
    if (TYPE_DOMAIN (type))
      {
        tree dtype = TYPE_DOMAIN (type);
        tree max = TYPE_MAX_VALUE (dtype);
        dehydra_defineStringProperty (this, obj, SIZE,
                                      (max && TREE_CODE(max) == INTEGER_CST 
                                       ? dehydra_intCstToString (max)
                                       : expr_as_string (max, 0)));
      }
    next_type = TREE_TYPE (type);
    break;
  default:
    warning (1, "Unhandled %s: %s", tree_code_name[TREE_CODE(type)],
           type_as_string (type, TFF_CHASE_TYPEDEF));
    dehydra_defineStringProperty (this, obj, NAME, type_as_string (type, 0));
    break;
  }
  dehydra_attachTypeAttributes (this, obj, type);
  if (next_type != NULL_TREE) {
    dehydra_defineProperty (this, obj, TYPE, dehydra_convert (this, next_type));
  }
  return OBJECT_TO_JSVAL (obj);
}

jsval dehydra_convertType (Dehydra *this, tree type) {
  /* Not allowed to pass in NULLs
     this prevents nodes without types from haivng undefined .type*/
  xassert (type);
  if (!typeMap) {
    typeMap = pointer_map_create ();
  }
  return dehydra_convert (this, type);
}

/* Return a string name for the given type to be used as the NAME property.
 *
 * The design of this function is rather unfortunate. Pretty-printing
 * can get complicated with templates, and the GCC code isn't easy to
 * reuse, so we're going to call GCC and then fix up the results if
 * needed. */
static const char *dehydra_typeString(tree type) {
  const char *ans = type_as_string(type, 0);
  const char *chop_prefix = "const ";
  if (!strncmp(ans, chop_prefix, strlen(chop_prefix))) {
    ans += strlen(chop_prefix);
  }
  return ans;
}
