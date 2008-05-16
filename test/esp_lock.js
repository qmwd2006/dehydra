require({ version: '1.8' });
require({ after_gcc_pass: 'cfg' });

include('treehydra.js');

include('util.js');
include('gcc_util.js');
include('gcc_print.js');
include('unstable/adts.js');
include('unstable/analysis.js');
include('unstable/esp.js');
include('unstable/liveness.js');

// Names of functions to check lock/unlock protocol on
CREATE_FUNCTION = 'create_mutex';
LOCK_FUNCTION = 'lock';
UNLOCK_FUNCTION = 'unlock';

MapFactory.use_injective = true;

// Print a trace for each function analyzed
let TRACE_FUNCTIONS = 1;
// Trace operation of the ESP analysis, use 2 or 3 for more detail
let TRACE_ESP = 2;
// Print time-taken stats 
let TRACE_PERF = 0;

// Filter functions to process per CLI
let func_filter;
if (this.arg == undefined || this.arg == '') {
  func_filter = function(fd) true;
} else {
  func_filter = function(fd) function_decl_name(fd) == this.arg;
}

function process_tree(fndecl) {
  if (!func_filter(fndecl)) return;

  // At this point we have a function we want to analyze
  if (TRACE_FUNCTIONS) {
    print('* function ' + decl_name(fndecl));
    print('    ' + loc_string(location_of(fndecl)));
  }
  if (TRACE_PERF) timer_start(fstring);

  let cfg = function_decl_cfg(fndecl);

  // We do live variable analysis so that we can throw away 
  // dead variables and make ESP faster.
  {
    let trace = 0;
    let b = new LivenessAnalysis(cfg, trace);
    b.run();
    for (let bb in cfg_bb_iterator(cfg)) {
      bb.keepVars = bb.stateIn;
    }
  }

  {
    let trace = TRACE_ESP;
    // Link switches so ESP knows switch conditions.
    let fts = link_switches(cfg);
    let a = new LockCheck(cfg, fts, trace);
    a.run();
  }
  
  if (TRACE_PERF) timer_stop(fstring);
}

// Abstract values. ESP abstract values should have a 'ch' field,
// which is a single-character label used in traces.
function AbstractValue(name, ch) {
  this.name = name;
  this.ch = ch;
}

AbstractValue.prototype.equals = function(v) {
  return this === v;
}

AbstractValue.prototype.toString = function() {
  return this.name + ' (' + this.ch + ')';
}

// Our actual abstract values. We have these specific values, plus
// any number of additional values for integer constants as needed.
// The integers are used to figure out GCC control flow.
let avspec = [
  // General-purpose abstract values
  [ 'BOTTOM',        '.' ],   // any value, also used for dead vars

  // Abstract values for lock status
  [ 'LOCKED',          'L' ],
  [ 'UNLOCKED',        'U' ],
  
  // Abstract values for booleans (control flags)
  [ 'ZERO',          '0' ],   // zero value
  [ 'NONZERO',       '1' ]    // nonzero value
];

let av = {};
for each (let [name, ch] in avspec) {
  av[name] = new AbstractValue(name, ch);
}

// Negations are needed to understand the conditions on else branches.
av.ZERO.negation = av.NONZERO;
av.NONZERO.negation = av.ZERO;

let cachedAVs = {};

// Abstract values for int constants. We use these to figure out feasible
// paths in the presence of GCC finally_tmp-controlled switches.
function makeIntAV(v) {
  let key = 'int_' + v;
  if (cachedAVs.hasOwnProperty(key)) return cachedAVs[key];

  let s = "" + v;
  let ans = cachedAVs[key] = new AbstractValue(s, s);
  ans.int_val = v;
  return ans;
}

/** Return the integer value if this is an integer av, otherwise undefined. */
av.intVal = function(v) {
  if (v.hasOwnProperty('int_val'))
    return v.int_val;
  return undefined;
}

