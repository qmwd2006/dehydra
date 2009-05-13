/* Convenience functions for using GCC data in JS. */

// Extract the FUNCTION_DECL being called from the arg of a call site,
// or a VAR_DECL/PARM_DECL if the value is a function pointer.
// If a complex expression is being called (not a declaration), this
// function will return null.
function callable_arg_function_decl(arg) {
  switch (TREE_CODE(arg)) {
  case ADDR_EXPR:
    return callable_arg_function_decl(arg.operands()[0]);
  case FUNCTION_DECL:
    return arg;
  case OBJ_TYPE_REF:
    return resolve_virtual_fun_from_obj_type_ref(arg);
  case COMPONENT_REF:
    return arg.operands()[1];
  case VAR_DECL:
  case PARM_DECL:
    return arg;
  default:
    if (!EXPR_P(arg))
      warning("Unexpected argument to a CALL_EXPR: " + TREE_CODE(arg),
              location_of(arg));
    
    return null;
  }
}

/** Return the string name of the given DECL. */
function decl_name_string(decl) {
  let name = DECL_NAME(decl);
  return name ? IDENTIFIER_POINTER(name) : '<anon>';
}

/** Return an iterator over the args of a FUNCTION_TYPE. */
function function_type_args(fntype) {
  for (let a = TYPE_ARG_TYPES(fntype);
       a && TREE_CODE(TREE_VALUE(a)) != VOID_TYPE;
       a = TREE_CHAIN(a)) {
    yield TREE_VALUE(a);
  }
}

/** Return the CFG of a FUNCTION_DECL. */
function function_decl_cfg(tree) {
  return tree.function_decl.f.cfg;
}

/** Return the name of the given FUNCTION_DECL. */
let function_decl_name = decl_name_string;

/** Return a string representation of the return type of the given
 *  FUNCTION_DECL. */
function function_decl_return_type_name(tree) {
  let type = TREE_TYPE(TREE_TYPE(tree));
  return type_string(type);
}

/** Iterate over the basic blocks in the CFG. */
function cfg_bb_iterator(cfg) {
  let bb_entry = cfg.x_entry_block_ptr;
  let bb_exit = cfg.x_exit_block_ptr;
  let bb = bb_entry;
  while (true) {
    yield bb;
    if (bb === bb_exit) break;
    bb = bb.next_bb;
  }
}

/** Iterate over the statements in a STATEMENT_LIST */
function iter_statement_list(sl)
{
  for (let ptr = STATEMENT_LIST_HEAD(sl);
       ptr;
       ptr = ptr.next)
    yield ptr.stmt;
}

// BB Access

/** Return a text label for a BB. 'cfg' is optional, and is used to add (entry) and (exit) to
  * those blocks. */
function bb_label(cfg, bb) {
  // cfg param is optional
  if (bb == undefined) {
    bb = cfg;
  }
  if (bb == undefined) return "BB? (undefined)";

  let s = 'BB' + bb.index;
  if (cfg) {
    if (bb === cfg.x_entry_block_ptr) s += ' (entry)';
    else if (bb === cfg.x_exit_block_ptr) s += ' (exit)';
  }
  return s;
}

/** Iterator over successor BBs of a BB. */
function bb_succs(bb) {
  for each (let edge in VEC_iterate(bb.succs)) {
    // Not sure why this happens, but it does seem to for exit blocks
    if (edge.dest != undefined) {
      yield edge.dest;
    }
  }
}

/** Iterator over outgoing edges of a BB. */
function bb_succ_edges(bb) {
  return VEC_iterate(bb.succs);
}

/** Iterator over predecessor BBs of a BB. */
function bb_preds(bb) {
  for each (let edge in VEC_iterate(bb.preds)) {
    yield edge.src;
  }
}

/** Iterator over incoming edges of a BB. */
function bb_pred_edges(bb) {
  return VEC_iterate(bb.preds);
}

/** stmt_list in a bb, may vary on gcc version.
 */
var bb_stmt_list = isGCC42 ?
  function (bb) { return bb.stmt_list; } :
  function (bb) {
    let iltree = bb.il.tree;
    return iltree ? iltree.stmt_list : undefined;
  }    

/** Last instruction of a BB. Throws an exception if there are no instructions. */
function bb_isn_last(bb) {
  let stmt_list = bb_stmt_list(bb).stmt_list;
  return stmt_list.tail.stmt;
}

