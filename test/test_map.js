// { 'test': 'dehydra,treehydra', 'input': 'empty.cc', 'output': 'unit_test' }
include("map.js")

function assert(t) {
  if (!t) throw new Error("Misbehaving map")
}

function test_map(map_constructor) {
  let m = new map_constructor();

  m.put('a', 1);
  m.put('b', 2);
  m.put('c', 3);
  assert(m.has('a'))
  assert(m.has('b'))
  assert(m.has('c'))
  m.remove('b');
  m.remove('c');

  m.put('c', 5);
  m.put('c', 7);
  assert(m.get('c') == 7)
  m.put(this, 11)
  assert(m.get(this) == 11)
  assert(!m.remove({}))
  assert(m.remove(this))
}

test_map (Map)
test_map (InjHashMap)
print("OK")