/** Meet function for our abstract values. */
av.meet = function(v1, v2) {
  // Important for following cases -- as output, undefined means top here.
  if (v1 == undefined) v1 = av.BOTTOM;
  if (v2 == undefined) v2 = av.BOTTOM;

  // These cases apply for any lattice.
  if (v1 == av.BOTTOM) return v2;
  if (v2 == av.BOTTOM) return v1;
  if (v1 == v2) return v1;

  // At this point we know v1 != v2.
  switch (v1) {
  case av.LOCKED:
  case av.UNLOCKED:
    return undefined;
  case av.ZERO:
    return av.intVal(v2) == 0 ? v2 : undefined;
  case av.NONZERO:
    return av.intVal(v2) != 0 ? v2 : undefined;
  default:
    let iv = av.intVal(v1);
    if (iv == 0) return v2 == av.ZERO ? v1 : undefined;
    if (iv != undefined) return v2 == av.NONZERO ? v1 : undefined;
    return undefined;
  }
}     

function LockCheck(cfg, finally_tmps, trace) {
  // Property variable list. ESP will track full tuples of abstract
  // states for these variables.
  this.psvar_list = [];

  // Scan for mutex vars and put them in the list. This isn't particularly
  // complete, it just works for the example.
  let found = create_decl_set(); // ones we already found
  for (let bb in cfg_bb_iterator(cfg)) {
    for (let isn in bb_isn_iterator(bb)) {
      if (TREE_CODE(isn) == CALL_EXPR) {
        let fname = call_function_name(isn);
        if (fname == CREATE_FUNCTION) {
          let arg = call_arg(isn, 0);
          if (DECL_P(arg)) {
            if (!found.has(arg)) {
              found.add(arg);
              this.psvar_list.push(arg);
            }
          }
        }
      }
    }
  }

  // Add mutex vars to the keep list so we always keep them
  for (let bb in cfg_bb_iterator(cfg)) {
    for each (let v in this.psvar_list) {
      bb.keepVars.add(v);
    }
  }

  // Add finally temps so we can decode GCC destructor usages.
  for each (let v in finally_tmps) {
    this.psvar_list.push(v);
  }

  if (trace) {
    print("PS vars");
    for each (let v in this.psvar_list) {
      print("    " + expr_display(v));
    }
  }

  ESP.Analysis.call(this, cfg, this.psvar_list, av.BOTTOM, av.meet, trace);
}

LockCheck.prototype = new ESP.Analysis;

// Return start state to use in entry block of CFG. This must assign
// a value to all property vars.
LockCheck.prototype.startValues = function() {
  let ans = create_decl_map();
  for each (let p in this.psvar_list) {
    ans.put(p, av.BOTTOM);
  }
  return ans;
}

// Edge state update. We'll drop dead variables that aren't property
// variables, although this is optional.
LockCheck.prototype.updateEdgeState = function(e) {
  e.state.keepOnly(e.dest.keepVars);
}

// State transition function. Mostly, we delegate everything to
// another function as either an assignment or a call.
LockCheck.prototype.flowState = function(isn, state) {
  switch (TREE_CODE(isn)) {
  case GIMPLE_MODIFY_STMT:
    this.processAssign(isn, state);
    break;
  case CALL_EXPR:
    this.processCall(undefined, isn, isn, state);
    break;
  case SWITCH_EXPR:
  case COND_EXPR:
    // This gets handled by flowStateCond instead, has no exec effect
    break;
  case RETURN_EXPR:
    let op = isn.operands()[0];
    if (op) this.processAssign(isn.operands()[0], state);
    break;
  case LABEL_EXPR:
  case RESX_EXPR:
  case ASM_EXPR:
    // NOPs for us
    break;
  default:
    print(TREE_CODE(isn));
    throw new Error("ni");
  }
}

// State transition function to apply branch filters. This is kind
// of boilerplate--we're just handling some stuff that GCC generates.
LockCheck.prototype.flowStateCond = function(isn, truth, state) {
  switch (TREE_CODE(isn)) {
  case COND_EXPR:
    this.flowStateIf(isn, truth, state);
    break;
  case SWITCH_EXPR:
    this.flowStateSwitch(isn, truth, state);
    break;
  default:
    throw new Error("ni " + TREE_CODE(isn));
  }
}

