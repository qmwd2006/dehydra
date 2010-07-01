// { 'test': 'treehydra', 'input': 'enum.cc', 'output': 'unit_test' }

require({ after_gcc_pass: "cfg" });

include('unstable/lazy_types.js')
include('unit_test.js')

let r = new TestResults();

function EnumTestCase(type) {
  this.type = type;
}

EnumTestCase.prototype = new TestCase();

EnumTestCase.prototype.runTest = function () {
  let type = this.type;
  this.assertEquals(type.kind, 'enum');
  this.assertEquals(type.name, 'here');
  this.assertEquals(type.members.length, 3);
  let m = type.members;
  this.assertEquals(m[0].name, 'a');
  this.assertEquals(m[0].value, 0);
  this.assertEquals(m[1].name, 'b');
  this.assertEquals(m[1].value, 1);
  this.assertEquals(m[2].name, 'c');
  this.assertEquals(m[2].value, 42);
}

// Test that 'enum' type is passed correctly
function process_type(type) {
  new EnumTestCase(type).run(r);
}

// Test enum handling of dehydra_convert
function process_tree(fn) {
  let cfg = function_decl_cfg(fn);
  for (let isn in cfg_isn_iterator(cfg)) {
    if (isn.tree_code() != GIMPLE_ASSIGN)
      continue;
    let lhs = gimple_op(isn, 0);
    let t = TREE_TYPE(lhs);
    t = dehydra_convert(t);
    new EnumTestCase(t).run(r);
  }
}

function input_end() {
  r.verifyExpectedTestsRun(2);
}
