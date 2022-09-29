#!smake
# @(#)Makefile	5.10 (Berkeley) 6/24/90

BASELIB=libcursesBSD.a
ALTLIBS=libcursesBSD_spec.a libcursesBSD_64spec.a $(ALTLIBSN32)

DSOEXPORTS=common.exports
DSOEXPORTS_N32=common.exports
DSOEXPORTS_64=common.exports

# we do not currently build n32 in redwood to save build time, add
# $(ALTLIBSN32) to ALTLIBS to build locally.
ALTLIBSN32=libcursesBSD_n32spec.a libcursesBSD_n32mips3spec.a libcursesBSD_n32mips3.a

include $(ROOT)/usr/include/make/librootdefs

TARGETS=$(LIBS) libcursesBSD.so
LLDLIBS=-lcurses -lc

default exports: $(TARGETS)

include $(LIBROOTRULES)

exports: 
	# $(INSTALL) -F $(INSTMIPS2NSDIR) -m 444 -src libcursesBSD_spec.a libcursesBSD.a

install: default
	$(INSTALL) -F /usr/lib -m 444 -idb "nostrip" -src libcursesBSD.so  libcursesBSD.so
	$(INSTALL) -F $(INSTDIR_32_M2_NS) -m 444 -idb "nostrip" -src libcursesBSD_spec.a  libcursesBSD.a
	$(INSTALL) -F /usr/include -m 444 -src src/curses.h  cursesBSD.h
	$(INSTALL) -F $(INSTDIR_64_NS) -m 444 -idb "nostrip" -src libcursesBSD_64spec.a  libcursesBSD.a
