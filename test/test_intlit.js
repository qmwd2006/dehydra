// { 'test': 'dehydra', 'input': 'intlit.cc', 'output': 'unit_test' }

// Test that integer literals are handled correctly.

include('unit_test.js');

let r = new TestResults();

// v comes from the intlic.cc body[] values, exp comes from our
// expected[] values
function MyTestCase(v, exp) {
  this.v = v;
  this.exp = exp;
}

MyTestCase.prototype = new TestCase();

MyTestCase.prototype.runTest = function() {
  // print(this.v[0]);
  this.assertEquals(this.v[0].value, this.exp);
};

let expected = [
  '1u',
  '4294967295u',
  '-10',
  '12',
  '0l',
  '0ll',
  '0u',
  '0ul',
  '0ull',
  '-1073741782',
  '-1073741782l',
  '-1073741782ll',
  '1073741866',
  '1073741866l',
  '1073741866ll',
  '-281474976710665ll',
  '281474976710665ll',
  '9223372036854775885ull'
];

function process_function(decl, body) {
  for (let i = 0; i < expected.length; ++i) {
    new MyTestCase(body[i].statements[0].assign, expected[i]).run(r);
  }
}

function input_end() {
  if (r.testsRun != expected.length) {
    print("Error: must be " + expected.length + " tests run, instead ran " + r.testsRun);
  } else {
    r.list();
  }
}
