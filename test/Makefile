CC1PLUS = ../../gcc-build/gcc/cc1plus
PLUGIN = ../gcc_dehydra.so
all: test.o assign.o longevity.o attributes.o

%.o: %.cc  $(PLUGIN)
	$(CC1PLUS) -plugin=$(PLUGIN) -plugin-arg=test.js -O0 $< -o $@ -fsyntax-only 
	rm -f $@

clean:
	rm -f *.o *~
