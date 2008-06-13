function cfg_dfs (visitor, bb, guard) {
  if (!guard) guard = new InjHashMap()
  if (guard.has(bb)) return
  guard.put(bb)
  visitor.push()
  for (let isn in bb_isn_iterator(bb)) {
    visitor.next_isn(isn)
  }
  for each (let s in VEC_iterate(bb.succs)) {
    cfg_dfs(visitor, s.dest, guard)
  }
  visitor.pop ()
}
