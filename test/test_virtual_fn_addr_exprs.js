// { 'test': 'treehydra', 'input': 'virtual_fn_addr_exprs.cc', 'output': 'unit_test', 'lang': 'c++0x' }

include('unit_test.js');

require({ "after_gcc_pass": "cfg"});

function ExpectedTable(table) {
  this.table = table;
  this.index = 0;
}

let cfg_expected = new ExpectedTable(
  [ "void a(this Derived* const)",
    "void a(this Base* const)",
    "void a(this Derived* const)",
    "void b(this NotDerived* const, y double)",
    "void a(this Derived* const)",
    "void a(this Base* const)",
    "void b(this NotDerived* const, y double)",
    "int c(this NotDerived* const)"
  ]
);

let pregen_expected = new ExpectedTable(
  [ "void a(this Derived* const)",
    "void a(this Base* const)",
    "void a(this Derived* const)",
    "void a(this Base* const)",
    "void a(this Derived* const)",
    "void b(this NotDerived* const, y double)",
    "void a(this Base* const)",
    "void a(this Derived* const)",
    "void a(this Derived* const)",
    "void a(this Base* const)",
    "double d(this Derived* const, x int)",
    "void c(this Derived* const)",
    "void b(this NotDerived* const, y double)"
  ]
);

let localdecls_expected = new ExpectedTable(
  [ "double d(this Derived* const, x int)",
    "void c(this Derived* const)",
    "void a(this Base* const)",
    "void a(this Derived* const)",
    "void a(this Derived* const)",
    "void a(this Base* const)"
  ]
);

let treedecl_expected = new ExpectedTable(
  [ "void a(this Derived* const)",
    "void a(this Base* const)",
    "void a(this Base* const)",
    "void a(this Derived* const)",
    "void a(this Base* const)",
    "void b(this NotDerived* const, y double)"
  ]
);

let r = new TestResults();

function FuncPtrTestCase(func, expected) {
  this.func = func;
  this.expected = expected;
}

FuncPtrTestCase.prototype = new TestCase();

FuncPtrTestCase.prototype.runTest = function () {
  let func = this.func;
  let expected = this.expected;
  let str = expected.table[expected.index++];
  if (!str)
    throw new Error("we've seen too many functions");
  this.assertEquals(pretty_func(func), str);
}

function process_tree(fn) {
  for (let d in local_decls_iterator(fn)) {
    let init = DECL_INITIAL(d);
    if (!init)
      continue;
    walk_tree(init, function (t) {
      for each (let f in resolve_virtual_fn_addr_exprs(t)) {
        new FuncPtrTestCase(f, localdecls_expected).run(r);
        r.list();
      }
    });
  }
  let cfg = function_decl_cfg(fn);
  for (let isn in cfg_isn_iterator(cfg)) {
    for each (let f in resolve_virtual_fn_addr_exprs(isn)) {
      new FuncPtrTestCase(f, cfg_expected).run(r);
      r.list();
    }
  }
}

function process_cp_pre_genericize(fn) {
  walk_tree(DECL_SAVED_TREE(fn), function (t) {
    for each (let f in resolve_virtual_fn_addr_exprs(t)) {
      new FuncPtrTestCase(f, pregen_expected).run(r);
      r.list();
    }
  });
} 

function process_tree_decl(decl) {
  let init = DECL_INITIAL(decl);
  if (!init)
    return;
  walk_tree(init, function (t) {
    for each (let f in resolve_virtual_fn_addr_exprs(t)) {
      new FuncPtrTestCase(f, treedecl_expected).run(r);
      r.list();
    }
  });
}

function pretty_func(fn) {
  return rfunc_string(rectify_function_decl(fn));
}

function input_end() {
  if (cfg_expected.index != cfg_expected.table.length)
    throw new Error("didn't see all the functions we expected walking the cfg");
  if (pregen_expected.index != pregen_expected.table.length)
    throw new Error("didn't see all the functions we expected with pre-generic code");
  if (localdecls_expected.index != localdecls_expected.table.length)
    throw new Error("didn't see all the functions we expected in local declarations");
  if (treedecl_expected.index != treedecl_expected.table.length)
    throw new Error("didn't see all the functions we expected in global declarations");
}
