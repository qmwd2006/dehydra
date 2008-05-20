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

  /** Apply an update function to the state. The update function
   * maps a state to a set of states. If it returns one state, it
   * may change the state in place, but otherwise, it should return
   * separate states. */
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

  /** Apply a filter to the state, which keeps substates only where
   * vbl == val. More formally, set the state to be the conjunction
   * of itself and the given condition vbl == val. */
  State.prototype.filter = function(vbl, val, blame) {
    let factory = this.factory;
    this.update(function(ss) {
      let cur_val = ss.get(vbl);
      let new_val = factory.meet(cur_val, val);
      if (new_val == undefined) return [];
      if (new_val == cur_val) return [ ss ];
      
      ss.assignValue(vbl, val, blame);
      return [ss];
    });
  };

  /** Update the state so that vbl is predicated on val(arg).
   * i.e.,  vbl := absvalue(arg) == val */
  State.prototype.predicate = function(vbl, val, arg, blame) {
    let factory = this.factory;
    this.assignMapped(vbl, function(ss) {
      let rval = ss.get(arg);
      if (rval == undefined || rval == factory.BOTTOM) {
        return undefined;
      } else if (rval == val) {
        return av.NONZERO;
      } else {
        return av.ZERO;
      }
    }, blame); 
  }

  /** Update the state by assigning the given abstract value to
   * the given variable. */
  State.prototype.assignValue = function(vbl, value, blame) {
    // TODO use lattice?
    this.update(function(ss) {
      ss.assignValue(vbl, value, blame);
      return [ss];
    });
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
      if (rhs == undefined || rhs == factory.BOTTOM) {
        if (factory.psvblset.has(lhs)) {
          let [s1, s2] = [ss, ss.copy()];
          s1.assignValue(lhs, av.ZERO, blame);
          s2.assignValue(lhs, av.NONZERO, blame);
          return [s1, s2];
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
   *  to BOTTOM. This effectively demotes these variables to execution
   *  variables, potentially decreasing the size of the state. */
  State.prototype.keepOnly = function(var_set) {
    let factory = this.factory;
    this.update(function(ss) {
      let keys = [];
      for (let v in ss.map.getKeys()) {
        keys.push(v);
      }
      for each (let v in keys) {
        if (ss.get(v) != factory.BOTTOM && !var_set.has(v)) {
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

  /** Return a string value that uniquely represents the property state
   * of this substate. */
  Substate.prototype.pskey = function() {
    return Array.join([ this.map.get(vbl).ch 
                        for each (vbl in this.factory.psvbls) ], '');
  };

  Substate.prototype.toString = function() {
    return this.pskey() + ' ' + 
      Array.join([ expr_display(vbl) + ':' + this.map.get(vbl).ch
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
      this.map.put(vbl, this.factory.BOTTOM);
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
   *   @param psvbls  Set of property state variables
   *   @param bottom  Abstract value representing BOTTOM state
   *                  for a variable, i.e., any value
   *   @param trace   Debug tracing level: 0, 1, 2, or 3.
   */
  let Analysis = function(cfg, psvbls, bottom, meet, trace) {
    if (meet == undefined) {
      meet = this.default_meet;
      // Can't issue a warning here because instances used as prototypes
      // would print warnings
    }

    this.cfg = cfg;
    this.trace = trace;
    this.BOTTOM = bottom;
    this.meet = meet;
    this.psvbls = psvbls;
    this.psvblset = create_decl_set(psvbls);
  };

/** Run the solver to a fixed point. */
Analysis.prototype.run = function() {
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
  for each (let bb in order) bb.ready = true;
  let readyCount = order.length;
  let next = 0;

  // Fixed point computation
  this.initStates(order, order[next]);
  this.cfg.x_entry_block_ptr.stateIn = 
    new State(this, new Substate(this, this.startValues(), create_decl_map()));;
  if (this.trace) {
    print("initial state:");
    this.cfg.x_entry_block_ptr.stateIn.list();
  }

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
    if (bb != this.cfg.x_entry_block_ptr) this.mergeStateIn(bb);
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
            print("  branch " + truth + ": " + isn_display(cond));
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
            print("  branch " + truth + ": " + isn_display(cond));
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

/** Set the stateIn to be the merge of all the pred stateOut values. */
Analysis.prototype.mergeStateIn = function(bb) {
  let state = this.top();
  for each (let e in bb_pred_edges(bb)) {
    if (this.trace >= 3) {
      print("merge " + edge_string(e, this.cfg));
      e.state.list();
    }
    this.merge(state, e.state);
  }
  bb.stateIn = state;
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
      print("    " + isn_display(isn));
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
   *  is purely optional. It can be used for things like dropping state
   *  on variables no longer of interest.
   *  @param edge  CFG edge, with src and dest BB properties. There
   *               is also the .state property. */
  Analysis.prototype.updateEdgeState = function(edge) {
  };

  /** Customization function. Return a mapping of variables to abstract
   *  values to use for the initialization state on entry to the CFG
   *  being analyzed. This would normally be a map that represents
   *  every variable having any possible value. */
  Analysis.prototype.startValues = function(isn, state) {
    throw new Error("abstract method");
  };

  /** Default meet operator. Note that this version is unsound. If used,
   *  a warning will be issued. Return value of undefined indicates
   *  empty set state (TODO, s/b lattice member?). */
  Analysis.default_meet = function(v1, v2) {
    return undefined;
  };

return {
  'Analysis': Analysis
};

}();
