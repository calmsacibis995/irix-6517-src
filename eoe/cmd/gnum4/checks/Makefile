# Generated automatically from Makefile.in by configure.
# Makefile for GNU m4 checks directory.
# Copyright (C) 1992, 1993, 1994 Free Software Foundation, Inc.

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

PRODUCT = m4
VERSION = 1.4

SHELL = /bin/sh

srcdir = .

# Should be GNU awk, for the get-them script.
AWK = nawk

# Vern says that the first star is required around an Alpha make bug.
CHECKS = *[0-9][0-9].*
DISTFILES = Makefile.in get-them check-them stamp-checks incl.m4 foo

all: stamp-checks

stamp-checks: get-them ../doc/m4.texinfo
	rm -f $(srcdir)/$(CHECKS)
	cd $(srcdir) && AWK=$(AWK) ./get-them ../doc/m4.texinfo
	touch $(srcdir)/stamp-checks

install:

uninstall:

check: stamp-checks
	PATH=`pwd`/../src:$$PATH; export PATH; \
	cd $(srcdir) && ./check-them $(CHECKS)

tags:

mostlyclean:

clean: mostlyclean

distclean: clean
	rm -f Makefile

realclean: distclean
	rm -f $(CHECKS) stamp-checks

dist: $(DISTFILES)
	@echo "Copying distribution files"
	@for file in $(DISTFILES); do \
	  ln $(srcdir)/$$file ../$(PRODUCT)-$(VERSION)/checks 2> /dev/null \
	    || cp -p $(srcdir)/$$file ../$(PRODUCT)-$(VERSION)/checks; \
	done
	@for file in $(srcdir)/$(CHECKS); do \
	  ln $$file ../$(PRODUCT)-$(VERSION)/checks \
	    || cp -p $$file ../$(PRODUCT)-$(VERSION)/checks; \
	done

Makefile: Makefile.in ../config.status
	cd .. && CONFIG_FILES=checks/$@ CONFIG_HEADERS= ./config.status

# Tell versions [3.59,3.63) of GNU make not to export all variables.
# Otherwise a system limit (for SysV at least) may be exceeded.
.NOEXPORT:
