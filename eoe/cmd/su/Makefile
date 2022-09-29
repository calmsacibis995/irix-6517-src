#ident	"$Revision: 1.20 $"

include	$(ROOT)/usr/include/make/commondefs

TARGETS= su
CFILES = su.c
LCDEFS = -D_SHAREII
LCOPTS = -fullwarn
LLDOPTS= -Wl,-I,$(ROOTRLDNAME) -Wl,-woff,85,-woff,134,-ignore_unresolved
LLDLIBS= $(ROOT)/usr/$(LIB_LOCATION)/libcmd.a \
	$(ROOT)/usr/$(LIB_LOCATION)/libgen.a \
	$(ROOT)/usr/$(LIB_LOCATION)/libcrypt.a \
	$(ROOT)/usr/$(LIB_LOCATION)/libiaf.a
DFL    = "config(suggest)"

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -u root -g sys -m 4755 -F /sbin $(TARGETS)
	$(INSTALL) -F /usr/bin -lns ../../sbin/su $(TARGETS)
	$(INSTALL) -F /etc/default -m 644 -idb $(DFL) -src su.dfl -O su
