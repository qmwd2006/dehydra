include ("treehydra.js")

function process_tree(f, b) {
  print (f.name + "()")
  sanity_check (b)
  pretty_walk (b)
}
