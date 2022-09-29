#ident  "cmd/bc/Makefile: $Revision: 1.24 $"
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

# This file is part of GNU bc, as ported to Silicon Graphics systems.

include $(ROOT)/usr/include/make/commondefs

TARGETS=libmath.h bc dc

INSTALL_PROGRAM = $(INSTALL)
INSTALL_SRC     = $(INSTALL)

SOURCEDIR=/usr/share/src/gnutools/bc

LDIRT=scan.c y.tab.h bc.c libmath.h

LIBS=-ll

LCFLAGS=-DDOT_IS_LAST

DISTFILES=Makefile COPYING bc.y bcdefs.h config.h const.h version.h \
            execute.c global.c global.h load.c main.c number.c storage.c \
	    number.h proto.h scan.l util.c \
	    fix_libmath.h libmath.b \
	    dc-array.c dc-eval.c dc-misc.c dc-number.c dc-stack.c \
	    dc-string.c dc.h dc-proto.h dc-regdef.h dc-version.h

BCOFILES=scan.o util.o main.o number.o storage.o load.o execute.o \
           getopt.o getopt1.o
DCOFILES = dc-misc.o dc-eval.o dc-stack.o dc-array.o dc-string.o dc-number.o


default: 
	smake libmath.h
	smake bc dc

bc: $& config.h bc.o $(BCOFILES) global.o
	$(CCF) -o bc bc.o $(BCOFILES) global.o $(LIBS)

dc: $(DCOFILES) number.o
	$(CCF) -o dc $(DCOFILES) number.o

libmath.h: libmath.b
	$(MAKE) fbc
	./fbc -c libmath.b </dev/null >libmath.h
	sh fix_libmath.h
	rm -f ./fbc ./global.o

fbc: $(BCOFILES) bc.o
	echo \"\" > libmath.h
	$(CCF) -c global.c
	$(CCF) -o fbc bc.o $(BCOFILES) global.o $(LIBS)

global.c: libmath.h

scan.c: scan.l y.tab.h
	$(LEXF) scan.l
	mv lex.yy.c scan.c

# since both y.tab.h and bc.c both use the default yacc targets (y.tab.c)
# only one can run at a time. Make them linear.
bc.c: y.tab.h
y.tab.h bc.c: bc.y
	@echo "expect 1 shift/reduce conflict"
	$(YACCF) -d bc.y
	mv y.tab.c bc.c

install: default ${BCSOURCE}
	$(INSTALL_PROGRAM) -F /usr/bin bc
	$(INSTALL_PROGRAM) -F /usr/bin dc
	$(INSTALL) -F /usr/lib libmath.b
	$(INSTALL_SRC) -m 755 -dir $(SOURCEDIR)
	$(INSTALL_SRC) -m 444 -F $(SOURCEDIR) $(DISTFILES)

include $(COMMONRULES)
