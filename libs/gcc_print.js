/* Functions for printing GCC objects as strings. */

/** Return a string representation of the given TYPE. */
function type_string(type) {
  // Fun special case -- handled like this in g++.
  // disabled for now -- test not impl
  if (false && TYPE_PTRMEMFUNC_P(type)) {
    return ptrmemfunc_type_string(type);
  }

  let prefix = TYPE_READONLY(type) ? "const " : "";
  let infix = TYPE_READONLY(type) ? " const " : "";

  let code = TREE_CODE(type);
  if (code == INTEGER_TYPE || code == REAL_TYPE || code == BOOLEAN_TYPE ||
      code == RECORD_TYPE || code == ENUMERAL_TYPE || code == UNION_TYPE) {
    if (TYPE_NAME(type) == undefined) {
      // Seems to happen for pointer to member types
      return "UNKNOWN";
    }
    return prefix + decl_name_string(TYPE_NAME(type));
  } else if (code == VOID_TYPE) {
    return 'void';
  } else if (code == POINTER_TYPE) {
    return type_string(TREE_TYPE(type)) + infix + '*';
  } else if (code == ARRAY_TYPE) {
    return type_string(TREE_TYPE(type)) + infix + '[]';
  } else if (code == REFERENCE_TYPE) {
    return type_string(TREE_TYPE(type)) + infix + '&';
  } else if (code == FUNCTION_TYPE) {
    return type_string(TREE_TYPE(type)) + ' (*)(' +
      [ type_string(TREE_VALUE(t)) for (t in flatten_chain(TYPE_ARG_TYPES(type))) ].join(', ') + ')';
      
  } else if (code == METHOD_TYPE) {
    return type_string(TREE_TYPE(type)) + ' (?::*)(' +
      [ type_string(TREE_VALUE(t)) for (t in flatten_chain(TYPE_ARG_TYPES(type))) ].join(', ') + ')';
  }

  print(TREE_CODE(type).name);
  do_dehydra_dump(type.type, 1, 2);
  throw new Error("unhandled type type");
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
  } else if (TREE_CODE(isn) == RETURN_EXPR) {
    let operand = isn.operands()[0];
    if (operand) {
      return isn_display_basic(operand, TREE_CODE(operand));
    } else {
      return [ 'void', '', '' ];
    }
  } else if (TREE_CODE(isn) == CALL_EXPR) {
    return [ 'void', ':=', expr_display(isn) ];
  } else if (TREE_CODE(isn) == COND_EXPR) {
    return [ 'if', '', expr_display(TREE_OPERAND(isn, 0)) ]
  } else if (TREE_CODE(isn) == SWITCH_EXPR) {
    return [ 'switch', '', expr_display(TREE_OPERAND(isn, 0)) ]
  } else {
    return ['[this tree code not impl]', '', ''];
  }
}

/** Return a string representation of the given expression node. */
function expr_display(expr) {
  let code = TREE_CODE(expr);
  if (code == INDIRECT_REF) {
    return '*' + expr_display(expr.exp.operands[0]);
  } else if (code == ADDR_EXPR) {
    return '&' + expr_display(TREE_OPERAND(expr, 0));
  } else if (code == PLUS_EXPR) {
    return expr_display(TREE_OPERAND(expr, 0)) + ' +  ' +
      expr_display(TREE_OPERAND(expr, 1));
  } else if (code == NE_EXPR) {
    return expr_display(TREE_OPERAND(expr, 0)) + ' != ' +
      expr_display(TREE_OPERAND(expr, 1));
  } else if (code == EQ_EXPR) {
    return expr_display(TREE_OPERAND(expr, 0)) + ' == ' +
      expr_display(TREE_OPERAND(expr, 1));
  } else if (code == PARM_DECL || code == VAR_DECL || code == RESULT_DECL) {
    let decl_name = DECL_NAME(expr);
    if (decl_name) return IDENTIFIER_POINTER(decl_name);
    let c = code == CONST_DECL ? 'C' : (code == RESULT_DECL ? 'R' : 'D');
    return c + '.' + DECL_UID(expr);
  } else if (code == CALL_EXPR) {
    let ans = '';
    let name = TREE_OPERAND(expr, 1);
    ans += call_name_display(name);

    ans += '(';
    let operand_count = TREE_INT_CST(TREE_OPERAND(expr, 0)).low;
    let arg_count = operand_count - 3;
    for (let i = 0; i < arg_count; ++i) {
      let operand_index = i + 3;
      let operand = TREE_OPERAND(expr, operand_index);
      if (i != 0) ans += ', ';
      ans += expr_display(operand);
    }
    ans += ')';
    return ans;
  } else if (code == INTEGER_CST) {
    return expr.int_cst.int_cst.low;
  } else {
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
    return decl_name_string(name);
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

/** Return a string representation of a return value from location_of. */
function loc_string(loc) {
  return loc.file + ':' + loc.line + ':' + loc.column;
}
