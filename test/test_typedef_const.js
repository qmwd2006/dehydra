// { 'test': 'dehydra', 'input': 'typedef.cc', 'output': 'unit_test' }

// Make sure that 'const' is handled correctly on typedefs

include('unit_test.js');

let r = new TestResults();

function ConstTypedefTest(type) {
  this.type = type;
}

ConstTypedefTest.prototype = new TestCase();

ConstTypedefTest.prototype.runTest = function() {
  this.assertTrue(this.type.isConst);
};

function process_decl(d)
{
  if (d.name == 'const_PRInt32')
    new ConstTypedefTest(d.type).run(r);
}

function input_end()
{
  r.verifyExpectedTestsRun(1)
}
