// { 'test': 'treehydra', 'input': 'locks_good.cc', 'output': 'unit_test' }
require({ after_gcc_pass: 'einline' });

ls = ["good1", "good2"]
function process_tree() {
  
  //only go into functions with bodies
  for (var f = sys.treehydra.gcc.cgraph_nodes; f; f = f.next)
    if (DECL_STRUCT_FUNCTION(f.decl))
      ls.splice(ls.indexOf(decl_name(f.decl)), 1)
}

function input_end() {
  if (ls.length)
    error("Did not see functions I wanted in the input file");
  else 
    print('OK')
}
