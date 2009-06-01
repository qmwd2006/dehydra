// { 'test': 'dehydra', 'input': 'onefunc.cc', 'output': 'unit_test' }

function process_decl(f) {
  if (f.shortName == "foo")
    print("OK")
}
