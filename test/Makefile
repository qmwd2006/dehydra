GCCDIR = ../../gcc-build/gcc
CXX = $(GCCDIR)/g++
CC1PLUS = $(GCCDIR)/cc1plus -quiet
PLUGIN = ../gcc_dehydra.so
PLUGINARG = -fplugin=$(PLUGIN)
SCRIPT=test.js
CXXFLAGS = $(PLUGINARG) -fplugin-arg=$(SCRIPT)

all: test.o assign.o longevity.o attributes.o typedef.o operator_new.o destr_order.o init.o virtual.o constructor.o types.o templ-spec.o constructor.o

# these aren't run by default because the PATH has contain $GCCDIR
g++: pipe warning

%.o: %.cc  $(PLUGIN)
	$(CC1PLUS) $(CXXFLAGS) -fpreprocessed $< -o /dev/null
	rm -f $@

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

tree: noassign.cc tree.js
	$(CC1PLUS) $(PLUGINARG) -fplugin-arg=tree.js -fpreprocessed $< -o /dev/null 

clean:
	rm -f *.o *~ a.out
