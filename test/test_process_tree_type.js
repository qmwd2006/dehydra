// { 'test': 'treehydra', 'input': 'assign.cc', 'output': 'unit_test', 'lang': 'c++' }
function process_tree_type(type) {
  // check if we hit class "book"
  if (decl_name(TYPE_NAME(type)) == "book")
    print("OK")
}
