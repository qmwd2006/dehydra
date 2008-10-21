// { 'test': 'treehydra', 'input': 'lazy_atts.cc', 'output': 'unit_test' }

include('gcc_util.js');

function process_tree(fndecl) {
  let aa = TYPE_ATTRIBUTES(TREE_TYPE(fndecl));
  let ac = translate_attributes(aa);
  print("OK");
}

