#! smake
#ident	"$Revision: 1.31 $

include	$(ROOT)/usr/include/make/commondefs

INS_CMD	=$(INSTALL)
INS_SUG	=$(INSTALL) -idb "config(suggest)"
INS_NOUP=$(INSTALL) -idb "config(noupdate)"

INSDIR	= /usr/lib/acct
USRBIN = /usr/bin
ETCINIT = /etc/init.d
OLDADMDIR = /usr/adm
ADMDIR	= /var/adm
WKDIR	=$(ADMDIR)/acct $(ADMDIR)/acct/nite $(ADMDIR)/acct/fiscal $(ADMDIR)/acct/sum
OLDWKDIR =$(OLDADMDIR)/acct $(OLDADMDIR)/acct/nite $(OLDADMDIR)/acct/fiscal $(OLDADMDIR)/acct/sum
GRPOWN = -u adm -g adm

WLIB= acctcms acctcon acctcon1 acctmerg acctprc acctprc1 
WOLIB= acctcon2 acctdisk acctdusg accton acctprc2 acctwtmp fwtmp wtmpfix \
		closewtmp utmp2wtmp
WDLIB= diskusg
WGLIB= acctcom
SCRIPTS= chargefee ckpacct dodisk monacct lastlogin nulladm prctmp prdaily \
		prtacct remove runacct acct shutacct startup turnacct
PRFILE= acct.profile
TARGETS= $(WLIB) $(WOLIB) $(WDLIB) $(SCRIPTS) $(WGLIB)
LIB = lib/a.a

default all:	dolib ${TARGETS} holidays

$(LIB) dolib:
	cd lib; $(MAKE)

${WLIB}:$$@.c  $(LIB)
	$(CCF) $@.c $(LIB) -o $@ $(LDFLAGS)

${WOLIB}:$$@.c 
	$(CCF) $@.c -o $@ $(LDFLAGS)

${WDLIB}:$$@.c 
	$(CCF) $@.c -ldisk -o $@ $(LDFLAGS) 

${WGLIB}:$$@.c $(LIB)
	$(CCF) $@.c $(LIB) -o $@ $(LDFLAGS) -lgen

include $(COMMONRULES)

install:all
	$(INS_CMD) $(GRPOWN) -dir "$(WKDIR)"
	$(INS_CMD) $(GRPOWN) -idb "delhist" -dir "$(OLDWKDIR)"
	$(INS_CMD) -F $(USRBIN) acctcom
	$(INS_CMD) $(GRPOWN) -F $(INSDIR)  "acctcms acctcon acctcon1 \
		acctcon2 acctdisk acctdusg acctmerg acctprc acctprc1 acctprc2 \
		acctwtmp chargefee ckpacct diskusg dodisk fwtmp \
		lastlogin monacct nulladm prctmp prdaily prtacct \
		remove runacct shutacct startup \
		turnacct wtmpfix closewtmp utmp2wtmp"
	$(INS_CMD) $(GRPOWN) -F $(INSDIR)  -m 0444 "ptecms.awk ptelus.awk"
	$(INS_SUG) $(GRPOWN) -F $(INSDIR)  -m 0444 holidays
	$(INS_CMD) -F $(INSDIR)  -u root -g sys -m 4755 accton
	$(INS_CMD) -F $(ETCINIT)  -u root -g sys -m 744 acct
	$(INS_CMD) -F /etc/rc2.d -lns ../init.d/acct S22acct
	$(INS_CMD) -F /etc/rc0.d -lns ../init.d/acct K22acct
	$(INS_NOUP) -F /etc/config -u root -g sys -m 644 -src acct.config acct
	$(INS_SUG) -F /var/spool/cron/crontabs \
			-u adm -g adm -m 644 -src crontab.adm adm
	$(INSTALL) -idb "delhist" -dir /usr/spool/cron/crontabs/adm
	for nm in $(PRFILE); do \
	    $(INS_CMD) $(GRPOWN) -F $(ADMDIR) -src $$nm `expr \
		    $$nm : 'acct\\(.*\\)'`;\
	    $(INS_CMD) $(GRPOWN) -idb "delhist" -dir $(OLDADMDIR)/`expr \
		    $$nm : 'acct\\(.*\\)'`;\
	done

libclobber:
	cd lib; $(MAKE) clobber

clobber: libclobber
