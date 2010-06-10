// { 'test': 'treehydra', 'input': 'hasOwn.cc', 'output': 'unit_test', 'lang': 'c++' }

// See bug 570195 for SpiderMonkey fix

function hasOwn(x) {
  return x.hasOwnProperty("blah");
}

function process_tree(fn) {
  // ok
  fn.hasOwnProperty("blah");

  // unhandledLazyProperty
  hasOwn(fn);

  print ("OK")
}
