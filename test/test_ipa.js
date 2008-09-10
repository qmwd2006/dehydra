// { 'test': 'treehydra', 'input': 'locks_good.cc', 'output': 'unit_test' }
require({ after_gcc_pass: isGCC42 ? 'einline' : 'einline_ipa' });

ls = ["good1", "good2"]
function process_cgraph(cgraph) {
  //only go into functions with bodies
  for (var f = cgraph_nodes; f; f = f.next)
    if (DECL_STRUCT_FUNCTION(f.decl))
      ls.splice(ls.indexOf(decl_name(f.decl)), 1)
}

function input_end() {
  if (ls.length)
    error("Did not see functions I wanted in the input file");
  else 
    print('OK')
}
