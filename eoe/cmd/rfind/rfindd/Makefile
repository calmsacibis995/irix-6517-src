#!smake

include ../commondefs

# CVERSION=-cckr

SHFILES=passwd.add.sh
TARGETS=passwd.add
LLDLIBS=$(LIBRFIND) -lgen

default: ${TARGETS}

include ../commonrules

install: ${TARGETS}
	${INSTALL} -u bin -g bin -F /var/rfindd -idb 'exitop("test x$$diskless = xnone && chroot $$rbase /bin/sh /var/rfindd/passwd.add || true")' passwd.add
	${INSTALL} ${USR_GRP} -F /var/rfindd -idb 'config(noupdate)' -m 644 -src forward .forward
	${INSTALL} ${USR_GRP} -F /var/rfindd -idb 'config(update)' -m 644 README
