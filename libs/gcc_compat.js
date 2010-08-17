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

/* Ports of GCC macros and functions. */

function TREE_CODE (tree) {
  return tree.tree_code()
}

function TREE_CODE_CLASS (code) {
  return tree_code_type[code.value]
}

function TREE_CODE_LENGTH (code) {
  return tree_code_length[code.value]
}

function IS_EXPR_CODE_CLASS (code_class) {
  return code_class.value >= tcc_reference.value 
    && code_class.value <= tcc_expression.value
}

var IS_GIMPLE_STMT_CODE_CLASS = isGCC43 ?
  function (code_class) { return code_class == tcc_gimple_stmt; } :
  function (code_class) { return false; }

function TreeCheckError (expected_code, actual_code) {
  var err = new Error("Expected " + expected_code + ", got " + actual_code)
  err.TreeCheckError = true
  return err
}

// comparable to CHECK_foo..except here foo is the second parameter
function TREE_CHECK (tree_node, expected_code) {
  const code = TREE_CODE (tree_node)
  if (code != expected_code) {
    const a = TREE_CHECK.arguments
    for (var i = 2; i < a.length; i++)
      if (a[i] == code)
        return tree_node
    throw TreeCheckError(expected_code, TREE_CODE (tree_node))
  }
  return tree_node
}

function TREE_CLASS_CHECK (node, expected_code) {
  const tree_code_class = TREE_CODE (node)
  if (TREE_CODE_CLASS (tree_code_class) != expected_code)
    throw Error("Expected " + expected_code + ", got " + TREE_CODE_CLASS (tree_code_class))
  return node
}

function STATEMENT_LIST_HEAD(node) {
  return node.stmt_list.head
}

function VL_EXP_CLASS_P (node) {
  return TREE_CODE_CLASS (TREE_CODE (node)) == tcc_vl_exp
}

function TREE_INT_CST (node) {
  return TREE_CHECK (node, INTEGER_CST).int_cst.int_cst
}

function TREE_INT_CST_LOW (node) {
  return TREE_INT_CST (node).low
}

function TREE_INT_CST_HIGH (node) {
  return TREE_INT_CST (node).high;
}

function tree_int_cst_sign(t) {
  if (TREE_INT_CST_LOW(t) == 0 &&
      TREE_INT_CST_HIGH(t) == 0)
    return 0;
  
  if (TYPE_UNSIGNED(TREE_TYPE(t)))
      return 1;
  
  if (TREE_INT_CST_HIGH(t) < 0)
    return -1;
  
  return 1;
}

function VL_EXP_OPERAND_LENGTH (node) {
  return (TREE_INT_CST_LOW ((TREE_CLASS_CHECK (node, tcc_vl_exp).exp.operands[0])))
}

function TREE_OPERAND_LENGTH (node)
{
  if (VL_EXP_CLASS_P (node))
    return VL_EXP_OPERAND_LENGTH (node);
  else
    return TREE_CODE_LENGTH (TREE_CODE (node));
}

var GIMPLE_STMT_P = isGCC43 ?
  function (node) {
    return TREE_CODE_CLASS(TREE_CODE((node))) == tcc_gimple_stmt;
  } :
  function (node) { return false; }

// This macro was eventually removed in GCC 4.5, however this
// is needed to simultaneously handle GENERIC and GIMPLE.
function GIMPLE_TUPLE_P (node) {
  return node.hasOwnProperty("gsbase");
}

function TREE_OPERAND (node, i) {
  return node.exp.operands[i]
}

var GIMPLE_STMT_OPERAND = isGCC42 ?
  TREE_OPERAND : isGCC43 ?
    function (node, i) {
      return node.gstmt.operands[i]
    } :
    gimple_op

function GENERIC_TREE_OPERAND (node, i)
{
  if (GIMPLE_STMT_P (node))
    return GIMPLE_STMT_OPERAND (node, i);
  return TREE_OPERAND (node, i);
}

function BIND_EXPR_VARS (node) {
  return TREE_OPERAND (TREE_CHECK (node, BIND_EXPR), 0)
}

function BIND_EXPR_BODY (node) {
  return TREE_OPERAND (TREE_CHECK (node, BIND_EXPR), 1)
}

