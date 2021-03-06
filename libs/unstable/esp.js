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

include('unstable/liveness.js');
/* ESP Program Analysis Library */

/** Namespace */
let ESP = function() {

  /** Map/Set label for ESP states. */
  let ps_get_label = function(pskey) {
    return pskey;
  };

  /** Map/Set key for ESP states. */
  let ps_get_key = function(pskey) {
    return pskey;
  };

  /** Full ESP state. This is a collection of substates, representing
   *  their union. */
  let State = function(factory, substate) {
    // Factory must provide:
    //   .psvbls     array of property state vars
    //   .psvblset   set of property state vars
    if (factory == undefined) throw new Error("undefined factory");
    this.factory = factory;
    // Property State -> Substate
    this.substates = MapFactory.create_map(function (x, y) x == y, function (x) x,
                                           ps_get_key, ps_get_label);
    if (substate != undefined) {
      this.add(substate);
    }
  };

  State.prototype.copy = function() {
    let ans = new State(this.factory);
    // We need to copy the substate objects as well to avoid sharing.
    for (let [k, v] in this.substates.getItems()) {
      ans.substates.put(k, v.copy());
    }
    return ans;
  };

  State.prototype.equals = function(ss) {
    return this.substates.equals(ss.substates);
  }

  State.prototype.list = function() {
    if (this.substates.isEmpty()) {
      print("      | Not Reached");
    } else {
      for (let [ps, ss] in this.substates.getItems()) {
        print("      | " + ss);
      }
    }
  };

  /** Add one substate to the current state. */
  State.prototype.add = function(ss) {
    let pskey = ss.pskey();
    let cur = this.substates.get(pskey);
    if (cur) {
      cur.mergeWith(ss);
    } else {
      // TODO This copy *may* not be needed
      this.substates.put(pskey, ss.copy());
    }
  };

  /** Apply an update function to the state.
   *
   *  This is the primary method used to transform ESP states. Several
   *  other methods wrap this one. The idea is that it's easy to write
   *  a transformation function that transforms a substate (i.e., a 
   *  simple mapping of vbls to abstract values). update() will apply
   *  one of those tranforms to the entire state. It is essentially a
   *  map-reduce operation that transforms each substate, then collects
   *  them into a new ESP state.
   *
   *  The transformer function gets one argument, an ESP substate. The
   *  return value is an array of states, so the transformer can not
   *  only change a value, it can also drop or split substates. 
   *
   *  The transformer is allowed to update a state in place, but if it
   *  returns more than one state, it must make copies of all but one. */
  State.prototype.update = function(func) {
    if (this.substates.isEmpty()) return;
    let old_substates = this.substates;
    this.substates = this.substates.create()
    for (let substate in old_substates.getValues()) {
      for each (let ns in func(substate)) {
        this.add(ns);
      }
    }
  };
  
  State.prototype.yieldPreconditions = function (variable) {
    for each (let substate in this.substates.getValues()) {
      ret = substate.get(variable)
      if (ret)
        yield [ret, substate.getBlame(variable)]
    }
  };
  
  function meet (v1, v2, next_meet) {
    if (v1 === v2) return v1
    if (v1 === ESP.TOP) return v2
    if (v2 === ESP.TOP) return v1
    return next_meet(v1, v2)
  }

  function join(v1, v2, next_join) {
    if (v1 === v2) return v1;
    if (v1 === ESP.TOP) return v1;
    if (v2 === ESP.TOP) return v2;
    return next_join(v1, v2);
  }

  /** Return an abstract value, the union of all possible values of vbl
   * here. */
  State.prototype.get = function (vbl) {
    let first = true;
    let v;
    for (let ss in this.substates.getValues()) {
      // We try to avoid exposing users directly to any NOT_REACHED values
      // so we avoid calling join on the first element. 
      if (first) {
        first = false;
        v = ss.get(vbl);
      } else {
        v = join(v, ss.get(vbl), this.factory.join);
      }
    }
    // Here we don't have a choice: the empty set would have no value.
    return first ? ESP.NOT_REACHED : v;
  }

  /** Apply a filter to the state, which keeps substates only where
   * vbl == val. More formally, set the state to be the conjunction
   * of itself and the given condition vbl == val. */
  State.prototype.filter = function(vbl, val, blame) {
    let factory = this.factory;
    this.update(function(ss) {
      let cur_val = ss.get(vbl);
      let new_val = meet(cur_val, val, factory.meet);
      if (new_val == ESP.NOT_REACHED) return [];
      if (new_val == cur_val) return [ ss ];
      
      ss.assignValue(vbl, val, blame);
      return [ss];
    });
  };

  /** Update the state by assigning the given abstract value to
   * the given variable. */
  State.prototype.assignValue = function(vbl, value, blame) {
    if (value == ESP.TOP) {
      this.remove(vbl, blame);
    } else {
      // TODO use lattice?
      this.update(function(ss) {
        ss.assignValue(vbl, value, blame);
        return [ss];
      });
    }
  };

  /** Update the state to reflect the assignment lhs := rhs, where
   *  both are variables. */
  State.prototype.assign = function(lhs, rhs, blame) {
    this.assignMapped(lhs, function (ss) ss.get(rhs), blame);
  };

  /** Update the state to reflect that we don't know the value. */
  State.prototype.remove = function(vbl, blame) {
    this.assignMapped(vbl, function(ss) undefined, blame);
  }

  /** lhs := func(substate) in all states, where func is a JS fn. */
  State.prototype.assignMapped = function(lhs, func, blame) {
    let factory = this.factory;
    this.update(function (ss) {
      let rhs = func(ss);
      if (rhs == undefined || rhs == factory.TOP) {
        if (factory.psvblset.has(lhs)) {
          let ans = [];
          for each (let v in factory.split(lhs, factory.TOP)) {
            let s = ss.copy();
            s.assignValue(lhs, v, blame);
            ans.push(s);
          }
          return ans;
        } else {
          ss.remove(lhs);
          return [ss];
        }
      }
      ss.assignValue(lhs, rhs, blame);
      return [ss];
    });
  };

  /** Set the abstract value of all variables not in the given set
   *  to TOP. This effectively demotes these variables to execution
   *  variables, potentially decreasing the size of the state. */
  State.prototype.keepOnly = function(var_set) {
    let factory = this.factory;
    this.update(function(ss) {
      let keys = [];
      for (let v in ss.map.getKeys()) {
        keys.push(v);
      }
      for each (let v in keys) {
        if (ss.get(v) != factory.TOP && !var_set.has(v)) {
          ss.remove(v);
        }
      }
      return [ss];
    });
  }

  /** Merge the given state into this one. */
  State.prototype.mergeWith = function(state) {
    for (let ss in state.substates.getValues()) {
      this.add(ss);
    }
  };

  /** ESP substate. This is a mapping of variables to abstract values. 
   *  The substate also tracks a 'blame' for each variable, which is
   *  a statement that caused it to have its current value. */
  let Substate = function(factory, map, blames) {
    this.factory = factory;
    this.map = map;
    this.blames = blames;
  };

  Substate.prototype.copy = function() {
    return new Substate(this.factory, this.map.copy(), this.blames.copy());
  };

  Substate.prototype.equals = function(ss) {
    return this.map.equals(ss.map);
  };

  function toShortString(latticeValue) {
    if (latticeValue === ESP.TOP)
      return "T"
    if (latticeValue.toShortString)
      return latticeValue.toShortString()

    return latticeValue.toString()
  }
  /** Return a string value that uniquely represents the property state
   * of this substate. */
  Substate.prototype.pskey = function() {
    return Array.join([ toShortString(this.map.get(vbl)) 
                        for each (vbl in this.factory.psvbls) ], '');
  };

  Substate.prototype.toString = function() {
    return this.pskey() + ' ' + 
      Array.join([ expr_display(vbl) + ':' + toShortString(this.map.get(vbl))
                   for (vbl in this.map.getKeys()) ], ', ');
  };

  /** Return the abstract value of the given variable. */
  Substate.prototype.get = function(vbl) {
    return this.map.get(vbl);
  }

  /** Return the instruction to blame for this variable having the value
   *  it currently has. */
  Substate.prototype.getBlame = function(vbl) {
    return this.blames.get(vbl);
  }

  // Substate mutators must only be called through State update functions.

  /** Not for public use. See source code. */
  Substate.prototype.assignValue = function(vbl, value, blame) {
    this.map.put(vbl, value);
    this.blames.put(vbl, blame);
  };

  /** Not for public use. See source code. */
  Substate.prototype.remove = function(vbl, value, blame) {
    if (this.factory.psvblset.has(vbl)) {
      this.map.put(vbl, this.factory.TOP);
      this.blames.put(vbl, blame);
    } else {
      this.map.remove(vbl);
    }
  };

  Substate.prototype.getVariables = function () {
    return this.map.getKeys();
  }

  /** Not for public use. See source code. */
  Substate.prototype.mergeWith = function(ss) {
    // We have to be very careful -- we keep a value only if present
    // on both sides and equal.
    let new_map = this.map.create();
    for (let [k,v] in this.map.getItems()) {
      if (this.factory.psvblset.has(k)) {
        if (v != ss.map.get(k))
          throw new Error("assert");
      }
      if (v == ss.map.get(k)) {
        new_map.put(k, v);
      }
    }
    this.map = new_map;
  };

  /** ESP program analysis solver framework.
   *   Based on:
   *   ESP: path-sensitive program verification in polynomial time.
   *   Manuvir Das, Sorin Lerner, and Mark Seigle.
   *   http://portal.acm.org/citation.cfm?id=512538 
   *
   *   @param cfg     CFG to analyze
   *   @param psvbls  List of PropVarSpec objects describing state variables
   *   @param bottom  Abstract value representing TOP state
   *                  for a variable, i.e., any value
   *   @param trace   Debug tracing level: 0, 1, 2, or 3.
   */
  let Analysis = function(cfg, psvbls, meet, trace) {
    // if used as prototype don't do anything
    if (!cfg) return
    if (meet == undefined) {
      meet = this.default_meet;
      // Can't issue a warning here because instances used as prototypes
      // would print warnings
    }

    this.cfg = cfg;
    this.trace = trace;
    this.meet = meet;
    this.psvbls = [];
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

    // Link switches so ESP knows switch conditions.
    for each (let finally_tmp in link_switches(cfg)) {
      // Add finally temps so we can decode GCC destructor usages.
      this.psvbls.push(finally_tmp)  
    }
    
    this.startValues = create_decl_map();
    for each (let v in psvbls) {
      this.psvbls.push(v.vbl);
      this.startValues.put(v.vbl, v.initial_val)
      if (!v.keep) continue
      for (let bb in cfg_bb_iterator(cfg)) {
        bb.keepVars.add(v.vbl);
      }
    }
    this.psvblset = create_decl_set(this.psvbls);
    
    // Resource limits -- Set these properties directly to control halting
    this.time_limit = undefined; // wall clock time in ms
    this.pass_limit = undefined; // number of passes per BB
    if (trace) {
      print("PS vars");
      for each (let v in this.psvbls) {
        print("    " + expr_display(v));
      }
    }
  };

/** Run the solver to a fixed point. */
Analysis.prototype.run = function() {
  let t0 = new Date();

  if (this.trace) print("analysis starting");

  if (this.meet == this.default_meet) {
    warning("ESP solver warning: using unsound default meet() function.\n" +
            "Some code paths may be wrongly excluded from analysis.");
  }
  
  // Make us valid as a factory for States, while still allowing us
  // to be a prototype in the usual idiom.
  if (this.psvbls == undefined) throw new Error("Invalid psvbls");

  // Initialize traversal order
  let order = postorder(this.cfg);
  order.reverse();
  // Start with everything ready so each block gets analyzed at least once.
  for each (let bb in order) {
    bb.ready = true;
    bb.pass_count = 0; // number of passes made over this BB
  }
  let readyCount = order.length;
  let next = 0;

  // Fixed point computation
  this.initStates(order, order[next]);
  this.cfg.x_entry_block_ptr.stateIn = 
    new State(this, new Substate(this, this.startValues, create_decl_map()));;
  if (this.trace) {
    print("initial state:");
    this.cfg.x_entry_block_ptr.stateIn.list();
  }

  while (readyCount) {
    let dt = new Date() - t0;
    if (dt > this.time_limit) 
      throw new Error("ESP halting: time limit exceeded (" + 
                      this.time_limit + " ms)");

    // Get next node needing to be processed
    let index = next;
    let bb = order[index];
    while (!bb.ready) {
      if (++index == order.length) index = 0;
      bb = order[index];
    }
    bb.ready = false;
    readyCount -= 1;
    if (this.trace) {
      let loc
      for each (let isn in bb_isn_iterator(bb)) {
        loc = location_of(isn)
        if (loc)
          break;
      }
      print("update bb: " + bb_label(this.cfg, bb), loc);
    }

    // Process bb
    if (bb != this.cfg.x_entry_block_ptr) {
      if (!this.mergeStateIn(bb) && bb.pass_count > 0) {
        if (this.trace) print("  in state unchanged");
        continue;
      }
    }

    ++bb.pass_count;
    if (bb.pass_count > this.pass_limit)
      throw new Error("ESP halting: BB pass limit exceeded (" +
                      bb_label(bb) + " on pass " + bb.pass_count + ")");

    if (this.trace) {
      print("  in state:");
      bb.stateIn.list();
    }
    let changed = this.flowStateOut(bb);
    if (this.trace) {
      print("  out state:" + (changed ? " (changed)" : " (same)"));
      bb.stateOut.list();
    }
    if (changed) {
      // Update states on outedges, applying facts from conditional
      // branches if applicable
      for each (let e in bb_succ_edges(bb)) {
        let bb_succ = e.dest;
        if (this.trace >= 2) print("  succ " + bb_label(bb_succ));
        if (e.flags & EDGE_TRUE_VALUE || e.flags & EDGE_FALSE_VALUE) {
          // Outgoing edges from an if
          let truth = (e.flags & EDGE_TRUE_VALUE) != 0;
          let edge_state = bb.stateOut.copy();
          let cond = bb_isn_last(bb);
          if (this.trace) {
            print("  branch " + truth + ": " + gs_display(cond));
          }
          this.flowStateCond(cond, truth, edge_state);
          e.state = edge_state;
          if (this.trace) {
            print ("  edge state:");
            e.state.list();
          }
        } else if (e.hasOwnProperty('case_val')) {
          // Outgoing edges from a switch
          let truth = e.case_val;
          let edge_state = bb.stateOut.copy();
          let cond = bb_isn_last(bb);
          if (this.trace) {
            print("  branch " + truth + ": " + gs_display(cond));
          }
          this.flowStateCond(cond, truth, edge_state);
          e.state = edge_state;
          if (this.trace) {
            print ("  edge state:");
            e.state.list();
          }
        } else {
          // Need to take a copy here as well because we're going to
          // keep live vars only, which can vary on succs.
          e.state = bb.stateOut.copy();
        }
        this.updateEdgeState(e);
        if (!bb_succ.ready) {
          bb_succ.ready = true;
          readyCount += 1;
        }
      }
    }
  }
};

/** Initialize abstract states for all BBs and edges. */
Analysis.prototype.initStates = function(bbs, exit_bb) {
  // Apparently some bbs don't get in the list sometimes, so let's
  // be sure to do them all.
  for (let bb in cfg_bb_iterator(this.cfg)) {
    bb.stateIn = this.top();
    bb.stateOut = this.top();
    for each (let e in bb_succ_edges(bb)) {
      e.state = this.top();
    }
  }
};

/** Set the stateIn to be the merge of all the pred stateOut values. 
 *  Return true if the result is different from the original stateIn. */
Analysis.prototype.mergeStateIn = function(bb) {
  let state = this.top();
  for each (let e in bb_pred_edges(bb)) {
    if (this.trace >= 3) {
      print("merge " + edge_string(e, this.cfg));
      e.state.list();
    }
    this.merge(state, e.state);
  }
  let ans = !bb.stateIn.equals(state);
  bb.stateIn = state;
  return ans;
};

/** Run the flow functions for a basic block, updating stateOut from
  * stateIn. */
Analysis.prototype.flowStateOut = function(bb) {
  let state = this.copyState(bb.stateIn);
  for (let isn in bb_isn_iterator(bb)) {
    if (this.trace) {
      if (this.trace >= 2) {
        print("    loc: " + location_of(isn));
      }
      print("    " + gs_display(isn));
    }
    this.flowState(isn, state);
    if (this.trace) {
      state.list();
    }
  }
  if (this.trace) {
    print("    old state:");
    bb.stateOut.list();
  }
  let ans = !bb.stateOut.equals(state);
  bb.stateOut = state;
  return ans;
}

/** Return the 'top' state representing no behavior. This should not
 *  need to be customized. */
Analysis.prototype.top = function() {
  return new State(this);
};

/** Merge state s2 into state s1. This should not need to be customized. */
Analysis.prototype.merge = function(s1, s2) {
  return s1.mergeWith(s2);
};

/** Copy a state. This should not need to be customized. */
Analysis.prototype.copyState = function(s) {
  return s.copy();
};

/** Return a string representation of the state for tracing. This
 *  should not need to be customized. */
Analysis.prototype.stateLabel = function(s) {
  return s.toString();
};

  /** Customization function. Update 'state' by abstractly interpreting
    * instruction 'isn' on the state. */
  Analysis.prototype.flowState = function(isn, state) {
    throw new Error("abstract method");
  };

  /** Customization function. Update 'state' according to following the
   *  given conditional branch.
   *  @param isn   Branch instruction: COND_EXPR or SWITCH_EXPR.
   *  @param truth Value of conditional expression of 'isn' for this branch.
   *  @param state State on arrival at branch. This function should update
   *               the state, although doing nothing is valid, just less
   *               precise.
   *  @see State.predicate */
  Analysis.prototype.flowStateCond = function(isn, truth, state) {
    throw new Error("abstract method");
  };

  /** Customization function. Update the state on the given edge. This
   *  is purely optional, override it to disable.
   *  This can be used for dropping state on variables no longer of interest.
   *  @param edge  CFG edge, with src and dest BB properties. There
   *               is also the .state property. */
  Analysis.prototype.updateEdgeState = function(edge) {
    edge.state.keepOnly(edge.dest.keepVars);
  };

  /** Default meet operator. Note that this version is unsound. If used,
   *  a warning will be issued. Return value of undefined indicates
   *  empty set state (TODO, s/b lattice member?). */
  Analysis.default_meet = function(v1, v2) {
    return ESP.NOT_REACHED;
  };

  /** Split a property state abstract value. The return value should
   *  be a list of abstract values whose join (union) is equal to the
   *  input. This is used by assignMapped to split values like TOP for
   *  later more precise handling. Generally, the right thing to do
   *  is to return the list of specific abstract values possible for
   *  the given property variable. The default implementation is sound.*/
  Analysis.prototype.split = function(vbl, v) {
    return [ v ];
  }
  
  /**
   * vbl: variable
   * keep: don't drop variables due to liveness
   * initial_val: initial lattice value..defaults to undefined which is ESP.TOP
   */
  PropVarSpec = function (vbl,keep,initial_val) {
    this.vbl = vbl
    this.keep = keep
    this.initial_val = initial_val
  }

  return {Analysis: Analysis, PropVarSpec: PropVarSpec, TOP:undefined, NOT_REACHED:{}};

}();

