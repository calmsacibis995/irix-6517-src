include /usr/include/make/commondefs
WOFF=


DEBUG = -g -DDEBUG
REGOPTS = -fullwarn -D_SGI_SOURCE
LCOPTS = $(REGOPTS)
LINTOPTS = $(DEBUG) -wlint -woff 813,852
LLDLIBS = -lmalloc
LLDOPTS = 

TARGETS = mapwrite writehole

default: $(TARGETS)

include $(COMMONRULES)

install: $(TARGETS)

mapwrite: mapwrite.o
	$(CCF) mapwrite.o $(LDFLAGS) -o $@

writehole: writehole.o
	$(CCF) writehole.o $(LDFLAGS) -o $@

debug:
	exec $(MAKE) LCOPTS='$(REGOPTS) $(DEBUG)'

lint:
	exec $(MAKE) LCOPTS='$(REGOPTS) $(LINTOPTS)'
