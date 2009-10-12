// { 'test': 'treehydra', 'input': 'empty.cc', 'output': 'unit_test', 'lang': 'c,c++' }

// Test that treehydra.js is loaded before this file, and that this
// file is loaded only once.

if (this.LOADED_BEFORE) {
  print("Err: loaded before");
} else {
  LOADED_BEFORE = true;
  if (TREE_CODE == undefined) {
    print("Err: treehydra.js not loaded");
  } else if (sameVar == undefined) {
    // Test that Dehydra lib is loaded too. This only applies as long
    // as treehydra contains dehydra.
    print("Err: system.js not loaded");    
  } else {
    print("OK");
  }
}