const CALL_EXPR_FN_OPERAND_INDEX = isGCC42 ? 0 : 1;
function CALL_EXPR_FN (node) {
  return TREE_OPERAND (TREE_CHECK (node, CALL_EXPR), CALL_EXPR_FN_OPERAND_INDEX)
}

var call_expr_arg_iterator = isGCC42 ?
  function (node) {
    for (let arg_holder = TREE_OPERAND(node, 1); arg_holder;
         arg_holder = TREE_CHAIN(arg_holder))
      yield TREE_VALUE(arg_holder);
  } :
  function (node) {
    let nargs = call_expr_nargs(node);
    for (let i = 0; i < nargs; ++i)
      yield CALL_EXPR_ARG(node, i);
  }

var CALL_EXPR_ARG = isGCC42 ?
  function (node, i) {
    let iter = call_expr_arg_iterator(node);
    for (let j = 0; j < i; ++j)
      iter.next();
    return iter.next();
  } :
  function (node, i) {
    return TREE_OPERAND (TREE_CHECK (node, CALL_EXPR), i + 3);
  }

var call_expr_nargs = isGCC42 ?
  function (node) {
    return [arg for (arg in call_expr_arg_iterator(node))].length;
  } :
  function (node) {
    return TREE_INT_CST_LOW(TREE_OPERAND(node, 0)) - 3;
  }

function TYPE_BINFO (node) {
  return TREE_CHECK (node, RECORD_TYPE, UNION_TYPE, QUAL_UNION_TYPE).type.binfo
}

function BINFO_BASE_ACCESSES(node) {
  return TREE_CHECK(node, TREE_BINFO).binfo.base_accesses;
}

var BINFO_VIRTUAL_P = isGCC42 ?
  function (node) { return TREE_CHECK(node, TREE_BINFO).common.static_flag; } :
  function (node) { return TREE_CHECK(node, TREE_BINFO).base.static_flag; }

// not sure if this will work same way in gcc 4.3
function TREE_CHAIN (node) {
  return node.common.chain
}

function TREE_VALUE (node) {
  return TREE_CHECK (node, TREE_LIST).list.value
}

function TREE_PURPOSE (node) {
  return node.list.purpose
}

function TREE_STRING_POINTER (node) {
  return node.string.str
}

function DECL_ATTRIBUTES (node) {
  return node.decl_common.attributes
}

function DECL_P (node) {
  return TREE_CODE_CLASS (TREE_CODE (node)) == tcc_declaration
}

function DECL_INITIAL (node) {
  // can't do DECL_COMMON_CHECK
  return node.decl_common.initial
}

function DECL_SIZE (node) {
  // can't do DECL_COMMON_CHECK
  return node.decl_common.size
}

function DECL_SIZE_UNIT (node) {
  // can't do DECL_COMMON_CHECK
  return node.decl_common.size_unit
}

function DECL_NAME (node) {
  return node.decl_minimal.name
}

function DECL_UID (node) {
  return node.decl_minimal.uid
}

function DECL_CONTEXT (node) {
  return node.decl_minimal.context
}

function DECL_SAVED_TREE (node) {
  return TREE_CHECK (node, FUNCTION_DECL).decl_non_common.saved_tree
}

function DECL_STRUCT_FUNCTION(node) {
  return node.function_decl.f
}

// see gcc/tree.h for |enum symbol_visibility| definition:
const VISIBILITY_DEFAULT   = 0;
const VISIBILITY_PROTECTED = 1;
const VISIBILITY_HIDDEN    = 2;
const VISIBILITY_INTERNAL  = 3;

function DECL_VISIBILITY(node) {
  return node.decl_with_vis.visibility;
}

function DECL_VISIBILITY_SPECIFIED(node) {
  return node.decl_with_vis.visibility_specified;
}

var TREE_PRIVATE = isGCC42 ?
  function (node) { return node.common.private_flag; } :
  function (node) { return node.base.private_flag; };

var TREE_PROTECTED = isGCC42 ?
  function (node) { return node.common.protected_flag; } :
  function (node) { return node.base.protected_flag; };

function IDENTIFIER_POINTER (node) {
  return node.identifier.id.str
}

