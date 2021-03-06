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

/* Program Analysis Library */

/* Analsis-specialized ADTs */

function decl_equals(x, y) {
  return DECL_UID(x) == DECL_UID(y);
}

function decl_hashcode(x) {
  return DECL_UID(x);
}

/** Return a Set specialized for DECL keys. */
function create_decl_set(items) {
  let ans = MapFactory.create_set(decl_equals, decl_hashcode,
                                  decl_get_key, decl_get_label);
  if (items != undefined) {
    for each (let item in items) {
      ans.add(item);
    }
  }
  return ans;
}

/** Return a Map with DECL keys. */
function create_decl_map() {
  return MapFactory.create_map(decl_equals, decl_hashcode, 
                               decl_get_key, decl_get_label);
}

/** Return a key usable for Maps with DECLs. */
function decl_get_key(d) {
  return DECL_UID(d);
}

/** Label function suitable for printing DECLs. */
function decl_get_label(d) {
  return expr_display(d);
}

/* Analysis-related accessors */

function defs_filter(ls) {
  for each(let d in ls) {
    if (!d)
      continue;

    // ignore |this|
    if (d.tree_code() == COMPONENT_REF)
      d = TREE_OPERAND(d, 1);
    
    try {
      walk_tree(d, function (t) {
        if (DECL_P(t))
          throw t;
      });
    } catch (t) { yield t; }
  }
}

function uses_filter(ls) {
  for each(let d in ls) {
    if (!d)
      continue;

    try {
      walk_tree(d, function (t) {
        if (DECL_P(t))
          throw t;
      });
    } catch (t) { yield t; }
  }
}

/** Iterate over variables defined by the instruction. 
    If kind == 'strong', iterate over only strong defs. */
function isn_defs(isn, kind) {
  let retls = []
  switch (gimple_code(isn)) {
  case GIMPLE_ASSIGN:
    retls.push(isn.gimple_ops[0]);
    if (kind != 'strong') {
      retls.push(isn.gimple_ops[1]);
      retls.push(isn.gimple_ops[2]);
    }
    break;
  case GIMPLE_CALL:
    {
      retls.push(gimple_call_lhs(isn));
      if (kind != 'strong') {
        for (let d in gimple_call_arg_iterator(isn))
          retls.push(d);
      }
    }
    break;
  case GIMPLE_RETURN:
    if (kind != 'strong') {
      let operand = isn.gimple_ops[0];
      retls.push(operand);
    }
    break;
  case GIMPLE_COND:
  case GIMPLE_SWITCH:
    break;
  default:
    throw new Error("isn_defs ni " + gimple_code(isn));
  }
  
  for each(let d in defs_filter(retls))
    yield d;
};

/** Iterate over variables used by the instruction.  */
function isn_uses(isn) {
  let retls = [];
  switch (gimple_code(isn)) {
  case GIMPLE_ASSIGN:
    retls.push(isn.gimple_ops[0]);
    retls.push(isn.gimple_ops[1]);
    break;
  case GIMPLE_CALL: 
    for (let d in gimple_call_arg_iterator(isn))
      retls.push(d);
    break;
  case GIMPLE_RETURN:
    retls.push(isn.gimple_ops[0]);
    break;
  case GIMPLE_COND:
    retls.push(isn.gimple_ops[0]);
    retls.push(isn.gimple_ops[1]);
    break;
  case GIMPLE_SWITCH:
    retls.push(isn.gimple_ops[0]);
    break;
  default:
    throw new Error("ni " + gimple_code(isn));
  }
  for each(let d in uses_filter(retls))
    yield d;
};

/** Iterate over variables defined by a call. All of these are weak defs.
  * This is unsound -- we just iterate over x where &x is an arg, without
  * attempting alias analysis. */
function call_expr_defs(isn) {

  let operand_count = TREE_INT_CST(TREE_OPERAND(isn, 0)).low;
  let arg_count = operand_count - 3;
  for (let i = 0; i < arg_count; ++i) {
    let operand_index = i + 3;
    let operand = TREE_OPERAND(isn, operand_index);
    if (TREE_CODE(operand) == ADDR_EXPR) {
      yield TREE_OPERAND(operand, 0);
    }
  }
};

