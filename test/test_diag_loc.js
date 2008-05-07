// { 'test': 'dehydra', 'input': 'onefunc.cc', 'output': 'stderr_has(":1:")' }

// Error message should refer to line 1.

function process_function(decl, body) {
  print(decl.loc);
  error("test_error", decl.loc);
}