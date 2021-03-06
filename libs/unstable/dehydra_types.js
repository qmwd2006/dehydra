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

include('gcc_print.js');
include('gcc_util.js');

// dehydra_types.c port

let TRACE = 0;

let typeMap = new Map();

// Return a Dehydra type object corresponding to the given GCC type.
function dehydra_convert(type) {
  if (TRACE) print("dehydra_convert " + type_as_string(type));
  let v = typeMap.get(type);
  if (v) {
    // For now, Treehydra should never produce incomplete types, so
    // that code has been removed.
    return v;
  }

  v = {};
  typeMap.put(type, v);
  return dehydra_convert2(type, v);
}

// Convert the given GCC type object to a Dehydra type by attaching
// attributes to 'obj'.
// Users should call dehydra_convert.
function dehydra_convert2(type, obj) {
  if (TRACE) print("dehydra_convert2 " + type_as_string(type));
  let next_type;
  let type_decl = TYPE_NAME(type);
  if (type_decl != undefined) {
    let original_type = DECL_ORIGINAL_TYPE (type_decl);
    if (original_type) {
      obj.name = decl_as_string(type_decl);
      obj.typedef = dehydra_convert(original_type);
      dehydra_setLoc(obj, type_decl);
      attach_attributes(DECL_ATTRIBUTES(type_decl, obj));
      return obj;
    }
  }

  if (TYPE_READONLY(type)) obj.isConst = true;
  if (TYPE_VOLATILE(type)) obj.isVolatile = true;
  if (TYPE_RESTRICT(type)) obj.restrict = true;

  switch (TREE_CODE (type)) {
  case POINTER_TYPE:
  case OFFSET_TYPE:
    obj.isPointer = true;
    next_type = TREE_TYPE(type);
    break;
  case REFERENCE_TYPE:
    obj.isReference = true;
    next_type = TREE_TYPE(type);
    break;
  case RECORD_TYPE:
  case UNION_TYPE:
  case ENUMERAL_TYPE:
    obj.kind = class_key_or_enum_as_string(type);
    // inlined dehydra_attachClassName
    obj.name = isAnonymousStruct(type) ? undefined : dehydra_typeString(type);

    if (!COMPLETE_TYPE_P (type)) {
      obj.isIncomplete = true;
    } else if (TREE_CODE (type) == ENUMERAL_TYPE) {
      dehydra_attachEnumStuff(obj, type);
    } else {
      dehydra_attachClassStuff(obj, type);
    }

    dehydra_attachTemplateStuff(obj, type);
    dehydra_setLoc(obj, type);
    break;
  case VOID_TYPE:
  case BOOLEAN_TYPE:
  case INTEGER_TYPE:
  case REAL_TYPE:
  case FIXED_POINT_TYPE:
    if (!type_decl) {
      obj.bitfieldBits = TYPE_PRECISION(type);
      type_decl = TYPE_NAME(type);
    }
    obj.name = type_decl ? IDENTIFIER_POINTER(DECL_NAME(type_decl)) :
      type_as_string(type);
    break;
  case COMPLEX_TYPE:
  case VECTOR_TYPE:
    /* maybe should add an isTemplateParam? */
  case TEMPLATE_TYPE_PARM:
  case TYPENAME_TYPE:
  case TYPE_ARGUMENT_PACK:
  case TYPE_PACK_EXPANSION:
    obj.name = dehydra_typeString(type);
    break;
  case FUNCTION_TYPE:
  case METHOD_TYPE:
    dehydra_convertAttachFunctionType (obj, type);
    break;
  case ARRAY_TYPE:
    obj.isArray = true;
    if (TYPE_DOMAIN (type)) {
      let dtype = TYPE_DOMAIN (type);
      let min_t = TYPE_MIN_VALUE (dtype);
      let max_t = TYPE_MAX_VALUE (dtype);
      // This logic differs from Dehydra, which simple calls expr_as_string
      // on max_t. But this version is better: for int[8], this code gives
      // 8, while Dehydra gives "7u".
      if (TREE_CODE(min_t) == INTEGER_CST && TREE_CODE(max_t) == INTEGER_CST) {
        let min_i = TREE_INT_CST_LOW(min_t);
        let max_i = TREE_INT_CST_LOW(max_t);
        obj.size = max_i - min_i + 1;
      }
    }
    next_type = TREE_TYPE(type);
    break;
  default:
    throw new Error("Unhandled: " + TREE_CODE(type));
  }

  if (next_type) {
    obj.type = dehydra_convert(next_type);
  }
  return obj;
}

