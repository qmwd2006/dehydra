include('unit_test.js');

let [arg1, arg2] = arguments;

let r = new TestResults();

function EqualsTest(v1, v2) {
  this._v1 = v1;
  this._v2 = v2;
}

EqualsTest.prototype = new TestCase();
EqualsTest.prototype.runTest = function()
{
  this.assertEquals(this._v1, this._v2);
}

new EqualsTest(_arguments, "hello  goodbye").run(r);
new EqualsTest(arg1, "hello").run(r);
new EqualsTest(arg2, "goodbye").run(r);

t = new TestCase();

function input_end()
{
  r.list();
}
