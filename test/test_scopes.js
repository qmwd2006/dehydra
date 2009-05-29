// { 'test': 'dehydra', 'input': 'scopes.cc', 'output': 'unit_test' }

// Test that scoping operators appear in type names as expected.

include('unit_test.js');

let expected_names = 'NS::Outer::Inner,NS::Outer,Outer2::Inner2,Outer2';
let check = {};
for each (let name in expected_names.split(',')) {
  check[name] = true;
}

let r = new TestResults();

// Test is constructed so all function types should have 2 args.
function ScopeTestCase(type) {
  this.type = type;
}

ScopeTestCase.prototype = new TestCase();

ScopeTestCase.prototype.runTest = function() {
  this.assertTrue(check[this.type.name]);
}

function process_type(type) {
  new ScopeTestCase(type).run(r);
}

function input_end() {
  r.verifyExpectedTestsRun(4)
}
