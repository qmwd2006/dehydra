// { 'test': 'dehydra', 'input': 'variant.cc', 'output': 'unit_test' }

// Make sure that const types (variants) point back to the main variant

include('unit_test.js');

let r = new TestResults();

function VariantTest(main, variant)
{
  this.main = main;
  this.variant = variant;
}

VariantTest.prototype = new TestCase();

VariantTest.prototype.runTest = function() {
  this.assertEquals(this.variant.variantOf, this.main);
};

function process_decl(d)
{
  print(d);
  new VariantTest(d.type.parameters[0].type, d.type.parameters[1].type).run(r);
}

function input_end()
{
  r.verifyExpectedTestsRun(1)
}
