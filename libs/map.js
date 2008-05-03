// Map data structures

/** Default equality function for maps. */
function base_equals(x, y) {
  return x === y;
}

/** Default key function for injective hash maps. */
function base_key(x) {
  return x.toString();
}

/** Create a new Map with an ES4-like interface. */
function Map(equals_func, hashcode_func) {
  this.equals_func = equals_func ? equals_func : base_equals;
  this.hashcode_func = hashcode_func ? hashcode_func : hashcode;
  this.data = {};
}

/** Create a new empty map with same customization functions as this one. */
Map.prototype.create = function() {
  return new Map(this.equals_func, this.hashcode_func);
}

/** Return a map that is a copy of this map: the map is distinct, but the
 *  keys and values are pointer copies (not deep copies). */
Map.prototype.copy = function() {
  let m = this.create();
  for (let [k, v] in this.getItems()) {
    m.put(k, v);
  }
  return m;
};

/** Structural equality. Two maps are equal iff they have their key sets
 *  are equal and the corresponding values are structurally equal.
 *  @see equals (global). */
Map.prototype.equals = function(m) {
  for (let [k1, v1] in this.getItems()) {
    let v2 = m.get(k1);
    if (!equals(v1, v2)) return false;
  }
  for (let [k1, v1] in m.getItems()) {
    if (!this.has(k1)) return false;
  }
  return true;
};

/** True if this map has no elements. */
Map.prototype.isEmpty = function() {
  for (let k in this.data) {
    return false;
  }
  return true;
};

/** Add a key-value entry. */
Map.prototype.put = function(k, v) {
  let h = this.hashcode_func(k);
  let chain = this.data[h];
  if (chain == undefined) {
    this.data[h] = [ [k, v] ];
    return;
  }
    
  for (let i = 0; i < chain.length; ++i) {
    let entry = chain[i];
    if (this.equals_func(entry[0], k)) {
      entry[1] = v;
      return;
    }
  }

  chain.push([k, v]);
}

/** Remove entry with given key. */
Map.prototype.remove = function(k) {
  let h = this.hashcode_func(k);
  let chain = this.data[h];
  if (chain == undefined) return false;
  for (let i = 0; i < chain.length; ++i) {
    let entry = chain[i];
    if (this.equals_func(entry[0], k)) {
      if (chain.length == 1) {
        delete this.data[h];
      } else {
        chain.splice(i, 1);
      }
      return true;
    }
  }
}

/** Return the value associated with the given key, or undefined if none. */
Map.prototype.get = function(k) {
  let h = this.hashcode_func(k);
  let chain = this.data[h];
  if (chain == undefined) return undefined;
  for (let i = 0; i < chain.length; ++i) {
    let entry = chain[i];
    if (this.equals_func(entry[0], k)) {
      return entry[1];
    }
  }
  return undefined;
}

/** True if this key is in the map. */
Map.prototype.has = function(k) {
  let h = this.hashcode_func(k);
  let chain = this.data[h];
  if (chain == undefined) return false;
  for (let i = 0; i < chain.length; ++i) {
    let entry = chain[i];
    if (this.equals_func(entry[0], k)) {
      return true;
    }
  }
  return false;
}

/** Iterate over key-value pairs. */
Map.prototype.getItems = function() {
  for each (let chain in this.data) {
    for (let i = 0; i < chain.length; ++i) {
      yield chain[i];
    }
  }
}

/** Iterate over just the keys. */
Map.prototype.getKeys = function() {
  for each (let chain in this.data) {
    for (let i = 0; i < chain.length; ++i) {
      yield chain[i][0];
    }
  }
}

/** Iterate over just the values. */
Map.prototype.getValues = function() {
  for each (let chain in this.data) {
    for (let i = 0; i < chain.length; ++i) {
      yield chain[i][1];
    }
  }
}

/** Create a new map with custom behavior.
  *
  * @param keyFunc    function key -> accessKey. This function must return the
  *            "accessKey" for a key used in the map. The accessKey
  *            defines equality of key objects, so must be unique for
  *            keys that are to be considered distinct.
  *
  * @param labelFunc  [optional] function value -> string. Used to print values
  *            when printing the Map, in place to toString, which doesn't
  *            work well for Treehydra objects.
  */
function InjHashMap(labelFunc, keyFunc) {
  this.getKey = keyFunc ? keyFunc : hashcode;
  this.getLabel = labelFunc == undefined ? function (s) s : labelFunc;
  this.data = {};
}

/** Create a new empty map with same customization functions as this one. */
InjHashMap.prototype.create = function() {
  return new InjHashMap(this.getLabel, this.getKey);
}

/** Return a map that is a copy of this map: the map is distinct, but the
 *  keys and values are pointer copies (not deep copies). */
InjHashMap.prototype.copy = function() {
  let m = this.create();
  for (let [k, v] in this.getItems()) {
    m.put(k, v);
  }
  return m;
};

/** Structural equality. Two maps are equal iff they have their key sets
 *  are equal and the corresponding values are structurally equal.
 *  @see equals (global). */
InjHashMap.prototype.equals = function(m) {
  for (let [k1, v1] in this.getItems()) {
    let v2 = m.get(k1);
    //print("MQ " + this.getKey(k1) + ' ' + this.getLabel(v1) + ' ' + this.getLabel(v1));
    if (!equals(v1, v2)) return false;
  }
  for (let [k1, v1] in m.getItems()) {
    if (!this.has(k1)) return false;
  }
  return true;
};

/** True if this map has no elements. */
InjHashMap.prototype.isEmpty = function() {
  for (let k in this.data) {
    return false;
  }
  return true;
};

/** Add a key-value entry. */
InjHashMap.prototype.put = function(k, v) {
  this.data[this.getKey(k)] = [k, v];
}

/** Remove entry with given key. */
InjHashMap.prototype.remove = function(k) {
  let key = this.getKey(k)
  let ret = this.data.hasOwnProperty (key)
  if (!ret) return false
  delete this.data[key];
  return true
}

/** Return the value associated with the given key, or undefined if none. */
InjHashMap.prototype.get = function(k) {
  let ak = this.getKey(k);
  // This is required for strict mode.
  if (!this.data.hasOwnProperty(ak)) return undefined;
  let [ko, vo] = this.data[ak];
  return vo;
}

/** True if this key is in the map. */
InjHashMap.prototype.has = function(k) {
  return this.data[this.getKey(k)] != undefined;
}

/** Iterate over key-value pairs. */
InjHashMap.prototype.getItems = function() {
  for (let k in this.data) {
    yield this.data[k];
  }
}

/** Iterate over just the keys. */
InjHashMap.prototype.getKeys = function() {
  for (let k in this.data) {
    let d = this.data[k];
    // Hack because of _hashcode installed by treehydra.js
    if (typeof d != 'object') continue;
    yield d[0];
  }
}

/** Iterate over just the values. */
InjHashMap.prototype.getValues = function() {
  for (let k in this.data) {
    let d = this.data[k];
    // Hack because of _hashcode installed by treehydra.js
    if (typeof d != 'object') continue;
    yield d[1];
  }
}
