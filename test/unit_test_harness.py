import os, re, sys
from subprocess import Popen, PIPE, STDOUT
from unittest import TestCase, TestSuite, TestLoader, TestResult

class PluginTestCase(TestCase):
    """Test case for running Treehydra and checking output."""
    def __init__(self):
        super(PluginTestCase, self).__init__()
        self.plugin = "dehydra"

    def runTest(self):
        cmd = self.getCommand()
        cmd_list = cmd.split()
        sub = Popen(cmd_list, stdout=PIPE, stderr=PIPE)
        out, err = sub.communicate()
        self.checkOutput(out, err)

    def getCommand(self):
        return TREEHYDRA_CMD_FORMAT%(self.plugin, self.filename, self.ccfile)

class CLITestCase(PluginTestCase):
    """Test case class for looking for expected items in the error output;
    or for no error output."""

    def __init__(self, script_path, ccfile, *expected_errors):
        super(CLITestCase, self).__init__()
        self.filename = script_path
        self.ccfile = ccfile
        self.expected_errors = expected_errors

    def checkOutput(self, out, err):
       if not self.expected_errors:
           self.failUnless(err == '', "Expected no error output; got error output")
       else:
           for e in self.expected_errors:
               if err.find(e) == -1:
                   self.fail("Expected '%s' in error output; not found"%e)

    def __str__(self):
       return "<test %s %s>"%(self.filename, self.ccfile)

class TreehydraCLITestCase(CLITestCase):
    def __init__(self, *args, **kw):
        super(TreehydraCLITestCase, self).__init__(*args, **kw)
        self.plugin = 'treehydra'

class JSUnitTestCase(PluginTestCase):
    """Test case class for running a JS unit test and checking for OK
    in the results."""

    def __init__(self, script_path, ccfile):
        super(JSUnitTestCase, self).__init__()
        self.filename = script_path
        self.ccfile = ccfile

    def checkOutput(self, out, err):
        # Get last line
        lines = out.split('\n')
        if lines and lines[-1] == '':
            lines = lines[:-1]
        self.failUnless(lines, "Expected 'OK' output; got empty output")
        last = lines[-1]
        self.failUnless(last.startswith('OK'), "Expected 'OK' output; got '%s'"%last)

class TreehydraJSUnitTestCase(JSUnitTestCase):
    def __init__(self, *args, **kw):
        super(TreehydraJSUnitTestCase, self).__init__(*args, **kw)
        self.plugin = 'treehydra'

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
s.addTest(CLITestCase('nofile.js', 'empty.cc', 'Cannot find include file'))
s.addTest(CLITestCase('syntax_error.js', 'empty.cc', 'SyntaxError'))
s.addTest(CLITestCase('semantic_error.js', 'empty.cc', 'TypeError'))

# Ensure that the full path appears
s.addTest(CLITestCase('lib_error.js', 'empty.cc', 'TypeError', '/treehydra.js'))

# Require pass tests
s.addTest(TreehydraCLITestCase('tc_pass1.js', 'onefunc.cc'))
s.addTest(TreehydraCLITestCase('tc_pass2.js', 'onefunc.cc', 'Cannot set gcc_pass_after'))

# Run Dehydra unit tests
s.addTest(JSUnitTestCase('test_include.js', 'empty.cc'))
s.addTest(JSUnitTestCase('test_include2.js', 'empty.cc'))
s.addTest(JSUnitTestCase('test_require.js', 'empty.cc'))

# Treehydra unit tests
s.addTest(TreehydraJSUnitTestCase('test_location.js', 'onefunc.cc'))


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
