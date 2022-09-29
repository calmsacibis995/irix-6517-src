#!smake
#
# $Id: Makefile,v 1.20 1998/03/10 01:37:41 kenmcd Exp $
#

PCP_SRC_DEPTH = ..
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

TARGETS		= pmrun
C++FILES	= main.c++ FlagList.c++ RunDialog.c++ RunForm.c++ RunFormUI.c++
DONTWARN	= -woff 3262,1233
LC++OPTS	= -fullwarn $(DONTWARN) $(PCP_INC_PATH)
LC++DEFS	= $(DEBUG)
LLDOPTS		= $(PCP_LIBS_PATH)
HELPLIB         = -all -lhelpmsg -none

#if $(BEFORE_IRIX6_5)
LC++DEFS	+= -DPMRUN_NO_IMD
LLDLIBS		= $(HELPLIB) -lpcp_dev -lpcp -ldesktopIcon -lvk -lSgm \
		  -lXpm -lXm -lXt -lX11 -lgen
# 1375 and 1201 surpress warnings in ozdesktop headers
DONTWARN	+= -woff 1375,1201
#else
LLDLIBS		= $(HELPLIB) -lpcp_dev -lpcp -limdUtil -limdApp -lvk -lSgm \
		  -lXpm -lXm -lXt -lX11 -lgen
#endif

IDBTAG		= PCP_EOE_MON
LDIRT		= ii_files

default:	$(C++FILES) $(TARGETS) app-defaults

include $(ROOT)/usr/include/make/commonrules

install: $(TARGETS) app-defaults
	$(INSTALL) -F /usr/pcp/bin -m 555 -idb "$(IDBTAG) $(STRIPBIN)" pmrun
	$(INSTALL) -F /usr/lib/X11/app-defaults -m 444 -idb "$(IDBTAG)" -src app-defaults PmRun

pmrun:	$(OBJECTS)
	rm -f $@
	$(C++F) $(LC++OPTS) -o $@ $(OBJECTS) $(LDOPTS) $(LDLIBS)
	$(TAG) 0x00010D13 $@
