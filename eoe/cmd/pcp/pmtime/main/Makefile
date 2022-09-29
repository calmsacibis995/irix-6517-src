#!smake
# $Id: Makefile,v 1.18 1999/04/30 01:44:04 kenmcd Exp $
#

PCP_SRC_DEPTH = ../..
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

WARNOFF		=
C++FILES	= pmtime.c++
TARGETS		= pmtime
IDBTAG		= PCP_EOE_MON

LC++OPTS	= -fullwarn $(WARNOFF) $(PCP_INC_PATH) $(DEBUG) 

LC++DEFS	= '-DPCP_VERSION="$(PCP_VERSION)"'

OBJECTS		= ../common/unimplemented.o \
		  ../common/tv.o \
		  ../common/timesrvr.o \
		  ../common/fdready.o \
                  ../archiveTime/ArchiveTimeForm.o \
		  ../archiveTime/ArchiveTimeFormUI.o \
		  ../archiveTime/VkPCParchiveTime.o \
		  ../archiveTime/ArchiveBounds.o \
		  ../archiveTime/ArchiveBoundsForm.o \
		  ../archiveTime/impl.o ../archiveTime/clients.o \
                  ../liveTime/LiveTimeForm.o ../liveTime/LiveTimeFormUI.o \
		  ../liveTime/VkPCPliveTime.o ../liveTime/impl.o \
		  ../liveTime/clients.o ../timer/VkPCPtimer.o \
		  ../pixmaps/pixmaps.o pmtime.o

#if $(IRIX) == "IRIX5_3"
OBJECTS		+= ../common/VkPixmap.o
#endif

HELPLIB         = -all -lhelpmsg -none

LLDOPTS		= $(PCP_LIBS_PATH)

LLDLIBS		= $(HELPLIB) -lpcp -lvk -lSgm -lXpm -lXm -lXt -lX11 -lgen

LDIRT		= 


default: $(C++FILES) $(TARGETS)

include $(COMMONRULES)

install: $(TARGETS) app-defaults pmtime.icon
	$(INSTALL) -F /usr/pcp/bin -m 555 -idb "$(IDBTAG) $(STRIPBIN)" pmtime
	$(INSTALL) -F /usr/lib/X11/app-defaults -m 444 -idb "$(IDBTAG)" -src app-defaults -O PmTime
	$(INSTALL) -F /usr/lib/images -m 444 -idb "$(IDBTAG)" -src pmtime.icon -O PmTime.icon
#if $(PCP_EOE_NOSHIP)
	$(INSTALL) -F /usr/pcp/bin -m 555 -idb "PCP_EOE_NOSHIP $(STRIPBIN)" pmtime
	$(INSTALL) -F /usr/lib/X11/app-defaults -m 444 -idb "PCP_EOE_NOSHIP" -src app-defaults -O PmTime
	$(INSTALL) -F /usr/lib/images -m 444 -idb "PCP_EOE_NOSHIP" -src pmtime.icon -O PmTime.icon
#endif

pmtime : $(OBJECTS)
	echo "expect warning 84 about libhelpmsg not resolving any symbols"
	$(C++F) $(LC++OPTS) -o $@ $(OBJECTS) $(LDOPTS) $(LDLIBS)
	$(TAG) 0x00010D13 $@
