// Stuff stolen from dmandelin's Treehydra libs
//
// TODO replace with proper imports of the Treehydra libs

// Copied from treehydra-analysis
function flatten_chain(chain_head) {
  for (let o = chain_head; o; o = TREE_CHAIN(o)) {
    yield o;
  }
}

function function_decl_cfg(tree) {
  return tree.function_decl.f.cfg;
}

let UNKNOWN_LOCATION = undefined;

// Return the source code location of the argument, which must be a tree.
// The result is the direct translation of a GCC location_t, which is 
// an int.
function location_of(t) {
  if (TREE_CODE (t) == PARM_DECL && DECL_CONTEXT (t))
    t = DECL_CONTEXT (t);
  else if (TYPE_P (t))
    t = TYPE_MAIN_DECL (t);
  // TODO disabling this for now
  //else if (TREE_CODE (t) == OVERLOAD)
  //  t = OVL_FUNCTION (t);

  if (DECL_P(t))
    return DECL_SOURCE_LOCATION (t);
  else if (EXPR_P(t))
    return EXPR_LOCATION(t);
  else if (GIMPLE_STMT_P(t))
    return GIMPLE_STMT_LOCUS(t);
  else
    return UNKNOWN_LOCATION;
}

function DECL_SOURCE_LOCATION(node) {
  return node.decl_minimal.locus;
}

function GIMPLE_STMT_LOCUS(tree) {
  return tree.gstmt.locus;
}

function EXPR_LOCATION(tree) {
  return tree.exp.locus;
}

function type_string(type) {
  // Fun special case -- handled like this in g++.
  // disabled for now -- test not impl
  if (false && TYPE_PTRMEMFUNC_P(type)) {
    return ptrmemfunc_type_string(type);
  }

  let code = TREE_CODE(type);
  if (code == INTEGER_TYPE || code == REAL_TYPE || code == BOOLEAN_TYPE ||
      code == RECORD_TYPE || code == ENUMERAL_TYPE || code == UNION_TYPE) {
    if (TYPE_NAME(type) == undefined) {
      // Seems to happen for pointer to member types
      return "UNKNOWN";
    }
    return IDENTIFIER_POINTER(DECL_NAME(TYPE_NAME(type)));
  } else if (code == VOID_TYPE) {
    return 'void';
  } else if (code == POINTER_TYPE) {
    return type_string(TREE_TYPE(type)) + '*';
  } else if (code == ARRAY_TYPE) {
    return type_string(TREE_TYPE(type)) + '[]';
  } else if (code == REFERENCE_TYPE) {
    return type_string(TREE_TYPE(type)) + '&';
  } else if (code == FUNCTION_TYPE) {
    return type_string(TREE_TYPE(type)) + ' (*)(' +
      [ type_string(TREE_VALUE(t)) for (t in flatten_chain(TYPE_ARG_TYPES(type))) ].join(', ') + ')';
      
  } else if (code == METHOD_TYPE) {
    return type_string(TREE_TYPE(type)) + ' (*::)(' +
      [ type_string(TREE_VALUE(t)) for (t in flatten_chain(TYPE_ARG_TYPES(type))) ].join(', ') + ')';
  }

  print(TREE_CODE(type).name);
  do_dehydra_dump(type.type, 1, 2);
  throw new Error("unhandled type type");
}
