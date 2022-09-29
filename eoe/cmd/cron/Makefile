#!smake
#ident	"$Revision: 1.32 $"

BASEVERSION=n32bit
WANTPARALLEL=yes-please
include	$(ROOT)/usr/include/make/cmdcommondefs

USR=/usr
LIB=$(USR)/lib
SPOOL=$(USR)/spool/cron
CRONLIB=$(LIB)/cron
CRONSPOOL=$(SPOOL)/crontabs
ATSPOOL=$(SPOOL)/atjobs

LCDEFS=-DCRONPROJ

CFILES=elm.c funcs.c permit.c at.c cron.c crontab.c
YFILES=att1.y
LFILES=att2.l

LDIRT=att1.h att1.c att2.c
LLDOPTS=-Wl,-T,7FC00000,-D,7FC40000
LWOFF+=,1110,1116,1167

COMMANDS=cron at crontab batch

default:$(TARGETS)

include $(CMDCOMMONRULES)

# even though default uid is root, leave it explict, in case the
# default changes in the future.
#
# Permission on cron should be 744 in order to make setuid over NFS work.
#
n32bitinstall: default 
	$(INSTALL) -F /usr/etc -m 744 cron 
	$(INSTALL) -F /etc -lns ../sbin/cron cron
	$(INSTALL) -F /sbin -lns ../usr/etc/cron cron
	$(INSTALL) -dir -u root -g sys /var/cron
	$(INSTALL) -F /etc/cron.d -lns ../../var/cron/log log
	$(INSTALL) -F /usr/bin -m 4755 -u root at crontab
	$(INSTALL) -F /usr/bin -m 755 batch 

cron:	cron.o funcs.o elm.o
	$(CCF) cron.o funcs.o elm.o -o cron $(LDFLAGS)

crontab: crontab.o permit.o funcs.o
	$(CCF) crontab.o permit.o funcs.o -o crontab $(LDFLAGS)

at:	at.o att1.o att2.o funcs.o permit.o
	$(CCF) at.o att1.o att2.o funcs.o permit.o -o at $(LDFLAGS)

att1.c att1.h: att1.y
	$(YACCF) -d att1.y
	mv y.tab.c att1.c
	mv y.tab.h att1.h

att2.c:	att2.l
	$(LEXF) att2.l
	ed - lex.yy.c < att2.ed >/dev/null
	mv lex.yy.c att2.c

att2.o:	att1.c

depend incdepend: att1.h
