#!/usr/sbin/smake
#
# Makefile for irix/cmd/flex
#

# Via a small amount of artifice in this Makefile we've managed to use the
# virgin flex sources.  If any non-SGI-specific changes are needed in the
# flex sources please communicate them immediately to the current flex
# maintainer so we can more easily upgrade to future releases.  The README
# file in the sources describes where to send bug reports.
#
SRCDIR=flex-2.5.4

BINDIR=/usr/bin
INCDIR=/usr/include
SHARESRCDIR=/usr/share/src/gnutools/flex
CAT1DIR=/usr/share/catman/p_man/cat1

# bison is now defined in commondefs.
#BISON=$(TOOLROOT)/usr/bin/bison

# We don't bother passing destination directory definitions down to the
# flex configure or Makefile since we do all the installations from this
# Makefile and flex doesn't use any ancillary files that it might need
# to find ...
#
CONFIGUREFLAGS=
LWOFF=,1110
MAKEFLAGS=CC=$(CC) CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)"

include	$(ROOT)/usr/include/make/commondefs

# We need to do the ``make .bootstrap'' because the flex Makefile doesn't
# have an explicit dependency for scan.c on .bootstrap.  As a result, this
# generates a race condition for parallel makes and scan.c tends not to get
# made in time for the cc command that tries to compile it ...
# 
default: touchconfig build/flex.z
	cd build; \
		$(MAKE) $(MAKEFLAGS) .bootstrap; \
		$(MAKE) $(MAKEFLAGS)

TMP=tmp$$$$
build/flex.1: touchconfig
build/flex.z: build/flex.1
	neqn $> | tbl | nroff -man > $(TMP); \
		pack -f $(TMP); \
		mv $(TMP).z $@

# We do the ``touch configure'' in the following rule to make sure that
# configure has a later date than configure.in.  If configure.in were to
# appear newer, then the build/Makefile would attempt to rebuild
# configure from it via the GNU autoconf untility ...
touchconfig:
	rm -rf build
	$(TLINK) $(SRCDIR) build
	cd build; \
		touch configure; \
		CC=$(CC) YACC="$(BISON) -y" ./configure $(CONFIGUREFLAGS); \
		$(MAKE) $(MAKEFLAGS) clean
	touch touchconfig

COMMONPREF=x
include $(COMMONRULES)

# We should really install build/libfl.a but then we'd have to get into
# the whole problem of compiling O32, N32, and N64 versions and frankly its
# not worth the effort right now since libfl.a simply contains the tiny
# stub routines yywrap(), yylex(), and a trivial main() that calls yylex()
# in a loop.  However, if we ever do distribute this as part of the IRIX EOE
# we'll probably have to tackle this problem ...
#
install: default
	cd build; \
		$(INSTALL) -F $(BINDIR) flex; \
		$(INSTALL) -F $(BINDIR) -lns flex flex++; \
		$(INSTALL) -F $(INCDIR) -m 444 FlexLexer.h; \
		$(INSTALL) -F $(CAT1DIR) -m 444 flex.z
	rm -rf dist
	$(TLINK) $(SRCDIR) dist
	cd dist; \
		$(INSTALL) -F $(SHARESRCDIR) -m 444 \
			`find . -type l -print | sed 's;\./;;'`

clean: $(COMMONPREF)$@ touchconfig
	cd build; $(MAKE) $(MAKEFLAGS) clean
	rm -f build/flex.z

clobber: $(COMMONPREF)$@
	rm -rf touchconfig build dist
