include ("map.js")
include ("platform.js")

function unhandledLazyProperty (prop) {
    /* Special case: the interpreter will look for this property when
     * iterating. It doesn't appear in the prototype chain either, but
     * apparently that's fine--the interpreter will use default iterator
     * behavior, but only if we return true here. */
  if (prop != "__iterator__")
    throw new Error("No " + prop + " in this lazy object")
}

function EnumValue (name, value) {
  this.name = name
  this.value = value
}

EnumValue.prototype.toString = function () {
  return this.name
}

if (TREE_CODE) {
  include('enums.js')
  include('useful_arrays.js')
}

// Class that the lazyness builds itself around of
function GCCNode () {
}

GCCNode.prototype.toString = function () {
  if (!this._struct_name)
    if (this.common) {
      let ls = ["{tree_code:" + this.tree_code()]
      let name = this.toCString()
      if (name)
        ls.push ('toCString:"' + name + '"')
      let operands = this.operands
      if (operands)
        ls.push ("operands[" + operands.length + "]")
      ls.push ("}")
      return ls.join(" ")
    }
  return "I am a " + this._struct_name
}

GCCNode.prototype.tree_code = isGCCApple ?
  function () { return this.common.code; } :
  function () { return this.base.code; }

/* Convienient thing along the lines of GENERIC_TREE_OPERAND */
GCCNode.prototype.operands = function () {
  if (GIMPLE_STMT_P (this))
    return this.gstmt.operands
  else if (IS_EXPR_CODE_CLASS (TREE_CODE_CLASS (this.tree_code())))
  {
    return this.exp.operands
  } else {
    throw new Error("no operands in this object");
  }
}

GCCNode.prototype.toCString = function () {
  if (DECL_P (this))
    return decl_name(this)
  throw new Error("not implemented; only DECLs are supported");
}

function TREE_CODE (tree) {
  return tree.tree_code()
}

function TREE_CODE_CLASS (code) {
  return tree_code_type[code.value]
}

function TREE_CODE_LENGTH (code) {
  return tree_code_length[code.value]
}

function IS_EXPR_CODE_CLASS (code_class) {
  return code_class.value >= tcc_reference.value 
    && code_class.value <= tcc_expression.value
}

var IS_GIMPLE_STMT_CODE_CLASS = isGCCApple ?
  function (code_class) { return false; } :
  function (code_class) { return code_class == tcc_gimple_stmt; }

// comparable to CHECK_foo..except here foo is the second parameter
function TREE_CHECK (tree_node, expected_code) {
  const code = TREE_CODE (tree_node)
  if (code != expected_code) {
    const a = TREE_CHECK.arguments
    for (var i = 2; i < a.length; i++)
      if (a[i] == code)
        return tree_node
    throw Error("Expected " + expected_code + ", got " + TREE_CODE (tree_node))
  }
  return tree_node
}

function TREE_CLASS_CHECK (node, expected_code) {
  const tree_code_class = TREE_CODE (node)
  if (TREE_CODE_CLASS (tree_code_class) != expected_code)
    throw Error("Expected " + expected_code + ", got " + TREE_CODE_CLASS (tree_code_class))
  return node
}

function STATEMENT_LIST_HEAD(node) {
  return node.stmt_list.head
}

function VL_EXP_CLASS_P (node) {
  return TREE_CODE_CLASS (TREE_CODE (node)) == tcc_vl_exp
}

function TREE_INT_CST (node) {
  return TREE_CHECK (node, INTEGER_CST).int_cst.int_cst
}

function TREE_INT_CST_LOW (node) {
  return TREE_INT_CST (node).low
}

function VL_EXP_OPERAND_LENGTH (node) {
  return (TREE_INT_CST_LOW ((TREE_CLASS_CHECK (node, tcc_vl_exp).exp.operands[0])))
}

function TREE_OPERAND_LENGTH (node)
{
  if (VL_EXP_CLASS_P (node))
    return VL_EXP_OPERAND_LENGTH (node);
  else
    return TREE_CODE_LENGTH (TREE_CODE (node));
}

var GIMPLE_STMT_P = isGCCApple ?
  function (NODE) { return false; } :
  function (NODE) {
    return TREE_CODE_CLASS (TREE_CODE ((NODE))) == tcc_gimple_stmt
  }

