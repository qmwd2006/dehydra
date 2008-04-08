import os, re, sys
from subprocess import Popen, PIPE, STDOUT
from unittest import TestCase, TestSuite, TestLoader, TestResult

class PluginTestCase(TestCase):
    """Test case for running Treehydra and checking output."""
    def __init__(self, plugin):
        super(PluginTestCase, self).__init__()
        self.plugin = plugin

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

    def __init__(self, plugin, script_path, ccfile, *expected_errors):
        super(CLITestCase, self).__init__(plugin)
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

class JSUnitTestCase(PluginTestCase):
    """Test case class for running a JS unit test and checking for OK
    in the results."""

    def __init__(self, plugin, script_path, ccfile):
        super(JSUnitTestCase, self).__init__(plugin)
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

class MyTestResult(TestResult):
    """The default version stores formatted tracebacks for the failures.
    We want just the message."""

    def addSuccess(self, *args, **kw):
        super(MyTestResult, self).addSuccess(*args, **kw)
        sys.stderr.write('.')

    def addError(self, *args, **kw):
        super(MyTestResult, self).addSuccess(*args, **kw)
        sys.stderr.write('e')

    def addFailure(self, test, err):
        self.failures.append((test, err[1].args))
        sys.stderr.write('x')

"""Assemble a test case from a test spec and the plugin to use."""
def makeTest(plugin, spec):
    cls = spec[0]
    args = [ plugin ] + list(spec[1:])
    return cls(*args)

BOTH_TESTS = [
    # These should all produce errors
    (CLITestCase, 'nofile.js', 'empty.cc', 'Cannot find include file'),
    (CLITestCase, 'syntax_error.js', 'empty.cc', 'SyntaxError'),
    (CLITestCase, 'semantic_error.js', 'empty.cc', 'TypeError'),

    # Ensure that the full path appears in error message
    (CLITestCase, 'lib_error.js', 'empty.cc', 'TypeError', '/treehydra.js'),

    # Test 'include'
    (JSUnitTestCase, 'test_include.js', 'empty.cc'),
    (JSUnitTestCase, 'test_include2.js', 'empty.cc'),

    # Test 'require'
    (JSUnitTestCase, 'test_require.js', 'empty.cc')
]

DEHYDRA_TESTS = [
    # This will produce a JS exception, but should not produce linker errors
    (CLITestCase, 'tc_pass1.js', 'onefunc.cc', 'Unrecognized require keyword'),

    # Verify scope/namespace printing.
    (JSUnitTestCase, 'test_scopes.js', 'scopes.cc'),

    # Verify the 'const' doesn't appear in the name field.
    (JSUnitTestCase, 'test_const.js', 'const.cc'),
]

TREEHYDRA_TESTS = [
    # Require pass option tests
    (CLITestCase, 'tc_pass1.js', 'onefunc.cc'),
    (CLITestCase, 'tc_pass2.js', 'onefunc.cc', 'Cannot set gcc_pass_after'),
    # Source location conversion
    (JSUnitTestCase, 'test_location.js', 'onefunc.cc'),
]
    
if len(sys.argv) != 3:
    print "usage: python %s <test-set> <*hydra-cmd-fmt>"%(sys.argv[0])
    sys.exit(1)

# This should be a format string with 3 %s, the plugin, script, and C++ file
TEST_SET = sys.argv[1]
TREEHYDRA_CMD_FORMAT = sys.argv[2]

if TEST_SET == 'dehydra':
    tests = [ makeTest('dehydra', t) for t in BOTH_TESTS + DEHYDRA_TESTS ]
elif TEST_SET == 'treehydra':
    tests = [ makeTest('treehydra', t) for t in BOTH_TESTS + TREEHYDRA_TESTS ]
elif TEST_SET == 'both':
    tests = [ makeTest('dehydra', t) for t in BOTH_TESTS + DEHYDRA_TESTS ] + \
        [ makeTest('treehydra', t) for t in BOTH_TESTS + TREEHYDRA_TESTS ]
else:
    print "%s: <test-set> must be one of dehydra, treehydra, both"%(sys.argv[0])

s = TestSuite()
for t in tests: s.addTest(t)
r = MyTestResult()

s.run(r)
sys.stderr.write('\n')

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
