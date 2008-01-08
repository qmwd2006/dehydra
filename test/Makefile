CC1PLUS = ../../gcc-build/gcc/cc1plus
PLUGIN = ../gcc_dehydra.so
all: test.o

%.o: %.cc 
	$(CC1PLUS) -plugin=$(PLUGIN) -plugin-arg=test.js -O0 $< -o $@ || rm -f $@

clean:
	rm -f *.o *~
