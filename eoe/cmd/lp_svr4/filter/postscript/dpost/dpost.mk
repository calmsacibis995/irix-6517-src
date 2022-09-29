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
# $Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/filter/postscript/dpost/RCS/dpost.mk,v 1.1 1992/12/14 13:14:38 suresh Exp $
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"@(#)lp:filter/postscript/dpost/dpost.mk	1.2"
#
# makefile for the troff post-processor for PostScript printers.
#

MAKEFILE=dpost.mk
ARGS=all

#
# Common header and source files have been moved to $(COMMONDIR).
#

COMMONDIR=../common

#
# There are potential conflicts on systems running DWB2.0. dpost is included in
# that package and installed in /usr/bin. If DWB_DPOST exists on this system and
# you're doing an install we'll replace it with the new program.
#

DWB_DPOST=/usr/bin/dpost

#
# dpost uses some floating point arithmetic, so if you're running on a system
# without floating point hardware add the -f option to the definition of CFLAGS.
#

SHELL   =/bin/sh
ETCDIR  =/etc

DESTROOT=

DEBUG	=-O2
MAXOPT	=$(DEBUG)
INCLUDE =-DSYSV -I$(COMMONDIR)
STDC	=
CFLAGS  =-systype svr4 -D_RISCOS $(STDC) $(MAXOPT) $(INCLUDE) $(ECFLAGS)
LFLAGS  =$(ELFLAGS)
YFLAGS  =$(EYFLAGS)
AR	= ar
ARFLAGS =crs

INSTALL =$(ETCDIR)/mipsinstall

CFILES=dpost.c draw.c color.c pictures.c ps_include.c\
       $(COMMONDIR)/glob.c \
       $(COMMONDIR)/misc.c \
       $(COMMONDIR)/request.c \
       $(COMMONDIR)/tempnam.c

HFILES=dpost.h ps_include.h \
       $(COMMONDIR)/comments.h \
       $(COMMONDIR)/dev.h \
       $(COMMONDIR)/ext.h \
       $(COMMONDIR)/gen.h \
       $(COMMONDIR)/path.h

DPOST=dpost.o draw.o color.o pictures.o ps_include.o\
       $(COMMONDIR)/glob.o\
       $(COMMONDIR)/misc.o\
       $(COMMONDIR)/request.o \
       $(COMMONDIR)/tempnam.o

ALLFILES=README $(MAKEFILE) $(HFILES) $(CFILES)


all : dpost

install : dpost
	@if [ ! -d "$(BINDIR)" ]; then \
	    mkdir $(BINDIR); \
	    $(CH)chmod 775 $(BINDIR); \
	    $(CH)chgrp $(GROUP) $(BINDIR); \
	    $(CH)chown $(OWNER) $(BINDIR); \
	fi
	$(INSTALL) -c -s  -m 775 -o $(OWNER) -g $(GROUP) dpost $(BINDIR)
#	cp dpost $(BINDIR)
#	chmod 775 $(BINDIR)/dpost
#	chgrp $(GROUP) $(BINDIR)/dpost
#	chown $(OWNER) $(BINDIR)/dpost
#	if [ -f "$(DWB_DPOST)" -a -w "$(DWB_DPOST)" -a -x "$(DWB_DPOST)" ]; then \
#	    cp dpost $(DWB_DPOST); \
#	fi

dpost : $(DPOST)
	$(CC) $(CFLAGS) -o dpost $(DPOST) -lm

$(COMMONDIR)/glob.o : $(COMMONDIR)/glob.c $(COMMONDIR)/gen.h
	cd $(COMMONDIR); $(CC) $(CFLAGS) -c glob.c

$(COMMONDIR)/misc.o : $(COMMONDIR)/misc.c $(COMMONDIR)/ext.h $(COMMONDIR)/gen.h
	cd $(COMMONDIR); $(CC) $(CFLAGS) -c misc.c

$(COMMONDIR)/request.o : $(COMMONDIR)/request.c $(COMMONDIR)/ext.h $(COMMONDIR)/gen.h $(COMMONDIR)/path.h
	cd $(COMMONDIR); $(CC) $(CFLAGS) -c request.c

$(COMMONDIR)/tempnam.o : $(COMMONDIR)/tempnam.c
	cd $(COMMONDIR); $(CC) $(CFLAGS) -c tempnam.c

dpost.o : $(HFILES)
draw.o : $(COMMONDIR)/ext.h $(COMMONDIR)/gen.h
color.o : $(COMMONDIR)/ext.h $(COMMONDIR)/gen.h
pictures.o : $(COMMONDIR)/comments.h $(COMMONDIR)/gen.h

ps_include.o : ps_include.h
ps_include.h : ps_include.ps
	awk -f ps_include.awk ps_include.ps >ps_include.h

clean :
	rm -f $(DPOST)

clobber : clean
	rm -f dpost

list :
	pr -n $(ALLFILES) | $(LIST)

