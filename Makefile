GCCDIR=../gcc
GCCBUILDDIR=../gcc-build
GCCSTAGE=gcc
#SM_INCLUDE=$(HOME)/work/spidermonkey
#SM_LIBDIR=$(SM_INCLUDE)/Linux_All_DBG.OBJ
SM_INCLUDE=/usr/local/include/js
SM_LIBDIR=/usr/local/lib
INCLUDE = -DIN_GCC -DHAVE_CONFIG_H -I$(GCCBUILDDIR)/$(GCCSTAGE) -I$(GCCDIR)/gcc \
	-I$(GCCDIR)/gcc/. -I$(GCCDIR)/gcc/../include -I$(GCCDIR)/gcc/../libcpp/include \
	-I$(GCCDIR)/gcc/../libdecnumber -I$(GCCDIR)/gcc/../libdecnumber/bid \
	-I$(GCCBUILDDIR)/libdecnumber -I$(GCCBUILDDIR) -I$(GCCDIR)/gcc/cp \
	-I$(SM_INCLUDE) -I/$(HOME)/local/include/js/ 
#-DDEBUG
CFLAGS= -Wall -fPIC -DXP_UNIX -g3 $(INCLUDE)

gcc_dehydra.so: dehydra.o plugin.o dehydra_builtins.o dehydra_ast.o dehydra_callbacks.o util.o dehydra_types.o dehydra_tree.o
	$(CC) -L$(HOME)/local/lib -L$(SM_LIBDIR) -ljs -shared -o $@ $+

tree_hydra.so:
	$(CC) -L$(HOME)/local/lib -L$(SM_LIBDIR) -ljs -shared -o $@ $+

%.o: %.c
	$(CC) $(CFLAGS) -c $<

dehydra_ast.o: dehydra_ast.c dehydra_ast.h

dehydra_callbacks.o: dehydra_callbacks.c dehydra_callbacks.h dehydra_ast.h dehydra.h

#for the following CXX has to be a plugin-enabled compiler
plugin.ii: plugin.c
	mv plugin.o _plugin.o
	$(MAKE) CC="$(CXX) -E -o $@" plugin.o
	mv _plugin.o plugin.o

plugin.ii.auto.h: plugin.ii convert_tree.js
	$(CXX) -fpreprocessed -fplugin=./gcc_dehydra.so -fplugin-arg=convert_tree.js -fsyntax-only $< -o /dev/null

clean:
	rm -f *.o gcc_dehydra.so *~ *.i
