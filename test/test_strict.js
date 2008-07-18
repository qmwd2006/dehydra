// { 'test': 'dehydra,treehydra', 'input': 'onefunc.cc', 'output': 'unit_test' }

// This is just to make sure the libs can load in strict mode.

require({ strict: true, werror: true });

function process_tree(fndecl) {
  print("OK");
}

function process_function(decl, body) {
  print("OK");
}