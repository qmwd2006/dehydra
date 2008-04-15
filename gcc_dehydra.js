// New GCC access/ports for Dehydra types code

function DECL_ORIGINAL_TYPE (node) {
  return node.decl_non_common.result;
}

function TYPE_SIZE (node) {
  return node.type.size;
}

function COMPLETE_TYPE_P (node) {
  return TYPE_SIZE(node) != undefined;
}

function TYPE_VOLATILE (node) {
  return node.base.volatile_flag;
}

function TYPE_READONLY (node) {
  return node.base.readonly_flag;
}

function TYPE_RESTRICT (node) {
  return node.base.restrict_flag;
}

function TYPE_LANG_SPECIFIC(node) {
  return node.type.lang_specific;
}

function CLASSTYPE_DECLARED_CLASS(node) {
  return LANG_TYPE_CLASS_CHECK(node).declared_class;
}

function LANG_TYPE_CLASS_CHECK(node) {
  return node.type.lang_specific.u.c;
}

function ANON_AGGRNAME_P(id_node) {
  return IDENTIFIER_POINTER(id_node).substr(0, 7) == '__anon_';
}

function TYPE_METHODS(node) {
  return node.type.maxval;
}

function TYPE_FIELDS(node) {
  return node.type.values;
}

function DECL_ARTIFICIAL(node) {
  return node.decl_common.artificial_flag;
}

function TREE_STATIC(node) {
  return node.base.static_flag;
}

function DECL_IMPLICIT_TYPEDEF_P(node) {
  return TREE_CODE(node) == TYPE_DECL && DECL_LANG_FLAG_2(node);
}

function DECL_LANG_FLAG_2(node) {
  return node.decl_common.lang_flag_2;
}

function TYPE_ATTRIBUTES(node) {
  return node.type.attributes;
}

function TYPE_SIZE_UNIT(node) {
  return node.type.size_unit;
}

function TYPE_MAIN_DECL(NODE) {
  return TYPE_STUB_DECL (TYPE_MAIN_VARIANT (NODE));
}

let TYPE_STUB_DECL = TREE_CHAIN;

function TYPE_MAIN_VARIANT(node) {
  return node.type.main_variant;
}

// TODO
let TEMPLATE_DECL = 4958945;

function DECL_CLONED_FUNCTION_P(node) {
  return (TREE_CODE(node) == FUNCTION_DECL || TREE_CODE(node) == TEMPLATE_DECL) &&
    DECL_LANG_SPECIFIC(node) &&
    !DECL_LANG_SPECIFIC(node).decl_flags.thunk_p &&
    DECL_CLONED_FUNCTION(node) != undefined;
}

function DECL_CLONED_FUNCTION(node) {
  return undefined;
  // TODO -- a whole bunch of union crap.
  do_dehydra_dump(DECL_LANG_SPECIFIC(node), 2, 1);
  return DECL_LANG_SPECIFIC(NON_THUNK_FUNCTION_CHECK(node)).u.f.u5.cloned_function;
}

// Just a bunch of debug checking in GCC.
function NON_THUNK_FUNCTION_CHECK(node) {
  return node;
}

function DECL_CONSTRUCTOR_P(node) {
  return DECL_LANG_SPECIFIC(node).decl_flags.constructor_attr;
}

function DECL_PURE_VIRTUAL_P(node) {
  return DECL_LANG_SPECIFIC(node).decl_flags.pure_virtual;
}

function DECL_VIRTUAL_P(node) {
  return node.decl_common.virtual_flag;
}

function DECL_LANG_SPECIFIC(node) {
  return node.decl_common.lang_specific;
}

function TYPE_ARG_TYPES(node) {
  return node.type.values;
}

function TYPE_DOMAIN(node) {
  return node.type.values;
}

function TYPE_MAX_VALUE(node) {
  return node.type.maxval;
}

function DECL_NONSTATIC_MEMBER_FUNCTION_P(node) {
  return TREE_CODE(TREE_TYPE(node)) == METHOD_TYPE;
}

function CLASSTYPE_TEMPLATE_INFO(node) {
  return LANG_TYPE_CLASS_CHECK(node).template_info;
}

function CLASSTYPE_TI_TEMPLATE(node) {
  return TI_TEMPLATE(CLASSTYPE_TEMPLATE_INFO(node));
}

function CLASSTYPE_TI_ARGS(node) {
  return TI_ARGS(CLASSTYPE_TEMPLATE_INFO(node));
}

let TI_TEMPLATE = TREE_PURPOSE;
let TI_ARGS = TREE_VALUE;

// TODO need unions
function DECL_TEMPLATE_INFO(node) {
  return undefined;
  //return DECL_LANG_SPECIFIC(node).decl_flags.u.template_info;
}

function TYPE_TEMPLATE_INFO(node) {
  return
  (TREE_CODE (NODE) == ENUMERAL_TYPE			
   ? ENUM_TEMPLATE_INFO (NODE) :			
   (TREE_CODE (NODE) == BOUND_TEMPLATE_TEMPLATE_PARM	
    ? TEMPLATE_TEMPLATE_PARM_TEMPLATE_INFO (NODE) :	
    (TYPE_LANG_SPECIFIC (NODE)				
     ? CLASSTYPE_TEMPLATE_INFO (NODE)			
     : NULL_TREE)));
}

function class_key_or_enum_as_string(t)
{
  if (TREE_CODE (t) == ENUMERAL_TYPE)
    return "enum";
  else if (TREE_CODE (t) == UNION_TYPE)
    return "union";
  else if (TYPE_LANG_SPECIFIC (t) && CLASSTYPE_DECLARED_CLASS (t))
    return "class";
  else
    return "struct";
}

function loc_as_string(loc) {
  return loc.file + ':' + loc.line + ':' + loc.column;
}

