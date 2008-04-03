import os, re, sys
from subprocess import Popen, PIPE, STDOUT
from unittest import TestCase, TestSuite, TestLoader, TestResult

class TreehydraTestCase(TestCase):
    """Test case for running Treehydra and checking output."""

    def runTest(self):
        cmd = self.getCommand()
        cmd_list = cmd.split()
        sub = Popen(cmd_list, stdout=PIPE, stderr=PIPE)
        out, err = sub.communicate()
        self.checkOutput(out, err)

    def getCommand(self):
        return TREEHYDRA_CMD_FORMAT%(self.filename, self.ccfile)

class CLITestCase(TreehydraTestCase):
    """Test case class for looking for expected items in the error output;
    or for no error output."""

    def __init__(self, script_path, *expected_errors):
        super(CLITestCase, self).__init__()
        self.filename = script_path
        self.ccfile = 'empty.cc'
        self.expected_errors = expected_errors

    def checkOutput(self, out, err):
       if not self.expected_errors:
           self.failUnless(err == '', "Expected no error output; got error output")
       else:
           for e in self.expected_errors:
               if err.find(e) == -1:
                   self.fail("Expected '%s' in error output; not found"%e)

class JSUnitTestCase(TreehydraTestCase):
    """Test case class for running a JS unit test and checking for OK
    in the results."""

    def __init__(self, script_path):
        super(JSUnitTestCase, self).__init__()
        self.filename = script_path
        self.ccfile = 'empty.cc'

    def checkOutput(self, out, err):
        # Get last line
        lines = out.split('\n')
        if lines and lines[-1] == '':
            lines = lines[:-1]
        self.failUnless(lines, "Expected 'OK' output; got empty output")
        last = lines[-1]
        self.failUnless(last.startswith('OK'), "Expected 'OK' output; got '%s'"%last)

class MyTestResult(TestResult):
    """The default version stores formatted tracebacks for the failures.
    We want just the message."""

    def addFailure(self, test, err):
        self.failures.append((test, err[1].args))

if len(sys.argv) != 2:
    print "usage: python %s <treehydra-cmd-fmt>"%(sys.argv[0])
    sys.exit(1)

# This should be a format string with 2 %s, the first for the script,
# the second for the C++ file.
TREEHYDRA_CMD_FORMAT = sys.argv[1]

s = TestSuite()

# Ensure that all of these produce errors.
s.addTest(CLITestCase('nofile.js', 'Cannot find include file'))
s.addTest(CLITestCase('syntax_error.js', 'SyntaxError'))
s.addTest(CLITestCase('semantic_error.js', 'TypeError'))

# Ensure that the full path appears
s.addTest(CLITestCase('lib_error.js', 'TypeError', '/treehydra.js'))

# Run Dehydra unit tests
s.addTest(JSUnitTestCase('test_include.js'))

r = MyTestResult()

s.run(r)

if r.wasSuccessful():
    print "OK: %d tests passed"%r.testsRun
else:
    for c, tr in r.errors:
        print "Test Error: Python Exception"
        print "    Test command: %s"%c.getCommand()
        print "    Stack trace:"
        print tr

    for c, tr in r.failures:
        print "Test Failure: %s"%""
        print "    Test command: %s"%c.getCommand()
        print "    Failure msg: %s"%tr

    print
    print "Unit Test Suite Summary:"
    print "    %3d passed"%(r.testsRun - len(r.failures) - len(r.errors))
    print "    %3d failed"%len(r.failures)
    print "    %3d error(s)"%len(r.errors)
