#
# |-----------------------------------------------------------|
# | Copyright (c) 1991, 1990 MIPS Computer Systems, Inc.      |
# | All Rights Reserved                                       |
# |-----------------------------------------------------------|
# |          Restricted Rights Legend                         |
# | Use, duplication, or disclosure by the Government is      |
# | subject to restrictions as set forth in                   |
# | subparagraph (c)(1)(ii) of the Rights in Technical        |
# | Data and Computer Software Clause of DFARS 252.227-7013.  |
# |         MIPS Computer Systems, Inc.                       |
# |         950 DeGuigne Avenue                               |
# |         Sunnyvale, California 94088-3650, USA             |
# |-----------------------------------------------------------|
#
# $Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/RCS/common.mk,v 1.1 1992/12/14 11:36:38 suresh Exp $
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"@(#)lp:common.mk	1.3"
#
# Common makefile definitions used by all makefiles
#


#####
#
# Each makefile defines $(TOP) to be a reference to this top-level
# directory (e.g. TOP=..).
#####


RM	=	/bin/rm -f
CP	=	/bin/cp
LN	=	/bin/ln

ETCDIR  =	/etc
#INS	=	$(ETCDIR)/mipsinstall
#INSTALL	=	$(ETCDIR)/mipsinstall
INS	=	/bin/sh
INSTALL	=	/bin/sh

LINT	=	$(PFX)lint


USRBIN	=	$(DESTROOT)/usr/bin

B_USRLIB=	../../..
R_USRLIB=	usr/var/lib
USRLIB=		$(DESTROOT)/$(R_USRLIB)
R_USRSBIN=	usr/sbin
USRSBIN	=	$(DESTROOT)/$(R_USRSBIN)
USRSHARELIB=	$(DESTROOT)/usr/share/lib
USRUCB	=	$(DESTROOT)/usr/svr4/usr/ucb

R_USRSHARELIBLP=usr/share/lib/lp
USRSHARELIBLP=	$(DESTROOT)/$(R_USRSHARELIBLP)
LPBINDIR=	$(USRSHARELIBLP)/bin

R_VAR	=	usr/var
VAR	=	$(DESTROOT)/$(R_VAR)
B_VARSPOOL=	../../..
VARSPOOL=	$(VAR)/spool

#####
#
# Typical owner and group for LP things. These can be overridden
# in the individual makefiles.
#####
OWNER	=	lp
GROUP	=	lp
SUPER	=	root

#####
#
# $(EMODES): Modes for executables
# $(SMODES): Modes for setuid executables
# $(DMODES): Modes for directories
#####
EMODES	=	0555
SMODES	=	04555
DMODES	=	0775


INC	=	$(ROOT)/usr/include
INCSYS  =       $(INC)/sys

LPINC	=	$(TOP)/include
LPLIB	=	$(TOP)/lib

LIBACC	=	$(LPLIB)/access/liblpacc.a
LIBCLS	=	$(LPLIB)/class/liblpcls.a
LIBFLT	=	$(LPLIB)/filters/liblpflt.a
LIBFRM	=	$(LPLIB)/forms/liblpfrm.a
LIBLP	=	$(LPLIB)/lp/liblp.a
LIBMSG	=	$(LPLIB)/msgs/liblpmsg.a
LIBOAM	=	$(LPLIB)/oam/liblpoam.a
LIBPRT	=	$(LPLIB)/printers/liblpprt.a
LIBREQ	=	$(LPLIB)/requests/liblpreq.a
LIBSEC	=	$(LPLIB)/secure/liblpsec.a
LIBSYS	=	$(LPLIB)/systems/liblpsys.a
LIBUSR	=	$(LPLIB)/users/liblpusr.a
LIBNET	=       $(LPLIB)/lpNet/liblpNet.a
LIBBSD  =       $(LPLIB)/bsd/liblpbsd.a

LINTACC	=	$(LPLIB)/access/llib-llpacc.ln
LINTCLS	=	$(LPLIB)/class/llib-llpcls.ln
LINTFLT	=	$(LPLIB)/filters/llib-llpflt.ln
LINTFRM	=	$(LPLIB)/forms/llib-llpfrm.ln
LINTLP	=	$(LPLIB)/lp/llib-llp.ln
LINTMSG	=	$(LPLIB)/msgs/llib-llpmsg.ln
LINTOAM	=	$(LPLIB)/oam/llib-llpoam.ln
LINTPRT	=	$(LPLIB)/printers/llib-llpprt.ln
LINTREQ	=	$(LPLIB)/requests/llib-llpreq.ln
LINTSEC	=	$(LPLIB)/secure/llib-llpsec.ln
LINTSYS	=	$(LPLIB)/systems/llib-llpsys.ln
LINTUSR	=	$(LPLIB)/users/llib-llpusr.ln
