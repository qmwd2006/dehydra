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

/* Base abstract interpretation for booleans and ints. This is
 * useful for a lot of basic control flow, especially GCC-generated
 * conditions and switches. */

/** Our value lattice. The lattice also includes all integer values,
 *  which are encoded directly as integers. */
const Lattice = {
  ZERO: 0, 
  NONZERO: {toString: function() {return "NONZERO"}, 
            toShortString: function() {return "!0"}}
  }

/** Negate an abstract value as a _condition_. By this, we mean return
  * the abstract value that represents all concrete values not in v. 
  * We do not mean operational negation, i.e., the effect of applying 
  * a C++ negation operator. */
function negate (v) {
  switch (v) {
  case Lattice.ZERO: return Lattice.NONZERO;
  case Lattice.NONZERO: return Lattice.ZERO;
  default:
    // It would be sound to return BOTTOM here, but this case shouldn't
    // be reached in current code.
    throw new Error("Tried to negate non-boolean abstract value " + v);
  }
}

function Zero_NonZero() {
}

Zero_NonZero.prototype.flowState = function(isn, state) {
  switch (TREE_CODE(isn)) {
  case GIMPLE_MODIFY_STMT:
    this.processAssign(isn, state);
    break;
  case RETURN_EXPR:
    let op = isn.operands()[0];
    if (op) this.processAssign(isn.operands()[0], state);
    break;
  }
}

// State transition for assignments. We'll just handle constants and copies.
// For everything else, the result is unknown (i.e., TOP).
Zero_NonZero.prototype.processAssign = function(isn, state) {
  let lhs = isn.operands()[0];
  let rhs = isn.operands()[1];

  if (DECL_P(lhs)) {
    // Unwrap NOP_EXPR, which is semantically a copy.
    if (TREE_CODE(rhs) == NOP_EXPR) {
      rhs = rhs.operands()[0];
    }

    if (DECL_P(rhs)) {
      state.assign(lhs, rhs, isn);
      return;
    }
    
    switch (TREE_CODE(rhs)) {
    case INTEGER_CST:
      // The exact value is required for handling control flow around
      // finally_tmp variables. We can use it always now that we have
      // a lattice with meet.
      let value = TREE_INT_CST_LOW(rhs);
      state.assignValue(lhs, value, isn);
      break;
    case NE_EXPR: {
      // We only care about gcc-generated x != 0 for conversions and such.
      let [op1, op2] = rhs.operands();
      if (DECL_P(op1) && expr_literal_int(op2) == 0) {
        state.assign(lhs, op1, isn);
      }
    }
      break;
    case CALL_EXPR:
      let fname = call_function_name(rhs);
      if (fname == '__builtin_expect') {
        // Same as an assign from arg 0 to lhs
        state.assign(lhs, call_args(rhs)[0], isn);
      } else {
        state.assignValue(lhs, ESP.TOP, isn);
        for (let arg in call_arg_iterator(rhs)) {
          if (arg.tree_code() == ADDR_EXPR) {
            let vbl = arg.operands()[0];
            if (DECL_P(vbl)) {
              state.assignValue(vbl, ESP.TOP, isn);
            }
          }
        }
      }
      break;

    case TRUTH_NOT_EXPR:
      let not_operand = rhs.operands()[0];
      if (DECL_P(not_operand)) {
        state.assignMapped(lhs, function(ss) {
          switch (ss.get(not_operand)) {
          case undefined:
            return undefined;
          case 0:
            return 1;
          default:
            return 0;
          }
        }, isn);
      } else {
        state.remove(lhs, isn);
      }
      break;

      // Stuff we don't analyze -- just kill the LHS info
    case EQ_EXPR: 
    case ADDR_EXPR:
    case POINTER_PLUS_EXPR:
    case ARRAY_REF:
    case COMPONENT_REF:
    case INDIRECT_REF:
    case FILTER_EXPR:
    case EXC_PTR_EXPR:
    case CONSTRUCTOR:

    case REAL_CST:
    case STRING_CST:

    case CONVERT_EXPR:
    case TRUTH_XOR_EXPR:
    case BIT_FIELD_REF:
      state.remove(lhs);
      break;
    default:
      if (UNARY_CLASS_P(rhs) || BINARY_CLASS_P(rhs) || COMPARISON_CLASS_P(rhs)) {
        state.remove(lhs);
        break;
      }
    }
  }
}

Zero_NonZero.prototype.flowStateCond = function(isn, truth, state) {
  switch (TREE_CODE(isn)) {
  case COND_EXPR:
    this.flowStateIf(isn, truth, state);
    break;
  case SWITCH_EXPR:
    this.flowStateSwitch(isn, truth, state);
    break;
  }
}

// Apply filter for an if statement. This handles only tests for
// things being zero or nonzero.
Zero_NonZero.prototype.flowStateIf = function(isn, truth, state) {
  let exp = TREE_OPERAND(isn, 0);

  if (DECL_P(exp)) {
    this.filter(state, exp, Lattice.NONZERO, truth, isn);
    return;
  }

  switch (TREE_CODE(exp)) {
  case EQ_EXPR:
  case NE_EXPR:
    // Handle 'x op <int lit>' pattern only
    let op1 = TREE_OPERAND(exp, 0);
    let op2 = TREE_OPERAND(exp, 1);
    if (expr_literal_int(op1) != undefined) {
      [op1,op2] = [op2,op1];
    }
    if (!DECL_P(op1)) break;
    if (expr_literal_int(op2) != 0) break;
    let val = TREE_CODE(exp) == EQ_EXPR ? Lattice.ZERO : Lattice.NONZERO;
    this.filter(state, op1, val, truth, isn);
    break;
  default:
    // Don't care about anything else.
  }
}

// State transition for switch cases.
Zero_NonZero.prototype.flowStateSwitch = function(isn, truth, state) {
  let exp = TREE_OPERAND(isn, 0);
  
  if (DECL_P(exp)) {
    if (truth != null) {
      this.filter(state, exp, truth, true, isn);
    }
    return;
  }
  throw new Error("ni");
}

// Apply a filter to the state. We need to special case it for this analysis
// because outparams only care about being a NULL pointer.
Zero_NonZero.prototype.filter = function(state, vbl, val, truth, blame) {
  if (truth != true && truth != false) throw new Error("ni " + truth);
  if (truth == false) {
    val = negate(val);
  }
  state.filter(vbl, val, blame);
}

/** Update an ESP state so that vbl is predicated on val(arg).
 * i.e.,  vbl := absvalue(arg) == val */
Zero_NonZero.prototype.predicate = function(state, vbl, val, arg, blame) {
  let factory = state.factory;
  state.assignMapped(vbl, function(ss) {
    let rval = ss.get(arg);
    if (rval == undefined || rval == factory.TOP) {
      return undefined;
    } else if (rval == val) {
      return Lattice.NONZERO;
    } else {
      return Lattice.ZERO;
    }
  }, blame); 
}

function meet (v1, v2) {
  // at this point possible values for v1, v2
  // are: NONZERO, some int value
  if (v1 == Lattice.NONZERO && v2 != 0) return v2
  if (v2 == Lattice.NONZERO && v1 != 0) return v1

  if ([v1, v2].indexOf(Lattice.NONZERO) != -1) return ESP.NOT_REACHED

  if (v1 == v2) return v2
  return ESP.NOT_REACHED
}

function join(v1, v2) {
  // Here we can assume that v1 != v2 and neither is TOP or NOT_REACHED.
  if (v1 == 0 || v2 == 0) return ESP.TOP;
  return Lattice.NONZERO;
}
