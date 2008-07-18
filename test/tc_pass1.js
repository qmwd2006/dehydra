// { 'test': 'treehydra', 'input': 'onefunc.cc', 'output': 'stderr_empty' }

// Test that pass option is accepted and we get CFG
// Should print: 0

include('treehydra.js');
require({ 'after_gcc_pass': 'cfg' });

function process_tree(fndecl) {
  let bb = fndecl.function_decl.f.cfg.x_entry_block_ptr;
  print(bb.index);
}