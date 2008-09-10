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

  v = dehydra_convert2(type);
  typeMap.put(type, v);
  return v;
}

function LazyType() {
}
LazyType.prototype.__defineGetter__('loc', function type_loc() {
  return location_of(this._type);
});
LazyType.prototype.__defineGetter__('isConst', function type_const() {
  return TYPE_READONLY(this._type);
});
LazyType.prototype.__defineGetter__('isVolatile', function type_volatile() {
  return TYPE_VOLATILE(this._type);
});
LazyType.prototype.__defineGetter__('restrict', function type_restrict() {
  return TYPE_RESTRICT(this._type);
});
LazyType.prototype.__defineGetter__('attributes', function type_atts() {
  return translate_attributes(TYPE_ATTRIBUTES(this._type));
});
LazyType.prototype.toString = function() {
  return "LazyType Object";
};

function LazyTypedef(typedecl, originaltype)
{
  this._type = typedecl;
  this._originaltype = originaltype;
}
LazyTypedef.prototype = new LazyType();
LazyTypedef.prototype.__defineGetter__('typedef', function get_typedef() {
  return dehydra_convert(this._originaltype);
});
LazyTypedef.prototype.__defineGetter__('name', function typedef_name() {
  return decl_name(this._type);
});
LazyTypedef.prototype.__defineGetter__('attributes', function typedef_atts() {
  return translate_attributes(DECL_ATTRIBUTES(this._type));
});
LazyTypedef.prototype.toString = function() {
  return "typedef " + this.name;
};

/* A type which has a .type subtype accessible via TREE_TYPE */
function LazySubtype(type) {
}
LazySubtype.prototype = new LazyType();
LazySubtype.prototype.__defineGetter__('type', function subtype_type() {
  return dehydra_convert(TREE_TYPE(this._type));
});

function LazyPointer(type) {
  this._type = type;
}
LazyPointer.prototype = new LazySubtype();
LazyPointer.prototype.isPointer = true;
LazyPointer.prototype.toString = function() {
  return this.type + " *";
};

function LazyReference(type) {
  this._type = type;
}
LazyReference.prototype = new LazySubtype();
LazyReference.prototype.isReference = true;
LazyReference.prototype.toString = function () {
  return this.type + " &";
};

function LazyRecord(type) {
  this._type = type;
  this.kind = class_key_or_enum_as_string(type);
  if (!isAnonymousStruct(type)) this.name = decl_name(TYPE_NAME(type));
  if (!COMPLETE_TYPE_P(type))
    this.isIncomplete = true;
}
LazyRecord.prototype = new LazyType();
LazyRecord.prototype.__defineGetter__('bases', function record_bases() {
  if (this.isIncomplete)
    throw Error("Can't get bases of incomplete type " + this.name);

  let binfo = TYPE_BINFO(this._type);
                                        
  // Also: expose .isVirtual on bases
  let accesses = VEC_iterate(BINFO_BASE_ACCESSES(binfo));
  let bases = [];
  let i = 0;
  for each (base_binfo in
            VEC_iterate(BINFO_BASE_BINFOS(binfo))) {
    bases.push({'access': IDENTIFIER_POINTER(accesses[i++]),
                'type': dehydra_convert(BINFO_TYPE(base_binfo)),
                'toString': function() { return this.access + ' ' + this.type; }});
  }
  return bases;
});
LazyRecord.prototype.__defineGetter__('members', function record_members() {
  let members = [];
  for (let func in flatten_chain(TYPE_METHODS(this._type))) {
    if (DECL_ARTIFICIAL(func)) continue;
    if (TREE_CODE(func) == TEMPLATE_DECL) continue;
    members.push(dehydra_convert(func));
  }
  for (let field in flatten_chain(TYPE_FIELDS(this._type))) {
    if (DECL_ARTIFICIAL(field) && !DECL_IMPLICIT_TYPEDEF_P(field)) continue;
    // ignore typedef of self field
    // my theory is that the guard above takes care of this one too
    if (TREE_CODE (field) == TYPE_DECL 
        && TREE_TYPE (field) == this._type) continue;
    if (TREE_CODE (field) != FIELD_DECL) continue;
    members.push(dehydra_convert(field));
  }
  return members;
});
LazyRecord.prototype.__defineGetter__('size_of', function record_size() {
  return TREE_INT_CST_LOW(TYPE_SIZE_UNIT(this._type));
});
LazyRecord.prototype.__defineGetter__('attributes', function record_atts() {
  let attrs = [];
  let decl_template_info = TYPE_TEMPLATE_INFO (this._type);
  if (decl_template_info) {
    let template_decl = TREE_PURPOSE (decl_template_info);
    let type = TREE_TYPE (template_decl);
    attrs = attrs.concat(translate_attributes(TYPE_ATTRIBUTES(type)));
  }
  attrs = attrs.concat(translate_attributes(TYPE_ATTRIBUTES(this._type)));
  return attrs;
});
LazyRecord.prototype.__defineGetter__('template', function record_args() {
  let type_name = TYPE_NAME (this._type);
  let decl_artificial = type_name ? DECL_ARTIFICIAL (type_name) : false;
  if (!(decl_artificial &&
        TYPE_LANG_SPECIFIC (this._type) &&
	CLASSTYPE_TEMPLATE_INFO (this._type) &&
        (TREE_CODE (CLASSTYPE_TI_TEMPLATE (this._type)) != TEMPLATE_DECL ||
         PRIMARY_TEMPLATE_P (CLASSTYPE_TI_TEMPLATE (this._type)))))
    return undefined;

  let tpl = CLASSTYPE_TI_TEMPLATE (this._type);
  if (!tpl) return undefined;

  let template = {};
  while (DECL_TEMPLATE_INFO(tpl))
    tpl = DECL_TI_TEMPLATE(tpl);

  template.name = IDENTIFIER_POINTER(DECL_NAME(tpl));

  let info = TYPE_TEMPLATE_INFO(this._type);
  let args;
  if (info)
    args = TI_ARGS(info);
  if (args) {
    if (TMPL_ARGS_HAVE_MULTIPLE_LEVELS(args))
      args = TREE_VEC_ELT(args, TREE_VEC_LENGTH(args) - 1);

    template.arguments = [convert_template_arg(arg)
			  for (arg in tree_vec_iterate(args))];
  }

  return template;
});
LazyRecord.prototype.toString = function() {
  return this.kind + " " + this.name;
};

