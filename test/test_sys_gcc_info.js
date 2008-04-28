// { 'test': 'dehydra,treehydra', 'input': 'empty.cc', 'output': 'unit_test' }
include('unit_test.js');
require({strict:true})

function SysTest(v1, v2) {
}

SysTest.prototype = new TestCase();
SysTest.prototype.runTest = function()
{
  this.assertEquals(sys.aux_base_name, "empty");
  this.assertTrue(/\d+\.\d+\.\d+/(sys.gcc_version))
  this.assertTrue(sys.gcc_pkgversion != undefined)
}

var r = new TestResults()
new SysTest().run(r);

function input_end()
{
  r.list();
}

