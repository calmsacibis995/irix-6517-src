#ident "$Revision: 1.12 $"

include $(ROOT)/usr/include/make/commondefs

CFILES=bootp.c
TARGETS=bootp
FILES=bootptab

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /etc -m 444 -idb "config(noupdate)" $(FILES)
	${INSTALL} -F /usr/etc -lns ../../etc/bootptab bootptab
	${INSTALL} -F /usr/etc $(TARGETS)

bootp: $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@
