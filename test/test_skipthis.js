include('unit_test.js');

let r = new TestResults();

// Test is constructed so all function types should have 2 args.
function SkipThisTestCase(fntype) {
  this.fntype = fntype;
}

SkipThisTestCase.prototype = new TestCase();

SkipThisTestCase.prototype.runTest = function() {
  this.assertEquals(this.fntype.parameters.length, 2);
}

function process_function(decl, body) {
  let rtype = decl.type.type;
  // The class type case
  let members = rtype.members;
  if (members != undefined) {
    for each (let m in members) {
      new SkipThisTestCase(m.type).run(r);
    }
  }
  // The fptr typedef case
  let type = rtype.typedef;
  if (type != undefined) {
    new SkipThisTestCase(type.type).run(r);
  }
}

function input_end() {
  if (r.testsRun != 3) {
    print("Error: must be 3 tests run, instead ran " + r.testsRun);
  } else {
    r.list();
  }
}