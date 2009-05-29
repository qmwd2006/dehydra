// { 'test': 'dehydra', 'input': 'template_member.cc', 'output': 'unit_test' }

// Test that templated function declarations are annotated usefully

include('unit_test.js');

let r = new TestResults();

function TemplateTestCase(type) {
  this.type = type;
}
TemplateTestCase.prototype = new TestCase();

TemplateTestCase.prototype.runTest = function() {
  let templatemember = this.type.members[0];
  this.assertEquals(templatemember.template[0].name, "B");
  this.assertEquals(templatemember.template[0].type.name, "B");
  this.assertTrue(templatemember.template[0].type.isTypename);

  this.assertTrue(templatemember.type.parameters[0].isTypename);
  this.assertEquals(templatemember.template[0].type,
                    templatemember.type.parameters[0]);
  
  templatemember = this.type.members[1];
  this.assertEquals(templatemember.template[0].name, "I");
  this.assertEquals(templatemember.template[0].type.name, "int");
}

function process_type(type) {
  new TemplateTestCase(type).run(r);
}

function input_end() {
  r.verifyExpectedTestsRun(1)
}