var IDENTIFIER_OPNAME_P = isGCC42 ?
  function (node) { return !!node.common.lang_flag_2; } :
  function (node) { return !!node.base.lang_flag_2; };

var IDENTIFIER_TYPENAME_P = isGCC42 ?
  function (node) { return !!node.common.lang_flag_4; } :
  function (node) { return !!node.base.lang_flag_4; };

function TREE_VEC_LENGTH (node) {
  return TREE_CHECK (node, TREE_VEC).vec.length
}

function TREE_VEC_ELT (node, i) {
  return TREE_CHECK (node, TREE_VEC).vec.a[i]
}

function CONSTRUCTOR_ELTS (node) {
  return TREE_CHECK (node, CONSTRUCTOR).constructor.elts
}

function TREE_TYPE (node) {
  return node.common.type
}

function TYPE_OFFSET_BASETYPE(node) {
  return node.type.maxval
}

function TYPE_NAME (node) {
  return node.type.name
}

function TYPE_PRECISION (node) {
  return node.type.precision;
}

function SCALAR_FLOAT_TYPE_P(t) {
  return TREE_CODE(t) == REAL_TYPE;
}

function TYPE_P (node) {
  return TREE_CODE_CLASS (TREE_CODE (node)) == tcc_type
}

function TYPE_CHECK (node) {
  return TREE_CLASS_CHECK(node, tcc_type);
}

function TYPE_MODE(node) {
  return node.type.mode;
}

function TYPE_LANG_SPECIFIC (node) {
  return node.type.lang_specific
}

function LANG_TYPE_IS_CLASS (node) {
  return TYPE_LANG_SPECIFIC(node).u.h.is_lang_type_class != 0;
}

function LANG_TYPE_CLASS_CHECK (node) {
  var lang_type = TYPE_LANG_SPECIFIC (node)
  if (!LANG_TYPE_IS_CLASS (node))
    throw new Error ("Not a class!")
  return lang_type.u.c
}

function CLASSTYPE_TEMPLATE_INFO (node) {
  return LANG_TYPE_CLASS_CHECK(node).template_info
}

const TI_TEMPLATE = isUsingGCCTuples ? TREE_TYPE : TREE_PURPOSE;

function CLASSTYPE_TI_TEMPLATE (node) {
  return TI_TEMPLATE (CLASSTYPE_TEMPLATE_INFO (node))
}

function DECL_LANG_SPECIFIC (node) {
  return node.decl_common.lang_specific
}

var decl_flags = isUsingGCCTuples?
  function (tree) { return tree.u.fn; } :
  function (tree) { return tree.decl_flags; }

function VAR_TEMPL_TYPE_OR_FUNCTION_DECL_CHECK(node) {
  return TREE_CHECK(node, VAR_DECL, FUNCTION_DECL, TYPE_DECL, TEMPLATE_DECL);
}

var DECL_TEMPLATE_INFO = isUsingGCCTuples ?
  function (node) {
    return DECL_LANG_SPECIFIC(VAR_TEMPL_TYPE_OR_FUNCTION_DECL_CHECK(node)).u.min.template_info;
  } :
  function (node) {
    return DECL_LANG_SPECIFIC(node).decl_flags.u.template_info;
  }

function DECL_TI_TEMPLATE(node) {
  return TI_TEMPLATE (DECL_TEMPLATE_INFO (node));
}

function TYPE_TEMPLATE_INFO(node) {
  if (TREE_CODE (node) == ENUMERAL_TYPE )
    return ENUM_TEMPLATE_INFO (node) 
  else if (TREE_CODE (node) == BOUND_TEMPLATE_TEMPLATE_PARM)
    return TEMPLATE_TEMPLATE_PARM_TEMPLATE_INFO (node) 
  else if (TYPE_LANG_SPECIFIC (node))
    return CLASSTYPE_TEMPLATE_INFO (node)
}

const TI_ARGS = isUsingGCCTuples ? TREE_CHAIN : TREE_VALUE;

function TMPL_ARGS_HAVE_MULTIPLE_LEVELS (node) {
  if (!node) return

  const elt = TREE_VEC_ELT (node, 0) 
  return elt && TREE_CODE (elt) == TREE_VEC_ELT
}

