// { 'test': 'dehydra,treehydra', 'input': 'virtual_inheritance.cc', 'output': 'unit_test' }
if (this.TREE_CODE)
  include('unstable/lazy_types.js');

var OK = false

function assert (t) {
  if (!t)
    throw new Error("visibility is busted");
}

function process_type(t) {
  if (t.name != "C")
    return

  print("class " + t.name + " : " +
        [b.access + " " + (b.isVirtual? "virtual ": "")
            + b.type.name for each (b in t.bases)].join(","))

  print("t.bases[0].isVirtual: " + t.bases[0].isVirtual);

  assert(! t.bases[0].isVirtual);
  assert(t.bases[1].isVirtual);

  OK = true
}

function process_tree(fn)
{
  if (decl_name(fn) != "getC")
    return
  let ttype = TREE_TYPE(fn)
  let lazyC = dehydra_convert(ttype).type.type
  print("--lazy---");
  process_type(lazyC)
}

function input_end() {
  print("OK")
}
