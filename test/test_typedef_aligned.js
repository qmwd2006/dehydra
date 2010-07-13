// { 'test': 'dehydra', 'input': 'typedef_aligned.cc', 'output': 'unit_test', 'version': '4.5' }

include('unit_test.js');
include('util.js');

let r = new TestResults();

TypedefTestCase.prototype = new TestCase();

function TypedefTestCase(type) {
  this.type = type;
}

TypedefTestCase.prototype.runTest = function () {
  let type = this.type;
  this.assertEquals(type.name, "Blah");
}

function process_type(t) {
  while(t) {
    new TypedefTestCase(t).run(r);
    r.list();
    t = t.typedef;
  }
}
