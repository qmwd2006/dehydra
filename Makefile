GCCDIR=../gcc
GCCBUILDDIR=../gcc-build
GCCSTAGE=gcc
INCLUDE = -DIN_GCC -DHAVE_CONFIG_H -I$(GCCBUILDDIR)/$(GCCSTAGE) -I$(GCCDIR)/gcc \
	-I$(GCCDIR)/gcc/. -I$(GCCDIR)/gcc/../include -I$(GCCDIR)/gcc/../libcpp/include \
	-I$(GCCDIR)/gcc/../libdecnumber -I$(GCCDIR)/gcc/../libdecnumber/bid \
	-I$(GCCBUILDDIR)/libdecnumber -I$(GCCBUILDDIR) -I$(GCCDIR)/gcc/cp \
        -I/$(HOME)/local/include/js/ -I/usr/local/include/js

CFLAGS=-Wall -fPIC -DXP_UNIX -g3 $(INCLUDE)

gcc_dehydra.so: dehydra.o plugin.o builtins.o dehydra_ast.o dehydra_callbacks.o util.o dehydra_types.o dehydra_tree.o
	$(CC) -L$(HOME)/local/lib -L/usr/local/lib -ljs -shared -o $@ $+

%.o: %.c
	$(CC) $(CFLAGS) -c $<

dehydra_ast.o: dehydra_ast.c dehydra_ast.h

dehydra_callbacks.o: dehydra_callbacks.c dehydra_callbacks.h dehydra_ast.h dehydra.h

clean:
	rm -f *.o gcc_dehydra.so *~
