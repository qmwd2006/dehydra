// { 'test': 'dehydra', 'input': 'bitfields.cc', 'output': 'unit_test' }

// Test that bitfields are represented correctly

include('unit_test.js');

/**
 * Check that a property path leads to an expected value.
 * 
 * @param v The object to check
 * @param path An array of properties (strings or numbers) to follow
 * @param expected The value that is expected... checked with ==
 */
function TestProperty(v, path, expected)
{
  this._v = v;
  this._path = path;
  this._expected = expected;
}
TestProperty.prototype = new TestCase();
TestProperty.prototype.runTest = function() {
  let v = this._v;
  for each (let p in this._path) {
    v = v[p];
  }
  this.assertEquals(v, this._expected);
};

let tests = [
  [['members', 0, 'name'],                       'Test::i'],
  [['members', 0, 'type', 'name'],               'int'],
  [['members', 0, 'type', 'bitfieldBits'],       undefined],
  [['members', 0, 'type', 'bitfieldOf'],         undefined],
  [['members', 1, 'name'],                       'Test::j'],
  [['members', 1, 'type', 'name'],               'int:22'],
  [['members', 1, 'type', 'bitfieldBits'],       22],
  [['members', 1, 'type', 'bitfieldOf', 'name'], 'int'],
  [['members', 2, 'name'],                       'Test::k'],
  [['members', 2, 'type', 'name'],               'unsigned int:23'],
  [['members', 2, 'type', 'bitfieldBits'],       23],
  [['members', 2, 'type', 'bitfieldOf', 'name'], 'unsigned int'],
  [['members', 3, 'name'],                       'Test::m'],
  [['members', 3, 'type', 'name'],               'float'],
  [['members', 4, 'name'],                       'Test::n'],
  [['members', 4, 'type', 'name'],               'long long int'],
  [['members', 4, 'type', 'bitfieldBits'],       undefined]
];
 
let r = new TestResults();

function process_type(t)
{
  if (t.name != 'Test')
    return;
  
  for each (test in tests) {
    let [path, expected] = test;
    new TestProperty(t, path, expected).run(r);
  }
}

function input_end()
{
  if (r.testsRun != tests.length) {
    throw new Error("Error: no tests were run!");
  }
  else {
    r.list();
  }
}
