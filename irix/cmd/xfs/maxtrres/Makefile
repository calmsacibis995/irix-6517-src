#!smake
#ident	"$Revision: 1.1 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS = xfs_maxtrres

CFILES = xfs_maxtrres.c
OFILES = $(CFILES:.c=.o)

LCDEFS+=-D_KMEMUSER -DSTATIC=static -DSIM -DXFS_BIG_FILES=1 -DXFS_BIG_FILESYSTEMS=1 -DSP
LCINCS+= -I../sim/src -I../mkfs
LCOPTS+= -fullwarn
LLDOPTS+=-L../sim
LLDLIBS+=-lsim_n32d -ldisk
WOFF=

default: $(TARGETS)

include $(COMMONRULES)

xfs_maxtrres: $(OFILES)
	$(CCF) -o $@ $(OFILES) $(LDFLAGS)

install: $(TARGETS)