const BINFO_TYPE = TREE_TYPE;

function TYPE_METHODS(node) {
  return node.type.maxval;
}

/* This is so much simpler than the C version 
 because it merely returns the vector array and lets
the client for each it*/
// undefined is used for empty vectors, so support it nicely here.
var VEC_iterate = isGCC42 ?
  function (vec_node) {
    if (!vec_node)
      return [];
    return vec_node.base ? vec_node.base.vec : vec_node.vec;
  } :
  function (vec_node) { return vec_node ? vec_node.base.vec : []; }

function EXPR_P(node) {
  return IS_EXPR_CODE_CLASS (TREE_CODE_CLASS (TREE_CODE (node)));
}

function CONSTANT_CLASS_P (node) {
  return TREE_CODE_CLASS (TREE_CODE (node)) == tcc_constant;
}

function BINARY_CLASS_P (node) {
  return TREE_CODE_CLASS (TREE_CODE (node)) == tcc_binary;
}

function UNARY_CLASS_P (node) {
  return TREE_CODE_CLASS (TREE_CODE (node)) == tcc_unary;
}

function COMPARISON_CLASS_P (node) {
  return TREE_CODE_CLASS (TREE_CODE (node)) == tcc_comparison;
}

function INDIRECT_REF_P(node) {
  let code = TREE_CODE(node);
  return code == INDIRECT_REF ||
         code == ALIGN_INDIRECT_REF ||
         code == MISALIGNED_INDIRECT_REF;
}

function DECL_ARGUMENTS(tree) {
  return tree.decl_non_common.arguments;
}

function OBJ_TYPE_REF_EXPR(tree) {
  return TREE_OPERAND(tree, 0);
}

function OBJ_TYPE_REF_OBJECT(tree) {
  return TREE_OPERAND(tree, 1);
}

function OBJ_TYPE_REF_TOKEN(tree) {
  return TREE_OPERAND(tree, 2);
}

function BINFO_VIRTUALS(tree) {
  return tree.binfo.virtuals;
}

function BINFO_BASE_BINFOS(tree) {
  return tree.binfo.base_binfos;
}

const BV_FN = TREE_VALUE;

function TYPE_ARG_TYPES(tree) {
  return tree.type.values;
}

function TYPE_METHOD_BASETYPE(tree) {
  return tree.type.maxval;
}

function TYPE_PTRMEMFUNC_P(tree) {
  return TREE_CODE(tree) == RECORD_TYPE &&
    TYPE_LANG_SPECIFIC(tree) &&
    TYPE_LANG_SPECIFIC(tree).u.c &&
    TYPE_PTRMEMFUNC_FLAG(tree);
}

function TYPE_PTRMEMFUNC_FLAG(tree) {
  return TYPE_LANG_SPECIFIC(tree).u.c.ptrmemfunc_flag;
}

function TYPE_PTRMEMFUNC_FN_TYPE(NODE) {
  return TREE_TYPE (TYPE_FIELDS (NODE));
}

function DECL_SOURCE_LOCATION(node) {
  return node.decl_minimal.locus;
}

function LOC_IS_BUILTIN(loc) {
  return loc._source_location <= 2;
}

function GIMPLE_STMT_LOCUS(tree) {
  return tree.gstmt.locus;
}

function EXPR_LOCATION(tree) {
  return tree.exp.locus;
}

let EDGE_TRUE_VALUE = 1024;
let EDGE_FALSE_VALUE = 2048;

function SWITCH_COND(expr) { return TREE_OPERAND(expr, 0); }
function SWITCH_BODY(expr) { return TREE_OPERAND(expr, 1); }
function SWITCH_LABELS(expr) { return TREE_OPERAND(expr, 2); }

function CASE_LOW(expr) { return TREE_OPERAND(expr, 0); }
function CASE_HIGH(expr) { return TREE_OPERAND(expr, 1); }
function CASE_LABEL(expr) { return TREE_OPERAND(expr, 2); }

var LABEL_DECL_UID = isUsingGCCTuples ?
  function (decl) { return decl.label_decl.label_decl_uid; } :
  function (decl) { return decl.decl_common.pointer_alias_set; }

