CC1PLUS = ../../gcc-build/gcc/g++
PLUGIN = ../gcc_dehydra.so
all: test.o assign.o longevity.o attributes.o typedef.o operator_new.o

%.o: %.cc  $(PLUGIN)
	$(CC1PLUS) -fplugin=$(PLUGIN) -fplugin-arg=test.js -O0 $< -o $@ -fsyntax-only
	rm -f $@

clean:
	rm -f *.o *~
