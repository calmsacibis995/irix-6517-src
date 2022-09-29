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
# $Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/RCS/lp.mk,v 1.1 1992/12/14 13:34:59 suresh Exp $
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"@(#)nlp:lp.mk	1.32"
#
# Top level makefile for the LP Print Service component
#


SHELL	=/bin/sh

TOP	=	.

include ./common.mk


ROOTLIBS=-dn
PERFLIBS=-dn
SHLIBS	=
NOSHLIBS=-dn
SYMLINK	=	 ln -s
#SYMLINK=	: ln -s


CMDDIR	=	./cmd
LIBDIR	=	./lib
INCDIR	=	./include
ETCSRCDIR=	./etc
MODELDIR=	./model
FILTDIR	=	./filter
CRONTABDIR=	./crontab
TERMINFODIR=	./terminfo

PLACES	=	$(LIBDIR) \
		$(CMDDIR) \
		$(ETCSRCDIR) \
		$(MODELDIR) \
		$(FILTDIR) \
		$(CRONTABDIR) \
		$(TERMINFODIR)

DIRS	= \
		$(VAR)/lp \
		$(VAR)/lp/classes \
		$(VAR)/lp/fd \
		$(VAR)/lp/forms \
		$(VAR)/lp/interfaces \
		$(VAR)/lp/logs \
		$(VAR)/lp/printers \
		$(VAR)/lp/pwheels \
		$(USRSHARELIBLP) \
		$(USRSHARELIBLP)/bin \
		$(USRSHARELIBLP)/model \
		$(USRSHARELIBLP)/postscript \
		$(VARSPOOL)/lp \
		$(VARSPOOL)/lp/admins \
		$(VARSPOOL)/lp/requests \
		$(VARSPOOL)/lp/system \
		$(VARSPOOL)/lp/fifos \
		$(VARSPOOL)/lp/fifos/private \
		$(VARSPOOL)/lp/fifos/public

PRIMODE	=	0771
PUBMODE	=	0773

DEBUG	=	-O


all:		libs \
		cmds \
		etcs \
		models \
		filters \
		crontabs \
		terminfos

#####
#
# Doing "make install" from the top level will install stripped
# copies of the binaries. Doing "make install" from a lower level
# will install unstripped copies.
#####
#install:	all strip realinstall
install:	all realinstall

realinstall:	dirs
	for d in $(PLACES); \
	do \
		cd $$d; \
		$(MAKE) $(MAKEARGS) install; \
		cd ..; \
	done

#####
#
# Lower level makefiles have "clobber" depend on "clean", but
# doing so here would be redundant.
#####
clean clobber:
	for d in $(PLACES); \
	do \
		cd $$d; \
		$(MAKE) $(MAKEARGS) $@; \
		cd ..; \
	done

strip:
	if [ -n "$(STRIP)" ]; \
	then \
		$(MAKE) $(MAKEARGS) STRIP=$(STRIP) -f lp.mk realstrip; \
	else \
		$(MAKE) $(MAKEARGS) STRIP=strip -f lp.mk realstrip; \
	fi

realstrip:
	for d in $(PLACES); \
	do \
		cd $$d; \
		$(MAKE) $(MAKEARGS) STRIP=$(STRIP) strip; \
		cd ..; \
	done

dirs:
	for d in $(DIRS); do if [ ! -d $$d ]; then mkdir -p $$d; fi; done
	$(CH)chmod $(DMODES) $(DIRS)
	$(CH)chmod $(PRIMODE) $(VARSPOOL)/lp/fifos/private
	$(CH)chmod $(PUBMODE) $(VARSPOOL)/lp/fifos/public
	$(CH)chgrp $(GROUP) $(DIRS)
	$(CH)chown $(OWNER) $(DIRS)

	for d in $(USRLIB)/lp \
		$(VARSPOOL)/lp/admins/lp \
		$(VARSPOOL)/lp/bin \
		$(VARSPOOL)/lp/model \
		$(VARSPOOL)/lp/logs \
		$(DESTROOT)/etc/lp ; \
	do \
		rm -rf $$d; \
	done
	-$(SYMLINK) $(B_USRLIB)/$(R_USRSHARELIBLP) $(USRLIB)/lp
	-$(SYMLINK) ../../$(B_VARSPOOL)/$(R_VAR)/lp $(VARSPOOL)/lp/admins/lp
	-$(SYMLINK) ../$(B_VARSPOOL)/$(R_USRSHARELIBLP)/bin $(VARSPOOL)/lp/bin
	-$(SYMLINK) ../$(B_VARSPOOL)/$(R_USRSHARELIBLP)/model $(VARSPOOL)/lp/model
	-$(SYMLINK) ../$(B_VARSPOOL)/$(R_VAR)/lp/logs $(VARSPOOL)/lp/logs
	-$(SYMLINK) ../$(R_VAR)/lp $(DESTROOT)/etc/lp

libs:
	cd $(LIBDIR); $(MAKE) $(MAKEARGS) DEBUG="$(DEBUG)" FUNCDCL="$(FUNCDCL)"

cmds:
	cd $(CMDDIR); $(MAKE) $(MAKEARGS) DEBUG="$(DEBUG)" LDFLAGS="$(LDFLAGS)"

etcs:
	cd $(ETCSRCDIR); $(MAKE) $(MAKEARGS) DEBUG="$(DEBUG)"

models:
	cd $(MODELDIR); $(MAKE) $(MAKEARGS) DEBUG="$(DEBUG)" LDFLAGS="$(LDFLAGS)"

filters:
	cd $(FILTDIR); $(MAKE) $(MAKEARGS) DEBUG="$(DEBUG)"

crontabs:
	cd $(CRONTABDIR); $(MAKE) $(MAKEARGS) DEBUG="$(DEBUG)"

terminfos:
	cd $(TERMINFODIR); $(MAKE) $(MAKEARGS) DEBUG="$(DEBUG)"

lint:
	for d in $(PLACES); \
	do \
		cd $$d; \
		$(MAKE) $(MAKEARGS) DEBUG="$(DEBUG)" FUNCDCL="$(FUNCDCL)" LINT="$(LINT)" LINTFLAGS="$(LINTFLAGS)" lint
		cd ..; \
	done

lintsrc:
	cd $(LIBDIR); $(MAKE) $(MAKEARGS) DEBUG="$(DEBUG)" FUNCDCL="$(FUNCDCL)" LINT="$(LINT)" LINTFLAGS="$(LINTFLAGS)" lintsrc

lintlib:
	cd $(LIBDIR); $(MAKE) $(MAKEARGS) DEBUG="$(DEBUG)" FUNCDCL="$(FUNCDCL)" LINT="$(LINT)" LINTFLAGS="$(LINTFLAGS)" lintlib