/** Given an OBJ_TYPE_REF, return a FUNCTION_DECL for the method. */
function resolve_virtual_fun_from_obj_type_ref(ref) {
  // This is a GCC macro definition, copied here for the use of this
  // function only.
  let TARGET_VTABLE_USES_DESCRIPTORS = 0;

  let obj_type = TREE_TYPE(OBJ_TYPE_REF_OBJECT(ref));
  let index = OBJ_TYPE_REF_TOKEN(ref).int_cst.int_cst.low
  let fun = BINFO_VIRTUALS (TYPE_BINFO (TREE_TYPE (obj_type)));
  while (index)
    {
      fun = TREE_CHAIN (fun);
      index -= (TARGET_VTABLE_USES_DESCRIPTORS
		? TARGET_VTABLE_USES_DESCRIPTORS : 1);
    }

  return BV_FN (fun);
}

/** True if the given FUNCTION_TYPE has stdargs. */
function stdarg_p(fntype) {
  let last;
  for (let a = TYPE_ARG_TYPES(fntype); a; a = TREE_CHAIN(a)) {
    last = TREE_VALUE(a);
  }
  return last != undefined && TREE_CODE(last) != VOID_TYPE;
}

let UNKNOWN_LOCATION = undefined;

/** Return the source code location of the argument, which must be a tree.
 *  The result is a JS object with 'file', 'line', and 'column' properties. */
function location_of(t) {
  if (GIMPLE_TUPLE_P(t))
    return t.gsbase.location;
  if (TREE_CODE (t) == PARM_DECL && DECL_CONTEXT (t))
    t = DECL_CONTEXT (t);
  else if (TYPE_P (t)) {
    t = TYPE_MAIN_DECL (t);
    /* pointer and reference types don't have a TYPE_MAIN_DECL, or a location */
    if (!t)
      return UNKNOWN_LOCATION;
  }
  
  // TODO disabling this for now
  //else if (TREE_CODE (t) == OVERLOAD)
  //  t = OVL_FUNCTION (t);
  
  if (t.tree_code() == TEMPLATE_PARM_INDEX)
    return UNKNOWN_LOCATION;

  if (DECL_P(t))
    return DECL_SOURCE_LOCATION (t);
  else if (EXPR_P(t))
    return EXPR_LOCATION(t);
  else if (GIMPLE_STMT_P(t))
    return GIMPLE_STMT_LOCUS(t);
  else
    return UNKNOWN_LOCATION;
}


function DECL_ORIGINAL_TYPE (node) {
  return node.decl_non_common.result;
}

function TYPE_SIZE (node) {
  return node.type.size;
}

function TYPE_VALUES (node) {
  return node.type.values;
}

function COMPLETE_TYPE_P (node) {
  return TYPE_SIZE(node) != undefined;
}

var TYPE_VOLATILE = isGCC42 ?
  function (node) { return node.common.volatile_flag; } :
  function (node) { return node.base.volatile_flag; };

var TYPE_READONLY = isGCC42 ?
  function (node) { return node.common.readonly_flag; } :
  function (node) { return node.base.readonly_flag; };

function TYPE_RESTRICT (node) {
  return node.type.restrict_flag;
}

// flag representation of above
let TYPE_UNQUALIFIED   = 0;
let TYPE_QUAL_CONST    = 1;
let TYPE_QUAL_VOLATILE = 2;
let TYPE_QUAL_RESTRICT = 4;

function TYPE_QUALS(NODE) {
  return ((TYPE_READONLY (NODE) * TYPE_QUAL_CONST) |
          (TYPE_VOLATILE (NODE) * TYPE_QUAL_VOLATILE) |
          (TYPE_RESTRICT (NODE) * TYPE_QUAL_RESTRICT));
}

function CLASSTYPE_DECLARED_CLASS(node) {
  return LANG_TYPE_CLASS_CHECK(node).declared_class;
}

function LANG_DECL_U2_CHECK (node, tf) {
  let lt = DECL_LANG_SPECIFIC (node);
  if (decl_flags(lt).u2sel != tf)
    throw Error("LANG_DECL_U2_CHECK failed! expected: " + lt + " got: " + decl_flags(lt).u2sel);
  return decl_flags(lt).u2;
}

