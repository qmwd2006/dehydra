// { 'test': 'dehydra', 'input': 'members.cc', 'output': 'unit_test', 'lang': 'c++' }

include('unit_test.js');

let classes = {'Outer': null,
	       'Outer::Inner': 'Outer',
	       'Outer::Inner::MostInner': 'Outer::Inner'};

function MemberTestCase(type, inner)
{
  this.type = type;
  this.inner = inner;
  this.name = this.type.name;
}
MemberTestCase.prototype = new TestCase();
MemberTestCase.prototype.runTest = function()
{
  print("Testing type: " + this.type.name);
  if (!this.inner)
    this.assertTrue(this.type.memberOf === undefined);
  else
    this.assertEquals(this.type.memberOf.name, this.inner);

  for each (let member in this.type.members)
    this.assertTrue(member.memberOf === this.type);
};

let r = new TestResults();

function process_type(type)
{
  if (classes.hasOwnProperty(type.name))
    new MemberTestCase(type, classes[type.name]).run(r);
}

function input_end()
{
  r.list();
}
