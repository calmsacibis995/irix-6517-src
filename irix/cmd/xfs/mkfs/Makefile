#!smake
#ident	"$Revision: 1.24 $"

BASEVERSION=n32bit
ALTVERSIONS=n32d n32ns
OTHERVERSIONS=o32d o32bit
WANTPARALLEL=yes-please

include $(ROOT)/usr/include/make/cmdcommondefs

SIMCFILES = rdwr.c uuid.c
REGCFILES = nonsim.c xfs_mkfs.c
CFILES = $(SIMCFILES) $(REGCFILES)
HFILES = maxtrres.h xfs_mkfs.h
OFILES = $(CFILES:.c=.o)
REGOFILES = $(REGCFILES:.c=.o)

LCDEFS += -D_KMEMUSER -DSTATIC=static -DSIM
LCOPTS+= -fullwarn
LDIRT = maxtrres.h

WOFF =
#if defined(DEBUG_VERSION)
OPTIMIZER=
#endif

default: $(TARGETS)

include $(CMDCOMMONRULES)

mkfs_xfs: $(REGOFILES)
	$(CCF) -o $@ $(REGOFILES) $(LDFLAGS)

mrmkfs_xfs: $(REGOFILES)
	$(CCF) -o $@ $(REGOFILES) $(LDFLAGS)

n32bitinstall: default
	$(INSTALL) -F /sbin mkfs_xfs

n32dinstall: default
	$(INSTALL) -F /sbin -idb nostrip -src mkfs_xfs mkfs_xfs.debug

n32nsinstall: default

xfs_mkfs.o: maxtrres.h
maxtrres.h: $(MAXTRRES)
	$(MAXTRRES) > $@
