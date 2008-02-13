GCCDIR=../gcc
GCCBUILDDIR=../gcc-build
GCCSTAGE=gcc
SM_INCLUDE=$(HOME)/work/spidermonkey
SM_LIBDIR=$(SM_INCLUDE)/Linux_All_DBG.OBJ
#SM_INCLUDE=/usr/local/include/js
#SM_LIBDIR=/usr/local/lib
INCLUDE = -DIN_GCC -DHAVE_CONFIG_H -I$(GCCBUILDDIR)/$(GCCSTAGE) -I$(GCCDIR)/gcc \
	-I$(GCCDIR)/gcc/. -I$(GCCDIR)/gcc/../include -I$(GCCDIR)/gcc/../libcpp/include \
	-I$(GCCDIR)/gcc/../libdecnumber -I$(GCCDIR)/gcc/../libdecnumber/bid \
	-I$(GCCBUILDDIR)/libdecnumber -I$(GCCBUILDDIR) -I$(GCCDIR)/gcc/cp \
	-I$(SM_INCLUDE) -I/$(HOME)/local/include/js/ 
#-DDEBUG
CFLAGS= -Wall -fPIC -DXP_UNIX -g3 $(INCLUDE)

gcc_dehydra.so: dehydra.o plugin.o builtins.o dehydra_ast.o dehydra_callbacks.o util.o dehydra_types.o dehydra_tree.o
	$(CC) -L$(HOME)/local/lib -L$(SM_LIBDIR) -ljs -shared -o $@ $+

%.o: %.c
	$(CC) $(CFLAGS) -c $<

dehydra_ast.o: dehydra_ast.c dehydra_ast.h

dehydra_callbacks.o: dehydra_callbacks.c dehydra_callbacks.h dehydra_ast.h dehydra.h

clean:
	rm -f *.o gcc_dehydra.so *~
