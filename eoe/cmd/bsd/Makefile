#!smake
#
# Main makefile for cmd/bsd
#
#ident	"$Revision: 2.72 $"

include	$(ROOT)/usr/include/make/commondefs


SUBDIRS	= ansi bind compress config dump finger ftp ftpd gated \
	  mrouted nettest ordist quotas rdist restore route \
	  routed rsvp talk talkd telnet telnetd tftp tftpd timed traceroute \
	  ttcp

# long-running daemons -- not ones invoked by inetd
CDAEMON = inetd.c syslogd.c
CCRYPT	= rexecd.c
# Get hostnames from /etc/hosts only -- don't use BIND & NIS
CHOSTS	= hostid.c ifconfig.c
CAFS    = rcp.c rsh.c rshd.c rlogin.c
CSIMPLE = apply.c arp.c hostname.c ifuid.c klogpp.c last.c logger.c \
	  mkpts.c ping.c rlogind.c ruptime.c \
	  rwho.c rwhod.c uuencode.c uudecode.c w.c
CFILES	= $(CSIMPLE) $(CDAEMON) $(CCRYPT) $(CHOSTS) $(CAFS) $(CMFILES)

SIMPLE	= $(CSIMPLE:.c=)
DAEMON	= $(CDAEMON:.c=)
CRYPT	= $(CCRYPT:.c=)
HOSTS	= $(CHOSTS:.c=)
AFS	= $(CAFS:.c=)
TARGETS	= $(SIMPLE) $(DAEMON) $(CRYPT) $(HOSTS) $(AFS) clear which

FILES = hosts hosts.equiv networks protocols services syslog.conf \
	syslog.conf-mr syslogpp-mr inetd.conf inetd.conf.sec

MR	= -idb "mr"

FILEPERM= -m 644 -u root -g sys
SUID	= -m 04555 -u root -g sys
GUID	= -m 02555 -u root -g sys

# Configuration files that should be discarded if user has one already.
NOUPDATE= $(FILEPERM) -idb "config(noupdate)"

# Configuration files that should override, but save, existing version.
UPDATE	= $(FILEPERM) -idb "config(update)"

# Configuration files that we are not quite so cock-sure of
SUGGEST	= $(FILEPERM) -idb "config(suggest)"


# When appropriate (4.4 port?) restore -lbsd and let stand BSD setpgrp etc.
# Use BSD style gettimeofday(3) and settimeofday(3).
# Use -ignore_unresolved to allow dynamic linking of liblim (rshd, rexecd)
LDLIBS	=
LCDEFS	= -D_BSD_TIME -D_SHAREII -DDCE
LCOPTS	= -signed
LLDOPTS = -Wl,-T,10000000,-D,10040000,-woff,85,-woff,134 -ignore_unresolved
LDDOPTS = -Wl,-T,7FC00000,-D,7FC40000 -ignore_unresolved
LDIRT	= klogppre.h

default: $(TARGETS) $(FILES)
	$(SUBDIRS_MAKERULE)

COMMONPREF=bsd_
include $(COMMONRULES)

# Use libpthread for DCE
$(CRYPT): $$@.c
	$(CCF) $@.c $(LDOPTS) -lcrypt -liaf -lpthread $(LDLIBS) -o $@

$(DAEMON): $$@.c
	$(CCF) $@.c $(GLDOPTS) $(LDDOPTS) $(LDLIBS) -o $@

# Get hostnames from /etc/hosts only -- don't use BIND & NIS
$(HOSTS): $$@.c
	$(CCF) -Dgethostbyname=_gethtbyname -Dgethostbyaddr=_gethtbyaddr \
		$@.c $(LDFLAGS) -o $@

# this and next 3 need -DAFS
rcp:
	$(CCF) -DAFS $@.c $(LDOPTS) $(LDLIBS) -o $@

rsh:
	$(CCF) -DAFS $@.c $(LDOPTS) $(LDLIBS) -o $@

# page align buffer again
rshd:
	$(CCF) -DAFS $@.c $(GLDOPTS) $(LDDOPTS) -liaf $(LDLIBS) -o $@

rlogin:
	$(CCF) -DAFS $@.c $(LDOPTS) $(LDLIBS) -o $@

klogpp:	klogppre.h $$@.c
	$(CCF) $@.c $(LDFLAGS) -lgen -o $@

klogppre.h: klogpp.re
	regcmp $?
	mv -f $?.i klogppre.h

mkpts:	mkpts.c
	$(CCF) -DMKPTS $@.c $(LDFLAGS) -o $@

install $(COMMONTARGS): $(COMMONPREF)$$@
	$(SUBDIRS_MAKERULE)

$(COMMONPREF)install: default
	$(INSTALL) $(MR) -dir /usr/bsd /usr/etc
	$(INSTALL)       -dir /var/spool/rwho
	$(INSTALL) -idb "delhist" -dir /usr/spool/rwho

	$(INSTALL) -F /etc $(NOUPDATE) hosts hosts.equiv networks
	$(INSTALL) -F /etc $(SUGGEST)  protocols services syslog.conf
	$(INSTALL) -F /etc $(MR) -src syslog.conf-mr syslog.conf

	$(INSTALL) -F /usr/bsd $(MR) $(SUID)  rcp rlogin rsh
	$(INSTALL) -F /usr/bsd $(MR) hostid hostname
	$(INSTALL) -F /usr/bsd ruptime rwho
	$(INSTALL) -F /usr/bsd $(MR) logger
	$(INSTALL) -F /usr/bsd uudecode uuencode
	$(INSTALL) -F /usr/bsd apply clear last which
	$(INSTALL) -F /usr/bsd $(GUID) w
	$(INSTALL) -F /usr/bsd -ln w uptime

	$(INSTALL) -F /usr/etc $(MR) ifconfig syslogd
	$(INSTALL) -F /usr/etc $(MR) $(SUID)  ping
	$(INSTALL) -F /usr/etc $(MR) arp
	$(INSTALL) -F /usr/etc iflabel ifuid
	$(INSTALL) -F /usr/etc inetd rexecd rlogind rshd rwhod
	$(INSTALL) -F /etc $(UPDATE) inetd.conf inetd.conf.sec
	$(INSTALL) -F /usr/etc -lns ../../etc/inetd.conf inetd.conf

	$(INSTALL) -F /usr/sbin $(MR) klogpp
	$(INSTALL) -F /var/adm $(MR) -lns /usr/sbin/klogpp klogpp
	$(INSTALL) -F /usr/sbin $(MR) -src syslogpp-mr syslogpp
	$(INSTALL) -F /var/adm $(MR) -lns /usr/sbin/syslogpp syslogpp
	$(INSTALL) -idb "delhist" -dir /usr/adm/klogpp
	$(INSTALL) -F /usr/sbin $(SUID) mkpts
	$(INSTALL) -F /var/adm  -lns /usr/sbin/mkpts mkpts
	$(INSTALL) -idb "delhist" -dir /usr/adm/mkpts

$(SUBDIRS): $(_FORCE)
	@$(ECHO) "====\t$@"; cd $@; $(MAKE)
