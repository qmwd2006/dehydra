// { 'test': 'dehydra,treehydra', 'input': 'empty.cc', 'output': 'stderr_has("TypeError", "/treehydra.js")' }

// Produce an error in an included library file in the treehydra dir.

include('treehydra.js');
let c = TREE_CODE(undefined);