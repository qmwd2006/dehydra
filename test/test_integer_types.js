// { 'test': 'treehydra', 'input': 'locks_good.cc', 'output': 'unit_test' }
var OK = false

function process_tree() {
  OK = sys.treehydra.gcc.integer_types[itk_char.value].tree_code() == INTEGER_TYPE
}

function input_end() {
  if (OK)
    print("OK")
}
