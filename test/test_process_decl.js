// { 'test': 'dehydra', 'input': 'decl.cc', 'output': 'unit_test' }

let assert_passes = 0;
function xassert (t) {
  if (!t)
    throw new Error("process_decl isn't working right");
  assert_passes++;
}

function process_decl(v) {
  if (v.name == "plain_var")
    xassert(v.type.name == "int")
  else if (v.name == "some_typedef")
    xassert(v.type.typedef)
  else if (v.name == "FooTemplate") {
    // normal template decl info
    xassert(v.type.template.arguments.length)
  }
  else if (v.name == "FooTemplate<char, char>"
          || v.name == "FooTemplate<T, int>")
    xassert (v.type.template.arguments.length == 2)
  else if (v.name == "forward_func_decl()")
    xassert(v.isFunction)
  else
    print(v)
}

function input_end() {
  if (assert_passes == 6)
    print("OK")
}

