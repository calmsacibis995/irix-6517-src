# Generated automatically from Makefile.in by configure.
# Makefile.in generated automatically by automake 1.1l from Makefile.am

# Copyright (C) 1994, 1995, 1996 Free Software Foundation, Inc.
# This Makefile.in is free software; the Free Software Foundation
# gives unlimited permission to copy, distribute and modify it.


SHELL = /bin/sh

srcdir = .
top_srcdir = ../..
prefix = /usr/gnu
exec_prefix = ${prefix}

bindir = ${exec_prefix}/bin
sbindir = ${exec_prefix}/sbin
libexecdir = ${exec_prefix}/libexec
datadir = ${prefix}/share
sysconfdir = ${prefix}/etc
sharedstatedir = ${prefix}/com
localstatedir = ${prefix}/var
libdir = ${exec_prefix}/lib
infodir = ${prefix}/info
mandir = ${prefix}/man
includedir = ${prefix}/include
oldincludedir = /usr/include

pkgdatadir = $(datadir)/textutils
pkglibdir = $(libdir)/textutils
pkgincludedir = $(includedir)/textutils

top_builddir = ../..

INSTALL = install -c
INSTALL_PROGRAM = ${INSTALL}
INSTALL_DATA = ${INSTALL} -m 644
INSTALL_SCRIPT = ${INSTALL_PROGRAM}
transform = s,x,x,
RANLIB = :
GENCAT = 
GMSGFMT = msgfmt
CC = cc -n32
TR = ../tr

x = tr
explicit =
maint_gen = t1.in t1.exp t2.in t2.exp t3.in t3.exp t4.in t4.exp t5.in t5.exp \
t6.in t6.exp t7.in t7.exp t8.in t8.exp t9.in t9.exp ta.in ta.exp tb.in tb.exp \
tc.in tc.exp td.in td.exp te.in te.exp tf.in tf.exp tg.in tg.exp th.in th.exp \
ti.in ti.exp tj.in tj.exp tk.in tk.exp tl.in tl.exp tm.in tm.exp tn.in tn.exp \
to.in to.exp tp.in tp.exp tq.in tq.exp tr.in tr.exp ts.in ts.exp tt.in tt.exp \
tu.in tu.exp tv.in tv.exp tw.in tw.exp tx.in tx.exp ty.in ty.exp tz.in tz.exp \
trange-a-a.in trange-a-a.exp tnull.in tnull.exp tupcase.in tupcase.exp \
tdncase.in tdncase.exp trep-cclass.in trep-cclass.exp trep-1.in trep-1.exp \
trep-2.in trep-2.exp trep-3.in trep-3.exp tesc.in tesc.exp tross-0a.in \
tross-0a.exp tross-0b.in tross-0b.exp tross-1a.in tross-1a.exp tross-1b.in \
tross-1b.exp tross-2.in tross-2.exp tross-3.in tross-3.exp tross-4.in \
tross-4.exp tross-5.in tross-5.exp tross-6.in tross-6.exp
run_gen = t1.out t1.err t2.out t2.err t3.out t3.err t4.out t4.err t5.out \
t5.err t6.out t6.err t7.out t7.err t8.out t8.err t9.out t9.err ta.out ta.err \
tb.out tb.err tc.out tc.err td.out td.err te.out te.err tf.out tf.err tg.out \
tg.err th.out th.err ti.out ti.err tj.out tj.err tk.out tk.err tl.out tl.err \
tm.out tm.err tn.out tn.err to.out to.err tp.out tp.err tq.out tq.err tr.out \
tr.err ts.out ts.err tt.out tt.err tu.out tu.err tv.out tv.err tw.out tw.err \
tx.out tx.err ty.out ty.err tz.out tz.err trange-a-a.out trange-a-a.err \
tnull.out tnull.err tupcase.out tupcase.err tdncase.out tdncase.err \
trep-cclass.out trep-cclass.err trep-1.out trep-1.err trep-2.out trep-2.err \
trep-3.out trep-3.err tesc.out tesc.err tross-0a.out tross-0a.err \
tross-0b.out tross-0b.err tross-1a.out tross-1a.err tross-1b.out tross-1b.err \
tross-2.out tross-2.err tross-3.out tross-3.err tross-4.out tross-4.err \
tross-5.out tross-5.err tross-6.out tross-6.err

EXTRA_DIST = mk-script.pl Test.pm $x-tests $(explicit) $(maint_gen)
noinst_SCRIPTS = $x-tests

PERL = /bin/perl
editpl = sed -e 's,@''PERL''@,$(PERL),g' -e 's,@''srcdir''@,.,g'

TESTS = $x-tests

SUFFIXES = .pl

CLEANFILES = $(run_gen)
mkinstalldirs = $(SHELL) $(top_srcdir)/mkinstalldirs
CONFIG_HEADER = ../../config.h
CONFIG_CLEAN_FILES = 
SCRIPTS =  $(noinst_SCRIPTS)

DIST_COMMON =  Makefile.am Makefile.in TODO


PACKAGE = textutils
VERSION = 1.22

DISTFILES = $(DIST_COMMON) $(SOURCES) $(HEADERS) \
	$(TEXINFOS) $(MANS) $(EXTRA_DIST)

TAR = tar
default: test

.SUFFIXES:
.SUFFIXES: .pl

test check: tr-tests
	./tr-tests $(TR)

clean-generic:
	test -z "$(CLEANFILES)" || rm -f $(CLEANFILES)

clean:  clean-generic


tr-tests: mk-script.pl Test.pm
	perl5 ./mk-script.pl $(TR) > $@.tmp
	chmod 755 $@.tmp
	mv -f $@.tmp $@

#.pl:
#	rm -f $@ $@.tmp
#	$(editpl) $< > $@.tmp
#	chmod +x-w $@.tmp
#	mv $@.tmp $@


# Tell versions [3.59,3.63) of GNU make to not export all variables.
# Otherwise a system limit (for SysV at least) may be exceeded.
.NOEXPORT:
