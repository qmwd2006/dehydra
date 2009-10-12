// { 'test': 'treehydra', 'input': 't4.cc', 'output': 'unit_test', 'lang': 'c++' }
function process_tree(fndecl) {
  let t = TREE_TYPE(fndecl);
  let rt = TREE_TYPE(t);
  
  for (let func = TYPE_METHODS (rt) ; func ; func = TREE_CHAIN (func)) {
    // Won't crash if this line is removed
    let a = DECL_LANG_SPECIFIC(func).decl_flags.thunk_p;
    
    // Crashes on this line -- Won't crash if .u5 is removed
    let b = DECL_LANG_SPECIFIC(func).u.f.u5;
  }
  print ("OK")
}
