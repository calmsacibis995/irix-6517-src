#!smake

#	Copyright (c) 1984 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

# $Revision: 1.55 $

include	$(ROOT)/usr/include/make/commondefs
MKDEPFILE=Makedepend


OWNER=uucp
GRP=uucp
LCDEFS=-DUUGETTY
# do not worry about functions re-declared without prototypes, since there
# would be so many of them to fix
LCOPTS=-fullwarn -woff 1692

BIN=/usr/bin
INSDIR=/usr/lib/uucp
CONINSDIR=/etc/uucp

INST_ME	= -u $(OWNER) -g $(GRP)
IDB_TAGM= $(INST_ME)
IDB_TAG1= -idb "config(noupdate)" -m 644
IDB_TAG2= -idb "config(suggest)" -m 755
IDB_TAG3= -idb "config(noupdate)" -m 400 $(INST_ME)
IDB_TAG4= -idb "config(suggest)" -m 644
IDB_TAG5= -idb "config(update)" -m 755


# changes to these directories must be made in uucp.h
#
SPOOL	=/var/spool
LOCKS	=$(SPOOL)/locks
PUBDIR	=$(SPOOL)/uucppublic
UUCPDIR	=$(SPOOL)/uucp
#		things are linked from UUCPDIR into XQTDIR and CORRUPT
XQTDIR	=$(UUCPDIR)/.Xqtdir
#		this is optional
CORRUPT	=$(UUCPDIR)/.Corrupt
#		for logfiles
LOGDIR	=$(UUCPDIR)/.Log
#		for sequence number files
SEQDIR	=$(UUCPDIR)/.Sequence
#		for STST files
STATDIR	=$(UUCPDIR)/.Status
LOGUUCP	=$(LOGDIR)/uucp
LOGUUX	=$(LOGDIR)/uux
LOGUUXQT=$(LOGDIR)/uuxqt
LOGCICO	=$(LOGDIR)/uucico
WORKSPACE=$(UUCPDIR)/.Workspace
ADMIN	=$(UUCPDIR)/.Admin
#		for saving old log files
OLDLOG=$(UUCPDIR)/.Old

GENC	=utility.c cpmv.c expfile.c gename.c getpwinfo.c \
	ulockf.c xqt.c logent.c versys.c gnamef.c systat.c \
	sysfiles.c strsave.c
GENO	=$(GENC:.c=.o)
UUCPC	=uucpdefs.c uucp.c gwd.c permission.c getargs.c uucpname.c\
	gtcfile.c chremdir.c mailst.c
UUXC	=uucpdefs.c uux.c gwd.c permission.c getargs.c getprm.c\
	uucpname.c gtcfile.c chremdir.c
UUXQTC	=uucpdefs.c uuxqt.c mailst.c getprm.c uucpname.c \
	permission.c getargs.c gtcfile.c \
	shio.c chremdir.c
UUCICOC	=uucpdefs.c cico.c conn.c callers.c cntrl.c pk0.c pk1.c \
	eio.c tio.c gio.c xio.c anlwrk.c permission.c getargs.c \
	gnxseq.c pkdefs.c imsg.c gtcfile.c \
	mailst.c uucpname.c line.c chremdir.c \
	interface.c statlog.c strecpy.c
UUNAMEC	=uuname.c uucpname.c uucpdefs.c getpwinfo.c sysfiles.c strsave.c
UUSTATC	=uustat.c gnamef.c expfile.c uucpdefs.c getpwinfo.c ulockf.c getargs.c \
	utility.c uucpname.c versys.c strsave.c sysfiles.c $(STRPBRK)
UUSCHEDC=uusched.c gnamef.c expfile.c uucpdefs.c getpwinfo.c ulockf.c \
	systat.c getargs.c utility.c
UUCLEANUPC=uucleanup.c gnamef.c expfile.c uucpdefs.c getpwinfo.c \
	uucpname.c ulockf.c getargs.c cpmv.c utility.c
UUGETTYC=uugetty.c callers.c conn.c uucpdefs.c ulockf.c getargs.c interface.c \
		strsave.c sysfiles.c strecpy.c line.c

CFILES	=$(GENC) $(UUCPC) $(UUXC) $(UUXQTC) $(UUCICOC) $(UUNAMEC) \
	$(UUSTATC) $(UUSCHEDC) $(UUCLEANUPC) $(UUGETTYC) \
	uucpd.c uucheck.c unknown.c

