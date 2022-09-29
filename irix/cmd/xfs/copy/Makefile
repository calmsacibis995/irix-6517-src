#!smake
#ident	"$Revision: 1.4 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS = xfs_copy

CFILES = xfs_copy.c locks.c
HFILES= locks.h
OFILES = $(CFILES:.c=.o)

LCDEFS+=-D_KMEMUSER -DSTATIC=static -DSIM -DXFS_BIG_FILES=1 -DXFS_BIG_FILESYSTEMS=1 -DXFSCOPY_HAVE_BEHAVIORS
LCINCS+= -I../sim/src
LCOPTS+= -fullwarn
LLDOPTS+=-L../sim
LLDLIBS+=-lsim_n32 -ldisk
WOFF=

default: $(TARGETS)

include $(COMMONRULES)

xfs_copy: $(OFILES)
	$(CCF) -o $@ $(OFILES) $(LDFLAGS)

install: $(TARGETS)
	$(INSTALL) -F /usr/sbin $(TARGETS)

