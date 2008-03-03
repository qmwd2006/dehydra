function EnumValue (name, value) {
  this.name = name
  this.value = value
}

EnumValue.prototype.toString = function () {
  return this.name
}

eval (read_file ("../enums.js"))
eval (read_file ("../useful_arrays.js"))

function TREE_CODE (tree) {
  return tree.base.code
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

function IS_GIMPLE_STMT_CODE_CLASS (code_class) {
  return code_class == tcc_gimple_stmt
}

// comparable to CHECK_foo..except here foo is the second parameter
function TREE_CHECK (tree_node, expected_code) {
  if (TREE_CODE (tree_node) != expected_code)
    throw Error("Expected " + expected_code + ", got " + TREE_CODE (tree_node))
  return tree_node
}

function TREE_CLASS_CHECK (node, expected_code) {
  const tree_code_class = TREE_CODE (node)
  if (TREE_CODE_CLASS (tree_code_class) != expected_code)
    throw Error("Expected " + expected_code + ", got " + TREE_CODE_CLASS (tree_code_class))
  return node
}

function STATEMENT_LIST_HEAD(NODE) {
  return TREE_CHECK (NODE, STATEMENT_LIST).stmt_list.head
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

function GIMPLE_STMT_P(NODE) {
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

function Map() {
}

Map.prototype.keys = []
Map.prototype.values = []

Map.prototype.put = function (key, value) {
  var k = this.keys.indexOf (key)
  if (k == -1) {
    this.keys.push (key)
    this.values.push (value)
  } else {
    this.keys[k] = key;
    this.values[k] = value;
  }
}

Map.prototype.get = function (key) {
  var k = this.keys.indexOf (key)
  if (k != -1)
    return this.values[k]
}

Map.prototype.has = function (key) {
  this.keys.indexOf (key) != -1
}

var depth = 0;
// func should "return" via throw
// *walk_subtrees=0 is the same as returning 0
function walk_tree (t, func, guard) {
  if ((guard && guard.has (t))
      || !t) {
    return
  }
  guard.put (t)
  
  var walk_subtrees = func (t)
  depth++;
  code = TREE_CODE (t)
  switch (code) {
  case STATEMENT_LIST:
    for (var i = tsi_start (t); !i.end (i); i.next()) {
      walk_tree (i.stmt (), func, guard);
    }
    break;
  default:
    if (IS_EXPR_CODE_CLASS (TREE_CODE_CLASS (code))
	|| IS_GIMPLE_STMT_CODE_CLASS (TREE_CODE_CLASS (code)))
    {
      //    print(t.exp.operands)
      var length = TREE_OPERAND_LENGTH (t)
      for (var i = 0; i < length;i++) {
        walk_tree (GENERIC_TREE_OPERAND (t, i), func, guard)
      }
    }
    break;
  }
  depth--;
}

//gczeal(2)
function process_tree(f, b) {
  function code_printer (t) {
    var str = "";
    for (var i = 0; i < depth; i++)
      str += " "
    var code = TREE_CODE (t)
    str += code
    switch (code) {
    case CALL_EXPR:
      str += " operands:" + uneval(t.exp.operands.map (function (x) {return typeof x}))
    case ADDR_EXPR:
      str += " length:" + TREE_OPERAND_LENGTH (t)
    }
    print (str)
  }
  walk_tree (b, code_printer, new Map())
//  print("process_tree:"+b)
}
