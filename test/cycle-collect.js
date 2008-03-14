function traverse_hierarchy(f, c) {
  if (f(c))
    return true
  for each (b in c.bases) {
    if (traverse_hierarchy(f, b))
      return true
  }
}

function implements_nsISupports(c) {
  function nsISupports (x) {
    return x.name == "nsISupports"
  }
  return traverse_hierarchy (nsISupports, c)
}

function implementsCC(c) {
  function has_mRefCnt (x) {
    for each (var m in x.members) {
      if (/::mRefCnt/(m.name)) {
        return true
      }
    }
  }
  return traverse_hierarchy (has_mRefCnt, c)
}

function contains_nsCOMPtr (c) {
  for each (var m in c.members) {
    var t = m.type
    // ignore nsCOMptr returning functions
    if (!t.parameters && t.template && /nsCOMPtr|nsRefPtr/(t.template.name)) {
      print (m.loc + " " + m.name)
      return true
    }
  }
}

function isAbstract (c) {
  for each (var m in c.members) {
    if (m.isVirtual == "pure") 
      return true
  }
}

var candidates = []
function process_type(type) {
  if (type.template
      || !/class|struct/(type.kind) 
      || !implements_nsISupports (type)
      || isAbstract(type)
      // skip cycle collected classes
      || implementsCC (type)) return
  if (contains_nsCOMPtr (type)) {
    candidates.push(type.name)
  }
}

function input_end() {
  print (candidates)
  write_file(this.aux_base_name + ".cycle", candidates.join("\n"))
}
