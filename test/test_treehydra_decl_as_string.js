// { 'test': 'treehydra', 'input': 'assign.cc', 'output': 'unit_test' }

function process_tree(d) {
  if (d._decl_name.str.indexOf("foo()") != -1)
    print("OK")
}
