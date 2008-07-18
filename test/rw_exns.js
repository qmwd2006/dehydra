let succ_count = 0;
let fail_count = 0;

// Make a unit tester that tests for whether an exception is thrown
function make_unit_test_exn(fn, name) {
  return function() {
    if (arguments.length < 1) throw new Error();
    let should_throw = arguments[0];
    let args = [ arguments[k] for (k in range(1, arguments.length)) ]
    print("test: " + name + " " + args);
    let threw = false;
    try {
      fn.apply(this, args);
    } catch (e) {
      print("    exception: " + e);
      threw = true;
    }
    check(should_throw, threw);
  }
}
      
function range(start, end) {
  for (let i = start; i < end; ++i) yield i;
}

// Check a test result
function check(expected, actual) {
  if (expected == actual) {
    ++succ_count;
    print("    PASS");
  } else {
    ++fail_count;
    print("    FAIL");
  }
}

function print_stats() {
  print("\nTest results:");
  print("    Passed: " + succ_count);
  print("    Failed: " + fail_count);
}

wf = make_unit_test_exn(write_file);
rf = make_unit_test_exn(read_file);

wf(true);
wf(true, 3);
wf(true, 'a/z', 'aa');
wf(false, 'out', 'a');

rf(true);
rf(true, 'asdflkj');
rf(false, 'out');

print_stats();