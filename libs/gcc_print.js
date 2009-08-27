include ("util.js");
include ("gcc_compat.js");

/* Functions for printing GCC objects as strings. */

/** Return a string representation of the given TYPE. */
function type_string(type) {
  let quals = [];
  if (TYPE_VOLATILE(type))
    quals.push('volatile');
  if (TYPE_RESTRICT(type))
    quals.push('restrict');
  if (TYPE_READONLY(type))
    quals.push('const');

  let prefix = quals.length ? quals.join(' ') + ' ' : '';
  let suffix = quals.length ? ' ' + quals.join(' ') : '';

  let type_decl = TYPE_NAME (type);
  if (type_decl) {
    if (TREE_CODE(type_decl) == IDENTIFIER_NODE)
      return prefix + IDENTIFIER_POINTER(type_decl);
    if (TREE_CODE(type_decl) == TYPE_DECL)
      return prefix + decl_name_string(type_decl);
    throw new Error("bad TREE_CODE for TYPE_NAME");
  }

  if (TYPE_PTRMEMFUNC_P(type)) {
    // skip the POINTER_TYPE and go straight to the METHOD_TYPE
    type = TREE_TYPE(TYPE_PTRMEMFUNC_FN_TYPE(type));
  }

  let code = TREE_CODE(type);

  switch (code) {
  case INTEGER_TYPE:
  case REAL_TYPE:
  case FIXED_POINT_TYPE:
  case BOOLEAN_TYPE:
    let prec = TYPE_PRECISION(type);
    type = c_common_type_for_mode (TYPE_MODE (type), TYPE_UNSIGNED (type));
    if (!type)
      throw new Error("c_common_type_for_mode failed");

    if (TYPE_NAME(type)) {
      let r = prefix + decl_name_string(TYPE_NAME(type));
      if (TYPE_PRECISION(type) != prec) {
        r += ":" + prec;
      }
      return r;
    }

    let r;
    switch (code) {
    case INTEGER_TYPE:
      r = TYPE_UNSIGNED(type) ? "<unnamed-unsigned:" : "<unnamed-signed:";
      break;
    case REAL_TYPE:
      r = "<unnamed-float:";
      break;
    case FIXED_POINT_TYPE:
      r = "<unnamed-fixed:";
      break;
    default:
      throw new Error("Unexpected code path");
    }
    return r + prec + ">";

  case VOID_TYPE:
    return "void";

  case POINTER_TYPE:
    return type_string(TREE_TYPE(type)) + '*' + suffix;

  case ARRAY_TYPE:
    return type_string(TREE_TYPE(type)) + '[]' + suffix;

  case REFERENCE_TYPE:
    return type_string(TREE_TYPE(type)) + '&' + suffix;

  case FUNCTION_TYPE:
    return type_string(TREE_TYPE(type)) + ' (*)(' + type_args_string(type) + ')';

  case METHOD_TYPE:
    // XXX exclude |this| from args?
    return type_string(TREE_TYPE(type)) + ' (' + type_string(TYPE_METHOD_BASETYPE(type)) + '::*)(' + type_args_string(type) + ')';

  case OFFSET_TYPE:
    return type_string(TREE_TYPE(type)) + " " +
           type_string(TYPE_OFFSET_BASETYPE(type)) + "::*";

  case VECTOR_TYPE:
    return prefix + "vector " + type_string(TREE_TYPE(type)) + "[" + (1 << TYPE_PRECISION(type)) + "]";

  case RECORD_TYPE:
  case ENUMERAL_TYPE:
  case UNION_TYPE:
    // for these codes we should always have a TYPE_NAME. however, the C FE can be nasty
    // and not give us one in cases involving typedefs (bug 511261). for such cases,
    // we can make a guess...
    if (sys.frontend != "GNU C")
      throw new Error("TYPE_NAME null for " + code);

    // walk the list of variants for this type, and get the last variant with a valid TYPE_NAME.
    // this should represent the type we originated from. (we'll probably walk over the actual
    // TYPE_NAME we're after, too, but we've no way of knowing which one - any variants declared
    // after our one would appear first in the list, and we're better off walking toward the
    // source than away from it, for purposes of finding a consistent identifier.)
    for (let t = TYPE_MAIN_VARIANT(type); t && TYPE_NEXT_VARIANT(t); t = TYPE_NEXT_VARIANT(t))
      if (TYPE_NAME(t))
        type_decl = TYPE_NAME(t);
    if (TREE_CODE(type_decl) != TYPE_DECL)
      throw new Error("expected TYPE_DECL; original type is " + TREE_CODE(type_decl));
    return prefix + decl_name_string(type_decl);

  case TYPE_DECL:
  case IDENTIFIER_NODE:
    throw new Error("Unexpected " + code);

  default:
    print(TREE_CODE(type).name);
    do_dehydra_dump(type.type, 1, 2);
    throw new Error("unhandled type type");
  }
}

