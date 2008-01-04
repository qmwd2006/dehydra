GCCDIR=$(HOME)/work/gcc
GCCBUILDDIR=$(GCCDIR)/../gcc-build
GCCSTAGE=gcc
INCLUDE = -DIN_GCC -DHAVE_CONFIG_H -I$(GCCBUILDDIR)/$(GCCSTAGE) -I$(GCCDIR)/gcc \
	-I$(GCCDIR)/gcc/. -I$(GCCDIR)/gcc/../include -I$(GCCDIR)/gcc/../libcpp/include \
	-I$(GCCDIR)/gcc/../libdecnumber -I$(GCCDIR)/gcc/../libdecnumber/bid \
	-I$(GCCBUILDDIR)/libdecnumber -I$(GCCBUILDDIR) -I$(GCCDIR)/gcc/cp \
        -I/home/tglek/local/include/js/

CFLAGS=-Wall -fPIC  -g $(INCLUDE)
CXXFLAGS=$(CFLAGS)

gcc_dehydra.so: dehydra.o dehydra_plugin.o
	$(CXX) -L$(HOME)/local/lib -ljs -shared -o $@ $+

clean:
	rm -f *.o gcc_dehydra.so *~