/** Iterate over variables used by an expression node. */
function expr_uses(expr) {
  if (DECL_P(expr)) {
    yield expr;
  } else if (CONSTANT_CLASS_P(expr)) {
    // no uses
  } else if (BINARY_CLASS_P(expr) || COMPARISON_CLASS_P(expr)) {
    yield TREE_OPERAND(expr, 0);
    yield TREE_OPERAND(expr, 1);
  } else if (UNARY_CLASS_P(expr)) {
    yield TREE_OPERAND(expr, 0);
  } else {
    switch (TREE_CODE(expr)) {
    case TRUTH_NOT_EXPR:
    case VIEW_CONVERT_EXPR:
      yield TREE_OPERAND(expr, 0);
      break;
    case TRUTH_XOR_EXPR:
    case TRUTH_AND_EXPR:
    case TRUTH_OR_EXPR:
      yield TREE_OPERAND(expr, 0);
      yield TREE_OPERAND(expr, 1);
      break;
    case INDIRECT_REF:
      yield expr;
      yield TREE_OPERAND(expr, 0);
      break;
    case ADDR_EXPR:
      // The value is not really being used here, but we'll pretend it is,
      // as having the address taken means it could be later on.
      yield TREE_OPERAND(expr, 0);
      break;
    case COMPONENT_REF:
      yield TREE_OPERAND(expr, 0);
      break;
    case ARRAY_REF:
      yield TREE_OPERAND(expr, 0);
      yield TREE_OPERAND(expr, 1);
      break;
    case BIT_FIELD_REF:
      yield TREE_OPERAND(expr, 0);
      break;
    case CALL_EXPR:
      for (let v in call_expr_uses(expr)) yield v;
      break;
    case EXC_PTR_EXPR:
    case FILTER_EXPR:
      // Ignore exception handling for now
      break;
    case CONSTRUCTOR:
      // From what we've seen so far, in C++ this appears w/o args
      break;
    default:
      throw new Error("expr_uses ni " + TREE_CODE(expr).name);
    }
  }
}

/** Iterate over variables used by a CALL_EXPR. */
var call_expr_uses = call_expr_arg_iterator;

/* General analysis functions */

/** Return an array of the basic blocks of the cfg in postorder. */
function postorder(cfg) {
  let order = new Array();
  do_postorder(cfg.x_entry_block_ptr, order);
  for each (let bb in order) delete bb._visited;
  return order;
}

/** Helper for postorder. */
function do_postorder(bb, order) {
  if (bb._visited) return;
  bb._visited = true;
  for (let bb_succ in bb_succs(bb)) {
    do_postorder(bb_succ, order);
  }
  order.push(bb);
}

/** Backward data flow analysis framework. To use, customize the abstract
 *  states, flow functions, and merge functions.
 *  @constructor 
 *  @param cfg   CFG to analyze.
 *  @param trace Level of debug tracing. 0 is no tracing. 1 is basic state
 *               tracing. 2 and 3 are increasingly verbose. */
function BackwardAnalysis(cfg, trace) {
  this.cfg = cfg;
  this.trace = trace;
}

/** Run the analysis: initialize states and iterate to a fixed point. */
BackwardAnalysis.prototype.run = function() {
  if (this.trace) print("analysis starting");

  // Initialize traversal order
  let order = postorder(this.cfg);
  // Start with everything ready so each block gets analyzed at least once.
  for each (let bb in order) bb.ready = true;
  let readyCount = order.length;
  let next = 0;

  // Fixed point computation
  this.initStates(order, order[next]);
  while (readyCount) {
    // Get next node needing to be processed
    let index = next;
    let bb = order[index];
    while (!bb.ready) {
      if (++index == order.length) index = 0;
      bb = order[index];
    }
    bb.ready = false;
    readyCount -= 1;
    if (this.trace) print("update bb: " + bb_label(this.cfg, bb));

    // Process bb
    this.mergeStateOut(bb);
    if (this.trace) print("  out state: " + this.stateLabel(bb.stateOut))
    let changed = this.flowStateIn(bb);
    if (this.trace) print("   in state: " + this.stateLabel(bb.stateIn) + ' ' +
                          (changed ? '(changed)' : '(same)'));
    if (changed) {
      for (let bb_pred in bb_preds(bb)) {
        if (!bb_pred.ready) {
          bb_pred.ready = true;
          readyCount += 1;
        }
      }
    }
  }
};

