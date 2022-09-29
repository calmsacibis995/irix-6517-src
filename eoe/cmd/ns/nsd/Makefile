#!smake

include $(ROOT)/usr/include/make/commondefs

CFILES= attr.c btree.c callback.c cred.c file.c loopback.c map.c mount.c \
	nfs.c nsd.c nsw.c portmap.c timeout.c util.c 
LCOPTS= -fullwarn
LCDEFS= -DNDEBUG
LLDLIBS= -lmdbm

TARGETS= nsd

targets default: $(TARGETS)

nsd: $(OBJECTS)
	$(CCF) $(LLDOPTS) $(OBJECTS) -o $@ $(LDFLAGS) 

include $(COMMONRULES)

install: default
	${INSTALL} -u root -g sys -m 755 -dir /var/ns
	${INSTALL} -u root -g sys -m 755 -dir /var/ns/cache
	${INSTALL} -u root -g sys -m 755 -dir /var/ns/lib
	${INSTALL} -u root -g sys -m 755 -dir /var/ns/domains
	${INSTALL} -u root -g sys -m 0755 -idb 'nostrip' -f /usr/etc nsd
	${INSTALL} -u root -g sys -m 0644 -idb 'config(suggest)' \
	    -f /etc nsswitch.conf
	${INSTALL} -u root -g sys -m 0644 -idb 'config(suggest)' \
	    -f /var/ns nsswitch.conf.nisserv
	${INSTALL} -u root -g sys -m 0644 -idb 'config(noupdate)' \
	    -f /etc/config -src nsd.config nsd
	${INSTALL} -u root -g sys -m 0644 -idb 'config(noupdate)' \
	    -f /etc/config nsd.options
	${INSTALL} -u root -g sys -m 0644 -f /var/ns/cache README
	${INSTALL} -u root -g sys -m 0755 \
	    -idb 'exitop("chroot $$rbase /tmp/nsdefaults; :") nohist' \
	    -f /tmp nsdefaults
