// { 'test': 'treehydra', 'input': 'onefunc.cc', 'output': 'unit_test' }

include('treehydra.js');
include('unit_test.js');
//include('/home/dmandelin/treehydra-analysis/util.js')

function process_tree(fndecl) {
  let r = new TestResults();
  new LocationTestCase(fndecl).run(r);
  r.list();
}

function LocationTestCase(fndecl) {
  this.fndecl = fndecl;
}

LocationTestCase.prototype = new TestCase();

LocationTestCase.prototype.runTest = function() {
  let body = fn_decl_body(this.fndecl);
  for (let i = tsi_start (body); !i.end (i); i.next()) {
    let stmt = i.stmt();
    if (stmt.gstmt) {
      let loc = stmt.gstmt.locus;
      this.assertEquals(loc_is_valid(loc), true);
    }
  }
}

function loc_is_valid(loc) {
  return loc != undefined && typeof loc.file == 'string' && 
    typeof loc.line == 'number' && typeof loc.column == 'number';
}