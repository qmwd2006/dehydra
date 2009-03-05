// { 'test': 'dehydra,treehydra', 'input': 'empty.cc', 'output': 'unit_test' }

function assert (t) {
  if (!t)
    throw new Error("hashcode is busted");
}
// primtives are left intact
assert (hashcode("str") === "str")
assert (hashcode(true) === true)
const h = hashcode(this)
assert (hashcode({}) != hashcode({}))
assert (hashcode(this) == h)
assert (hashcode(this) != this)
assert (hashcode(this) != hashcode(assert))
print("OK")

