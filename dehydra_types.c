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
#include "builtins.h"
#include "util.h"
#include "dehydra.h"

struct Types {
  JSObject *rootedTypesArray;
  struct pointer_map_t *typeMap;
};

typedef struct Types Types;

static Types dtypes = {0};
static const char *POINTER = "isPointer";
static const char *REFERENCE = "isReference";
static const char *KIND = "kind";
static const char *TYPEDEF = "typedef";
/*static const char *CONST = "const";*/
static const char *ARRAY = "isArray";
static const char *SIZE = "size";

static jsval dehydra_convert (Dehydra *this, tree type);

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

static jsval dehydra_convert (Dehydra *this, tree type) {
  void **v = pointer_map_contains(dtypes.typeMap, type);
  if (v) {
    return OBJECT_TO_JSVAL ((JSObject*) *v);
  }
  JSObject *obj = JS_ConstructObject (this->cx, &js_ObjectClass, NULL, 
                                     this->globalObj);
  unsigned int length = dehydra_getArrayLength (this, dtypes.rootedTypesArray);
  JS_DefineElement (this->cx, dtypes.rootedTypesArray, 
                    length, OBJECT_TO_JSVAL(obj),
                    NULL, NULL, JSPROP_ENUMERATE);
  *pointer_map_insert (dtypes.typeMap, type) = obj;
  
  tree next_type = NULL_TREE;
  tree type_decl = TYPE_NAME (type);
  if (type_decl != NULL_TREE ) {
    tree original_type = DECL_ORIGINAL_TYPE (type_decl);
    if (original_type) {
      dehydra_defineStringProperty (this, obj, NAME, 
                                    decl_as_string (type_decl, 0));

      dehydra_defineProperty (this, obj, TYPEDEF, 
                              dehydra_convert (this, original_type));
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
    dehydra_defineStringProperty (this, obj, KIND, class_key_or_enum_as_string (type));
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
        dehydra_defineProperty (this, obj, SIZE,
                                /* use a C-like size */
                                INT_TO_JSVAL (1 + TREE_INT_CST_LOW (max)));
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
  if (!dtypes.rootedTypesArray) {
    dtypes.rootedTypesArray = JS_NewArrayObject(this->cx, 0, NULL);
    xassert (JS_AddRoot (this->cx, &dtypes.rootedTypesArray));
    dtypes.typeMap = pointer_map_create ();
  }
  return dehydra_convert (this, type);
}