function LazyEnum(type) {
  this._type = type;
  if (!isAnonymousStruct(type)) this.name = decl_name(TYPE_NAME(type));
  if (!COMPLETE_TYPE_P(type))
    this.isIncomplete = true;
}
LazyEnum.prototype = new LazyType();
LazyEnum.prototype.kind = 'enum';  
LazyEnum.prototype.__defineGetter__('members', function enum_members() {
  return [{'name': IDENTIFIER_POINTER(TREE_PURPOSE(tv)),
	   'value': TREE_INT_CST_LOW(TREE_VALUE(tv))}
	  for (tv in flatten_chain(TYPE_VALUES(this._type)))];
});
LazyEnum.prototype.toString = function() {
  return "enum " + this.name;
};

function LazyNumber(type, type_decl) {
  if (!type_decl) {
    this.precision = TYPE_PRECISION(type);
    type_decl = TYPE_NAME(type);
  }
  if (type_decl)
    this.name = IDENTIFIER_POINTER(DECL_NAME(type_decl));
  else
    this.name = type_as_string(type);
};
LazyNumber.prototype.toString = function() {
  return this.name;
};

function LazyArray(type) {
  this._type = type;
}
LazyArray.prototype = new LazySubtype();
LazyArray.prototype.__defineGetter__('size', function array_size() {
  if (TYPE_DOMAIN(this._type)) {
    let dtype = TYPE_DOMAIN(this._type);
    let min_t = TYPE_MIN_VALUE(dtype);
    let max_t = TYPE_MAX_VALUE(dtype);
    if (TREE_CODE(min_t) == INTEGER_CST && TREE_CODE(max_t) == INTEGER_CST)
      return TREE_INT_CST_LOW(max_t) - TREE_INT_CST_LOW(min_t) + 1;
  }
  return undefined;
});
LazyArray.prototype.toString = function() {
  return this.type + " [" + (this.size === undefined ? "" : this.size) + "]";
};

function LazyFunctionType(type) {
  this._type = type;
}
LazyFunctionType.prototype = new LazySubtype();
LazyFunctionType.prototype.__defineGetter__('parameters', function fntype_parameters() {
  return [dehydra_convert(arg_type)
	  for (arg_type in function_type_args(this._type))];
});
LazyFunctionType.prototype.toString = function() {
  return this.type + " (*)(" + this.parameters.join(', ') + ")";
};

