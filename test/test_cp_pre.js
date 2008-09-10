// { 'test': 'treehydra', 'input': 'assign.cc', 'output': 'unit_test' }
include("gcc_util.js")

let OK = false

function process_cp_pre_genericize(fn) {
  let ls = BIND_EXPR_BODY(DECL_SAVED_TREE(fn).tree_check(BIND_EXPR)).tree_check(STATEMENT_LIST)
  for (let s in iter_statement_list(ls)) {
    //print(s.tree_code())
  }
  OK = true
}

function input_end() {
  if (OK)
    print("OK")
}
