include('gcc_print.js');
include('gcc_util.js');

include('unstable/BigInt.js');

// dehydra_types.c port

let TRACE = 0;

let typeMap = new Map();

function LazyType(type) {
  this._type = type;
}
LazyType.prototype.__defineGetter__('loc', function type_loc() {
  return location_of(this._type);
});
LazyType.prototype.__defineGetter__('isConst', function type_const() {
  return TYPE_READONLY(this._type) ? true : undefined;
});
LazyType.prototype.__defineGetter__('isVolatile', function type_volatile() {
  return TYPE_VOLATILE(this._type) ? true : undefined;
});
LazyType.prototype.__defineGetter__('restrict', function type_restrict() {
  return TYPE_RESTRICT(this._type) ? true : undefined;
});
LazyType.prototype.__defineGetter__('attributes', function type_atts() {
  if (!TYPE_P(this._type))
    throw new Error("code " + TREE_CODE(this._type) + " is not a type!");
  return translate_attributes(TYPE_ATTRIBUTES(this._type));
});
LazyType.prototype.__defineGetter__('variantOf', function type_variant() {
  let variant = TYPE_MAIN_VARIANT(this._type);
  if (variant === this._type)
    return undefined;
  return dehydra_convert(variant);
});
LazyType.prototype.__defineGetter__('typedef', function type_typedef() {
  let type_decl = TYPE_NAME (this._type);
  if (type_decl && TREE_CODE(type_decl) == TYPE_DECL) {
    let original_type = DECL_ORIGINAL_TYPE(type_decl);
    if (original_type)
      return dehydra_convert(original_type);
  }
  return undefined;
});
LazyType.prototype.__defineGetter__('name', function type_name() {
  return type_string(this._type);
});
LazyType.prototype.getQuals = function type_getquals() {
  let q = [];
  // if (this.isConst)
  //   q.push('const ');
  if (this.isVolatile)
    q.push('volatile ');
  if (this.restrict)
    q.push('restrict ');
  return q.join('');
}
LazyType.prototype.toString = function() {
  return this.name;
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
LazyPointer.prototype.__defineGetter__('precision', function pointer_prec() {
  return TYPE_PRECISION(this._type);
});
LazyPointer.prototype.toString = function() {
  return this.type + "*";
};

function LazyReference(type) {
  this._type = type;
}
LazyReference.prototype = new LazySubtype();
LazyReference.prototype.isReference = true;
LazyReference.prototype.__defineGetter__('precision', function ref_prec() {
  return TYPE_PRECISION(this._type);
});
LazyReference.prototype.toString = function () {
  return this.type + "&";
};

function LazyRecord(type) {
  this._type = type;
  this.kind = class_key_or_enum_as_string(type);
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
                'isVirtual': BINFO_VIRTUAL_P(base_binfo) ? true : undefined,
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
  return this.name;
};

function LazyEnum(type) {
  this._type = type;
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
  this._type = type;
  if (!type_decl) {
    type_decl = TYPE_NAME(type);
  }
};
LazyNumber.prototype = new LazyType();
LazyNumber.prototype.__defineGetter__('isUnsigned', function number_unsigned() {
  return TYPE_UNSIGNED(this._type) ? true : undefined;
});
LazyNumber.prototype.__defineGetter__('isSigned', function number_signed() {
  return TYPE_UNSIGNED(this._type) ? undefined : true;
});
LazyNumber.prototype.__defineGetter__('bitfieldBits', function number_bf() {
  let parent = c_common_type_for_mode(TYPE_MODE(this._type),
                                      TYPE_UNSIGNED(this._type));
  if (parent === this.type)
    return undefined;
  
  return TYPE_PRECISION(this._type);
});
LazyNumber.prototype.__defineGetter__('bitfieldOf', function numbef_bfof() {
  let parent = c_common_type_for_mode(TYPE_MODE(this._type),
                                      TYPE_UNSIGNED(this._type));
                                        
  if (parent === this._type)
    return undefined;

  return dehydra_convert(parent);
});
LazyNumber.prototype.__defineGetter__('min', function number_min() {
  let min = TYPE_MIN_VALUE(this._type);
  if (!min)
    return undefined;
                                        
  return dehydra_convert(min);
});
LazyNumber.prototype.__defineGetter__('max', function number_max() {
  let max = TYPE_MAX_VALUE(this._type);
  if (!max)
    return undefined;
                                        
  return dehydra_convert(max);
});
LazyNumber.prototype.__defineGetter__('precision', function number_prec() {
  return TYPE_PRECISION(this._type);
});
LazyNumber.prototype.toString = function() {
  return this.name;
};

function LazyArray(type) {
  this._type = type;
}
LazyArray.prototype = new LazySubtype();
LazyArray.prototype.isArray = true;
LazyArray.prototype.__defineGetter__('max', function array_max() {
  let d = TYPE_DOMAIN(this._type);
  if (d === undefined)
    return undefined;

  let max = TYPE_MAX_VALUE(d);
  if (max === undefined || TREE_CODE(max) != INTEGER_CST)
    return undefined;
                                       
  return tree_to_bigint(max);
});
LazyArray.prototype.__defineGetter__('variableLength', function array_vlength() {
  return this.max === undefined;
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
  return this.type + " ()(" + this.parameters.join(', ') + ")";
};

const hexMap = [];
hexMap[48] = 102; // 0 -> f
hexMap[49] = 101;
hexMap[50] = 100;
hexMap[51] = 99;
hexMap[52] = 98;
hexMap[53] = 97;
hexMap[54] = 57;
hexMap[55] = 56;
hexMap[56] = 55;
hexMap[57] = 54;
hexMap[97] = 53;
hexMap[98] = 52;
hexMap[99] = 51;
hexMap[100] = 50;
hexMap[101] = 49;
hexMap[102] = 48;

function bitwiseNot(hexString)
{
  return String.fromCharCode.apply(null,
                                   [hexMap[hexString.charCodeAt(i)]
                                    for (i in hexString)]);
}

function tree_to_bigint(t)
{
  switch (TREE_CODE(t)) {
  case INTEGER_CST:
    let cst = TREE_INT_CST(t);

    let v = new BigInt('0x' + cst.high_str + cst.low_str);
    
    if (tree_int_cst_sign(t) < 0) {
      // manually do ~val + 1 arithmetic

      let nc = cst.high_str.length * 2;
      function ncrange() { for (let i = 0; i < nc; ++i) yield i; }
      let maxval = new BigInt('0x' + ['f' for (i in ncrange())].join(''));
      
      return bigint_uminus(bigint_plus(bigint_minus(maxval, v), 1));
    }
    return v;
    
  default:
    throw Error("Unhandled number type: " + TREE_CODE(t));
  }
}

function LazyLiteral(type) {
  this._type = type;
  this.value = tree_to_bigint(type);
}
LazyLiteral.prototype.__defineGetter__('type', function lazyliteral_type() {
  let t = TREE_TYPE(this._type);
  if (t !== undefined)
    return dehydra_convert(t);
  return undefined;
});
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
  let n = decl_name(this._type);
  if (this.isFunction) {
    let plist = this.type.parameters;
    if (this.memberOf)
      plist.unshift(); // remove "this"
    n += "(" + [p.toString() for each (p in plist)].join(", ") + ")";
  }
  return n;
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
LazyDecl.prototype.__defineGetter__('isExtern', function lazydecl_isextern() {
  let code = TREE_CODE(this._type);
  if (code != VAR_DECL ||
      this.memberOf === undefined ||
      TREE_STATIC(this._type))
    return true;
                                      
  return undefined;
});
LazyDecl.prototype.__defineGetter__('parameters', function lazydecl_params() {
  if (TREE_CODE(this._type) != FUNCTION_DECL)
    return undefined;
                                      
  return [dehydra_convert(t)
          for (t in flatten_chain(DECL_ARGUMENTS(this._type)))];
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
function dehydra_convert(type) {
  if (TRACE) print("dehydra_convert " + type_as_string(type));

  if (TREE_CODE(type) == TYPE_DECL && DECL_ORIGINAL_TYPE(type) !== undefined)
      type = TREE_TYPE(type);
  
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
    return new LazyType(type);
  case FUNCTION_TYPE:
  case METHOD_TYPE:
    return new LazyFunctionType(type);
  case CONSTRUCTOR:
    return new LazyConstructor(type);
  case INTEGER_CST:
    return new LazyLiteral(type);
  };

  if (DECL_P(type))
    return new LazyDecl(type);

  throw new Error("Unknown type. TREE_CODE: " + TREE_CODE(type));
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

// This is a rough port of the GCC function meant for the use of this
// module only.
function type_as_string(t) {
  return type_string(t);
}