# "update" configuration files which are scripts
UPSCRIPTS=uudemon.clnu uudemon.hour uudemon.adm \
	fix-dsi fix-intel fix-hayes fix-telebit fix-usr fix-zyxel

# "suggest" configuration files which are scripts
SUGSCRIPTS=uudemon.poll genperm pollhost

# simple configuration files
CONSIMP	=Poll Dialcodes Devconfig Sysfiles Maxuuxqts Maxuuscheds

# "suggest" configuration files
CONSUG	=Devices Dialers

CON	=$(UPSCRIPTS) $(SUGSCRIPTS) $(CONSIMP) $(CONSUG)

COMMANDS=uucp uux uuxqt uucico uusched
TOOLS	=uuname uucheck uustat uucleanup uucpd uugetty unknown
TARGETS	=$(COMMANDS) $(TOOLS)


default:$(TARGETS) $(CON)

include $(COMMONRULES)

install:default
	#always ship /usr/lib/uucp/Systems for sendmail
	$(INSTALL) -m 755 -dir $(INSDIR)
	$(INSTALL) -m 755 -dir $(CONINSDIR)
	$(INSTALL) -F $(INSDIR) -lns ../../../etc/uucp/Systems Systems
	$(INSTALL) -F $(INSDIR) -lns ../../../etc/uucp/Devconfig Devconfig
	$(INSTALL) -F $(INSDIR) -lns ../../../etc/uucp/Devices Devices
	$(INSTALL) -F $(INSDIR) -lns ../../../etc/uucp/Dialcodes Dialcodes
	$(INSTALL) -F $(INSDIR) -lns ../../../etc/uucp/Dialers Dialers
	$(INSTALL) -F $(INSDIR) -lns ../../../etc/uucp/Maxuuscheds Maxuuscheds
	$(INSTALL) -F $(INSDIR) -lns ../../../etc/uucp/Maxuuxqts Maxuuxqts
	$(INSTALL) -F $(INSDIR) -lns ../../../etc/uucp/Permissions Permissions
	$(INSTALL) -F $(INSDIR) -lns ../../../etc/uucp/Poll Poll
	$(INSTALL) -F $(INSDIR) -lns ../../../etc/uucp/Sysfiles Sysfiles
	$(INSTALL) -F $(INSDIR) -lns ../../../etc/uucp/Systems Systems
	$(INSTALL) -F $(INSDIR) -lns ../../../etc/uucp/fix-dsi fix-dsi
	$(INSTALL) -F $(INSDIR) -lns ../../../etc/uucp/fix-hayes fix-hayes
	$(INSTALL) -F $(INSDIR) -lns ../../../etc/uucp/fix-intel fix-intel
	$(INSTALL) -F $(INSDIR) -lns ../../../etc/uucp/fix-telebit fix-telebit
	$(INSTALL) -F $(INSDIR) -lns ../../../etc/uucp/fix-usr fix-usr
	$(INSTALL) -F $(INSDIR) -lns ../../../etc/uucp/fix-zyxel fix-zyxel
	$(INSTALL) -F $(INSDIR) -lns ../../../etc/uucp/genperm genperm
	$(INSTALL) -F $(INSDIR) -lns ../../../etc/uucp/pollhost pollhost
	$(INSTALL) -F $(INSDIR) -lns ../../../etc/uucp/uudemon.adm uudemon.adm
	$(INSTALL) -F $(INSDIR) -lns ../../../etc/uucp/uudemon.clnu uudemon.clnu
	$(INSTALL) -F $(INSDIR) -lns ../../../etc/uucp/uudemon.hour uudemon.hour
	$(INSTALL) -F $(INSDIR) -lns ../../../etc/uucp/uudemon.poll uudemon.poll
	$(INSTALL) $(IDB_TAG3) -F $(CONINSDIR) Systems
	$(INSTALL) $(IDB_TAGM) -m 755 -dir "$(XQTDIR) $(CORRUPT) $(LOGDIR) \
			    $(SEQDIR) $(STATDIR) $(LOGUUCP) $(LOGUUX) \
			    $(LOGUUXQT) $(LOGCICO) $(WORKSPACE) \
			    $(ADMIN) $(OLDLOG) $(UUCPDIR)"
	$(INSTALL) $(IDB_TAGM) -m 777 -dir $(PUBDIR)
	$(INSTALL) $(IDB_TAG1) -F $(CONINSDIR) $(CONSIMP)
	$(INSTALL) $(IDB_TAG4) -F $(CONINSDIR) $(CONSUG)
	#install the locks directory for public use
	$(INSTALL) $(INST_ME) -m 777 -dir "$(LOCKS)"
	$(INSTALL) -idb "delhist" $(INST_ME) -m 777 -dir /usr/spool/locks
	$(INSTALL) $(IDB_TAG3) -F $(CONINSDIR) Permissions
	$(INSTALL) $(IDB_TAG1) -F $(SPOOL)/cron/crontabs -src uucp.cron uucp
	$(INSTALL) $(IDB_TAG5) -F $(CONINSDIR) $(UPSCRIPTS)
	$(INSTALL) $(IDB_TAG2) -F $(CONINSDIR) $(SUGSCRIPTS)
	$(INSTALL) -m 555 -F $(BIN) uulog uuto uupick
	$(INSTALL) $(IDB_TAGM) -m 4111 -F $(BIN) uuname uucp uux uustat
	$(INSTALL) $(IDB_TAGM) -m 4111 -F $(INSDIR) uuxqt uucico uusched unknown
	$(INSTALL) -m 555 -F $(INSDIR) uugetty Uutry uucpd
	$(INSTALL) -m 555 -F $(INSDIR) uugetty Uutry
	# uucheck & uucleanup should only be run by root or uucp administrator
	$(INSTALL) $(IDB_TAGM) -m 110 -F $(INSDIR) uucheck uucleanup
	$(INSTALL) -idb "delhist" -dir /usr/spool/cron/crontabs/uucp
	$(INSTALL) -idb "delhist" -dir /usr/spool/uucp
	$(INSTALL) -idb "delhist" -dir /usr/spool/uucp/.Admin
	$(INSTALL) -idb "delhist" -dir /usr/spool/uucp/.Corrupt
	$(INSTALL) -idb "delhist" -dir /usr/spool/uucp/.Log
	$(INSTALL) -idb "delhist" -dir /usr/spool/uucp/.Log/uucico
	$(INSTALL) -idb "delhist" -dir /usr/spool/uucp/.Log/uucp
	$(INSTALL) -idb "delhist" -dir /usr/spool/uucp/.Log/uux
	$(INSTALL) -idb "delhist" -dir /usr/spool/uucp/.Log/uuxqt
	$(INSTALL) -idb "delhist" -dir /usr/spool/uucp/.Old
	$(INSTALL) -idb "delhist" -dir /usr/spool/uucp/.Sequence
	$(INSTALL) -idb "delhist" -dir /usr/spool/uucp/.Status
	$(INSTALL) -idb "delhist" -dir /usr/spool/uucp/.Workspace
	$(INSTALL) -idb "delhist" -dir /usr/spool/uucp/.Xqtdir
	$(INSTALL) -idb "delhist" -dir /usr/spool/uucppublic


