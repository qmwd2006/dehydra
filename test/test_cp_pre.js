// { 'test': 'treehydra', 'input': 'assign.cc', 'output': 'unit_test' }
include("gcc_util.js")

let counter = 2

function process_cp_pre_genericize(fn) {
  let ls = BIND_EXPR_BODY(DECL_SAVED_TREE(fn).tree_check(BIND_EXPR)).tree_check(STATEMENT_LIST)
  for (let s in iter_statement_list(ls)) {
    //print(s.tree_code())
  }
  --counter;
}

function process_tree_decl(decl) {
  if (decl_name(decl) == "foo" && decl.tree_check(FUNCTION_DECL))
    --counter
}

function input_end() {
  if (counter == 0)
    print("OK")
}
