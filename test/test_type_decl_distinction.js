// { 'test': 'dehydra', 'input': 'numinfo.cc', 'output': 'unit_test' }

include('unit_test.js')

let ok = true

function process_type(t) {
  ok = ok && t.constructor == DehydraType
  for each (let m in t.members) {
    ok = ok && m instanceof DehydraDecl
  }
}

function input_end() {
  if (ok)
    print("OK")
}
