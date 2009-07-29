// { 'test': 'treehydra', 'input': 'assign.cc', 'output': 'unit_test' }
require({treehydra_debug:true})

function process_tree(d) {
  if (d.hasOwnProperty("SEQUENCE_N") &&
      d.base.hasOwnProperty("_struct_name"))
    print("OK")
}
