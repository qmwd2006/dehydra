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

static const char *POINTER = "isPointer";
static const char *REFERENCE = "isReference";
static const char *KIND = "kind";
static const char *TYPEDEF = "typedef";
static const char *ARRAY = "isArray";
static const char *SIZE = "size";
static const char *INCOMPLETE = "isIncomplete";

static struct pointer_map_t *typeMap = NULL;

static jsval dehydra_convert (Dehydra *this, tree type);
static jsval dehydra_convert2 (Dehydra *this, tree type, JSObject *obj);

void dehydra_attachTypeAttributes (Dehydra *this, JSObject *obj, tree type) {
  JSObject *destArray = JS_NewArrayObject (this->cx, 0, NULL);
  dehydra_defineProperty (this, obj, ATTRIBUTES,
                          OBJECT_TO_JSVAL (destArray));
  /* first add attributes from template */
  tree decl_template_info = TYPE_TEMPLATE_INFO (type);
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
  dehydra_attachTypeAttributes (this, objEnum, enumeral_type);
}

static void dehydra_attachClassStuff (Dehydra *this, JSObject *objClass, tree record_type) {
  JSObject *destArray = JS_NewArrayObject (this->cx, 0, NULL);
  tree binfo = TYPE_BINFO (record_type);
  
  int n_baselinks = binfo ? BINFO_N_BASE_BINFOS (binfo) : 0;
  int i;
  for (i = 0; i < n_baselinks; i++)
    {
      tree base_binfo;
      jsval baseval;

      if (!i)
        dehydra_defineProperty (this, objClass, BASES, 
                                OBJECT_TO_JSVAL(destArray));

      base_binfo = BINFO_BASE_BINFO (binfo, i);
      baseval = dehydra_convert(this, BINFO_TYPE(base_binfo));

      JS_DefineElement (this->cx, destArray, i, 
                        baseval,
                        NULL, NULL, JSPROP_ENUMERATE);
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
    dehydra_addVar (this, func, destArray);
  }

  tree field;
  for (field = TYPE_FIELDS (record_type); field ; field = TREE_CHAIN (field)) {
    if (DECL_ARTIFICIAL(field) && !DECL_IMPLICIT_TYPEDEF_P(field)) continue;
    // ignore typedef of self field
    // my theory is that the guard above takes care of this one too
    if (TREE_CODE (field) == TYPE_DECL 
        && TREE_TYPE (field) == record_type) continue;
    if (TREE_CODE (field) != FIELD_DECL) continue;
    dehydra_addVar (this, field, destArray);
  }
  dehydra_attachTypeAttributes (this, objClass, record_type);
}

static void dehydra_convertAttachFunctionType (Dehydra *this, JSObject *obj, tree type) {
  tree arg_type = TYPE_ARG_TYPES (type);
  /* Skip "this" argument.  */
  if (DECL_NONSTATIC_MEMBER_FUNCTION_P (type))
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
    obj = JS_ConstructObject (this->cx, &js_ObjectClass, NULL, 
                              this->globalObj);
    dehydra_rootObject (this, OBJECT_TO_JSVAL (obj));
    *pointer_map_insert (typeMap, type) = obj;
  }
  return dehydra_convert2 (this, type, obj);
}

static bool isAnonymousStruct(tree t) {
  tree name = TYPE_NAME (t);
  if (name)
    name = DECL_NAME (name);
  return !name || ANON_AGGRNAME_P (name);
}

static jsval dehydra_convert2 (Dehydra *this, tree type, JSObject *obj) {
  tree next_type = NULL_TREE;
  tree type_decl = TYPE_NAME (type);
  if (type_decl != NULL_TREE) {
    tree original_type = DECL_ORIGINAL_TYPE (type_decl);
    if (original_type) {
      dehydra_defineStringProperty (this, obj, NAME, 
                                    decl_as_string (type_decl, 0));
      jsval subval = dehydra_convert (this, original_type);
      dehydra_defineProperty (this, obj, TYPEDEF, subval);
      location_t loc = location_of (type_decl);
      if (loc)
        dehydra_defineStringProperty (this, obj, LOC, loc_as_string (loc));
      
      return OBJECT_TO_JSVAL (obj);
    }
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
  switch (TREE_CODE (type)) {
  case POINTER_TYPE:
  case OFFSET_TYPE:
    dehydra_defineProperty (this, obj, POINTER, JSVAL_TRUE);
    next_type = TREE_TYPE (type);
    break;
  case REFERENCE_TYPE:
    dehydra_defineProperty (this, obj, REFERENCE, JSVAL_TRUE);
    next_type = TREE_TYPE (type);
    break;
  case RECORD_TYPE:
  case UNION_TYPE:
  case ENUMERAL_TYPE:
    dehydra_defineStringProperty (this, obj, KIND, 
                                  class_key_or_enum_as_string (type));
    if (!COMPLETE_TYPE_P (type)) {
      /* need to do specal treatment for incomplete types
         Those need to be looked up later and finished
      */
      dehydra_defineProperty (this, obj, INCOMPLETE, JSVAL_TRUE);
    } else if (TREE_CODE (type) == ENUMERAL_TYPE)
      dehydra_attachEnumStuff (this, obj, type);
    else
      dehydra_attachClassStuff (this, obj, type);

    if (!isAnonymousStruct (type))
      dehydra_defineStringProperty (this, obj, NAME, type_as_string (type, 0));
    else 
      dehydra_defineProperty (this, obj, NAME, JSVAL_VOID);

    location_t loc = location_of (type);
    if (loc)
      dehydra_defineStringProperty (this, obj, LOC, loc_as_string (loc));
    
    break;
  case VOID_TYPE:
  case BOOLEAN_TYPE:
  case INTEGER_TYPE:
  case REAL_TYPE:
  case FIXED_POINT_TYPE:
    if (!type_decl)
      {
        int prec = TYPE_PRECISION (type);
        dehydra_defineProperty (this, obj, BITFIELD, INT_TO_JSVAL (prec));
        if (ALL_FIXED_POINT_MODE_P (TYPE_MODE (type)))
          type = c_common_type_for_mode (TYPE_MODE (type), TYPE_SATURATING (type));
        else
          type = c_common_type_for_mode (TYPE_MODE (type), TYPE_UNSIGNED (type));
        type_decl = TYPE_NAME (type);
      }
    dehydra_defineStringProperty (this, obj, NAME,
                                  type_decl 
                                  ? IDENTIFIER_POINTER (DECL_NAME(type_decl))
                                  : type_as_string (type, 0xff));
    break;
  case COMPLEX_TYPE:
  case VECTOR_TYPE:
    /* maybe should add an isTemplateParam? */
  case TEMPLATE_TYPE_PARM:
  case TYPENAME_TYPE:
    dehydra_defineStringProperty (this, obj, NAME, type_as_string (type, 0));
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
                                      expr_as_string (max, 0));
      }
    next_type = TREE_TYPE (type);
    break;
  default:
    warning (1, "Unhandled %s: %s", tree_code_name[TREE_CODE(type)],
           type_as_string (type, TFF_CHASE_TYPEDEF));
    dehydra_defineStringProperty (this, obj, NAME, type_as_string (type, 0));
    break;
  }

  if (next_type != NULL_TREE) {
    dehydra_defineProperty (this, obj, TYPE, dehydra_convert (this, next_type));
  }
  return OBJECT_TO_JSVAL (obj);
}

jsval dehydra_convertType (Dehydra *this, tree type) {
  if (!typeMap) {
    typeMap = pointer_map_create ();
  }
  return dehydra_convert (this, type);
}
