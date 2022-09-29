# make sendmail configuration file
# "$Revision: 1.9 $"

include	$(ROOT)/usr/include/make/commondefs
include ${COMMONRULES}

CFILES=canonhost.c
TARGETS=canonhost
STATICTARGETS= configmail
PREOP=preop("if test X$$instmode = Xnormal ; then if test X$$rbase = X/ ; then if test ! -f /etc/sendmail.mc ; then /etc/init.d/mail stop; fi; fi; fi")

default all: $(TARGETS)

configmail:

install: $(TARGETS)
	$(INSTALL) -m 755 -F /usr/sbin -idb "EOE" canonhost
	$(INSTALL) -m 755 -F /usr/etc -idb 'EOE exitop("chroot $$rbase /bin/sh /usr/etc/configmail migrate2mc")' configmail
	$(INSTALL) -m 644 -F /etc -idb "config(noupdate) EOE" sendmail.cf
	$(INSTALL) -m 644 -F /etc -idb "EOE" sendmail.fc 
	$(INSTALL) -m 644 -F /etc -idb 'config(suggest) $(PREOP) EOE' sendmail.mc
	$(INSTALL) -m 644 -F /etc -idb "config(suggest) EOE" aliases
	#$(INSTALL) -idb "std.sw.unix" -u root -g mail -m 0775 -dir /var/spool/mqueue
	$(INSTALL) -idb "delhist" -idb "std.sw.unix" -dir /usr/spool/mqueue
	#$(INSTALL) -idb "std.sw.unix" -F /usr/bsd -lns ../lib/sendmail mailq
	$(INSTALL) -idb "std.sw.unix" -F /etc -lns ../usr/lib/sendmail.hf sendmail.hf