/** Return a string representation of the args part of a function type,
  * not including the parens. */
function type_args_string(type)
{
  // GCC specialness: if the last item is void, then the function does
  // not use stdargs. Otherwise it does.
  let items = [];
  let stdarg = true;
  for (let o = TYPE_ARG_TYPES(type); o; o = TREE_CHAIN(o)) {
    let t = TREE_VALUE(o);
    if (TREE_CODE(t) == VOID_TYPE) {
      stdarg = false;
      break;
    }
    items.push(type_string(t));
  }
  if (stdarg) items.push('...');
  return items.join(', ');
}

/* Return a display representation of the given CFG instruction. */
function isn_display(isn) {
  let code = TREE_CODE(isn);
  let kind = rpad(code.name, 24);
  let [lhs, op, rhs] = isn_display_basic(isn, code);
  return kind + rpad(lhs, 8) + ' ' + rpad(op, 4) + rhs;
}

/** Helper for isn_display. This returns a representation without showing
 *  the TREE_CODE and padding, i.e., the statement-type-specific part. */
function isn_display_basic(isn, code) {
  if (code == GIMPLE_MODIFY_STMT) {
    let operands = isn.operands();
    return [expr_display(operands[0]), ":=", expr_display(operands[1])]
  } 
  switch (TREE_CODE(isn)) {
  case RETURN_EXPR:
    let operand = isn.operands()[0];
    if (operand) {
      return isn_display_basic(operand, TREE_CODE(operand));
    } else {
      return [ 'void', '', '' ];
    }
  case CALL_EXPR:
    return [ 'void', ':=', expr_display(isn) ];
  case COND_EXPR:
    return [ 'if', '', expr_display(TREE_OPERAND(isn, 0)) ];
  case SWITCH_EXPR:
    return [ 'switch', '', expr_display(TREE_OPERAND(isn, 0)) ];
  case LABEL_EXPR:
    return [ decl_name(isn.operands()[0]) + ':', '', '' ];
  default:
    return ['[this tree code not impl]', '', ''];
  }
}

/** Return a string representation of the given expression node. */
function expr_display(expr) {
  let code = TREE_CODE(expr);
  switch (code) {
  case INDIRECT_REF:
    return '*' + expr_display(expr.exp.operands[0]);
  case ARRAY_REF:
    return expr_display(TREE_OPERAND(expr, 0)) +
           '[' + expr_display(TREE_OPERAND(expr, 1)) + ']';
  case ADDR_EXPR:
    return '&' + expr_display(TREE_OPERAND(expr, 0));
  case TRUTH_NOT_EXPR:
    return '!' + expr_display(TREE_OPERAND(expr, 0));
  case NOP_EXPR:
    return expr_display(TREE_OPERAND(expr, 0));
  case PLUS_EXPR:
  case POINTER_PLUS_EXPR:
    return expr_display(TREE_OPERAND(expr, 0)) + ' +  ' +
      expr_display(TREE_OPERAND(expr, 1));
  case NE_EXPR:
    return expr_display(TREE_OPERAND(expr, 0)) + ' != ' +
      expr_display(TREE_OPERAND(expr, 1));
  case EQ_EXPR:
    return expr_display(TREE_OPERAND(expr, 0)) + ' == ' +
      expr_display(TREE_OPERAND(expr, 1));
  case CONVERT_EXPR:
    return '(' + type_string(TREE_TYPE(expr)) + ') ' +
      expr_display(expr.operands()[0]);
  case CALL_EXPR:
    let ans = '';
    let name = CALL_EXPR_FN(expr);
    ans += call_name_display(name);

    ans += '(';
    let first = true;
    for (let operand in call_arg_iterator(expr)) {
      if (first)
        first = false;
      else
        ans += ', ';
      ans += expr_display(operand);
    }
    ans += ')';
    return ans;
  case INTEGER_CST:
    return expr.int_cst.int_cst.low;
  case COMPONENT_REF:
    let ops = expr.operands()
    let struct = expr_display (ops[0])
    if (TREE_CODE(ops[0]) == INDIRECT_REF)
      struct = "(" + struct + ")"
    return struct + "." + expr_display (ops[1])
  default:
    if (DECL_P(expr)) {
      return decl_name(expr, true)
    }
    //print("    *** " + code.name);
    //do_dehydra_dump(expr, 4, 2);
    return code.name;
  }
}

