// { 'test': 'dehydra', 'input': 'location.cc', 'output': 'unit_test' }
include('util.js');
function process_function(decl, body) {
  for each (var stmts in body) {
    for each (var stmt in stmts.statements) {
      if (stmt.isFcall && stmt.name == "f()" && stmts.loc.line == 4) {
        print('OK');
      }
    }
  }
}
