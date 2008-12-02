// { 'test': 'treehydra', 'input': 'onefunc.cc', 'output': 'stderr_has("Cannot set gcc_pass_after")' }

// Test that pass option is not accepted in process_tree.

include('treehydra.js');

function process_tree(fndecl) {
  require({ 'after_gcc_pass': 'cfg' });
}