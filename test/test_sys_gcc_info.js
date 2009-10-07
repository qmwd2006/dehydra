// { 'test': 'dehydra,treehydra', 'input': 'empty.cc', 'output': 'unit_test' }
include('unit_test.js');
require({strict:true})

function SysTest(v1, v2) {
}

SysTest.prototype = new TestCase();
SysTest.prototype.runTest = function()
{
  // aux_base_name has the filename passed to gcc -extension
  this.assertEquals(sys.aux_base_name, "empty");
  this.assertEquals(sys.main_input_filename, "empty.cc");
}

function input_end()
{
  var r = new TestResults()
  new SysTest().run(r);
  r.list();
}

