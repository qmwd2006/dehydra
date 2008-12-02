// { 'test': 'dehydra', 'input': 'csttype.cc', 'output': 'unit_test' }

// test that .type is properly setup on constant nodes

include('unit_test.js')

let r = new TestResults();

function CstDotTypeTestCase(cst, expType, expTypeName) {
  this.cst = cst;
  this.expType = expType;
  this.expTypeName = expTypeName;
}

CstDotTypeTestCase.prototype = new TestCase();

CstDotTypeTestCase.prototype.runTest = function () {
  this.assertTrue(this.cst.type === this.expType);
  this.assertEquals(this.cst.type.name, this.expTypeName);
}

let expNames = [
  'int',
  'unsigned int',
  'long int',
  'float',
  'double' ];

function process_function(decl, body) {
  for (let i = 0; i < expNames.length; ++i) {
    let stmt = body[i].statements[0];
    new CstDotTypeTestCase(stmt.assign[0], stmt.type, expNames[i]).run(r);
  }
}

function input_end() {
  if (r.testsRun != expNames.length) {
    print("Error: must be " + expNames.length + " tests run, instead ran " +
	  r.testsRun);
  } else {
    r.list();
  }
}