function DECL_ACCESS(node) {
  return LANG_DECL_U2_CHECK(node, 0).access;
}

function ANON_AGGRNAME_P(id_node) {
  return IDENTIFIER_POINTER(id_node).substr(0, 7) == '__anon_';
}

function TYPE_FIELDS(node) {
  return node.type.values;
}

function DECL_ARTIFICIAL(node) {
  return node.decl_common.artificial_flag;
}

var TREE_STATIC = isGCC42 ?
  function (node) { return node.common.static_flag; } :
  function (node) { return node.base.static_flag; };

var TREE_PUBLIC = isGCC42 ?
  function (node) { return node.common.public_flag; } :
  function (node) { return node.base.public_flag; };

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
  return TYPE_CHECK(node).type.main_variant;
}

function TYPE_NEXT_VARIANT(node) {
  return TYPE_CHECK(node).type.next_variant;
}

function DECL_CLONED_FUNCTION_P(node) {
  return (TREE_CODE(node) == FUNCTION_DECL || TREE_CODE(node) == TEMPLATE_DECL) &&
    DECL_LANG_SPECIFIC(node) &&
    !decl_flags(DECL_LANG_SPECIFIC(node)).thunk_p &&
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
  return decl_flags(DECL_LANG_SPECIFIC(node)).constructor_attr;
}

function DECL_DESTRUCTOR_P(node) {
  return decl_flags(DECL_LANG_SPECIFIC(node)).destructor_attr;
}

function DECL_PURE_VIRTUAL_P(node) {
  return decl_flags(DECL_LANG_SPECIFIC(node)).pure_virtual;
}

function DECL_VIRTUAL_P(node) {
  return node.decl_common.virtual_flag;
}

function TYPE_DOMAIN(node) {
  return node.type.values;
}

function TYPE_MIN_VALUE(node) {
  return node.type.minval;
}

function TYPE_MAX_VALUE(node) {
  return node.type.maxval;
}

function DECL_NONSTATIC_MEMBER_FUNCTION_P(node) {
  return TREE_CODE(TREE_TYPE(node)) == METHOD_TYPE;
}

function CLASSTYPE_TI_ARGS(node) {
  return TI_ARGS(CLASSTYPE_TEMPLATE_INFO(node));
}

function PRIMARY_TEMPLATE_P(node) {
  return DECL_PRIMARY_TEMPLATE(node) == node;
}

function DECL_PRIMARY_TEMPLATE(node) {
  return TREE_TYPE(DECL_INNERMOST_TEMPLATE_PARMS(node));
}

function DECL_INNERMOST_TEMPLATE_PARMS(node) {
  return INNERMOST_TEMPLATE_PARMS(DECL_TEMPLATE_PARMS(node));
}

function DECL_TEMPLATE_PARMS(node) {
  return node.decl_non_common.arguments;
}

function DECL_ASSEMBLER_NAME(node) {
  return node._decl_assembler_name.identifier;
}

function DECL_NONLOCAL(node) {
  return node.decl_common.nonlocal_flag
}

let INNERMOST_TEMPLATE_PARMS = TREE_VALUE;

function class_key_or_enum_as_string(t)
{
  if (TREE_CODE (t) == ENUMERAL_TYPE)
    return "enum";
  if (TREE_CODE (t) == UNION_TYPE)
    return "union";
  if (TYPE_LANG_SPECIFIC (t) && LANG_TYPE_IS_CLASS (t)) {
    if (CLASSTYPE_DECLARED_CLASS (t))
      return "class";
    return "struct";
  }
  // C struct
  if (TREE_CODE (t) == RECORD_TYPE)
    return "struct";
  return undefined;
}

function loc_as_string(loc) {
  return loc.file + ':' + loc.line + ':' + loc.column;
}

var TYPE_UNSIGNED = isGCC42 ?
  function (t) { return t.common.unsigned_flag; } :
  function (t) { return t.base.unsigned_flag; };

/* The following globals are functions because they are tree nodes which must
 * not be held past the current treehydra callback.
 */

