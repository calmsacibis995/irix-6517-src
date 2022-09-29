#ident "$Revision: 1.9 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=m4

LLDOPTS=-Wl,-I,$(ROOTRLDNAME)
LLDLIBS=-ly

CFILES=m4.c m4ext.c m4macs.c
YFILES=m4y.y

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /sbin $(TARGETS)
	$(INSTALL) -F /usr/bin -lns ../../sbin/$(TARGETS) $(TARGETS)

$(TARGETS): $(OBJECTS)
	 ${CCF} ${OBJECTS} ${LDFLAGS} -o $@
