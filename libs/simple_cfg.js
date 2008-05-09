function SimpleCFGBody () {
}

SimpleCFGBody.prototype.flowState = function (t, state) {
  return state
}

SimpleCFGBody.prototype.path_end = function () {
}

SimpleCFGBody.prototype.states_equal = function (x, y) {
  return true
}

SimpleCFGBody.prototype.copyState = function (o) {
  return o
}

SimpleCFGBody.prototype.initialState = function () {
}
// f and path_end_f are the functions to apply
// cfg is the gcc cfg
function SimpleCFGIterator (body, cfg) {
  this.body = body
  this.cfg = cfg
  this.guard = new Map()
}

// contents of state variable will not be modified in recursion
// instead traverse_cfg_via_bb will duplicate it whenever modifcation looms
SimpleCFGIterator.prototype.traverse_cfg_via_bb = function (state, bb) {
  let priorStates = this.guard.get(bb)
  if (priorStates) {
    // If this Basic block has been entered with the same precondition before
    // it will end up computing same postcodition, so skip it
    for each (let prec in priorStates) {
      if (this.body.states_equal (state, prec))
        return
    }
  } else {
    priorStates = []
    this.guard.put (bb, priorStates);
  }
  // no need to duplicate here as this state should never be modified
  priorStates.push (state)
  // duplicate state to avoid mutation
  state = this.body.copyState(state)
  if (bb == this.cfg.x_exit_block_ptr) {
    this.body.path_end (state)
    return
  }
  
  for (let isn in bb_isn_iterator (bb)) {
    state = this.body.flowState (isn, state)
  }
  for each (let s in VEC_iterate(bb.succs)) {
    // safe to call with unduplicated state
    this.traverse_cfg_via_bb (state, s.dest)
  }
}

SimpleCFGIterator.prototype.go = function () {
  this.traverse_cfg_via_bb (this.body.initialState (),
                            this.cfg.x_entry_block_ptr)
}
