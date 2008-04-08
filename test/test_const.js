// Test that 'const' is handled correctly.

include('unit_test.js');

let r = new TestResults();

// Test is constructed so all function types should have 2 args.
function ConstTestCase(type) {
  this.type = type;
}

ConstTestCase.prototype = new TestCase();

ConstTestCase.prototype.runTest = function() {
  this.assertTrue(this.type.isPointer);
  let type = this.type.type;
  this.assertTrue(type.isConst);
  this.assertEquals(type.name, "C");
};

function process_function(decl, body) {
  let ret_type = decl.type.type;
  new ConstTestCase(ret_type).run(r);
}

function input_end() {
  if (r.testsRun != 1) {
    print("Error: must be 1 tests run, instead ran " + r.testsRun);
  } else {
    r.list();
  }
}
