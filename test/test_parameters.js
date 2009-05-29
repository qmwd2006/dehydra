// { 'test': 'dehydra', 'input': 'parameters.cc', 'output': 'unit_test' }

// Test that function parameters are exposed in the .parameters array

include('unit_test.js');

function DeclTestCase(d) {
  this.decl = d;
}
DeclTestCase.prototype = new TestCase();
DeclTestCase.prototype.runTest = function() {
  this.assertEquals(this.decl.parameters[0].name, 'i');
  this.assertEquals(this.decl.parameters[1].name, 'j');
};

let r = new TestResults();

function process_decl(d)
{
  new DeclTestCase(d).run(r);
}

function input_end() {
  r.verifyExpectedTestsRun(1)
}
