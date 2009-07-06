// { 'test': 'dehydra', 'input': 'extern_c.cc', 'output': 'unit_test' }

function assert (t) {
  if (!t)
    throw new Error("isExternC is busted!");
}

let decls = {"c_function()":true, "c_int":true,
             "cc_function()":undefined, "cc_int":undefined}
function process_decl(v) {
  if (v.name in decls) {
    isExternC = decls[v.name]
    print(v.isExternC)
    if(isExternC)
      assert(v.isExternC)
    else
      assert(!v.isExternC)
    delete decls[v.name]
  } 
}

function input_end() {
  if (decls.toString() == {}.toString())
    print("OK");
    
}