// Apply filter for an if statement. This handles only tests for
// things being zero or nonzero.
LockCheck.prototype.flowStateIf = function(isn, truth, state) {
  let exp = TREE_OPERAND(isn, 0);

  if (DECL_P(exp)) {
    this.filter(state, exp, av.NONZERO, truth, isn);
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
    let val = TREE_CODE(exp) == EQ_EXPR ? av.ZERO : av.NONZERO;

    this.filter(state, op1, val, truth, isn);
    break;
  default:
    // Don't care about anything else.
  }

};

// State transition for switch cases.
LockCheck.prototype.flowStateSwitch = function(isn, truth, state) {
  let exp = TREE_OPERAND(isn, 0);

  if (DECL_P(exp)) {
    if (truth != null) {
      this.filter(state, exp, makeIntAV(truth), true, isn);
    }
    return;
  }
  throw new Error("ni");
}

// Apply a filter to the state. We need to special case it for this analysis
// because outparams only care about being a NULL pointer.
LockCheck.prototype.filter = function(state, vbl, val, truth, blame) {
  if (truth != true && truth != false) throw new Error("ni " + truth);
  if (truth == false) {
    val = val.negation;
  }
  state.filter(vbl, val, blame);
};

// State transition for assignments. We'll just handle constants and copies.
// For everything else, the result is unknown (i.e., BOTTOM).
LockCheck.prototype.processAssign = function(isn, state) {
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
      if (is_finally_tmp(lhs)) {
        // Need to know the exact int value for finally_tmp.
        let v = TREE_INT_CST_LOW(rhs);
        state.assignValue(lhs, makeIntAV(v), isn);
      } else {
        // Otherwise, just track zero/nonzero.
        let value = expr_literal_int(rhs) == 0 ? av.ZERO : av.NONZERO;
        state.assignValue(lhs, value, isn);
      }
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
        this.processCall(lhs, rhs, isn, state);
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
    case TRUTH_NOT_EXPR:
    case TRUTH_XOR_EXPR:
    case BIT_FIELD_REF:
      state.remove(lhs);
      break;
    default:
      if (UNARY_CLASS_P(rhs) || BINARY_CLASS_P(rhs) || COMPARISON_CLASS_P(rhs)) {
        state.remove(lhs);
        break;
      }
      print(TREE_CODE(rhs));
      throw new Error("ni");
    }
    return;
  }

  switch (TREE_CODE(lhs)) {
  case INDIRECT_REF: // unsound
  case COMPONENT_REF: // unsound
  case ARRAY_REF: // unsound
  case EXC_PTR_EXPR:
  case FILTER_EXPR:
    break;
  default:
    print(TREE_CODE(lhs));
    throw new Error("ni");
  }
}

LockCheck.prototype.processCall = function(dest, expr, blame, state) {
  let fname = call_function_name(expr);
  if (fname == LOCK_FUNCTION) {
    this.processLock(expr, av.LOCKED, av.UNLOCKED, blame, state);
  } else if (fname == UNLOCK_FUNCTION) {
    this.processLock(expr, av.UNLOCKED, av.LOCKED, blame, state);
  } else if (fname == CREATE_FUNCTION) {
    this.processLock(expr, av.UNLOCKED, undefined, blame, state);
  } else {
    if (dest != undefined && DECL_P(dest)) {
      state.assignValue(dest, av.BOTTOM, blame);
    }
  }
};

// State transition for a lock API.
LockCheck.prototype.processLock = function(call, val, precond, blame, state) {
  let arg = call_arg(call, 0);
  if (DECL_P(arg)) {
    if (precond != undefined) 
      this.checkPrecondition(arg, precond, blame, state);
    state.assignValue(arg, val, blame);
  } else {
    warning("applying lock to a nonvariable so we don't check it",
            location_of(blame));
  }
};


// Check that a precondition is held.
LockCheck.prototype.checkPrecondition = function(vbl, precond, blame, state) {
  for (let substate in state.substates.getValues()) {
    this.checkSubstate(vbl, precond, blame, substate);
  }
}

LockCheck.prototype.checkSubstate = function(vbl, precond, blame, ss) {
  let val = ss.get(vbl);
  if (val != precond) {
    warning("precondition not met: expected " + precond + ", got " +
            val, location_of(blame));
  }
}

