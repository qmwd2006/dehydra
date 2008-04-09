// { 'test': 'dehydra,treehydra', 'input': 'empty.cc', 'output': 'unit_test' }

include('unit_test.js');

function RequireTestCase() {
}

RequireTestCase.prototype = new TestCase();

RequireTestCase.prototype.testBadKey = function() {
  // Fails with a warning -- should look for that, but not doing it now.
  require({ a: 7 });
}

RequireTestCase.prototype.testVersion = function() {
  let opts = require({ version: '1.8' });
  let r = [ 'a' for (i in [ 1, 2, 3 ]) ];
  this.assertEquals(opts.version, '1.8');
}

RequireTestCase.prototype.testEmpty = function() {
  require({ });
}

RequireTestCase.prototype.testStrict1 = function() {
  let opts = require({ strict: true });
  this.assertEquals(opts.strict, true);
  require({ strict: false });
}

RequireTestCase.prototype.testStrict2 = function() {
  let opts = require({ strict: true, werror: true });
  this.assertEquals(opts.strict, true);
  this.assertEquals(opts.werror, true);
  let ok = false;
  try {
    globalsNotAllowed = 9;
  } catch (e) {
    ok = true;
  }
  this.assertTrue(ok);
  require({ strict: false, werror: false });
}

let r = new TestResults();
new RequireTestCase().runTestCases(r);
r.list();