function isAnonymousStruct(t) {
  let name = TYPE_NAME(t);
  if (name) name = DECL_NAME(name);
  return !name || ANON_AGGRNAME_P(name);
}

function dehydra_attachTypeAttributes(obj, type) {
  let attrs = []
  let decl_template_info = TYPE_TEMPLATE_INFO (type);
  if (decl_template_info) {
    let template_decl = TREE_PURPOSE (decl_template_info);
    let type = TREE_TYPE (template_decl);
    attrs = attrs.concat(translate_attributes(TYPE_ATTRIBUTES(type)));
  }
  attrs = attrs.concat(translate_attributes(TYPE_ATTRIBUTES(type)));
  if (attrs.length) obj.attributes = attrs;
}

function dehydra_attachEnumStuff(objClass, enum_type) {
  objClass.members = [{'name': IDENTIFIER_POINTER(TREE_PURPOSE(tv)),
                       'value': TREE_INT_CST_LOW(TREE_VALUE(tv))}
                      for (tv in flatten_chain(TYPE_VALUES(enum_type)))];
}

/* Note that this handles class|struct|union nodes */
function dehydra_attachClassStuff(objClass, record_type) {
  if (TRACE) print("dehydra_attachClassStuff " + type_as_string(record_type));

  let binfo = TYPE_BINFO(record_type);
  if (binfo) {
    let bases = [ dehydra_convert(BINFO_TYPE(base_binfo))
                  for each (base_binfo in
                            VEC_iterate(BINFO_BASE_BINFOS(binfo))) ];
    if (bases.length) objClass.bases = bases;
  }
  
  objClass.members = [];
  /* Output all the method declarations in the class.  */
  for (let func = TYPE_METHODS (record_type) ; func ; func = TREE_CHAIN (func)) {
    if (DECL_ARTIFICIAL(func)) continue;
    /* Don't output the cloned functions.  */
    if (DECL_CLONED_FUNCTION_P (func)) continue;
    if (TREE_CODE(func) == TEMPLATE_DECL) continue;
    dehydra_addVar (func, objClass.members);
  }

  for (let field = TYPE_FIELDS (record_type); field ; field = TREE_CHAIN (field)) {
    if (DECL_ARTIFICIAL(field) && !DECL_IMPLICIT_TYPEDEF_P(field)) continue;
    // ignore typedef of self field
    // my theory is that the guard above takes care of this one too
    if (TREE_CODE (field) == TYPE_DECL 
        && TREE_TYPE (field) == record_type) continue;
    if (TREE_CODE (field) != FIELD_DECL) continue;
    dehydra_addVar (field, objClass.members);
  }
  dehydra_attachTypeAttributes (objClass, record_type);
  objClass.size_of = TREE_INT_CST_LOW(TYPE_SIZE_UNIT(record_type));
}

function dehydra_attachTemplateStuff(parent, type) {
  if (TRACE) print("dehydra_attachTemplateStuff " + type_as_string(type));
  let type_name = TYPE_NAME (type);
  let decl_artificial = type_name ? DECL_ARTIFICIAL (type_name) : false;
  if (!(decl_artificial && TREE_CODE (type) != ENUMERAL_TYPE
        && TYPE_LANG_SPECIFIC (type) && CLASSTYPE_TEMPLATE_INFO (type)
        && (TREE_CODE (CLASSTYPE_TI_TEMPLATE (type)) != TEMPLATE_DECL
            || PRIMARY_TEMPLATE_P (CLASSTYPE_TI_TEMPLATE (type))))) {
    return;
  }
        
  let tpl = CLASSTYPE_TI_TEMPLATE (type);
  if (!tpl) return;

  let obj = parent.template = {};
  
  while (DECL_TEMPLATE_INFO (tpl))
    tpl = DECL_TI_TEMPLATE (tpl);

  obj.name = IDENTIFIER_POINTER(DECL_NAME(tpl));
  
  let info = TYPE_TEMPLATE_INFO (type);
  let args = info ? TI_ARGS (info) : undefined;
  
  if (!args) return;
  if (TMPL_ARGS_HAVE_MULTIPLE_LEVELS (args))
    args = TREE_VEC_ELT (args, TREE_VEC_LENGTH (args) - 1);

  obj.arguments = [ convert_template_arg(arg) for (arg in tree_vec_iterate(args))];
}

/** Iterator over a TREE_VEC. Note that this is entirely different from
 *  the vector iterated over by VEC_iterate. */
