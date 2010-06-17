// { 'test': 'treehydra', 'input': 'c_struct_prseg_reduced.c', 'output': 'unit_test', 'lang': 'c,c++' }

include('unit_test.js');

let test = new TestCase();

function process_tree_type(type) {
  let type = class_key_or_enum_as_string(type);
  test.assertEquals(type, "struct");
}

function input_end() {
  print('OK');
}