/** Return a string representation the function being called in the
 *  given CALL_EXPR function operand. */
function call_name_display(name) {
  switch (TREE_CODE(name)) {
  case ADDR_EXPR:
    return call_name_display(TREE_OPERAND(name, 0));
  case VAR_DECL:
  case PARM_DECL:
    return '*' + decl_name_string(name);
  case FUNCTION_DECL:
    return decl_name(name);
  case OBJ_TYPE_REF:
    let func = resolve_virtual_fun_from_obj_type_ref(name);
    return call_name_display(func);
  default:
    throw new Error("call_name_display ni " + TREE_CODE(name).name);
  }
}

/** Dump the given CFG to stdout. */
function cfg_dump(cfg) {
  for (let bb in cfg_bb_iterator(cfg)) {
    print(bb_label(cfg, bb));
    for each (let edge in VEC_iterate(bb.succs)) {
      let bbn = edge.dest;
      print('  -> ' + bb_label(cfg, bbn));
      //do_dehydra_dump(bbn, 1, 1);
    }
    print("  statements:");
    for (let isn in bb_isn_iterator(bb)) {
      print_indented(isn_display(isn), 2);
/*
      let [c, o] = rectify_statement(isn);
      print_indented(c.name, 2);
      do_dehydra_dump(o, 2, 3);
      print("    --");
*/
    }
  }
}

/** Return a string representation of the given edge in the given CFG. */
function edge_string(edge, cfg) {
  return bb_label(cfg, edge.src) + ' -> ' + bb_label(cfg, edge.dest);
}

/** Return a string representation of a return value from location_of. 
 *  Use loc.toString() instead; this is for compatibility with existing
 *  scripts. */
function loc_string(loc) {
  return loc.toString();
}

// tuple stuff
function gs_display(gs) {
  switch(gs.gsbase.code) {
  case GIMPLE_CALL: {
    let fn = gimple_call_fndecl(gs);
    let lhs = gimple_call_lhs(gs);
    let args = []
    for (a in gimple_call_arg_iterator(gs)) {
      args.push(expr_display(a))
    }
    return (lhs ? call_name_display(lhs) + " = " : "")
      + call_name_display(fn) + "(" + args.join(",") +")"
  }
  break
  case GIMPLE_ASSIGN:
    return expr_display(gs.gimple_ops[0]) + " = " + expr_display(gs.gimple_ops[1])
  case GIMPLE_RETURN: {
    let op = gs.gimple_ops[0];
    return "return " + (op ? expr_display(gs.gimple_ops[0]) : "")
  }
  case GIMPLE_LABEL: 
    return decl_name(gs.gimple_ops[0]) + ':'
  case GIMPLE_COND:
    return "if ("+expr_display(gs.gimple_ops[0])+")"
  case GIMPLE_SWITCH:
    return "switch ("+expr_display(gs.gimple_ops[0])+")"
  default:
    return "Unhandled " + gs.gsbase.code
  }
}
