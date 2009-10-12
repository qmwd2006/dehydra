// { 'test': 'treehydra', 'input': 'longevity.cc', 'output': 'unit_test', 'lang': 'c++' }

function process_tree (function_decl) {
  var b = DECL_SAVED_TREE (function_decl)
  sanity_check (b)
}

function input_end() {
  print("OK")
}
