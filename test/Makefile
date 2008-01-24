GCCDIR = ../../gcc-build/gcc
CXX = $(GCCDIR)/g++
CC1PLUS = $(GCCDIR)/cc1plus
PLUGIN = ../gcc_dehydra.so
PLUGINARG = -fplugin=$(PLUGIN)
CXXFLAGS = $(PLUGINARG) -fplugin-arg=test.js 
all: test.o assign.o longevity.o attributes.o typedef.o operator_new.o destr_order.o init.o

# these aren't run by default because the PATH has contain $GCCDIR
g++: pipe warning

%.o: %.cc  $(PLUGIN)
	$(CC1PLUS) $(CXXFLAGS) -fsyntax-only -fpreprocessed $< -o $@ 
	rm -f $@

#tests workaround for outputing to stdout when shouldn't
pipe: destr_order.cc
	$(CXX) -c $< $(CXXFLAGS) -pipe -o $@
	rm $@

warning: test.cc
	$(CXX) -c $< $(PLUGINARG) -fplugin-arg=warning.js -o $@
	rm -f $@

clean:
	rm -f *.o *~ a.out
