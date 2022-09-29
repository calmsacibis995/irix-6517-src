#!smake

include ../commondefs

SHFILES=runfsdump.sh fslist.build.sh

TARGETS=runfsdump fslist.build

default: $(TARGETS)

include ../commonrules

install: default
	${INSTALL} ${USR_GRP} -F /var/spool/cron/crontabs -idb 'config(suggest)' -m 644 -src crontab rfindd
	${INSTALL} -idb "delhist" -dir /usr/spool/cron/crontabs/rfindd
	${INSTALL} -u bin -g bin -F /var/rfindd -idb 'exitop("test x$$diskless = xnone && chroot $$rbase /bin/sh /var/rfindd/fslist.build || true")' fslist.build
	${INSTALL} ${USR_GRP} -F /var/rfindd runfsdump
	${INSTALL} ${USR_GRP} -F /var/rfindd -idb 'preop("test -d $$rbase/usr/lib/rfindd -a ! -d $$rbase/var/rfindd && mv $$rbase/usr/lib/rfindd $$rbase/var/rfindd || true") config(noupdate)' -m 644 fslist fsoptions
