include ("treehydra.js")

function is_GCFinalizable (record_type) {
  function f (record_type) {
    return IDENTIFIER_POINTER (DECL_NAME (TYPE_NAME (record_type))) == "GCFinalizable"
  }
  return walk_hierarchy (f, record_type)
}

function is_finalizer_safe (function_decl) {
  return get_user_attribute (DECL_ATTRIBUTES (function_decl)) == "finalizer_safe"
}

function process_tree(function_decl) {
  if (!is_finalizer_safe (function_decl)) {
    var context = DECL_CONTEXT (function_decl)
    if (!context 
        || TREE_CODE (context) != RECORD_TYPE
        || !is_GCFinalizable (context)) return
  }
    
  var body = DECL_SAVED_TREE (function_decl)

  var uniqueness_map = new Map()

  function walker(t, stack) {
    var code = TREE_CODE (t)
    switch (code) {
    case CALL_EXPR:
      var fn = CALL_EXPR_FN (t)
      if (TREE_CODE (fn) == ADDR_EXPR)
        fn = TREE_OPERAND (fn, 0)
      if (!is_finalizer_safe (fn))
          error ("Calling a function that hasn't been annotated as finalizer_safe:" +decl_name(fn))
      break;
    default:
    }
  }

  walk_tree (body, walker, uniqueness_map)
}

