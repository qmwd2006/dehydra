include('map.js');

function create_set(map_ctor) {
  let Set = function(keyFunc, labelFunc) {
    this.map = new map_ctor(labelFunc, keyFunc);
  }

  /** Iterate over the items in the set. */
  Set.prototype.items = function() {
    for (let e in this.map.getKeys()) {
      yield e;
    }
  };

  /** Add an element to the set. */
  Set.prototype.add = function(e) {
    this.map.put(e, e);
  };

  /** Remove an element from the set. */
  Set.prototype.remove = function(e) {
    this.map.remove(e);
  };

  /** True if this element is in the set. */
  Set.prototype.has = function(e) {
    return this.map.has(e);
  };

  /** Add all elements in s2 to this set. */
  Set.prototype.unionWith = function(s2) {
    for (let e in s2.items()) {
      this.map.put(e, e);
    }
  };

  /** Shallow copy of this set. */
  Set.prototype.copy = function() {
    let s = new Set(this.map.getKey, this.map.getLabel);
    s.unionWith(this);
    return s;
  };

  /** String representation of the set. */
  Set.prototype.toString = function() {
    let ss = [ this.map.getLabel(z) for (z in this.items()) ];
    return Array.join(ss, ', ');
  };

  Set.prototype.equals = function(m) {
    for (let k1 in this.items()) {
      if (!m.has(k1)) return false;
    }
    for (let k1 in m.items()) {
      if (!this.has(k1)) return false;
    }
    return true;
  };

  return Set;
}

Set = create_set(Map);

InjHashSet = create_set(InjHashMap);
