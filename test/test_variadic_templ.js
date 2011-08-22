// { 'test': 'dehydra', 'input': 'var_templ.cc', 'output': 'unit_test', 'lang': 'c++0x' }

// Test that variadic templates are handled correctly

include('unit_test.js');

let r = new TestResults();

function FuncTestCase(decl) {
  this.decl = decl;
}
FuncTestCase.prototype = new TestCase();
FuncTestCase.prototype.runTest = function() {
  this.assertTrue(this.decl.isFunction);
  let params = this.decl.type.parameters;
  this.assertEquals(params.length, 1);
  this.assertTrue(params[0].isTypename);
  this.assertEquals(params[0].name, "A ...");
};

function ClassTestCase(type) {
  this.type = type;
}
ClassTestCase.prototype = new TestCase();
ClassTestCase.prototype.runTest = function() {
  this.assertEquals(this.type.name, "C<B>");
  this.assertEquals(this.type.template.arguments.length, 1);
  this.assertTrue(this.type.template.arguments[0].isTypename);
  this.assertEquals(this.type.template.arguments[0].name, "B ...");
};

function process_decl(decl) {
  if (decl.name == 'func')
    new FuncTestCase(decl).run(r);
  else if (decl.name == 'C')
    new ClassTestCase(decl.type).run(r);
}

function input_end() {
  r.verifyExpectedTestsRun(2)
}
