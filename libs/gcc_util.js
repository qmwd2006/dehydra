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

/** Return the expression returned by a given GIMPLE_RETURN or
 *  RETURN_EXPR. Return undefined with void returns. */
function return_expr(exp) {
  TREE_CHECK(exp, RETURN_EXPR, GIMPLE_RETURN);

  let ret = exp.operands()[0];
  if (!ret)
    return undefined;
  switch (ret.tree_code()) {
  case INIT_EXPR:
  case GIMPLE_MODIFY_STMT:{
    let rhs = ret.operands()[1];
    return rhs;
  }
  default:
    return ret;
  }
}

/** Yields all virtual member function decls in a node containing
 *  expressions of the form &Class::foo, where foo is virtual.
 *  We don't handle (frontend specific?) PTRMEM_CST nodes.
 */
function resolve_virtual_fn_addr_exprs(node) {
  switch (node.tree_code()) {
  case CONSTRUCTOR:
    switch (TREE_TYPE(node).tree_code()) {
    case ARRAY_TYPE:
    case RECORD_TYPE:
    case UNION_TYPE:
      for each (let ce in VEC_iterate(CONSTRUCTOR_ELTS(node))) {
        if (ce.index.tree_code() == FIELD_DECL &&
            ce.value.tree_code() == INTEGER_CST &&
            IDENTIFIER_POINTER(DECL_NAME(ce.index)) == "__pfn")
        {
          yield resolve_virtual_fn_addr_expr(ce.index, ce.value);
        }
      }
      break;
    case LANG_TYPE:
      break;
    default:
      throw new Error("Unexpected type in constructor: " + TREE_TYPE(node).tree_code());
    }
    break;
  case GIMPLE_ASSIGN:
    let lhs = gimple_op(node, 0);
    if (lhs.tree_code() != COMPONENT_REF)
      return;
    let field_decl = TREE_OPERAND(lhs, 1)
    if (field_decl.tree_code() != FIELD_DECL)
      return;
    if (IDENTIFIER_POINTER(DECL_NAME(field_decl)) != "__pfn")
      return;
    let rhs = gimple_op(node, 1);
    if (rhs.tree_code() != INTEGER_CST)
      return;
    yield resolve_virtual_fn_addr_expr(field_decl, rhs);
  }
}

/** Helper for resolve_virtual_fn_addr_exprs */
function resolve_virtual_fn_addr_expr(pfn, vtable_index_tree) {
  let base = TYPE_METHOD_BASETYPE(TREE_TYPE(TYPE_PTRMEMFUNC_FN_TYPE(DECL_CONTEXT(pfn))));
  let func = BINFO_VIRTUALS(TYPE_BINFO(base));
  let ptr_size = TREE_INT_CST_LOW(TYPE_SIZE_UNIT(TREE_TYPE(vtable_index_tree)));
  let vtable_index = TREE_INT_CST_LOW(vtable_index_tree);
  while (vtable_index > 1) {
    func = TREE_CHAIN(func);
    vtable_index -= ptr_size;
  }
  return BV_FN(func);
}

/** Iterate over a functions local variables */
function local_decls_iterator(fndecl) {
  for (let list = DECL_STRUCT_FUNCTION(fndecl).local_decls;
       list;
       list = TREE_CHAIN(list))
    yield TREE_VALUE(list);
}

/** Iterate over every statement of every block in the CFG. */
function cfg_isn_iterator(cfg) {
  for (let bb in cfg_bb_iterator(cfg))
    for (let isn in bb_isn_iterator(bb))
      yield isn;
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
function bb_stmt_list(bb) {
  let iltree = bb.il.tree;
  return iltree ? iltree.stmt_list : undefined;
}    

/** Iterate over statements of a BB. */
let bb_isn_iterator;
/** Iterate over statements of a BB in reverse. */
let bb_isn_iterator_reverse;
/** Last instruction of a BB. Throws an exception if there are no instructions. */
let bb_isn_last;

// link_switches is a bit of a rough fit here but ok for now.

/** Link up switches with the CFG by annotating the outgoing edges
 *  with the case labels in a .case_val field, with null for default.
 *
 *  Return a list of finally_tmp variables found.
 *
 *  The purpose of this function is to allow analyses such as ESP to
 *  track the conditions that hold on branches of a switch. */
let link_switches;

function bb_gimple_seq(bb) {
  let gimple_bb_info = bb.il.gimple
  return gimple_bb_info ? gimple_bb_info.seq : undefined
}

bb_isn_iterator = function (bb) {
  let seq = bb_gimple_seq(bb)
  if (!seq)
    return
  for(let seq_node = seq.first;seq_node;seq_node=seq_node.next)
    yield seq_node.stmt
}

bb_isn_iterator_reverse = function (bb) {
  let seq = bb_gimple_seq(bb)
  if (!seq)
    return
  let seq_node = seq.last
  for(;seq_node;seq_node=seq_node.prev)
    yield seq_node.stmt
}

bb_isn_last = function (bb) {
  let seq = bb_gimple_seq(bb)
  return seq.last.stmt;
}
  
link_switches = function (cfg) {
  /** Helper for link_switches. */
  function link_switch(cfg, bb, isn) {
    // GCC uses a default label for finally_tmp vars, even though there
    // is really only one possibility. We'll find it and put that in, 
    // to help later analyses track branches.
    let label_count = gimple_switch_num_labels (isn)
    
    let max_val;
    
    for (i = 0; i < label_count; i++) {
      cl = gimple_switch_label (isn, i)
      let case_val = CASE_LOW(cl) == undefined ? null : TREE_INT_CST_LOW(CASE_LOW(cl));
      if (case_val != null && (max_val == undefined || case_val > max_val))
        max_val = case_val;
      if (is_finally_tmp(gimple_switch_index(isn)) && case_val == null) {
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
  let ans = [];
  for (let bb in cfg_bb_iterator(cfg)) {
    for (let isn in bb_isn_iterator(bb)) {
      if (gimple_code(isn) == GIMPLE_SWITCH) {
        let cond = gimple_switch_index(isn)
        if (is_finally_tmp(cond)) {
          ans.push(cond);
        }
        link_switch(cfg, bb, isn, ans);
      }
    }
  }
  return ans;
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
  let args = [];
  for (let a in flatten_chain(tree)) {
    let val = TREE_VALUE(a);
    args.push(TREE_CODE(val) == INTEGER_CST ?
              TREE_INT_CST_LOW(val) : TREE_STRING_POINTER(val));
  }
  return args;
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

/** The argument must be a GIMPLE_CALL. If it represents a call to a named
 * function (not a method or function pointer), then return the name.
 * Otherwise return undefined. */
function gimple_call_function_name(gs) {
  let decl = gimple_call_fndecl(gs);
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

/** Return an array of the arguments of a CALL_EXPR. */
function call_expr_args(call_expr) {
  return [ a for (a in call_arg_iterator(call_expr)) ];
}

/** Return an array of the arguments of a GIMPLE_CALL. */
function gimple_call_args(gimple_call) {
  return [ a for (a in gimple_call_arg_iterator(gimple_call)) ];
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

