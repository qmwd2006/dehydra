// { 'test': 'dehydra', 'input': 'attributes2.cc', 'output': 'unit_test' }

function process_type(c) {
  //print (c)
}

function process_function(f,arr) {
  //print(arr)
}

function process_var(v) {
  if (v.name == "varAttrOnType") {
    var a = v.type.attributes[0]
    if (a.name != "user" || a.value[0] != "value")
      throw new Error ("Attributes on types don't work")
    print("OK")
  }
}

