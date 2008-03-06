include ("treehydra.js")

function process_tree(f, b) {
  sanity_check (b)
  function code_printer (t, depth) {
    var str = "";
    while (depth--)
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
}
