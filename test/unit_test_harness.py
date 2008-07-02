"""Test Harness

Specify a test by a JS file in this directory with the first line
like this:

// { 'test': 'treehydra', 'input': 'onefunc.cc', 'output': 'unit_test' }

  test:   comma-separated list of plugins to test, dehydra or treehydra
  input:  C++ input file to use for test
  output: eval'd in this script to yield a checker function applied
          to the output. See checkers, below.
"""

import os, re, sys
from subprocess import Popen, PIPE, STDOUT
from unittest import TestCase, TestSuite, TestLoader, TestResult

class PluginTestCase(TestCase):
    """Test case for running Treehydra and checking output."""
    def __init__(self, plugin, jsfile, ccfile, checker, checker_str):
        super(PluginTestCase, self).__init__()
        self.plugin = plugin
        self.jsfile = jsfile
        self.ccfile = ccfile
        self.checker = checker
        self.checker_str = checker_str

    def runTest(self):
        cmd = self.getCommand()
        sub = Popen(cmd, stdout=PIPE, stderr=PIPE, shell=True)
        out, err = sub.communicate()
        self.checker(self, 0, out, err)

    def getCommand(self):
        return TREEHYDRA_CMD_FORMAT%(self.plugin, self.jsfile, self.ccfile)

    def __str__(self):
        return '%-10s %-18s %-16s %s'%(
            self.plugin, self.jsfile, self.ccfile + ':', self.checker_str)

class MyTestResult(TestResult):
    """The default version stores formatted tracebacks for the failures.
    We want just the message."""

    def addSuccess(self, *args, **kw):
        super(MyTestResult, self).addSuccess(*args, **kw)
        sys.stderr.write('.')

    def addError(self, *args, **kw):
        super(MyTestResult, self).addError(*args, **kw)
        sys.stderr.write('e')

    def addFailure(self, test, err):
        self.failures.append((test, err[1].args))
        sys.stderr.write('x')

# Checkers
def stderr_has(*args):
    def checker(test, ec, out, err):
        for e in args:
            if err.find(e) == -1:
                test.fail("Expected '%s' in error output; not found. stderr:%s"%(e,err))
    return checker

def stdout_has(*args):
    def checker(test, ec, out, err):
        for e in args:
            if out.find(e) == -1:
                test.fail("Expected '%s' in output; not found. stdout:%s"%(e, out))
    return checker

def stderr_empty(test, ec, out, err):
    test.failUnless(err == '', "Expected no error output, got error output")

def unit_test(test, ec, out, err):
    # Get last line
    lines = out.split('\n')
    if lines and lines[-1] == '':
        lines = lines[:-1]
    test.failUnless(lines, "Expected 'OK' output; got empty output")
    last = lines[-1]
    test.failUnless(last.startswith('OK'), "Expected 'OK' output; got '%s'"%last)

def firstLine(filename):
    """Read the first line of the given file."""
    for line in open(filename):
        return line
    return ''

def extractTests(filename):
    """Attempt to extract a test case from the given JS file. Return
    a list of extracted test cases, which may be empty."""

    line = firstLine(filename)
    m = re.match(r'\s*//\s*({.*)', line)
    if not m: return []
    spec = eval(m.group(1))

    plugins = spec.get('test')
    if not plugins: raise TestSpecException(filename, "missing test")
    plugins = plugins.split(',')
    for p in plugins:
        if p not in ('dehydra', 'treehydra'):
            raise TestSpecException(filename, "invalid plugin %s"%p)
    
    ccfile = spec.get('input')
    if ccfile is None:
        ccfile = 'empty.cc'

    checker_str = spec.get('output')
    if checker_str is None:
        checker_str = 'unit_test'
    checker = eval(checker_str)

    return [ PluginTestCase(plugin, filename, ccfile, checker, checker_str) 
             for plugin in plugins ]

VERBOSE = False

from getopt import getopt
optlist, args = getopt(sys.argv[1:], 'v')
for opt, val in optlist:
    if opt == '-v':
        VERBOSE = True
    else:
        print >> sys.stderr, "%s: invalid option %s"%(sys.argv[0], opt)
        sys.exit(1)

if len(args) != 2:
    print "usage: python %s [-v] <test-set> <*hydra-cmd-fmt>"%(sys.argv[0])
    sys.exit(1)

plugin_str = args[0]
if plugin_str == 'both':
    PLUGINS = [ 'dehydra', 'treehydra' ]
else:
    PLUGINS = plugin_str.split(',')
# This should be a format string with 3 %s, the plugin, script, and C++ file
TREEHYDRA_CMD_FORMAT = args[1]

from glob import glob
tests = []
# This test can't go in a file, because it tests when the file doesn't exist.
for plugin in ("dehydra", "treehydra"):
    tests.append(PluginTestCase(plugin, 'nofile.js', 'empty.cc',
                                stderr_has('Cannot find include file'), ''))
# Tests for the argument
for plugin in ("dehydra", "treehydra"):
    tests.append(PluginTestCase(plugin, '"test_arg.js hello  goodbye"', 'empty.cc',
                                unit_test, 'unit_test'))

# For now, we'll put this here, but if more are created, we should make
# the test harness search dirs.
for plugin in ("dehydra", "treehydra"):
    tests.append(PluginTestCase(plugin, 'subdir/main.js', 'empty.cc',
                                unit_test, ''))


for f in glob('*.js'):
    try:
        tests += extractTests(f)
    except:
        print "Warning: %s: error extracting tests"%f
        import traceback
        traceback.print_exc()

if VERBOSE:
    print 'Test Cases:'
    for t in tests:
        if t.plugin in PLUGINS:
            tag = 'enabled'
        else:
            tag = 'disabled'
        print "  %-10s %s"%(tag, str(t))

s = TestSuite()
for t in tests: 
    if t.plugin in PLUGINS:
        s.addTest(t)
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
    sys.exit(1)
