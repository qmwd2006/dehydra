// { 'test': 'dehydra,treehydra', 'input': 'empty.cc', 'output': 'unit_test', 'lang': 'c,c++' }

const GUARD_AGAINST_MULTIPLE_INCLUSIONS = 0

var foo = {}
include ("test_include2_lib.js", foo)
if (foo.includeCounter !== 1) {
  print (foo)
  throw new Error("Failed to do namespace include")
}
include ("test_include2_lib.js", foo)
if (foo.includeCounter !== 1)
  throw new Error("Double inclusion shouldn't be possible")

include ("test_include2_lib.js" )
if (includeCounter !== 1)
  throw new Error("Failed to do include")

include("test_include2.js")
print("OK")

