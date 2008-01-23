CXX = ../../gcc-build/gcc/g++
PLUGIN = ../gcc_dehydra.so
CXXFLAGS = -fplugin=$(PLUGIN) -fplugin-arg=test.js 
all: test.o assign.o longevity.o attributes.o typedef.o operator_new.o destr_order.o init.o

%.o: %.cc  $(PLUGIN)
	$(CXX) $(CXXFLAGS) -fsyntax-only -fpreprocessed $< -o $@ 
	rm -f $@

#tests workaround for outputing to stdout when shouldn't
destr_order.o: destr_order.cc
	$(CXX) -c $< $(CXXFLAGS) -pipe -o $@
	rm $@

clean:
	rm -f *.o *~ a.out
