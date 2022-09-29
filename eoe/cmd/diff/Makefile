# Makefile for GNU DIFF
# Copyright (C) 1988, 1989 Free Software Foundation, Inc.

include $(ROOT)/usr/include/make/commondefs

# This file is part of GNU DIFF, as ported to Silicon Graphics systems.

# GNU DIFF is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 1, or (at your option)
# any later version.
# 
# GNU DIFF is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with GNU DIFF; see the file COPYING.  If not, write to
# the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
#
# Report problems and direct all questions to:
#
#       Your Silicon Graphics representative
#       or the Silicon Graphics Geometry Hotline.
#
# This Makefile has been radically altered by SGI from the one
# in the original 1.15 diff release distributed by the Free
# Software Foundation, to better fit the Makefile tools
# supplied by SGI.  Those porting this diff command to
# non-SGI systems should examine the original diff Makefile
# from FSF, as it contains instructions and options designed
# to facilitate ports to various systems.  A copy of the original
# Makefile is included in this source as: Makefile.gnu.orig
#
# Those modifying and porting diff for use on SGI systems
# should find this Makefile well suited for their needs.

LCDEFS=-Dsparc
LLDOPTS =-Wl,-woff,85 # don't complain about regex stuff overriding libc

USRBIN=/usr/bin

SOURCEDIR=/usr/share/src/gnutools/diff

TARGETS=diff diff3

CFILES=diff.c analyze.c io.c context.c ed.c normal.c ifdef.c util.c dir.c \
	version.c regex.c limits.h diff3.c alloca.c

HFILES=diff.h regex.h

DIFFOBJS=diff.o analyze.o io.o context.o ed.o normal.o util.o dir.o regex.o\
	ifdef.o version.o

DIFF3OBJS=diff3.o

GNUSRCFILES=$(CFILES) $(HFILES) README diagmeet.note \
	Makefile COPYING ChangeLog Makefile.gnu.orig Changes.gnu-sgi.README \
	Changes.gnu-sgi.diff.out  getopt.c getopt1.c getopt.h limits.h 

default: $(TARGETS)

include $(COMMONRULES)

diff3: diff3.o
	$(CCF) -o $@ $(DIFF3OBJS) $(LDFLAGS)

diff: $(DIFFOBJS)
	$(CCF) -o $@ $(DIFFOBJS) $(LDFLAGS)

diff3.o: diff3.c
	$(CCF) -c -DDIFF_PROGRAM=\"$(USRBIN)/diff\" diff3.c

install: default
	$(INSTALL) -F $(USRBIN) diff
	$(INSTALL) $(I_FLAGS) -F $(USRBIN) diff3
	$(INSTALL) -m 444 -F $(SOURCEDIR) "$(GNUSRCFILES)"
