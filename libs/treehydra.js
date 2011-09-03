/*
 * Dehydra and Treehydra scriptable static analysis tools
 * Copyright (C) 2007-2010 The Mozilla Foundation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

include ("xhydra.js")
include ("map.js")
include ("platform.js")
include ("gcc_compat.js")
include ("gcc_util.js")
include ("gcc_print.js")

function unhandledLazyProperty (prop) {
    /* Special case: the interpreter will look for this property when
     * iterating. It doesn't appear in the prototype chain either, but
     * apparently that's fine--the interpreter will use default iterator
     * behavior, but only if we return true here. */
  if (prop != "__iterator__")
    throw new Error("No " + prop + " in this lazy object")
}

EnumValue.prototype.toString = function () {
  return this.name
}

if (TREE_CODE) {
  include('enums.js')
  include('useful_arrays.js')
}

if (isGCC42) {
  // try to increase the chances of linux scripts working on osx out of the box
  this.GIMPLE_MODIFY_STMT = MODIFY_EXPR;
  this.POINTER_PLUS_EXPR = PLUS_EXPR;
  this.FIXED_POINT_TYPE = {};
 }

if (!isUsingGCCTuples) {
  this.gimple_code = TREE_CODE;
  this.GIMPLE_CALL = CALL_EXPR;
  this.GIMPLE_ASSIGN = GIMPLE_MODIFY_STMT;
  this.GIMPLE_COND = COND_EXPR;
  this.GIMPLE_SWITCH = SWITCH_EXPR;
  this.GIMPLE_RETURN = RETURN_EXPR;
  this.GIMPLE_LABEL = LABEL_EXPR;
  this.gimple_call_fn = CALL_EXPR_FN;
  this.gimple_call_fndecl = call_function_decl;
  this.gimple_call_function_name = call_function_name;
  this.gimple_call_args = call_expr_args;
  this.gimple_call_arg = call_arg;
  this.gimple_call_arg_iterator = call_arg_iterator;
  this.gs_display = isn_display;
  this.gimple_op = function(isn, i) { return isn.operands()[i]; }
} else {
  this.FILTER_EXPR = undefined;
  this.EXC_PTR_EXPR = undefined;
  this.GIMPLE_MODIFY_STMT = undefined;
}

// Class that the lazyness builds itself around of
function GCCNode () {
}

GCCNode.prototype.toString = function () {
  if (!this._struct_name) {
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
  } else if (this._struct_name == 'location_s') {
    return this.file + ':' + this.line
  }
  return "I am a " + this._struct_name
}

GCCNode.prototype.tree_code = isGCC42 ?
  function () { return this.common.code; } :
  function () { return GIMPLE_TUPLE_P(this) ? this.gsbase.code : this.base.code; }

/* Convienient thing along the lines of GENERIC_TREE_OPERAND */
GCCNode.prototype.operands = function () {
  if (GIMPLE_TUPLE_P (this))
    return this.gimple_ops;
  else if (GIMPLE_STMT_P (this))
    return this.gstmt.operands;
  else if (IS_EXPR_CODE_CLASS (TREE_CODE_CLASS (this.tree_code())))
    return this.exp.operands;
  else
    throw new Error("no operands in this object");
}

GCCNode.prototype.toSource = function () {
  throw new Error("Unevaling treehydra objects is unsupported");
}

GCCNode.prototype.toCString = function () {
  if (DECL_P (this))
    return decl_name(this)
  throw new Error("not implemented; only DECLs are supported");
}

GCCNode.prototype.tree_check = function (expected_code) {
  return TREE_CHECK(this, expected_code)
}

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
  if (!t)
    return
  else if (guard) {
    if (guard.has (t))
      return
    guard.put (t)
  }
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
        stack.push(decl);
	/* Walk the DECL_INITIAL and DECL_SIZE.  We don't want to walk
	       into declarations that are just mentioned, rather than
	       declared; they don't really belong to this part of the tree.
	       And, we can see cycles: the initializer for a declaration
	       can refer to the declaration itself.  */
	WALK_SUBTREE (DECL_INITIAL (decl));
	WALK_SUBTREE (DECL_SIZE (decl));
	WALK_SUBTREE (DECL_SIZE_UNIT (decl));
        stack.pop();
      }
      WALK_SUBTREE (BIND_EXPR_BODY (t));
      break;
    }
  case STATEMENT_LIST:
    for (var i = tsi_start (t); !i.end (i); i.next()) {
      walk_tree (i.stmt (), func, guard, stack);
    }
    break;
  case TEMPLATE_PARM_INDEX:
  case PTRMEM_CST:
  case USING_DECL:
  case OVERLOAD:
    /* expressions with no operands */
    break;
  default:
    if (IS_EXPR_CODE_CLASS (TREE_CODE_CLASS (code))
        || IS_GIMPLE_STMT_CODE_CLASS (TREE_CODE_CLASS (code))) {
      for each (let o in t.operands())
        walk_tree (o, func, guard, stack)
    } else if (GIMPLE_TUPLE_P (t)) {
      for (let i = 0; i < gimple_num_ops(t); ++i)
        walk_tree(gimple_op(t, i), func, guard, stack);
    }
  }
  stack.pop ()
}

/* prints a nice name for decls */
function decl_name (decl, no_context) {
  var str = "";
  var context = no_context ? null : DECL_CONTEXT (decl)

  if (context) {
    var code = TREE_CODE (context) 
    if (TYPE_P (context)) {
      context = TYPE_NAME (context)
    }
    if (code != FUNCTION_DECL && code != TRANSLATION_UNIT_DECL) {
      str = (context ? decl_name (context) : '<anonymous?>') + "::"
    }
  }
  var name = DECL_NAME (decl)
  if (name) {
    str += IDENTIFIER_POINTER (name)
  } else {
    switch (TREE_CODE (decl)) {
    case CONST_DECL: 
      str += 'C'
      break;
    case RESULT_DECL:
      str += 'R'
      break;
    default:
      str += 'D'
    }
    str += "_" + DECL_UID (decl)
  }
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
    if (code == OBJ_TYPE_REF) {
      t = resolve_virtual_fun_from_obj_type_ref(t)
    }
    if (DECL_P (t))
      str += " " + decl_name (t);
    else {
      switch (code) {
      case STRING_CST: 
        str += " \"" + TREE_STRING_POINTER(t) + "\"";
        break;
      case INTEGER_CST:
        str += " " + TREE_INT_CST_LOW(t)
        break;
      }
    }
    print (str)
    if (code == CONSTRUCTOR) {
      depth.push(t)
      for each (var ce in VEC_iterate (CONSTRUCTOR_ELTS (t))) {
        walk_tree (ce.index, code_printer, null, depth); // pretty_walk addition
        walk_tree (ce.value, code_printer, null, depth)
      }
      depth.pop()
      //indicate we don't need to visit subtrees
      return false;
    }
  }
  try {
    walk_tree (body, code_printer)
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
