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

var Zero_NonZero = {}
include('unstable/zero_nonzero.js', Zero_NonZero);

// Names of functions to check lock/unlock protocol on
CREATE_FUNCTION = 'create_mutex';
LOCK_FUNCTION = 'lock';
UNLOCK_FUNCTION = 'unlock';

MapFactory.use_injective = true;

// Print a trace for each function analyzed
let TRACE_FUNCTIONS = 0;
// Trace operation of the ESP analysis, use 2 or 3 for more detail
let TRACE_ESP = 0;
// Print time-taken stats 
let TRACE_PERF = 0;

function process_tree(fndecl) {
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
}

AbstractValue.prototype.toString = function() {
  return this.name + ' (' + this.name[0] + ')';
}

AbstractValue.prototype.toShortString = function() {
  return this.name[0]
}

// Our actual abstract values.
// Abstract values for lock status 
let av = {};
for each (let name in ['LOCKED', 'UNLOCKED']) {
  av[name] = new AbstractValue(name);
}

/** Meet function for our abstract values. */
av.meet = function(v1, v2) {
  // At this point we know v1 != v2.
  let values = [v1,v2]
  if (values.indexOf(av.LOCKED) != -1
      || values.indexOf(av.UNLOCKED) != -1)
    return ESP.NOT_REACHED;

  return Zero_NonZero.meet(v1, v2)
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

  this.zeroNonzero = new Zero_NonZero.Zero_NonZero()
  ESP.Analysis.call(this, cfg, this.psvar_list, av.meet, trace);
}

LockCheck.prototype = new ESP.Analysis;

// Return start state to use in entry block of CFG. This must assign
// a value to all property vars.
LockCheck.prototype.startValues = function() {
  let ans = create_decl_map();
  for each (let p in this.psvar_list) {
    ans.put(p, ESP.TOP);
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
  case SWITCH_EXPR:
  case COND_EXPR:
    // This gets handled by flowStateCond instead, has no exec effect
    break;
  case CALL_EXPR:
    if (this.processCall(isn, isn, state))
      break;
  default:
    this.zeroNonzero.flowState(isn, state)
  }
}

// State transition function to apply branch filters. This is kind
// of boilerplate--we're just handling some stuff that GCC generates.
LockCheck.prototype.flowStateCond = function(isn, truth, state) {
  this.zeroNonzero.flowStateCond (isn, truth, state)
}

LockCheck.prototype.processCall = function(expr, blame, state) {
  let fname = call_function_name(expr);
  if (fname == LOCK_FUNCTION) {
    this.processLock(expr, av.LOCKED, av.UNLOCKED, blame, state);
  } else if (fname == UNLOCK_FUNCTION) {
    this.processLock(expr, av.UNLOCKED, av.LOCKED, blame, state);
  } else if (fname == CREATE_FUNCTION) {
    this.processLock(expr, av.UNLOCKED, undefined, blame, state);
  } else {
    return false
  }
  return true
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
    error("precondition not met: expected " + precond + ", got " +
            val, location_of(blame));
  }
}
