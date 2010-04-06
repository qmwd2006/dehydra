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

/* Data structures for program analysis. */

include('map.js');
include('set.js');

// Speed measurements

let timers = {};

function timer_start(name) {
  if (name == undefined) name = 'default';
  timers[name] = new Date();
}

function timer_stop(name) {
  if (name == undefined) name = 'default';
  let t1 = new Date();
  let t0 = timers[name];
  let dt = t1-t0;
  print("timer '" + name + "': " + dt);
}

// Equality function

/** Structural equality function. Return true if the two objects are
 *  equal. Equality of arrays is defined as equality of corresponding
 *  elements. Equality of other objects is defined by calling
 *  v1.equals(v2). Equality of all other values is according to ==. 
 *
 *  This function works with Maps and Sets. */
function equals(v1, v2) {
  if (v1 instanceof Object && v2 instanceof Object) {
    if (v1 instanceof Array && v2 instanceof Array) {
      if (v1.length != v2.length) return false;
      for (let i = 0; i < v1.length; ++i) {
        if (!equals(v1[i], v2[i])) return false;
      }
      return true;
    } else if (v1.equals) {
      return v1.equals(v2);
    }
  } 
  return v1 == v2;
}

function test_perf() {
  let m = new Map(function (x, y) x === y, function(x) x);
  do_test_perf(m, 'Map');
  m = new InjHashMap(undefined, function(x) x);
  do_test_perf(m, 'InjHashMap');

  let s = new Set(function (x, y) x === y, function(x) x);
  do_test_perf_set(s, 'Set');
  s = new InjHashSet(function(x) x);
  do_test_perf_set(s, 'InjHashSet');
}

function do_test_perf(m, tag) {
  let size = 100000;
  timer_start(tag);
  for (let i = 0; i < size; ++i) {
    m.put(i, i * 3);
  }
  for (let j = 0; j < 3; ++j) {
    for (let i = 0; i < size; ++i) {
      m.get(i);
    }
  }
  timer_stop(tag);
}

function do_test_perf_set(s, tag) {
  let size = 100000;
  timer_start(tag);
  for (let i = 0; i < size; ++i) {
    s.add(i * 3);
  }
  for (let j = 0; j < 3; ++j) {
    for (let i = 0; i < size; ++i) {
      s.has(i);
    }
  }
  timer_stop(tag);
}

//test_perf();

/** Singleton. */
function MapFactoryCtor() {
  this.use_injective = false;
}

MapFactoryCtor.prototype.create_map = function(equals_func, hash_func, key_func) {
  if (this.use_injective) {
    return new InjHashMap(undefined, key_func);
  } else {
    return new Map(equals_func, hash_func);
  }
}

MapFactoryCtor.prototype.create_set = function(equals_func, hash_func, key_func) {
  if (this.use_injective) {
    return new InjHashSet(key_func);
  } else {
    return new Set(equals_func, hash_func);
  }
}

/** Factory for maps and sets. Allows the user to globally configure which
 *  implementation is used. */
MapFactory = new MapFactoryCtor();
