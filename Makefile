GCCDIR=$(HOME)/work/gcc
GCCBUILDDIR=$(GCCDIR)/../gcc-build
GCCSTAGE=stage2-gcc
INCLUDE = -DIN_GCC -DHAVE_CONFIG_H -I$(GCCBUILDDIR)/$(GCCSTAGE) -I$(GCCDIR)/gcc \
	-I$(GCCDIR)/gcc/. -I$(GCCDIR)/gcc/../include -I$(GCCDIR)/gcc/../libcpp/include \
	-I$(GCCDIR)/gcc/../libdecnumber -I$(GCCDIR)/gcc/../libdecnumber/bid \
	-I$(GCCBUILDDIR)/libdecnumber -I$(GCCBUILDDIR) -I$(GCCDIR)/gcc/cp
CFLAGS=-Wall -fPIC  -g $(INCLUDE)
gccplugin.so: chydra.o
	$(CXX) -shared -nostartfiles -o $@ $+
clean:
	rm -f *.o gccplugin.so gccplugin *~