uucp:	$(UUCPC:.c=.o) $(GENO)
	$(CCF) $(UUCPC:.c=.o) $(GENO) $(LDFLAGS) -o uucp

uucpd:	uucpd.o
	$(CCF)  uucpd.o $(LDFLAGS) -o uucpd

uux:	$(UUXC:.c=.o) $(GENO)
	$(CCF) $(UUXC:.c=.o) $(GENO) $(LDFLAGS) -o uux

uuxqt:	$(UUXQTC:.c=.o) $(GENO)
	$(CCF) $(UUXQTC:.c=.o) $(GENO) $(LDFLAGS) -o uuxqt

uucico:	$(UUCICOC:.c=.o) $(GENO)
	$(CCF) $(UUCICOC:.c=.o) $(GENO) $(LDFLAGS) -o uucico

uuname:	$(UUNAMEC:.c=.o)
	$(CCF) $(UUNAMEC:.c=.o) $(LDFLAGS) -o uuname

uustat:	$(UUSTATC:.c=.o)
	$(CCF) $(UUSTATC:.c=.o) $(LDFLAGS) -o uustat

uucleanup:$(UUCLEANUPC:.c=.o)
	$(CCF) $(UUCLEANUPC:.c=.o) $(LDFLAGS) -o uucleanup

uusched:$(UUSCHEDC:.c=.o)
	$(CCF) $(UUSCHEDC:.c=.o) $(LDFLAGS) -o $@

uucheck:uucheck.o uucpname.o
	$(CCF)  uucheck.o uucpname.o $(LDFLAGS) -o uucheck

uugetty:$(UUGETTYC:.c=.o)
	$(CCF) $(UUGETTYC:.c=.o) $(LDFLAGS) -o uugetty
