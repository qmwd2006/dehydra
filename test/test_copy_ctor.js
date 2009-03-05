// { 'test': 'dehydra', 'input': 'copy_ctor.cc', 'output': 'unit_test' }
let failed = false
let run = 0
const spec = [[/^t::t/, 1], [/^f/, 1, true], [/^g/, 1] ]

function find_ctor_calls(loc, v) {
  if (v.isFcall) {
    warning("# Fcall '" + v.name + "' with " +
            v.arguments.length + " args" /*+ " memberOf" + v.memberOf*/);
    for each (var arg in v.arguments) {
      if (arg.isConstructor)
        warning("Constructor '" + arg.name + "' used in"
                + "argument to '" + v.name + "'");
      find_ctor_calls(loc, arg);
    }
    for each (let [regexp, len, arg_constructor] in spec) {
      if (regexp.exec(v.name)) {
        run++;
        if (v.arguments.length != len) {
          error(v.name + " has the wrong number of arguments", loc);
          failed = true
        }
        if (arg_constructor && !v.arguments[0].isConstructor) {
          error(v.name + " expects a copy constructor as argument", loc)
          failed = true
        }
      }
    }
  }
  if (v.assign) {
    for each (var v2 in v.assign)
    find_ctor_calls(loc, v2);
  }
}

function process_function(decl, body) {
  for each (var stmts in body)
  for each (var stmt in stmts.statements)
  find_ctor_calls(stmts.loc, stmt);
}

function input_end() {
  if (!failed && run)
    print("OK")
}
