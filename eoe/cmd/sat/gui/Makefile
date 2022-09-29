#
#ident	"sat/gui/Makefile $Revision: 1.7 $"
#

include	$(ROOT)/usr/include/make/commondefs

TARGETS	= satconfig
CFILES	= satconfig.c satclass.c
LLDLIBS	= -lXm -lXt -lX11 -lgen
OWN	= -u root -g sys

default: $(TARGETS)

include $(COMMONRULES)

satconfig: $(OBJECTS)
	$(CCF) $(OBJECTS) -o $@ $(LDFLAGS)

install: default
	$(INSTALL) $(MAC) $(OWN) -m 555 -F /usr/sbin satconfig
	$(INSTALL) -F /usr/bin -lns ../sbin/satconfig satconfig
	$(INSTALL) $(OWN) -m 444 -F /usr/lib/X11/app-defaults Satconfig
	$(INSTALL) $(OWN) -m 555 -dir /usr/lib/onlineHelp
	$(INSTALL) $(OWN) -m 555 -dir /usr/lib/onlineHelp/Sat
	$(INSTALL) $(OWN) -m 444 -F /usr/lib/onlineHelp/Sat Events.hlp
	$(INSTALL) $(OWN) -m 444 -F /usr/lib/onlineHelp/Sat Interface.hlp
