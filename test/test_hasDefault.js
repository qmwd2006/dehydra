// { 'test': 'dehydra', 'input': 'hasDefault.cc', 'output': 'unit_test' }

// Test for correct results for hasDefault

include('unit_test.js');

let r = new TestResults();

function TestDefaults(member, values)
{
  this.member = member;
  this.values = values;
}

TestDefaults.prototype = new TestCase();

TestDefaults.prototype.runTest = function() {
  let params = this.member.parameters;
  this.assertEquals(params.length, this.values.length);
  for (let i = 0; i < params.length; i++)
    this.assertEquals(params[i].hasDefault, this.values[i]);
};

function getMember(t, mname)
{
  let test = t.name + '::' + mname;
  for each (let m in t.members) {
    if (m.name.indexOf(test) != -1)
      return m;
  }
  
  throw Error("Member " + test + " not found.");
}

function getValues(n, tests)
{
  for each (let t in tests) {
    if (t[0] == n)
      return t[1];
  }
  
  throw Error("Function " + n + " not found.");
}

let type_tests = [['a',   [undefined]],
                  ['b',   [undefined, undefined]],
                  ['c',   [undefined, true]],
                  ['d',   [undefined, undefined, true, true]],
                  ['s_a', []],
                  ['s_b', [undefined]],
                  ['s_c', [true]],
                  ['s_d', [undefined, true, true]]];

let func_tests = [['f',   [undefined]],
                  ['g',   [true]],
                  ['h',   [undefined, true, true]],
                  ['i',   [true, true, true]]];

function process_type(t)
{
  if (t.name == 'C') {
    for each (let [name, values] in type_tests)
      new TestDefaults(getMember(t, name), values).run(r);
  }
}

function process_function(d, b)
{
  new TestDefaults(d, getValues(d.shortName, func_tests)).run(r);
}

function input_end()
{
  let testCount = type_tests.length + func_tests.length;

  r.verifyExpectedTestsRun(testCount);
}
