// { 'test': 'treehydra', 'input': 'onefunc.cc', 'output': 'stderr_has(":1:")', 'lang': 'c,c++' }

// Error message should refer to line 1.

function process_tree(fndecl) {
  error("test_error", location_of(fndecl));
}
