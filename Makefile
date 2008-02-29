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
COMMON=dehydra.o dehydra_builtins.o util.o dehydra_types.o

gcc_dehydra.so: dehydra_plugin.o dehydra_ast.o $(COMMON)
	$(CC) -L$(HOME)/local/lib -L$(SM_LIBDIR) -ljs -lm -shared -o $@ $+

gcc_treehydra.so: dehydra_tree.o treehydra_plugin.o $(COMMON)
	$(CC) -L$(HOME)/local/lib -L$(SM_LIBDIR) -ljs -lm -shared -o $@ $+

%.o: %.c
	$(CC) $(CFLAGS) -c $<

dehydra_ast.o: dehydra_ast.c dehydra_ast.h

treehydra_generated.h: gcc_cp_headers.h convert_tree.js useful_arrays.js
#	$(CXX) $(CFLAGS) -fplugin=./gcc_dehydra.so -fplugin-arg=convert_tree.js -fsyntax-only $< -C -E -o foo.ii
	$(CXX) -fshow-column $(CFLAGS) -fplugin=./gcc_dehydra.so -fplugin-arg=convert_tree.js -fsyntax-only $<

useful_arrays.ii: useful_arrays.c
	$(CC) $(INCLUDE) -E $< -o $@

useful_arrays.js: useful_arrays.ii
	sed -e 's/.*tree_code_class/var/' -e 's/^#.*//' -e 's/\[\]//' -e 's/{/[/' -e 's/}/]/' $< > $@

dehydra_tree.o: dehydra_tree.c dehydra_tree.h treehydra_generated.h

clean:
	rm -f *.o *.so *~ *.ii *_generated.h
