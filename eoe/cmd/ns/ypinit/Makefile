#!smake

include $(ROOT)/usr/include/make/commondefs

TARGETS=

targets default: $(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -u root -g sys -m 0755 -f /var/yp mdbm_dump ypinit ypmake
	${INSTALL} -u root -g sys -m 0755 -f /var/yp -idb 'config(suggest)' \
	    mdbm_parse
	${INSTALL} -u root -g sys -m 0644 -f /var/yp -idb 'config(suggest)' \
	    make.script
