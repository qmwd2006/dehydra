// { 'test': 'dehydra', 'input': 'isExtern.cc', 'output': 'unit_test' }

// Test for correct results for isExtern

include('unit_test.js');

let r = new TestResults();

function TestExtern(type, isExtern)
{
  this.name = type.name + ".isExtern";
  this.type = type;
  this.isExtern = isExtern;
}

TestExtern.prototype = new TestCase();

TestExtern.prototype.runTest = function() {
  this.assertEquals(this.type.isExtern, this.isExtern);
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

let tests = [['a', undefined],
             ['f', true],
             ['g', undefined]];

function process_type(t)
{
  if (t.name == 'A') {
    for each (let [name, value] in tests)
      new TestExtern(getMember(t, name), value).run(r);
  }
}

function process_decl(d)
{
  if (d.name == 'A::b')
    new TestExtern(d, true).run(r);
  else if (d.name == 'A::c')
    new TestExtern(d, undefined).run(r);
}

function input_end()
{
  let testCount = tests.length + 2;

  r.verifyExpectedTestsRun(testCount);
}
