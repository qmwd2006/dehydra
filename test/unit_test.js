/*
# test cases

# include nonexistent file
# include file with syntax error
# include file with semantic error

# all of the above with cli file
*/

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

TestCase.prototype.fail = function(msg) {
  throw new Error(msg);
};