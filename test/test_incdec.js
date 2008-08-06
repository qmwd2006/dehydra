// { 'test' : 'dehydra', 'input' : 'incdec.cc', 'output': 'unit_test' }

// Test for prefix and postfix -- and ++

include('unit_test.js');

let r = new TestResults();

function process_function(decl, body) {
  let tc = new TestCase();
  tc.runTest = function () {
    let assign_c = 0;
    let names = {};
    for each (let bodyItem in body)
      for each (let stmtItem in bodyItem.statements) {
	if (stmtItem.assign)
	  ++assign_c;
	names[stmtItem.name] = true;
      }
    tc.assertEquals(assign_c, 4);
    tc.assertTrue(names['prepp']);
    tc.assertTrue(names['premm']);
    tc.assertTrue(names['postpp']);
    tc.assertTrue(names['postmm']);
  };
  tc.run(r);
}

function input_end() {
  if (r.testsRun != 1)
    print('Error: must be 1 tests run, instead ran ' + r.testsRun);
  else
    r.list();
}
