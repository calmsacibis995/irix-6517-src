#!smake
#ident	"$Revision: 1.6 $"

include $(ROOT)/usr/include/make/commondefs
WOFF=
KERN=../../../kern
XFS=$(KERN)/fs/xfs

SRCS = xfs_estimate.c
OBJS = $(SRCS:.c=.o)

LCOPTS = -fullwarn
LCDEFS = -DSTATIC=static -D_KMEMUSER -D_PAGESZ=4096
LLDLIBS = -lgen

TARGETS = xfs_estimate

default: $(TARGETS)

include $(COMMONRULES)

install: $(TARGETS)
	$(INSTALL) -F /usr/sbin $(TARGETS)