function char_type_node() sys.treehydra.gcc.integer_types[itk_char.value];
function signed_char_type_node() sys.treehydra.gcc.integer_types[itk_signed_char.value];
function unsigned_char_type_node() sys.treehydra.gcc.integer_types[itk_unsigned_char.value];
function short_integer_type_node() sys.treehydra.gcc.integer_types[itk_short.value];
function short_unsigned_type_node() sys.treehydra.gcc.integer_types[itk_unsigned_short.value];
function integer_type_node() sys.treehydra.gcc.integer_types[itk_int.value];
function unsigned_type_node() sys.treehydra.gcc.integer_types[itk_unsigned_int.value];
function long_integer_type_node() sys.treehydra.gcc.integer_types[itk_long.value];
function long_unsigned_type_node() sys.treehydra.gcc.integer_types[itk_unsigned_long.value];
function long_long_integer_type_node() sys.treehydra.gcc.integer_types[itk_long_long.value];
function long_long_unsigned_type_node() sys.treehydra.gcc.integer_types[itk_unsigned_long_long.value];

function c_common_type_for_mode (mode, unsignedp)
{
  if (TYPE_MODE(integer_type_node()) == mode)
    return unsignedp ? unsigned_type_node() : integer_type_node();
  
  if (TYPE_MODE(signed_char_type_node()) == mode)
    return unsignedp ? unsigned_char_type_node() : signed_char_type_node();
  
  if (TYPE_MODE(short_integer_type_node()) == mode)
    return unsignedp ? short_unsigned_type_node() : short_integer_type_node();
  
  if (TYPE_MODE(long_integer_type_node()) == mode)
    return unsignedp ? long_unsigned_type_node() : long_integer_type_node();
  
  if (TYPE_MODE(long_long_integer_type_node()) == mode)
    return unsignedp ? long_long_unsigned_type_node() : long_long_integer_type_node();

  return undefined;
}

// tuple stuff
function gimple_code (gs) {
  return gs.gsbase.code
}

function gimple_location (gs)
{
  return gs.gsbase.location;
}
function gimple_op(gs, i) {
  return gs.gimple_ops[i]
}

function gimple_call_fn (gs)
{
  return gs.gimple_ops[1]
}

function gimple_call_lhs (gs)
{
  return gimple_op (gs, 0);
}

function gimple_call_fndecl (gs)
{
  let addr = gimple_call_fn (gs);
  if (TREE_CODE (addr) == ADDR_EXPR)
    {
      return TREE_OPERAND (addr, 0);
    }
  return undefined;
}

function gimple_num_ops (gs)
{
  return gs.gsbase.num_ops;
}

function gimple_call_arg_iterator (gs) {
  let num_ops = gimple_num_ops (gs)
  for (let i = 3;i<num_ops;i++) {
    yield gs.gimple_ops[i]
  }
}

function gimple_call_arg (gs, i) {
  return gs.gimple_ops[3+i]
}

function gimple_call_num_args (gs) {
  return gimple_num_ops (TREE_CHECK (gs, GIMPLE_CALL)) - 3;
}

function gimple_switch_index (gs) {
  return gs.gimple_ops[0]
}

function gimple_switch_num_labels (gs)
{
  return gimple_num_ops (gs) - 1;
}

function gimple_switch_label (gs, index)
{
  return gs.gimple_ops[index + 1];
}

let _comparisons = null;

var gimple_cond_code = isUsingGCCTuples ?
  function (gs) {
    if (!this._comparisons) {
      this._comparisons = {}
      let enums = [LT_EXPR, LE_EXPR, GT_EXPR, GE_EXPR, EQ_EXPR, NE_EXPR];
      for each (let e in enums)
        this._comparisons[e.value] = e
    }
    let ret = _comparisons[gs.gsbase.subcode];
    if (!ret)
      throw new Error("Unexpected gimple_cond subcode");
    return ret
  } :
  function (cond_expr) {
    return TREE_CODE(TREE_OPERAND(cond_expr, 0));
  }

var condition_operand = isUsingGCCTuples ? 
  function (gimple_cond, i) {
    return gimple_op(gimple_cond, i);
  } :
  function (cond_expr, i) {
    let expr = TREE_OPERAND(cond_expr, 0);
    let cond_op = TREE_OPERAND(expr, i);
    return cond_op;
  }