/** Initialize abstract states for all BBs. */
BackwardAnalysis.prototype.initStates = function(bbs, exit_bb) {
  // Apparently some bbs don't get in the list sometimes, so let's
  // be sure to do them all.
  for (let bb in cfg_bb_iterator(this.cfg)) {
  //for each (let bb in bbs) {
    bb.stateIn = this.top();
    bb.stateOut = this.top();
  }
};

/** Set the stateOut to be the merge of all the succ stateIn values. */
BackwardAnalysis.prototype.mergeStateOut = function(bb) {
  let state = this.top();
  for each (let bb_succ in bb_succs(bb)) {
    this.merge(state, bb_succ.stateIn);
  }
  bb.stateOut = state;
};

/** Run the flow functions for a basic block, updating stateIn from
  * stateOut. */
BackwardAnalysis.prototype.flowStateIn = function(bb) {
  let state = this.copyState(bb.stateOut);
  for (let isn in bb_isn_iterator_reverse(bb)) {
    if (this.trace) print("    " + isn_display(isn));
    this.flowState(isn, state);
    if (this.trace) print("     | " + this.stateLabel(state));
  }
  let ans = !equals(bb.stateIn, state);
  bb.stateIn = state;
  return ans;
}

// Functions to be customized per analysis -- defaults are reasonable
// except flowState, which must be implemented.

/** Customization function. Return the 'top' value to be used in initializing
 *  states. */
BackwardAnalysis.prototype.top = function() {
  return create_decl_set();
};

/** Customization function. Merge state s2 into s1. That is, do
 *    s1 := s1 \merge s2. */
BackwardAnalysis.prototype.merge = function(s1, s2) {
  return s1.unionWith(s2);
};

/** Customization function. Copy a state. The copy must be 'deep enough'
 *  that modifications to the copy will not affect the original argument. */
BackwardAnalysis.prototype.copyState = function(s) {
  return s.copy();
};

/** Customization function. Return a string representation of the state
 *  suitable for tracing. */
BackwardAnalysis.prototype.stateLabel = function(s) {
  return s.toString();
};

/** Customization function. Execute the flow function. I.e., update state
 *  corresponding to abstractly interpreting the given instruction. */
BackwardAnalysis.prototype.flowState = function(isn, state) {
  throw new Error("abstract method");
};

/** Write a DOT representation of the cfg to the given file. */
function write_dot_graph(cfg, name, filename) {
  let content = 'digraph ' + name + ' {\n';
  content += '\tnode [shape="rect", fontname="Helvetica"];\n'
  for (let bb in cfg_bb_iterator(cfg)) {
    //content += '\t' + bb.index + '[label="' + bb_label(cfg, bb) + '"];\n';
    let items = [
      bb_label(bb),
      [ isn_display(isn) for (isn in bb_isn_iterator(bb)) ].join('\\n'),
      'live: ' + [ expr_display(v) for (v in bb.stateOut.items()) ].join(', ')
    ];
    let label = '{' + items.join('|') + '}';
    // Dot requires escaping with record shapes.
    label = label.replace('>', '\\>')
    label = label.replace('<', '\\<')
    content += '\t' + bb.index + '[shape=record,label="' + label + '"];\n'
    for (let bb_succ in bb_succs(bb)) {
      content += '\t' + bb.index + ' -> ' + bb_succ.index + ';\n'
    }
  }
  content += '}\n';
  write_file(filename, content);
}
