// { 'test': 'treehydra', 'input': 'test_treehydra_type_as_string.cc', 'output': 'unit_test' }
require({ after_gcc_pass: "cfg" });
include('unstable/dehydra_types.js');

let tests = {
  "t1": "int",
  "t2": "bool",
  "t3": "long double",
  "t4": "const long int",
  "t5": "s1",
  "t6": "const s1* const&",
  "t7": "const vector int[2]",
  "t8": "const complex long double",
  "t9": "volatile short unsigned int* const*[]",
  "t10": "t9* const&",
  "t11": "void (s1::*)(s1*, int)",
  "t12": "t11 (s1::*)(s1*, int)",
  "t13": "int s1::*"
};

function process_tree_type(t) {
  // pull the name of the typedef
  let tn = TYPE_NAME(t);
  if (!tn || TREE_CODE(tn) != TYPE_DECL)
    return;
  let name = decl_as_string(tn);

  // pull the original type
  t = DECL_ORIGINAL_TYPE(tn);
  if (!t)
    return;

  // look up the expected result
  let expected = tests[name];
  if (!expected)
    throw("couldn't find test");

  let result = type_as_string(t);
//  print('  "' + name + '": "' + result + '",');
  if (expected != result)
    throw new Error("Incorrect type for " + name + "; expected " + expected + ", got " + result)
  delete tests[name];
  print("OK");
}

function input_end() {
  let i = 0;
  for each (let t in tests)
    ++i;
  if (i != 0)
    throw new Error("missed tests " + tests);
}

