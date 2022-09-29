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
# $Header: /proj/irix6.5.7m/isms/eoe/cmd/lp_svr4/filter/postscript/postplot/RCS/postplot.mk,v 1.1 1992/12/14 13:19:23 suresh Exp $
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"@(#)lp:filter/postscript/postplot/postplot.mk	1.2"
#
# postplot makefile
#

MAKEFILE=postplot.mk
ARGS=all

#
# Common header and source files have been moved to $(COMMONDIR).
#

COMMONDIR=../common

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

CFILES=postplot.c \
       $(COMMONDIR)/request.c \
       $(COMMONDIR)/glob.c \
       $(COMMONDIR)/misc.c

HFILES=postplot.h \
       $(COMMONDIR)/comments.h \
       $(COMMONDIR)/ext.h \
       $(COMMONDIR)/gen.h \
       $(COMMONDIR)/path.h

POSTPLOT=postplot.o\
       $(COMMONDIR)/request.o\
       $(COMMONDIR)/glob.o\
       $(COMMONDIR)/misc.o

ALLFILES=README $(MAKEFILE) $(HFILES) $(CFILES)


all : postplot

install : postplot
	@if [ ! -d "$(BINDIR)" ]; then \
	    mkdir $(BINDIR); \
	    $(CH)chmod 775 $(BINDIR); \
	    $(CH)chgrp $(GROUP) $(BINDIR); \
	    $(CH)chown $(OWNER) $(BINDIR); \
	fi
	$(INSTALL) -c -s -m 775 -o $(OWNER) -g $(GROUP) postplot $(BINDIR) 
#	cp postplot $(BINDIR)
#	$(CH)chmod 775 $(BINDIR)/postplot
#	$(CH)chgrp $(GROUP) $(BINDIR)/postplot
#	$(CH)chown $(OWNER) $(BINDIR)/postplot

postplot : $(POSTPLOT)
	$(CC) $(CFLAGS) -o postplot $(POSTPLOT)

$(COMMONDIR)/glob.o : $(COMMONDIR)/glob.c $(COMMONDIR)/gen.h
	cd $(COMMONDIR); $(CC) $(CFLAGS) -c glob.c

$(COMMONDIR)/misc.o : $(COMMONDIR)/misc.c $(COMMONDIR)/ext.h $(COMMONDIR)/gen.h
	cd $(COMMONDIR); $(CC) $(CFLAGS) -c misc.c

$(COMMONDIR)/request.o : $(COMMONDIR)/request.c $(COMMONDIR)/ext.h $(COMMONDIR)/gen.h $(COMMONDIR)/path.h
	cd $(COMMONDIR); $(CC) $(CFLAGS) -c request.c

postplot.o : $(HFILES)

clean :
	rm -f $(POSTPLOT)

clobber : clean
	rm -f postplot

list :
	pr -n $(ALLFILES) | $(LIST)

