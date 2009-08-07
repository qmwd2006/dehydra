include ../config.mk

PLUGIN = ../gcc_dehydra.so
GCCPLUGIN = -fplugin=$(PLUGIN)
SCRIPT=test.js
CXXFLAGS = $(GCCPLUGIN) $(PLUGIN_ARG)=$(SCRIPT)

DEHYDRA_OBJECTS = \
  test.o \
  assign.o \
  typedef.o \
  operator_new.o \
  destr_order.o \
  init.o \
  virtual.o \
  constructor.o \
  types.o \
  templ-spec.o \
  constructor.o \
  stack_fieldOf.o \
  templ-simple.o \
  longevity.o \
  finalizers.o \
  $(NULL)

GLOBAL_DEPS = \
  $(MAKEFILE_LIST) \
  $(PLUGIN) \
  $(NULL)

#do treehydra checks if treehydra is built by default
check:: check_dehydra $(BUILD_TREEHYDRA:gcc_treehydra.so=check_treehydra)
	@printf "'make check' only runs the dehydra test suite. To run all tests, run 'make check_both'.\n"

# Unit tests
check_both: $(OBJECTS)
	python unit_test_harness.py both "$(CC1PLUS)"

check_dehydra: $(OBJECTS)
	python unit_test_harness.py dehydra "$(CC1PLUS)"

check_treehydra:
	python unit_test_harness.py treehydra "$(CC1PLUS)"

%.o: %.cc $(PLUGIN) $(GLOBAL_DEPS)
	$(CC1PLUS) $(CXXFLAGS) $< -o /dev/null

#tests workaround for outputing to stdout when shouldn't
pipe: destr_order.cc $(GLOBAL_DEPS)
	$(CXX) -c $< $(CXXFLAGS) -pipe -o $@
	rm $@

warning: test.cc $(GLOBAL_DEPS)
	$(CXX) -c $< $(GCCPLUGIN) $(PLUGIN_ARG)=warning.js -o $@
	rm -f $@

tree: tree.js ../gcc_treehydra.so $(GLOBAL_DEPS)
	$(MAKE) PLUGIN=../gcc_treehydra.so SCRIPT=tree.js $(TEST)

finalizer: finalizers.cc finalizers.js $(GLOBAL_DEPS)
	$(MAKE) PLUGIN=../gcc_treehydra.so SCRIPT=finalizers.js finalizers.o

clean:
	rm -f *.o *~ a.out *.gcno