/** Iterate over statements of a BB. */
function bb_isn_iterator(bb) {
  let outer_stmt_list = bb_stmt_list(bb);
  if (outer_stmt_list != undefined) {
    let stmt_list = outer_stmt_list.stmt_list;
    let head = stmt_list.head;
    let tail = stmt_list.tail;
    let cur = head;
    // Apparently this can be undefined. Not sure why.
    if (!cur) return;
    while (true) {
      yield cur.stmt;
      if (cur == tail) break;
      cur = cur.next;
    }
  }
}

/** Iterator over statements of a BB in reverse order. */
function bb_isn_iterator_reverse(bb) {
  let outer_stmt_list = bb_stmt_list(bb);
  if (outer_stmt_list != undefined) {
    let stmt_list = outer_stmt_list.stmt_list;
    let head = stmt_list.head;
    let tail = stmt_list.tail;
    let cur = tail;
    // Apparently this can be undefined. Not sure why.
    if (!cur) return;
    while (true) {
      yield cur.stmt;
      if (cur == head) break;
      cur = cur.prev;
    }
  }
}


// Translate GCC FUNCTION_DECL to a pure JS object

// Declaration translators

/** Iterator over elements of a GCC TREE_CHAIN list representation. */
function flatten_chain(chain_head) {
  for (let o = chain_head; o; o = TREE_CHAIN(o)) {
    yield o;
  }
}

/** Return pure JS representation of arguments of an attribute. */
function rectify_attribute_args(tree) {
  return [ TREE_STRING_POINTER(TREE_VALUE(a)) for (a in flatten_chain(tree)) ]
}

/** Return pure JS representation of attribtues of an AST node. */
function rectify_attributes(tree) {
  return [ { 
    name: IDENTIFIER_POINTER(TREE_PURPOSE(attr)),
    args: rectify_attribute_args(TREE_VALUE(attr))
  }
           for (attr in flatten_chain(tree)) ]
}

/** Return pure JS representation of a FUNCTION_DECL. */
function rectify_function_decl(tree) {
  return {
    name: decl_name_string(tree),
    resultType: function_decl_return_type_name(tree),
    params: [ { 
      name: decl_name_string(o),
      type: type_string(TREE_TYPE(o)),
      attrs: rectify_attributes(DECL_ATTRIBUTES(o))
    }
              for (o in flatten_chain(DECL_ARGUMENTS(tree))) ]
  };
}

/** Return a string representation of output of rectify_function_decl. */
function rfunc_string(rfunc) {
  return rfunc.resultType + ' ' + rfunc.name + '(' + 
    [ a.name + ' ' + a.type for each (a in rfunc.params) ].join(', ') + ')';
}

/** Iterator over parameters of a FUNCTION_DECL. */
function function_decl_params(tree) {
  // void is used as a sentinel-type thing by GCC.
  return (v for (v in flatten_chain(DECL_ARGUMENTS(tree)))
    if (v && TREE_CODE(TREE_TYPE(v)) != VOID_TYPE));
}

/** If the given node represents an integer literal expression, return
 * the value of the integer. Otherwise return undefined. */
function expr_literal_int(expr) {
  if (TREE_CODE(expr) == INTEGER_CST) {
    return TREE_INT_CST_LOW(expr);
  } else {
    return undefined;
  }
}

function call_function_decl(expr) {
  let fptr = CALL_EXPR_FN(expr);
  if (fptr.tree_code() != ADDR_EXPR) return undefined;
  let decl = TREE_OPERAND(fptr, 0);
  if (decl.tree_code() != FUNCTION_DECL) return undefined;
  return decl
}

/** The argument must be a CALL_EXPR. If it represents a call to a named
 * function (not a method or function pointer), then return the name.
 * Otherwise return undefined. */
function call_function_name(expr) {
  let decl = call_function_decl(expr)
  return decl ? decl_name_string(decl) : undefined;
}

/** Return the ith argument of the function, counting from zero and including
 * 'this' in the list if it is present. */
function call_arg(expr, i) {
  let arg_count = call_expr_nargs(expr);
  if (i < 0 || i >= arg_count)
    throw new Error("arg index out of range: call has " + arg_count + " args");
  return CALL_EXPR_ARG(expr, i);
}

