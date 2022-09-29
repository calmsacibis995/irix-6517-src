#!smake
#
# $Id: Makefile,v 2.14 1997/10/24 06:16:45 markgw Exp $

TARGETS	= pmkstat
CFILES	= pmkstat.c

PCP_SRC_DEPTH = ../..

include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs
include versiondefs

LCDEFS	= $(DEBUG)
LCOPTS	= -fullwarn $(PCP_INC_PATH)
LLDOPTS	= $(PCP_LIBS_PATH)
LLDLIBS	= -lpcp_dev -lpcp

default: $(TARGETS)

include $(COMMONRULES)

pmkstat:	$(OBJECTS)
	rm -f $@
	$(CCF) $(OBJECTS) -o $@ $(LDOPTS) $(LDLIBS) $(VERSION_LDLIBS)
	$(TAG) 0x00010D0A $@

pmkstat.o:	pmkstat.c

#if $(ISM_NAME) == "pcp"
remake_pmnsmap.h:	../../pmgenmap/pmgenmap pmnsmap.spec
	../../pmgenmap/pmgenmap pmnsmap.spec >pmnsmap.h
#endif
