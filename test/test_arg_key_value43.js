// { 'test': 'dehydra', 'input': 'empty.cc', 'output': 'unit_test', 'args': { 'blah': 'bleh', 'bip': 'bop' }, 'version': '4.3' }

include('unstable/getopt.js');
include('unit_test.js');

let [opts, args] = getopt();

let r = new TestResults();

function EqualsTest(v1, v2) {
  this._v1 = v1;
  this._v2 = v2;
}

EqualsTest.prototype = new TestCase();
EqualsTest.prototype.runTest = function() {
  this.assertEquals(this._v1, this._v2);
}

new EqualsTest(opts.blah, "bleh").run(r);
new EqualsTest(opts.bip, "bop").run(r);

function input_end() {
  r.list();
}
