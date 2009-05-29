// { 'test': 'dehydra', 'input': 'enum.cc', 'output': 'unit_test' }

// Test that 'enum' type is passed correctly

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

function process_type(type) {
  new EnumTestCase(type).run(r);
}

function input_end() {
  r.verifyExpectedTestsRun(1)
}