function TREE_OPERAND (node, i) {
  return node.exp.operands[i]
}

function GIMPLE_STMT_OPERAND (node, i) {
  return node.gstmt.operands[i]
}

function GENERIC_TREE_OPERAND (node, i)
{
  if (GIMPLE_STMT_P (node))
    return GIMPLE_STMT_OPERAND (node, i);
  return TREE_OPERAND (node, i);
}

function BIND_EXPR_VARS (node) {
  return TREE_OPERAND (TREE_CHECK (node, BIND_EXPR), 0)
}

function BIND_EXPR_BODY (node) {
  return TREE_OPERAND (TREE_CHECK (node, BIND_EXPR), 1)
}

function CALL_EXPR_FN (node) {
  return TREE_OPERAND (TREE_CHECK (node, CALL_EXPR), 1)
}

function CALL_EXPR_ARG(node, i) {
  return TREE_OPERAND (TREE_CHECK (node, CALL_EXPR), i + 3)
}

function GIMPLE_TUPLE_P (node) {
  return GIMPLE_STMT_P (node) || TREE_CODE (node) == PHI_NODE
}

function TYPE_BINFO (node) {
  return TREE_CHECK (node, RECORD_TYPE, UNION_TYPE, QUAL_UNION_TYPE).type.binfo
}

// not sure if this will work same way in gcc 4.3
function TREE_CHAIN (node) {
  if (GIMPLE_TUPLE_P (node))
    throw new Error ("TREE_CHAIN refuses to accept GIMPLE_TUPLE_P() stuff")
  return node.common.chain
}

function TREE_VALUE (node) {
  return TREE_CHECK (node, TREE_LIST).list.value
}

function TREE_PURPOSE (node) {
  return node.list.purpose
}

function TREE_STRING_POINTER (node) {
  return node.string.str
}

function DECL_ATTRIBUTES (node) {
  return node.decl_common.attributes
}

function DECL_P (node) {
  return TREE_CODE_CLASS (TREE_CODE (node)) == tcc_declaration
}

function DECL_INITIAL (node) {
  // can't do DECL_COMMON_CHECK
  return node.decl_common.initial
}

function DECL_SIZE (node) {
  // can't do DECL_COMMON_CHECK
  return node.decl_common.size
}

function DECL_SIZE_UNIT (node) {
  // can't do DECL_COMMON_CHECK
  return node.decl_common.size_unit
}

function DECL_NAME (node) {
  return node.decl_minimal.name
}

function DECL_UID (node) {
  return node.decl_minimal.uid
}

function DECL_CONTEXT (node) {
  return node.decl_minimal.context
}

function DECL_SAVED_TREE (node) {
  return TREE_CHECK (node, FUNCTION_DECL).decl_non_common.saved_tree
}

function IDENTIFIER_POINTER (node) {
  return node.identifier.id.str
}

function TREE_VEC_LENGTH (node) {
  return TREE_CHECK (node, TREE_VEC).vec.length
}

function TREE_VEC_ELT (node, i) {
  return TREE_CHECK (node, TREE_VEC).vec.a[i]
}

function CONSTRUCTOR_ELTS (node) {
  return TREE_CHECK (node, CONSTRUCTOR).constructor.elts
}

function TREE_TYPE (node) {
  if (GIMPLE_TUPLE_P (node))
    throw new Error ("Don't be passing GIMPLE stuff to TREE_TYPE")
  return node.common.type
}

function TYPE_NAME (node) {
  return node.type.name
}

function TYPE_P (node) {
  return TREE_CODE_CLASS (TREE_CODE (node)) == tcc_type
}

function BINFO_BASE_BINFOS (node) {
  return node.binfo.base_binfos
}

function TYPE_LANG_SPECIFIC (node) {
  return node.type.lang_specific
}

function LANG_TYPE_CLASS_CHECK (node) {
  var lang_type = TYPE_LANG_SPECIFIC (node)
  if (!lang_type.u.h.is_lang_type_class)
    throw new Error ("Not a class!")
  return lang_type.u.c
}

function CLASSTYPE_TEMPLATE_INFO (node) {
  return LANG_TYPE_CLASS_CHECK(node).template_info
}

