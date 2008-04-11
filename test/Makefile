GCCDIR = ../../gcc-build/gcc
CXX = $(GCCDIR)/g++
CC1PLUS = $(GCCDIR)/cc1plus -quiet
PLUGIN = ../gcc_dehydra.so
PLUGINARG = -fplugin=$(PLUGIN)
SCRIPT=test.js
CXXFLAGS = $(PLUGINARG) -fplugin-arg=$(SCRIPT)

all: test.o assign.o typedef.o operator_new.o destr_order.o init.o virtual.o constructor.o types.o templ-spec.o constructor.o stack_fieldOf.o templ-simple.o longevity.o finalizers.o dehydra_unit_tests

# Unit tests
unit_tests:
	python unit_test_harness.py both "$(CC1PLUS) -fplugin=../gcc_%s.so -o /dev/null -fplugin-arg=%s %s"

dehydra_unit_tests:
	python unit_test_harness.py dehydra "$(CC1PLUS) -fplugin=../gcc_%s.so -o /dev/null -fplugin-arg=%s %s"

treehydra_unit_tests:
	python unit_test_harness.py treehydra "$(CC1PLUS) -fplugin=../gcc_%s.so -o /dev/null -fplugin-arg=%s %s"

%.o: %.cc $(PLUGIN)
	$(CC1PLUS) $(CXXFLAGS) $< -o /dev/null

#tests workaround for outputing to stdout when shouldn't
pipe: destr_order.cc
	$(CXX) -c $< $(CXXFLAGS) -pipe -o $@
	rm $@

warning: test.cc
	$(CXX) -c $< $(PLUGINARG) -fplugin-arg=warning.js -o $@
	rm -f $@

virtual.o: virtual.cc
	$(CC1PLUS) $(PLUGINARG) -fplugin-arg=print-functions.js -fpreprocessed $< -o $@ 	
	rm -f $@

tree: tree.js ../gcc_treehydra.so
	$(MAKE) PLUGIN=../gcc_treehydra.so SCRIPT=tree.js $(TEST)

finalizer: finalizers.cc finalizers.js
	$(MAKE) PLUGIN=../gcc_treehydra.so SCRIPT=finalizers.js finalizers.o
clean:
	rm -f *.o *~ a.out
