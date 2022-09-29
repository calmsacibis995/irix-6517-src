#!smake
#
# $Id: Makefile,v 2.11 1997/10/24 06:13:39 markgw Exp $

TARGETS	= pmda_proc.so 
CFILES	= proc.c \
	  pglobal.c pmemory.c pracinfo.c proc_aux.c \
	  pscred.c psinfo.c pstatus.c psusage.c \
	  ttyname.c procmem.c ctltab.c nameinfo.c


PCP_SRC_DEPTH = ../../..

include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs
include versiondefs

# over-ride default, to remove -all ... object code comes from explicit .o's
#if $(IRIX) == "IRIX5_3"
LDDSOOPTS	= -elf -shared $(VLDDSOOPTS) $(LLDDSOOPTS) $(GLDDSOOPTS)
#else
GLDDSOOPTS	= -elf -shared $(MKDEPOPT) $(DSONAMEOPT)
#endif

LCDEFS	= $(DEBUG)
LCOPTS	= -fullwarn
LCINCS  = $(PCP_INC_PATH)
LLDOPTS	= $(PCP_LIBS_PATH)

#if $(IRIX) != "IRIX5_3"
LLDOPTS	+= -woff 49
#endif
LLDLIBS	= -lpcp -lpcp_pmda -lc

default! $(TARGETS)

include $(COMMONRULES)

pmda_proc.so:	$(OBJECTS)
	rm -f $@
	$(LD) $(VERSION_LDDSOOPTS) $(LDOPTS) $(OBJECTS) $(LDLIBS) -o $@ $(VERSION_LDLIBS)

