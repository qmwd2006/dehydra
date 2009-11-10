# -*- coding: utf-8 -*-
"""Test Harness

Specify a test by a JS file in this directory with the first line
like this:

// { 'test': 'dehydra', 'input': 'empty.cc', 'output': 'unit_test', 'lang': 'c'}

  test:   comma-separated list of plugins to test, dehydra or treehydra
  input:  C++ input file to use for test
  output: eval'd in this script to yield a checker function applied
          to the output. See checkers, below.
  lang:   comma-separated list of languages to test, c or c++. defaults to c++

"""

import os, re, sys
from subprocess import Popen, PIPE, STDOUT
from unittest import TestCase, TestSuite, TestLoader, TestResult

class PluginTestCase(TestCase):
    """Test case for running Treehydra and checking output."""
    def __init__(self, plugin, lang, jsfile, ccfile, checker, checker_str):
        super(PluginTestCase, self).__init__()
        self.plugin = plugin
        self.lang = lang
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
        command = CC1PLUS
        # turn .../cc1plus into .../cc1
        if self.lang == 'c':
            command = command[:-4]
        command += " -quiet -fplugin=../gcc_" + self.plugin + ".so -o /dev/null"
        if ("PLUGINS_MOZ" in config_opts) :
            command += " -fplugin-arg=" + self.jsfile
        else :
            command += " -fplugin-arg-gcc_" + self.plugin + "-=" + self.jsfile
        command += " " + self.ccfile
        return command

    def __str__(self):
        return '%-10s %-18s %-16s %s'%(
            self.plugin, self.jsfile, self.ccfile + ':', self.checker_str)

class MyTestResult(TestResult):
    """The default version stores formatted tracebacks for the failures.
    We want just the message."""

    def addSuccess(self, *args, **kw):
        super(MyTestResult, self).addSuccess(*args, **kw)
        self.printResult('pass', args[0])

    def addError(self, *args, **kw):
        super(MyTestResult, self).addError(*args, **kw)
        self.printResult('error', args[0])

    def addFailure(self, test, err):
        self.failures.append((test, err[1].args))
        self.printResult('fail', args[0])

class TinderboxTestResult(MyTestResult):
    tagMap = { 'pass':  'TEST-PASS',
               'error': 'TEST-UNEXPECTED-FAIL (error)',
               'fail':  'TEST-UNEXPECTED-FAIL' }
    
    def printResult(self, tag, test):
        print('%s | %s | %s | %s' % 
              (self.tagMap[tag], test.plugin, test.ccfile, test.jsfile))

    def finish(self):
        pass

class DevTestResult(MyTestResult):
    tagMap = { 'pass': '.', 'error': 'e', 'fail': 'x' }
    
    def printResult(self, tag, test):
        sys.stderr.write(self.tagMap[tag])

    def finish(self):
        sys.stderr.write('\n')

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

def stdout_has_re(*args):
    def checker(test, ec, out, err):
        for e in args:
            if not re.search(e, out, re.M):
                test.fail("Expected to find regular expression '%s' in output; not found. stdout:%s"%(e, out))
    return checker

def stderr_empty(test, ec, out, err):
    test.failUnless(err == '', "Expected no error output, got error output :%s" % err)

def unit_test(test, ec, out, err):
    # Get last line
    lines = out.split('\n')
    if lines and lines[-1] == '':
        lines = lines[:-1]
    msg = "got empty stdout, stderr"
    if err != '':
        msg = "Errors:\n %s" % err
    test.failUnless(lines, "Expected 'OK' output; %s" % msg)
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
    
    srcfile = spec.get('input')
    if srcfile is None:
        srcfile = 'empty.cc'

    checker_str = spec.get('output')
    if checker_str is None:
        checker_str = 'unit_test'
    checker = eval(checker_str)

    langs = spec.get('lang')
    if langs is None:
        langs = 'c,c++'
    # only pay attention to lang attribute if C is enabled
    if not 'C_SUPPORT' in config_opts:
        langs = 'c++'
    langs = langs.split(',')
    for lang in langs:
        if lang not in ('c', 'c++'):
            raise TestSpecException(filename, "invalid language %s"%lang)

    return [ PluginTestCase(plugin, lang, filename, srcfile, checker, checker_str)
             for plugin in plugins for lang in langs]

def parseConfigFile(config_filename):
    config = dict()
    f = open("../config.mk")
    for line in f:
        line = line.strip()
        if line.startswith("#"):
            continue
        fragments = line.split("=")
        config[fragments[0]] = "=".join(fragments[1:])
    f.close()
    return config


from optparse import OptionParser
parser = OptionParser()
parser.add_option('-v', '--verbose', action='store_true', dest='verbose',
                  help='verbose output')
parser.add_option('--tinderbox', action='store_true', dest='tinderbox',
                  help='tinderbox-compatible output')
OPTIONS, args = parser.parse_args()

if len(args) != 2:
    print "usage: python %s [-v] <test-set> <compiler options>"%(sys.argv[0])
    sys.exit(1)

plugin_str = args[0]
if plugin_str == 'both':
    PLUGINS = [ 'dehydra', 'treehydra' ]
else:
    PLUGINS = plugin_str.split(',')

# This should be the compiler to use
CC1PLUS = args[1]

# Parse the configuration file
config_opts = parseConfigFile("../config.mk")

from glob import glob
tests = []
# This test can't go in a file, because it tests when the file doesn't exist.
for plugin in ("dehydra", "treehydra"):
    tests.append(PluginTestCase(plugin, 'c++', 'nofile.js', 'empty.cc',
                                stderr_has('Cannot find include file'), ''))
# Tests for the argument
for plugin in ("dehydra", "treehydra"):
    tests.append(PluginTestCase(plugin, 'c++', '"test_arg.js hello  goodbye"',
                                'empty.cc', unit_test, 'unit_test'))

# For now, we'll put this here, but if more are created, we should make
# the test harness search dirs.
for plugin in ("dehydra", "treehydra"):
    tests.append(PluginTestCase(plugin, 'c++', 'subdir/main.js', 'empty.cc',
                                unit_test, ''))


for f in glob('*.js'):
    try:
        tests += extractTests(f)
    except:
        print "Warning: %s: error extracting tests"%f
        import traceback
        traceback.print_exc()

if OPTIONS.verbose:
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
if OPTIONS.tinderbox:
    r = TinderboxTestResult()
else:
    r = DevTestResult()

s.run(r)
r.finish()

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
