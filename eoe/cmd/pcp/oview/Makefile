#!smake
# $Id: Makefile,v 1.13 1998/03/10 01:36:25 kenmcd Exp $
#

PCP_SRC_DEPTH = ..
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

C++FILES	= Scene.c++ Config.c++ Topology.c++ Sprout.c++ oview.c++

TARGETS		= oview
IDBTAG		= PCP_EOE_MON

# uncomment next line if oview should be licensed
#OVIEW_LICENSED	= -DOVIEW_LICENSED

WARNOFF		= -woff 1201,1233,1375

LC++OPTS	= -fullwarn $(WARNOFF) $(PCP_OMCINV_INC_PATH) $(PCP_INC_PATH) \
		  $(DEBUG) $(OVIEW_LICENSED)

LC++DEFS        = '-DPCP_VERSION="$(PCP_VERSION)"'

OBJECTS		= $(C++FILES:S/.c++/.o/g)

LLDOPTS		= $(PCP_LIBS_PATH)

HELPLIB		= -all -lhelpmsg -none

LLDLIBS		= $(HELPLIB) $(PCP_OMCINV_LIBS) -lpcp_dev -lpcp -lInventor -lInventorXt \
		  -lGL -lvk -lSgm -lXpm -lXm -lXt -lX11 -lgen -lm

LDIRT		= ii_files

default: $(C++FILES) $(TARGETS)

include $(COMMONRULES)

install: $(TARGETS) app-defaults oview.icon pmlogger.config
	$(INSTALL) -F /usr/sbin -m 555 -idb "$(IDBTAG) $(STRIPBIN)" oview
	$(INSTALL) -F /usr/lib/X11/app-defaults -m 444 -idb "$(IDBTAG)" -src app-defaults -O OView
	$(INSTALL) -F /usr/lib/images -m 444 -idb "$(IDBTAG)" -src oview.icon -O OView.icon
	$(INSTALL) -F $(PMLOGGERCONFIG) -m 444 -idb "$(IDBTAG)" -src pmlogger.config config.oview

oview : $(OBJECTS)
	echo "NOTE: expect ld warnings 84 and 85 about libhelpmsg"
	$(C++F) $(LC++OPTS) -o $@ $(OBJECTS) $(LDOPTS) $(LDLIBS)
	$(TAG) 0x00010D08 $@

oview.o:	$(PCP_SRC_DEPTH)/include/commondefs