function tree_vec_iterate(t) {
  let len = TREE_VEC_LENGTH(t);
  for (let i = 0; i < len; ++i) {
    yield TREE_VEC_ELT(t, i);
  }
}

function convert_template_arg(arg) {
  if (TYPE_P(arg))
    return dehydra_convert(arg);
  if (TREE_CODE(arg) == INTEGER_CST)
    return TREE_INT_CST_LOW(arg);

  warning("Couldn't convert template parameter with tree code " + TREE_CODE(arg));
  return "??";
}

function dehydra_convertAttachFunctionType(obj, type) {
  if (TRACE) print("dehydra_convertAttachFunctionType " + type_as_string(type));
  let arg_type = TYPE_ARG_TYPES(type);

  /* Skip "this" argument.  */
  // Original dehydra -- this shouldn't work.
  //if (DECL_NONSTATIC_MEMBER_FUNCTION_P (type))
  if (TREE_CODE(type) == METHOD_TYPE) {
    obj.methodOf = dehydra_convert(TREE_TYPE(TREE_VALUE(arg_type)));
    arg_type = TREE_CHAIN (arg_type);
  }

  /* return type */
  obj.type = dehydra_convert(TREE_TYPE(type));
  let params = obj.parameters = [];
  while (arg_type && TREE_CODE(TREE_VALUE(arg_type)) != VOID_TYPE) {
    params.push(dehydra_convert(TREE_VALUE(arg_type)));
    arg_type = TREE_CHAIN (arg_type);
  }
}

function dehydra_addVar(v, parentArray) {
  if (!parentArray) throw new Error("missing parentArray");
  let obj = {};
  parentArray.push(obj);
  if (!v) return obj;

  if (TRACE) print("dehydra_addVar " + decl_as_string(v));

  if (DECL_P(v)) {
    /* Common case */
    obj.name = decl_as_string(v);
    let typ = TREE_TYPE (v);
    if (TREE_CODE (v) == FUNCTION_DECL) {
      obj.isFunction = true;
      if (DECL_CONSTRUCTOR_P (v)) obj.isConstructor = true;
      if (TREE_CODE (typ) == METHOD_TYPE || DECL_CONSTRUCTOR_P (v)) {
        obj.methodOf = dehydra_convert(DECL_CONTEXT(v));
      }
      if (DECL_PURE_VIRTUAL_P (v))
        obj.isVirtual = "pure";
      else if (DECL_VIRTUAL_P (v))
        obj.isVirtual = true;
    }
    obj.type = dehydra_convert(typ);
    attach_attributes(DECL_ATTRIBUTES(v), obj);
    if (TREE_STATIC (v)) obj.isStatic = true;
  } else if (TREE_CODE(v) == CONSTRUCTOR) {
    /* Special case for this node type */
    let type = TREE_TYPE(v);
    obj.name = type_as_string(type);
    obj.isConstructor = true;
    obj.methodOf = dehydra_convert(type);
  } else {
    /* Invalid argument tree code */
    throw new Error("invalid arg code " + TREE_CODE(v));
  }

  dehydra_setLoc(obj, v);
  return obj;
}

function attach_attributes(a, typeobj)
{
  if (a) {
    typeobj.attributes = translate_attributes(a);
  }
}

function dehydra_setLoc(obj, t) {
  let loc = location_of(t);
  if (!loc) return;
  obj.loc = loc_as_string(loc);
}

function dehydra_typeString(t) {
  let ans = type_string(t);
  if (ans.substr(0, 6) == "const ") ans = ans.substr(6);
  return ans;
}

// This is a rough port of the GCC function meant for the use of this
// module only.
function type_as_string(t) {
  return type_string(t);
}

// This is a rough port of the GCC function meant for the use of this
// module only.
function decl_as_string(t) {
  let ans = decl_name(t);
  if (TREE_CODE(t) == FUNCTION_DECL) {
    let type = TREE_TYPE(t);
    let args = TYPE_ARG_TYPES(type);
    if (TREE_CODE(type) == METHOD_TYPE) {
      // skip this
      args = TREE_CHAIN(args);
    }
    // The 'if' checks for void_list_node. This isn't precisely what
    // GCC checks but should be ok -- it's not like void is a real arg type.
    let params = [ type_as_string(TREE_VALUE(pt)) 
                   for (pt in flatten_chain(args)) 
                     if (TREE_CODE(TREE_VALUE(pt)) != VOID_TYPE) ];
    ans += '(' + params.join(',') + ')';
  }
  return ans;
}

