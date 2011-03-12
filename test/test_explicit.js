// { 'test': 'dehydra', 'input': 'explicit.cc', 'output': 'unit_test' }

// Test for correct results for hasDefault

include('unit_test.js');

let r = new TestResults();

function TestDefaults(member, value)
{
  this.member = member;
  this.value = value;
}

TestDefaults.prototype = new TestCase();

TestDefaults.prototype.description = function() {
  return "isExplicit " + this.member.name + ": "
}

TestDefaults.prototype.runTest = function() {
  this.assertEquals(!!this.member.isExplicit, this.value);
};

let tests={'C::C()': false,
           'C::C(int)': false,
           'C::C(char)': true,
           'C::C(double, int)': false,
           'C::C(double, char)': true,
           'C::C(int, char)': false,
           'C::C(int, float)': true};

if (/^4.3/.exec(sys.gcc_version)) {
  delete tests['C::C()']
  delete tests['C::C(int, char)']
}

function getMember(t, name)
{
  for each (let m in t.members) {
    if (m.name == name)
      return m;
  }
  
  throw Error("Member " + name + " not found.");
}

function process_type(t)
{
  if (t.name == 'C') {
    for (let name in tests)
      new TestDefaults(getMember(t, name), tests[name]).run(r);
  }
}

function process_function(d, b)
{
  new TestDefaults(d, getValues(d.shortName, func_tests)).run(r);
}

function input_end()
{
  let testCount = 0;
  for (name in tests)
    testCount++;
  r.verifyExpectedTestsRun(testCount);
}
