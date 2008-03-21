include ("treehydra.js")

function process_tree(function_decl) {
  print (IDENTIFIER_POINTER (DECL_NAME (function_decl)) + "()")
  var b = fn_decl_body (function_decl)
  sanity_check (b)
  pretty_walk (b)
}

function user_print(value) {
  print("user_print:"+value)
}
