#!smake
#
# $Id: Makefile,v 2.15 1997/11/28 03:19:24 markgw Exp $

TARGETS	= pmda_pmcd.so
CFILES	= pmcd.c

PCP_SRC_DEPTH = ../../..

include ${ROOT}/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs
include versiondefs

CFLAGS	+= '-DPCP_VERSION="$(PCP_VERSION)"'

# unresolved symbols are OK ... they will be in the pmcd that dlopen()'s
# the DSO
LD_NO_UNRESOLVED=

# over-ride default, to remove -all ... object code comes from explicit .o's
#if $(IRIX) == "IRIX5_3"
LDDSOOPTS	= -elf -shared $(VLDDSOOPTS) $(LLDDSOOPTS) $(GLDDSOOPTS)
#else
GLDDSOOPTS	= -elf -shared $(MKDEPOPT) $(DSONAMEOPT)
#endif

LCDEFS	= $(DEBUG)
LCOPTS	= -fullwarn $(PCP_INC_PATH) -I../../..
LLDOPTS = $(PCP_LIBS_PATH)

#if $(IRIX) != "IRIX5_3"
LLDOPTS	+= -woff 49
#endif
LLDLIBS	= -lpcp -lpcp_pmda -lc

default! $(TARGETS)

include $(COMMONRULES)

pmda_pmcd.so:	$(OBJECTS)
	rm -f $@
	$(LD) $(VERSION_LDDSOOPTS) $(LDOPTS) $(OBJECTS) $(LDLIBS) -o $@ $(VERSION_LDLIBS)

pmcd.o:	$(PCP_SRC_DEPTH)/include/commondefs
