#ident "$Revision: 1.1 $"

include $(ROOT)/usr/include/make/commondefs

CFILES=main.c
TARGETS=fmtmsg

default:$(TARGETS)

include $(COMMONRULES)

fmtmsg: $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

install: default
	$(INSTALL) -F /usr/bin $(TARGETS)
