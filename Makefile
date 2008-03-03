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
#-I/$(HOME)/local/include/js/ 
CFLAGS= -Wall -fPIC -DXP_UNIX -g3 $(INCLUDE) -DDEBUG
COMMON=dehydra.o dehydra_builtins.o util.o dehydra_types.o
LDFLAGS=-L$(HOME)/local/lib -L$(SM_LIBDIR) -lm -ljs -shared
TREEHYDRA_OBJS=dehydra_tree.o treehydra_plugin.o $(COMMON)

gcc_dehydra.so: dehydra_plugin.o dehydra_ast.o $(COMMON)
	$(CC) $(LDFLAGS) -o $@ $+

gcc_treehydra.so: gcc_dehydra.so $(TREEHYDRA_OBJS) useful_arrays.js
	$(CC) $(LDFLAGS) -o $@ $(TREEHYDRA_OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

dehydra_ast.o: dehydra_ast.c dehydra_ast.h

treehydra_generated.h: gcc_cp_headers.h convert_tree.js
#	$(CXX) $(CFLAGS) -fplugin=./gcc_dehydra.so -fplugin-arg=convert_tree.js -fsyntax-only $< -C -E -o foo.ii
	$(CXX) -DTREEHYDRA_CONVERT_JS -fshow-column $(CFLAGS) -fplugin=./gcc_dehydra.so -fplugin-arg=convert_tree.js -fsyntax-only $<

useful_arrays.ii: useful_arrays.c
	$(CC) $(INCLUDE) -E $< -o $@

useful_arrays.js: useful_arrays.ii
	sed -e 's/.*tree_code/var tree_code/' -e 's/^#.*//' -e 's/\[\]//' -e 's/{/[/' -e 's/}/]/' $< > $@

dehydra_tree.o: dehydra_tree.c dehydra_tree.h treehydra_generated.h

clean:
	rm -f *.o *.so *~ *.ii *_generated.h
