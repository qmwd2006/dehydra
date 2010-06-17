// { 'test': 'treehydra', 'input': 'stringlit.cc', 'output': 'unit_test' }

var OK = true;

var expected = [
  'a-test',
  'b-te\0st',
  'c-test',
  'd-te\0st'
];

function process_cp_pre_genericize(fndecl) {
  function checker (t) {
    if (TREE_CODE (t) != STRING_CST)
      return true;
    var s = TREE_STRING_POINTER(t);
    //print (s.replace(/\0/g, "\\0"));
    if (s != expected.shift())
      OK = false;
  };
  walk_tree (DECL_SAVED_TREE (fndecl), checker);
}

function input_end() {
  if (OK && expected.length == 0)
    print("OK");
}
