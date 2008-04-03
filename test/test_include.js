include('unit_test.js');

function IncludeTestCase(filename) {
  this.filename = filename;
}

IncludeTestCase.prototype = new TestCase();

IncludeTestCase.prototype.toString = function() {
  return "IncludeTestCase(" + this.filename + ")";
}

IncludeTestCase.prototype.runTest = function() {
  try {
    include(this.filename);
  } catch (e) {
    // This is exactly what we want.
    return;
  }
  this.fail("no include error");
};

let names = ['nofile.js', 'syntax_error.js', 'semantic_error.js' ];
let r = new TestResults();
for each (let name in names) {
  let t = new IncludeTestCase(name);
  t.run(r);
}
r.list();