function LazyDecl(type) {
  this._type = type;
}
LazyDecl.prototype.__defineGetter__('loc', function lazydecl_loc() {
  return location_of(this._type);
});
LazyDecl.prototype.__defineGetter__('attributes', function lazydecl_atts() {
  return translate_attributes(DECL_ATTRIBUTES(this._type));
});
LazyDecl.prototype.__defineGetter__('type', function lazydecl_type() {
  return dehydra_convert(TREE_TYPE(this._type));
});
LazyDecl.prototype.__defineGetter__('name', function lazydecl_name() {
  return decl_name(this._type);
});
LazyDecl.prototype.__defineGetter__('shortName', function lazydecl_shortName() {
  return IDENTIFIER_POINTER(DECL_NAME(this._type));
});
LazyDecl.prototype.__defineGetter__('isFunction', function lazydecl_isfunc() {
  return TREE_CODE(this._type) == FUNCTION_DECL;
});
LazyDecl.prototype.__defineGetter__('isConstructor', function lazydecl_constructor() {
  if (!this.isFunction)
    return undefined;
  if (DECL_CONSTRUCTOR_P(this._type))
    return true;
  return false;
});
LazyDecl.prototype.__defineGetter__('isDestructor', function lazydecl_destructor() {
  if (!this.isFunction)
    return undefined;
  return isDestructor(this._type);
});
LazyDecl.prototype.__defineGetter__('memberOf', function lazydecl_methodof() {
  let cx = DECL_CONTEXT(this._type);
  if (cx && TREE_CODE(cx) == RECORD_TYPE)
    return dehydra_convert(cx);
  return undefined;
});
LazyDecl.prototype.__defineGetter__('access', function lazydecl_access() {
  if (TREE_PRIVATE(this._type))
    return "private";
  else if (TREE_PROTECTED(this._type))
    return "protected";

  return "public";
});
LazyDecl.prototype.__defineGetter__('isVirtual', function lazydecl_isvirt() {
  if (!this.isFunction)
    return undefined;
  if (DECL_PURE_VIRTUAL_P(this._type))
    return "pure";
  else if (DECL_VIRTUAL_P(this._type))
    return true;
  else
    return false;
});
LazyDecl.prototype.__defineGetter__('isStatic', function lazydecl_isstatic() {
  let code = TREE_CODE(this._type);
  if (code == VAR_DECL && TREE_STATIC(this._type))
    return true;
  if (code == FUNCTION_DECL && !TREE_PUBLIC(this._type))
    return true;
  if (TREE_CODE(TREE_TYPE(this._type)) == FUNCTION_TYPE && this.memberOf)
    return true;
  return undefined;
});
LazyDecl.prototype.toString = function() {
  return this.name;
}

function LazyConstructor(type) {
  this._type = type;
}
LazyConstructor.prototype = new LazyType();
LazyConstructor.prototype.__defineGetter__('name', function lazycons_name() {
  return type_as_string(TREE_TYPE(this._type));
});
LazyConstructor.prototype.isConstructor = true;
LazyConstructor.prototype.__defineGetter__('memberOf', function lazycons_methodOf() {
  return dehydra_convert(TREE_TYPE(this._type));
});
LazyConstructor.prototype.toString = function() {
  return "LazyConstructor instance";
}

// Convert the given GCC type object to a lazy Dehydra type
// Users should call dehydra_convert.
function dehydra_convert2(type) {
  if (TRACE) print("dehydra_convert2 " + type_as_string(type));

  if (TYPE_P(type)) {
    let type_decl = TYPE_NAME(type);
    if (type_decl != undefined) {
      let original_type = DECL_ORIGINAL_TYPE (type_decl);
      if (original_type)
	return new LazyTypedef(type_decl, original_type);
    }
  }

  switch (TREE_CODE(type)) {
  case POINTER_TYPE:
  case OFFSET_TYPE:
    return new LazyPointer(type);
  case REFERENCE_TYPE:
    return new LazyReference(type);
  case RECORD_TYPE:
  case UNION_TYPE:
    return new LazyRecord(type);
   case ENUMERAL_TYPE:
    return new LazyEnum(type);
  case VOID_TYPE:
  case BOOLEAN_TYPE:
  case INTEGER_TYPE:
  case REAL_TYPE:
  case FIXED_POINT_TYPE:
    return new LazyNumber(type);
  case ARRAY_TYPE:
    return new LazyArray(type);
  case COMPLEX_TYPE:
  case VECTOR_TYPE:
    /* maybe should add an isTemplateParam? */
  case TEMPLATE_TYPE_PARM:
  case TYPENAME_TYPE:
    return {'name': decl_name(TYPE_NAME(type))};
  case FUNCTION_TYPE:
  case METHOD_TYPE:
    return new LazyFunctionType(type);
  case CONSTRUCTOR:
    return new LazyConstructor(type);
  };

  if (DECL_P(type))
    return new LazyDecl(type);

  throw new Error("Unknown type. TREE_CODE: " + TREE_CODE(type));
}

function isAnonymousStruct(t) {
  let name = TYPE_NAME(t);
  if (name) name = DECL_NAME(name);
  return !name || ANON_AGGRNAME_P(name);
}

function isDestructor(fndecl) {
  return fndecl.decl_common.lang_specific.decl_flags.destructor_attr;
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

function translate_attributes(atts) {
  return [{'name': IDENTIFIER_POINTER(TREE_PURPOSE(a)),
	   'value': [TREE_STRING_POINTER(TREE_VALUE(arg))
		     for (arg in flatten_chain(TREE_VALUE(a)))]}
	  for (a in flatten_chain(atts))];
}

// This is a rough port of the GCC function meant for the use of this
// module only.
function type_as_string(t) {
  return type_string(t);
}
