#!smake

include $(ROOT)/usr/include/make/commondefs

TARGETS=

targets default: $(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -u root -g sys -m 0755 -f /usr/sbin nsadmin
