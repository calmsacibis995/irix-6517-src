#!smake
#ident	"$Revision: 1.3 $"

include $(ROOT)/usr/include/make/commondefs

CFILES = jdm.c xfs_bstat.c
OFILES = $(CFILES:.c=.o)

LCDEFS = -D_SGI_SOURCE -D_SGI_MP_SOURCE -D_SGI_REENTRANT_FUNCTIONS -DCLRMTAUD -DEXTATTR -DBASED -DDOSOCKS -DINVCONVFIX -DSIZEEST -DPIPEINVFIX -DBANYAN -DFICUS -DNDEBUG
LLDLIBS= -B static -ldm
LCOPTS= -fullwarn

INSTDIR = /usr/sbin

TARGETS = xfs_bstat

default: $(TARGETS)

xfs_bstat: $(OFILES)
	$(CCF) -o $@ $(OFILES) $(LDFLAGS)

include $(COMMONRULES)

install: $(TARGETS)
	$(INSTALL) -F $(INSTDIR) xfs_bstat
