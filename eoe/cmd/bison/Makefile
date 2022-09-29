#!/usr/sbin/smake
#
# Makefile for irix/cmd/bison
#

# Via a small amount of artifice in this Makefile we've managed to use the
# virgin bison sources with only a single small change in files.c to support
# finding bison.simple and bison.hairy relative to $ROOT.  If any non-SGI-
# specific changes are needed in the bison sources please communicate them
# immediately to the current bison maintainer so we can more easily upgrade
# to future releases.  The README file in the sources describes where to send
# bug reports.
#
SRCDIR=bison-1.25

# We use /usr/lib for SHARELIBDIR instead of /usr/share/lib because using the
# later introduces some conflict with the spec/idb nightmare that insists that
# we don't have /usr/share and non-share files coming out of the same
# subsystems.  Since the only thing we put in SHARELIBDIR are the bison.simple
# and bison.hairy templates and since bison is currently the only such problem
# child, we just toss them into /usr/lib ...
# 
BINDIR=/usr/bin
SHARELIBDIR=/usr/lib
SHARESRCDIR=/usr/share/src/gnutools/bison
CAT1DIR=/usr/share/catman/p_man/cat1

# We have to pass the datadir definition down to the bison Makefile because
# it doesn't follow the autoconfigure rules.  The datadir definition
# is used by bison to find bison.simple and bison.hairy.  All the other
# destination directory configuration is unimportant since we do the
# installations here.
#
CONFIGUREFLAGS=--datadir=$(SHARELIBDIR)
LWOFF=,1009
MAKEFLAGS=datadir=$(SHARELIBDIR) CC=$(CC) CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)"

include	$(ROOT)/usr/include/make/commondefs

default: touchconfig build/bison.z
	cd build; \
		$(MAKE) $(MAKEFLAGS) bison

TMP=tmp$$$$
build/bison.1: touchconfig
build/bison.z: build/bison.1
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
		CC=$(CC) ./configure $(CONFIGUREFLAGS); \
		$(MAKE) $(MAKEFLAGS) clean
	touch touchconfig

COMMONPREF=x
include $(COMMONRULES)

headers: touchconfig
	cd build; \
		$(INSTALL) -F $(SHARELIBDIR) -m 444 bison.simple bison.hairy

install: default headers
	cd build; \
		$(INSTALL) -F $(BINDIR) bison; \
		$(INSTALL) -F $(CAT1DIR) -m 444 bison.z
	rm -rf dist
	$(TLINK) $(SRCDIR) dist
	cd dist; \
		$(INSTALL) -F $(SHARESRCDIR) -m 444 \
			`find . -type l -print | sed 's;\./;;'`

clean: $(COMMONPREF)$@ touchconfig
	cd build; $(MAKE) $(MAKEFLAGS) clean
	rm -f build/bison.z

clobber: $(COMMONPREF)$@
	rm -rf touchconfig build dist
