// { 'test': 'dehydra,treehydra', 'input': 'access.cc', 'output': 'unit_test' }
if (this.TREE_CODE)
  include('unstable/lazy_types.js');

var OK = false

function assert (t) {
  if (!t)
    throw new Error("visibility is busted");
}

function process_type(t) {
  if (t.name != "B")
    return

  print("class " + t.name + " : " +
        [b.access + " " + b.type.name for each (b in t.bases)].join(","))
  for each (let m in t.members) {
    print(m.access  + " " + m.name)
  }
  assert(t.bases[0].access == "public")
  OK = true
}

function process_tree(fn)
{
  if (decl_name(fn) != "getB")
    return
  let ttype = TREE_TYPE(fn)
  let lazyB = dehydra_convert(ttype).type.type
  print("--lazy---");
  process_type(lazyB)
}

function input_end() {
  print("OK")
}

function process_decl(v) {
  if (v.name == "A::static_member")
    assert(v.access == "private")
}
