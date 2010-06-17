// { 'test': 'treehydra', 'input': 'return_expr.cc', 'output': 'unit_test', 'lang': 'c,c++' }

require({ after_gcc_pass: "cfg" });

include('unit_test.js');

var test = new TestCase();

function process_tree(fn) {
  let cfg = function_decl_cfg(fn);
  for (let bb in cfg_bb_iterator(cfg)) {
    for (let isn in bb_isn_iterator(bb)) {
      if (isn.tree_code() == GIMPLE_RETURN) {
        let ret = return_expr(isn);
        test.assertTrue(ret.tree_code() == VAR_DECL);
      }
    }
  }
}

function process_cp_pre_genericize(fn) {
  walk_tree(DECL_SAVED_TREE(fn), function (t) {
    if (t.tree_code() == RETURN_EXPR) {
      let ret = return_expr(t);
      test.assertTrue(ret.tree_code() == VAR_DECL);
    }
  });
}

function input_end() {
  print("OK")
}
