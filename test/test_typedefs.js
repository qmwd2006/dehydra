// { 'test': 'treehydra', 'input': 'typedef.cc', 'output': 'unit_test' }

include('unit_test.js');
include('unstable/lazy_types.js');

function TypedefCheck(name, check)
{
  this.name = 'TypedefCheck<' + name + '>';
  this.check = check;
}
TypedefCheck.prototype = new TestCase();
TypedefCheck.prototype.dehydra = null;
TypedefCheck.prototype.treehydra = null;
TypedefCheck.prototype.runTest = function() {
  this.assertTrue(this.dehydra);
  this.check(this.dehydra);
  this.assertTrue(this.treehydra);
  this.check(this.treehydra);
};

const tests = {
  'PRUint32': new TypedefCheck('PRUint32', function(got) {
    this.assertEquals(got.typedef.name, 'unsigned int');
  }),

  'PRInt32': new TypedefCheck('PRInt32', function(got) {
    this.assertEquals(got.typedef.name, 'int');
  }),
  
  'PRInt32_2': new TypedefCheck('PRInt32_2', function(got) {
    this.assertEquals(got.typedef.name, 'PRInt32');
  }),
  
  'PRInt322': new TypedefCheck('PRInt322', function(got) {
    this.assertTrue(got.isPointer);
    this.assertTrue(got.typedef.isPointer);
    this.assertEquals(got.typedef.type.name, 'PRInt32');
  }),
  
  'TypedefTypedef': new TypedefCheck('TypedefTypedef', function(got) {
    this.assertEquals(got.typedef.name, 'PRUint32');
  }),
  
  'stype': new TypedefCheck('stype', function(got) {
    this.assertEquals(got.typedef.name, 's');
  }),
  
  /**
   * GCC bug: typedefs decls in class context don't have DECL_ORIGINAL_CONTEXT,
   * so we don't think they are typedefs. Yuck.
  's::size_type': new TypedefCheck(function(got) {
    this.assertEquals(got.typedef.name, 'PRUint32');
    this.assertEquals(got.memberOf.name, 's');
  }),
  */
  
  '__m64': new TypedefCheck('__m64', function(got) {}),
};

let r = new TestResults();

function process_type(t)
{
  if (!t.typedef)
    return;
  
  let test = tests[t.name];
  if (test === undefined)
    throw new Error("Unexpected typedef: " + t.name);
  
  test.dehydra = t;
}

function process_tree_type(t)
{
  t = dehydra_convert(t);

  print("Found treehydra: " + t);
  
  if (!t.typedef)
    return;

  let test = tests[t.name];
  if (test === undefined)
    throw new Error("Unexpected typedef: " + t.name);
  
  test.treehydra = t;
  test.run(r);
}

function input_end()
{
  for (let testname in tests)
    if (tests[testname].treehydra === null)
      r.addError(tests[testname], "Declaration not processed: " + testname);
  
  r.list();
}
