// {'test': 'treehydra', 'input': 'types.cc', 'output': 'unit_test' }

// Test that the output of lazy_types is the same as the output of
// dehydra. We rely on the behavior that process_tree_type is always called
// immediately after the matching process_type

include('unit_test.js');
include('unstable/lazy_types.js');

function ValueCheck(v1, v2, path, message)
{
  if (v1 != v2)
    throw new Error(message + "\nat path: " + path +
                    " dehydra: " + v1 +
                    " lazy: " + v2);
}

function BigIntCheck(v1, v2, path, message)
{
  if (bigint_cmp(v1, v2) != 0)
    throw new Error(message + "\nat path: " + path +
                    " dehydra: " + v1 +
                    " lazy: " + v2);
}

function JSMatchesTestCase(d, lazy)
{
  this._d = d;
  this._lazy = lazy;
}
JSMatchesTestCase.prototype = new TestCase();
JSMatchesTestCase.prototype.runTest = function() {
  let m = new Map();
  let t = this;
  
  function check(v1, v2, path) {
    /* sometimes dehydra returns things as strings which lazytypes always makes
     * into numbers...
     */
    if (typeof v1 == 'string' && v2 instanceof BigInt) {
      let s = (/^[-\d]+/(v1))[0];
      v1 = new BigInt(s);
    }

    if (typeof v1 == 'string' && typeof v2 == 'number')
      v1 = Number(/^[-\d+]+/(v1));
    
    ValueCheck(typeof v1, typeof v2, path, "types don't match");

    if ((v1 instanceof BigInt) && (v2 instanceof BigInt)) {
      BigIntCheck(v1, v2, path, "BigInts don't match");
      return;
    }
    
    switch (typeof v1) {
    case "undefined":
      return;
      
    case "number":
    case "string":
    case "boolean":
      ValueCheck(v1, v2, path, "values don't match");
      return;
      
    case "object":
      if (v1 instanceof Array)
        check_array(v1, v2, path);
      else
        check_object(v1, v2, path);
      return;
      
    default:
      throw Error("unexpected typeof: " + typeof v1 + ": " + v1);
    }
  }
  
  function check_array(a1, a2, path) {
    if (!(a2 instanceof Array))
      throw Error("treehydra isn't an array: " + a2);
    
    ValueCheck(a1.length, a2.length, path, "array lengths don't match");
    
    if (m.has(a1))
      return;
    m.put(a1);

    for (let i = 0; i < a1.length; ++i)
      check(a1[i], a2[i], path + "." + i);
  }
  
  function check_object(o1, o2, path) {
    if (m.has(o1))
      return;
    m.put(o1);

    // we don't check locations because definitions and declarations end up oddly in treehydra
    for (let p in o1)
      if (p != 'loc' && o1.hasOwnProperty(p)) {
        check(o1[p], o2[p], path + "." + p);
      }
  }

  try {
    check(this._d, this._lazy, "root");
  }
  catch (e) {
    // error(e + "\n" + e.stack);
    throw e;
  }
}

var r = new TestResults();

var gSavedType;

function process_type(t)
{
  gSavedType = t;
}

function process_tree_type(t)
{
  new JSMatchesTestCase(gSavedType, dehydra_convert(t)).run(r);
}

var gSavedDecl;

function process_decl(d)
{
  gSavedDecl = d;
}

function process_tree_decl(d)
{
  new JSMatchesTestCase(gSavedDecl, dehydra_convert(d)).run(r);
}

function input_end()
{
  if (r.testsRun < 1)
    print("Error: not a single test was run!");
  
  r.list();
}
