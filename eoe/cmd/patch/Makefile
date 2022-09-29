#ident  "cmd/patch/Makefile: $Revision: 1.5 $"
# Copyright (C) 1993, 1994 Free Software Foundation, Inc.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

# This file is part of GNU patch, as ported to Silicon Graphics systems.

include $(ROOT)/usr/include/make/commondefs

INSTALL_SRC     = $(INSTALL)
INSTALL_PROGRAM = $(INSTALL)

SOURCEDIR=/usr/share/src/gnutools/patch

#CFLAGS = -g
#LDFLAGS = -g
LCFLAGS= -I.

SHELL = /bin/sh

SRCS = backupfile.c getopt.c getopt1.c inp.c patch.c pch.c util.c \
	version.c rename.c alloca.c
OBJS = backupfile.o getopt.o getopt1.o inp.o patch.o pch.o util.o \
	version.o alloca.o  
HDRS = EXTERN.h INTERN.h backupfile.h common.h getopt.h \
	inp.h patchlevel.h pch.h util.h version.h
MISC = COPYING ChangeLog INSTALL Makefile.in README NEWS \
	configure configure.in config.h.in 
DISTFILES = $(MISC) $(SRCS) $(HDRS) Makefile config.h

default all: patch 

include $(COMMONRULES)

patch: $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) $(LIBS) -o $@

install: all
	$(INSTALL_PROGRAM) -F /usr/sbin patch
	$(INSTALL_SRC) -m 755 -dir $(SOURCEDIR)
	$(INSTALL_SRC) -m 444 -F $(SOURCEDIR) $(DISTFILES)
