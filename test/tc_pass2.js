// Test that pass option is not accepted in process_tree.

include('treehydra.js');

function process_tree(fndecl) {
  require({ 'after_gcc_pass': 'cfg' });
}