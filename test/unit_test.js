/** Unit test framework for JS */

function TestResults() {
  this.testsRun = 0;
  this.errors = new Array();
  this.failures = new Array();
}

TestResults.prototype.startTest = function (test) {
  ++this.testsRun;
};

TestResults.prototype.addError = function (test, exc) {
  this.errors.push([test, exc]);
};

TestResults.prototype.addFailure = function (test, exc) {
  this.failure.push([test, exc]);
};

TestResults.prototype.list = function () {
  if (this.errors.length == 0 && this.failures.length == 0) {
    print("OK: " + this.testsRun + " tests passed");
  } else {
    for each (let errors in [ this.errors, this.failures ]) {
      for (let i = 0; i < errors.length; ++i) {
        let [t, e] = errors[i];
        print("ERR  " + t + "   " + e);
      }
    }
  }
};

function TestCase() {
  this.results = undefined;
  this.name = "TestCase";
}

TestCase.prototype.toString = function() {
  return this.name;
}

TestCase.prototype.run = function(results_arg) {
  this.results = results_arg ? results_arg : new TestResults();
  this.results.startTest(this);
  try {
    this.runTest();
  } catch (e) {
    this.results.addError(this, e);
  }
};

// Build test cases for methods named 'test' in this object.
TestCase.prototype.buildTestCases = function() {
  let ans = new Array();

  for (let k in this) {
    let v = this[k];
    if (k.substr(0, 4) == 'test' && typeof v == 'function') {
      let c = new TestCase();
      c.name = k;
      c.runTest = v;
      ans.push(c);
    }
  }
  return ans;
};

TestCase.prototype.runTestCases = function(results) {
  let cs = this.buildTestCases()
  for (let i = 0; i < cs.length; ++i) {
    cs[i].run(results);
  }
}

TestCase.prototype.fail = function(msg) {
  throw new Error(msg);
};

TestCase.prototype.assertEquals = function(v1, v2) {
  if (v1 != v2) {
    throw new Error(v1 + ' != ' + v2);
  }
};

TestCase.prototype.assertTrue = function(v) {
  if (!v) {
    throw new Error(v + ' is false');
  }
};