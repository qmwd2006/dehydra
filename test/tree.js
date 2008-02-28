function EnumValue (name, value) {
  this.name = name
  this.value = value
}

EnumValue.prototype.toString = function () {
  return this.name
}

eval (read_file ("../tree_code.js"))
eval (read_file ("../tree_code_class.js"))
eval (read_file ("../useful_arrays.js"))

function TREE_CODE_CLASS (node) {
}

function CHECK (tree_node, expected_code) {
  if (tree_node.base.code.value != expected_code.value)
    throw ("Expected " + expected_code + ", got " + tree_node.base.code)
  return tree_node
}

function STATEMENT_LIST_HEAD(NODE) {
  return CHECK (NODE, STATEMENT_LIST).stmt_list.head
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

function TREE_CODE (tree) {
  return tree.base.code
}

// func should "return" via throw
// *walk_subtrees=0 is the same as returning 0
function walk_tree (t, func, guard) {
  if (guard && guard[t]) 
    return
  guard[t] = 1
  
  var walk_subtrees = func (t)
  code = TREE_CODE (t)
  switch (code.name) {
  case "STATEMENT_LIST":
    for (var i = tsi_start (t); !i.end (i); i.next()) {
      walk_tree (i.stmt (), func, guard);
    }
    break;
  default:
    
  }
}

function process_tree(f, b) {
  walk_tree (b, function (x) {
    print (x.base.code)
//    print (x.stmt_list.head.stmt)
  }, {})
//  print("process_tree:"+b)
}