/** Iterator over the argument list of the function call. */
var call_arg_iterator = call_expr_arg_iterator;

/** Return an array of the arguments of a function call. */
function call_args(expr) {
  return [ a for (a in call_arg_iterator(expr)) ];
}

/** Iterate over the base hierarchy of the given RECORD_TYPE in DFS order,
 * including the type itself. */
function dfs_base_iterator(record_type) {
  // Worklist of BINFOs.
  let work = [ TYPE_BINFO(record_type) ];
  // Set of already-visited types -- We don't use decl_name_string here because the <anon> return
  // values would not be unique, although we shouldn't get any anyway.
  let done = MapFactory.create_set(
    function (x, y) DECL_UID(x) == DECL_UID(y),
    function (x) DECL_UID(x),
    function (t) IDENTIFIER_POINTER(DECL_NAME(TYPE_NAME(t))),
    function (t) type_string(t));
  while (work.length > 0) {
    let binfo = work.pop();
    let type = BINFO_TYPE(binfo);
    if (done.has(type)) continue;
    done.add(type);
    yield type;
    for each (let base in VEC_iterate(BINFO_BASE_BINFOS(binfo))) {
      work.push(base);
    }
  }
}

// link_switches is a bit of a rough fit here but ok for now.

/** Link up switches with the CFG by annotating the outgoing edges
 *  with the case labels in a .case_val field, with null for default.
 *
 *  Return a list of finally_tmp variables found.
 *
 *  The purpose of this function is to allow analyses such as ESP to
 *  track the conditions that hold on branches of a switch. */
function link_switches(cfg) {
  let ans = [];
  for (let bb in cfg_bb_iterator(cfg)) {
    for (let isn in bb_isn_iterator(bb)) {
      if (TREE_CODE(isn) == SWITCH_EXPR) {
        if (is_finally_tmp(SWITCH_COND(isn))) {
          ans.push(SWITCH_COND(isn));
        }
        link_switch(cfg, bb, isn, ans);
      }
    }
  }
  return ans;
}

/** Helper for link_switches. */
function link_switch(cfg, bb, isn) {
  // GCC uses a default label for finally_tmp vars, even though there
  // is really only one possibility. We'll find it and put that in, 
  // to help later analyses track branches.
  let max_val;
  for each (let cl in SWITCH_LABELS(isn).vec.a) {
    let case_val = CASE_LOW(cl) == undefined ? null : TREE_INT_CST_LOW(CASE_LOW(cl));
    if (case_val != null && (max_val == undefined || case_val > max_val))
      max_val = case_val;
    if (is_finally_tmp(SWITCH_COND(isn)) && case_val == null) {
      case_val = max_val + 1;
    }

    let label_uid = LABEL_DECL_UID(CASE_LABEL(cl));
    let bb_succ = cfg.x_label_to_block_map.base.vec[label_uid];
    //print(label_uid + ' ' + bb_label(bb_succ));
    let found = false;
    for each (let e in bb_succ_edges(bb)) {
      //print('BBS ' + e.dest.index + ' ' + bb_succ.index);
      if (e.dest == bb_succ) {
        e.case_val = case_val;
        found = true;
        //print('LINK ' + e.src.index + ' -> ' + e.dest.index + ": " + e.case_val);
        break;
      }
    }
    if (!found)
      throw new Error("not found");
  }
}

/** Return true if the given variable is a 'finally_tmp' variable, i.e., introduced
 *  by GCC for a finally block (including destructors). */
function is_finally_tmp(decl) {
  let s = expr_display(decl);
  return s.substr(0, 11) == 'finally_tmp';
}

function translate_attributes(atts) {
  return [{'name': IDENTIFIER_POINTER(TREE_PURPOSE(a)),
	   'value': [translate_attribute_param(TREE_VALUE(arg))
		     for (arg in flatten_chain(TREE_VALUE(a)))]}
	  for (a in flatten_chain(atts))];
}

function translate_attribute_param(param) {
  switch (param.tree_code()) {
  case STRING_CST:
    return TREE_STRING_POINTER(param);
  case INTEGER_CST:
    return TREE_INT_CST_LOW(param);
  case IDENTIFIER_NODE:
    return IDENTIFIER_POINTER(param);
  }
}

