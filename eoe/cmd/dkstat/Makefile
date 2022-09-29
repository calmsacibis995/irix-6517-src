#!smake
#
# $Header: /proj/irix6.5.7m/isms/eoe/cmd/dkstat/RCS/Makefile,v 1.7 1999/02/12 03:08:20 kenmcd Exp $
#
BASEVERSION=32bit
ALTVERSIONS=64bit

include ${ROOT}/usr/include/make/cmdcommondefs

COMMANDS = dkstat
IDBTAG = PCP_EOE

CFILES=dkstat.c spindle.c
HFILES=spindle.h

default: $(TARGETS)

include $(CMDCOMMONRULES)

# uncomment next line for debug
#LCFLAGS += -g

LLDLIBS += -lpcp_dev -lpcp -lcurses

dkstat : $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDOPTS) -o $@ $(LDLIBS) $(VERSION_LDLIBS)

32bitinstall: default
	$(INSTALL) -F /usr/sbin -idb "$(IDBTAG) mach(MODE=32bit)" -g sys -m 2755 dkstat

64bitinstall: default
	$(INSTALL) -F /usr/sbin -idb "$(IDBTAG) mach(MODE=64bit)" -g sys -m 2755 dkstat
