include ("treehydra.js")

function process_tree(function_decl) {
  print (IDENTIFIER_POINTER (DECL_NAME (function_decl)) + "()")
  var b = DECL_SAVED_TREE (function_decl)
  sanity_check (b)
  pretty_walk (b)
}

function user_print(value) {
  print("user_print:"+value)
}
