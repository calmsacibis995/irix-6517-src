#!smake

BASEVERSION=n32bit
WANTPARALLEL=yes-please
include $(ROOT)/usr/include/make/cmdcommondefs

LINK =          -lns
DESTBINDIR =    /usr/bin
DESTLIBDIR =    /usr/lib

MANCFILES = man.c path.c glob2regex.c
MANWHATCFILES = manwhat.c gzip.c inflate.c unlzw.c
AWF_FILES = common dev.dumb mac.man mac.ms pass1 pass2.base pass2.man pass2.ms pass3

MANOBJECTS = $(MANCFILES:.c=.o)
MANWHATOBJECTS = $(MANWHATCFILES:.c=.o)

CFILES = $(MANCFILES) getcatNAME.c
LCOPTS = -fullwarn

LDIRT = getcatNAME.out

LLDLIBS = -lgen
COMMANDS = man manwhat getcatNAME

default: $(TARGETS)

include $(CMDCOMMONRULES)

man: $(MANOBJECTS)
	$(CCF) $(MANOBJECTS) $(LDFLAGS) -lcurses -o $@

manwhat: $(MANWHATOBJECTS)
	$(CCF) $(MANWHATOBJECTS) $(LDFLAGS) -o $@

test: testgetcatNAME

testgetcatNAME: getcatNAME
	rm -f getcatNAME.out
	( for i in getcatNAME.tests/*.man ; \
	do \
		getcatNAME < $$i ; \
	done ) | sort > getcatNAME.out
	@echo ">>>"
	@echo ">>> Test passes if no output from diff"
	@echo ">>>"
	diff getcatNAME.out getcatNAME.tests/getcatNAME.out.ref
	    

n32bitinstall: default
	$(INSTALL) -F $(DESTBINDIR) man awf
	$(INSTALL) -F $(DESTBINDIR) $(LINK) man apropos
	$(INSTALL) -F $(DESTBINDIR) $(LINK) man whatis
	$(INSTALL) -F $(DESTLIBDIR) -src makewhatis.sh makewhatis
	$(INSTALL) -F $(DESTLIBDIR) getcatNAME manwhat
	$(INSTALL) -F $(DESTLIBDIR)/awf $(AWF_FILES)
