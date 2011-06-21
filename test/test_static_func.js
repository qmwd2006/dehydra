// { 'test': 'dehydra', 'input': 'test_static_func.cc', 'output': 'unit_test', 'lang': 'c++' }
function assert (t) {
  if (!t)
    throw new Error("static isn't correct");
}

function process_function(f) {
  print(f.name + f.isStatic)  
  switch (f.name) {
  case "X::st()":
  case "gst()":    
  case "<unnamed>::nst()":
  case "{anonymous}::nst()":
    assert(f.isStatic)
    break;
  default:
    assert(!f.isStatic)
  }
}

function input_end() {
  print("OK");
}