const TI_TEMPLATE = TREE_PURPOSE

function CLASSTYPE_TI_TEMPLATE (node) {
  return TI_TEMPLATE (CLASSTYPE_TEMPLATE_INFO (node))
}

function DECL_LANG_SPECIFIC (node) {
  return node.decl_common.lang_specific
}

function DECL_TEMPLATE_INFO (node) {
  return DECL_LANG_SPECIFIC (node).decl_flags.u.template_info
}

function TYPE_TEMPLATE_INFO(node) {
  if (TREE_CODE (node) == ENUMERAL_TYPE )
    return ENUM_TEMPLATE_INFO (node) 
  else if (TREE_CODE (node) == BOUND_TEMPLATE_TEMPLATE_PARM)
    return TEMPLATE_TEMPLATE_PARM_TEMPLATE_INFO (node) 
  else if (TYPE_LANG_SPECIFIC (node))
    return CLASSTYPE_TEMPLATE_INFO (node)
}

const TI_ARGS = TREE_VALUE

function TMPL_ARGS_HAVE_MULTIPLE_LEVELS (node) {
  if (!node) return

  const elt = TREE_VEC_ELT (node, 0) 
  return elt && TREE_CODE (elt) == TREE_VEC_ELT
}

var BINFO_TYPE = TREE_TYPE

/* This is so much simpler than the C version 
 because it merely returns the vector array and lets
the client for each it*/
// undefined is used for empty vectors, so support it nicely here.
var VEC_iterate = isGCCApple ?
  function (vec_node) { return vec_node ? vec_node.vec : []; } :
  function (vec_node) { return vec_node ? vec_node.base.vec : []; }

function tree_stmt_iterator (ptr, container) {
  this.ptr = ptr
  this.container = container
}

function tsi_start (t) {
  return new tree_stmt_iterator (STATEMENT_LIST_HEAD (t), t);
}

// tsi_end_p
tree_stmt_iterator.prototype.end = function () {
  return !this.ptr
}

// tsi_stmt_ptr (tree_stmt_iterator i)
tree_stmt_iterator.prototype.stmt = function () {
  return this.ptr.stmt
}

//tsi_next (tree_stmt_iterator *i)
tree_stmt_iterator.prototype.next = function () {
  this.ptr = this.ptr.next;
}

// func should "return" via throw
// *walk_subtrees=0 is the same as returning false from func
function walk_tree (t, func, guard, stack) {
  function WALK_SUBTREE (t) {
    walk_tree (t, func, guard, stack)
  }
  if (!t || guard.has (t)) {
    return
  }
  guard.put (t)
  if (!stack)
    stack = [];
  var walk_subtrees = func (t, stack)
  if (walk_subtrees === false)
    return
  code = t.tree_code ()
  stack.push (t)
  switch (code) {
  case TREE_LIST:
    WALK_SUBTREE (TREE_VALUE (t));
    WALK_SUBTREE (TREE_CHAIN (t));
    break;
  case TREE_VEC:
    {
      var len = TREE_VEC_LENGTH (t);
      while (len--) {
	WALK_SUBTREE (TREE_VEC_ELT (t, len));
      }
      break;
    }
  case CONSTRUCTOR: 
    for each (var ce in VEC_iterate (CONSTRUCTOR_ELTS (t))) {
      WALK_SUBTREE (ce.value);
    }
    break;
  case BIND_EXPR:
    {
      for (var decl = BIND_EXPR_VARS (t); decl; decl = TREE_CHAIN (decl))
      {
	/* Walk the DECL_INITIAL and DECL_SIZE.  We don't want to walk
	       into declarations that are just mentioned, rather than
	       declared; they don't really belong to this part of the tree.
	       And, we can see cycles: the initializer for a declaration
	       can refer to the declaration itself.  */
	WALK_SUBTREE (DECL_INITIAL (decl));
	WALK_SUBTREE (DECL_SIZE (decl));
	WALK_SUBTREE (DECL_SIZE_UNIT (decl));
      }
      WALK_SUBTREE (BIND_EXPR_BODY (t));
      break;
    }
  case STATEMENT_LIST:
    for (var i = tsi_start (t); !i.end (i); i.next()) {
      walk_tree (i.stmt (), func, guard, stack);
    }
    break;
  default:
    if (IS_EXPR_CODE_CLASS (TREE_CODE_CLASS (code))
        || IS_GIMPLE_STMT_CODE_CLASS (TREE_CODE_CLASS (code)))
    {
      for each (let o in t.operands()) {
        walk_tree (o, func, guard, stack)
      }
    }
    break;
  }
  stack.pop ()
}

