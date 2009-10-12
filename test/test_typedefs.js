// { 'test': 'treehydra', 'input': 'typedef.cc', 'output': 'unit_test', 'lang': 'c,c++' }

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
   * sizetype isn't even seen by gcc <=4.3
   * GCC bug: typedefs decls in class context don't have DECL_ORIGINAL_CONTEXT,
   * so we don't think they are typedefs. Yuck.
   */
  'size_type': new TypedefCheck('size_type', function(got) {
    this.assertEquals(got.typedef.name, 'PRUint32');
  }),
 
  
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
  for (let testname in tests) {
    let t = tests[testname]
    // warn about typedefs processed by dehydra but not treehydra
    if (t.dehydra && t.treehydra === null)
      r.addError(t, "Declaration not processed: " + testname);
  }
  r.list();
}