/* prints a nice name for decls */
function decl_name (decl) {
  var str = ""
  var context = DECL_CONTEXT (decl)

  if (context) {
    var code = TREE_CODE (context) 
    if (TYPE_P (context)) {
      context = TYPE_NAME (context)
    }
    if (code != FUNCTION_DECL) {
      str = decl_name (context) + "::"
    }
  }
  var name = DECL_NAME (decl)
  str += name ? IDENTIFIER_POINTER (name) : ("<anonymous" + DECL_UID (decl) + ">")
  return str
}

function pretty_walk (body, limit) {
  var counter = 0;
  function code_printer (t, depth) {
    if (++counter == limit) throw "done"

    var str = "";
    for (var i = 0; i< depth.length;i++)
      str += " "
    var code = TREE_CODE (t)
    str += code
    
    if (DECL_P (t))
      str += " " + decl_name (t);

    print (str)
  }
  try {
    walk_tree (body, code_printer, new Map())
  } catch (e if e == "done") {
  }

}
function C_walk_tree_array() {
  return C_walk_tree().split("\n")
}

/* Gets the C printout of the current function body
 and ensures that the js traversal matches */
function sanity_check (body) {
  var c_walkls = C_walk_tree_array ()
  var current_tree_node = 0;
  
  function checker (t, stack) {
    function bail_info () {
      // this causes C_walk_tree() to happen again, so lazy nodes and sequence_ns get along
      print ("C Walk:\n" + C_walk_tree_array().slice (0, current_tree_node+1).join("\n"))
      print ("JS Walk:")
      pretty_walk (b, current_tree_node + 1)
      if (stack.length) {
        var top = stack.pop()
        print ("Within " + TREE_CODE(top) + " seq:" + top.SEQUENCE_N)
        print ("Conflicted node seq:" + t.SEQUENCE_N)
      }
    }
    var code = TREE_CODE (t)
    var strname = tree_code_name[code.value]
    if (current_tree_node == c_walkls.length) {
      bail_info()
      throw new Error ("walk_tree made me walk more nodes than the C version. on " + code)
    }
    if (c_walkls[current_tree_node].indexOf(strname) != 0) {
      bail_info();
      throw new Error ("walk_tree in C differs from JS, at " + current_tree_node + " expected " 
                   + c_walkls[current_tree_node] + " instead of "
                   + strname)
    }
    ++current_tree_node;
  }
  walk_tree (body, checker, new Map())
  if (current_tree_node != c_walkls.length) {
    throw Error("walk_tree didn't visit enough nodes, " + current_tree_node 
               + " out of " + c_walkls.length + "visited")
  }
}

function fn_decl_body (function_decl) {
  body_chain = DECL_SAVED_TREE (function_decl)
  if (body_chain && TREE_CODE (body_chain) == BIND_EXPR)
    return BIND_EXPR_BODY (body_chain)
  return body_chain
}

// runs f against record_type and all base classes of it
function walk_hierarchy (f, record_type) {
  var ret = f (record_type)
  if (ret) return ret;

  var binfo = TYPE_BINFO (record_type)
  for each (var base in VEC_iterate(BINFO_BASE_BINFOS (binfo))) {
    var ret = walk_hierarchy (f, BINFO_TYPE (base))
    if (ret)
      return ret
  }
  return undefined;
}

function get_user_attribute (attributes) {
  for (var a = attributes;
       a;
       a = TREE_CHAIN (a)) {
    const name = IDENTIFIER_POINTER (TREE_PURPOSE (a));
    if (name != "user")
      continue
    var args = TREE_VALUE (a);
    for (; args; args = TREE_CHAIN (args)) {
      return TREE_STRING_POINTER(TREE_VALUE(args))
    }
  }
  return undefined;
}
