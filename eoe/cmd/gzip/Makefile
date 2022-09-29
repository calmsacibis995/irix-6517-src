# Generated automatically from Makefile.in by configure.
# Makefile for gzip (GNU zip)    -*- Indented-Text -*-
# Copyright (C) 1992-1993 Jean-loup Gailly and the Free Software Foundation

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

# $Id: Makefile,v 1.9 1997/05/18 22:29:05 olson Exp $

#### Start of system configuration section. ####
include $(ROOT)/usr/include/make/mandefs

MANPREF=man
IDB_PATH=/usr/share/catman/u_man/cat1

srcdir = .

INSTALL_PROGRAM = $(INSTALL)
INSTALL_MAN     = $(INSTALL)
INSTALL_SRC     = $(INSTALL)

INST_SRC_PATH = /usr/share/src/gnutools/gzip

# Things you might add to DEFS (configure usually figures out what to do):
# -DGNU_STANDARD	Behave as gzip even if invoked as gunzip (GNU standard)
# -DDIRENT	        Use <dirent.h>  for recursion (-r)
# -DSYSDIR	        Use <sys/dir.h> for recursion (-r)
# -DSYSNDIR	        Use <sys/ndir.h> for recursion (-r)
# -DNDIR		Use <ndir.h> for recursion (-r)
# -DSTDC_HEADERS	Use <stdlib.h>
# -DHAVE_UNISTD_H	Use <unistd.h>
# -DNO_FCNTL_H	        Don't use <fcntl.h>
# -DNO_UTIME_H		Don't use <utime.h>
# -DHAVE_SYSUTIME_H	Use <sys/utime.h>
# -DNO_MEMORY_H	 	Don't use <memory.h>. Not needed if STDC_HEADERS.
# -DNO_STRING_H	 	Use strings.h, not string.h. Not needed if STDC_HEADERS
# -DRETSIGTYPE=int      Define this if signal handlers must return an int.
# -DNO_SYMLINK	  	OS defines S_IFLNK but does not support symbolic links
# -DNO_MULTIPLE_DOTS    System does not allow file names with multiple dots
# -DNO_UTIME		System does not support setting file modification time
# -DNO_CHOWN		System does not support setting file owner
# -DNO_DIR		System does not support readdir()
# -DPROTO		Force function prototypes even if __STDC__ not defined
# -DASMV		Use asm version match.S
# -DMSDOS		MSDOS specific
# -DOS2			OS/2 specific
# -DVAXC		Vax/VMS with Vax C compiler
# -DVMS			Vax/VMS with gcc
# -DDEBUG		Debug code
# -DDYN_ALLOC		Use dynamic allocation of large data structures
# -DMAXSEG_64K		Maximum array size is 64K (for 16 bit system)
# -DRECORD_IO	   	read() and write() are rounded to record sizes.
# -DNO_STDIN_FSTAT      fstat() is not available on stdin
# -DNO_FSTAT	    	fstat() is not available
# -DNO_SIZE_CHECK       stat() does not give a reliable file size

LCDEFS =  -DSTDC_HEADERS=1 -DHAVE_UNISTD_H=1 -DDIRENT=1

SEDCMD = 1d

G=g
# To install znew, zmore, etc... as gznew, gzmore... use: G=g

ZCAT=gzcat
# To install zcat executable and man page as gzcat, use: ZCAT=gzcat

bindir = /usr/sbin

TAR = tar

CFILES = gzip.c zip.c deflate.c trees.c bits.c unzip.c inflate.c util.c crypt.c\
       lzw.c unlzw.c unpack.c unlzh.c getopt.c

HFILES = gzip.h lzw.h tailor.h revision.h crypt.h getopt.h

MANPAGES= gzexe.mn.1 gzip.mn.1 gzip.mn.z zdiff.mn.1 zforce.mn.1 \
	  zgrep.mn.1 zmore.mn.1 znew.mn.1

MANCAT=$(MANPAGES:.1=.z)

LDIRT=$(MANCAT)

GENFILES =  README NEWS INSTALL Makefile.in configure.in configure COPYING \
  TODO THANKS ChangeLog $(CFILES) $(HFILES) zmore.in znew.in zdiff.in zgrep.in \
  zforce.in gzexe.in $(MANPAGES) \
  gzip.doc algorithm.doc gzip.texi texinfo.tex gpl.texinfo gzip.info

sampleFILES =  sample/makecrc.c sample/zread.c sample/add.c sample/sub.c \
       sample/ztouch sample/zfile

DISTFILES = $(GENFILES) $(sampleFILES) $(msdosFILES) $(os2FILES) $(ntFILES)\
   $(vmsFILES) $(amigaFILES) $(atariFILES) $(primosFILES)

SCRIPTS = $(G)zdiff $(G)zgrep $(G)zmore $(G)znew $(G)zforce gzexe

TARGETS=gzip $(G)zdiff $(G)zgrep $(G)zmore $(G)znew $(G)zforce gzexe

default:  $(TARGETS)

install: installbin installman installsrc

installbin: default
	$(INSTALL_PROGRAM) -F /usr/sbin gzip
	$(INSTALL_PROGRAM) -F /usr/sbin -lns gzip gunzip
	$(INSTALL_PROGRAM) -F /usr/sbin -lns gzip ungzip
	$(INSTALL_PROGRAM) -F /usr/sbin -lns gzip $(ZCAT)
	$(INSTALL_PROGRAM) -F /usr/sbin $(G)zdiff
	$(INSTALL_PROGRAM) -F /usr/sbin $(G)zgrep
	$(INSTALL_PROGRAM) -F /usr/sbin $(G)zmore
	$(INSTALL_PROGRAM) -F /usr/sbin $(G)znew
	$(INSTALL_PROGRAM) -F /usr/sbin $(G)zforce
	$(INSTALL_PROGRAM) -F /usr/sbin gzexe
	$(INSTALL_PROGRAM) -F /usr/sbin -lns $(G)zdiff $(G)zcmp

installman: $(MANCAT)
	$(INSTALL_MAN) -F /usr/share/catman/u_man/cat1 \
			-src gzip.mn.z gzip.z
	$(INSTALL_MAN) -F /usr/share/catman/u_man/cat1 \
			-src gzexe.mn.z gzexe.z
	$(INSTALL_MAN) -F /usr/share/catman/u_man/cat1 \
			-src zdiff.mn.z $(G)zdiff.z
	$(INSTALL_MAN) -F /usr/share/catman/u_man/cat1 \
			-src zforce.mn.z $(G)zforce.z
	$(INSTALL_MAN) -F /usr/share/catman/u_man/cat1 \
			-src zgrep.mn.z $(G)zgrep.z
	$(INSTALL_MAN) -F /usr/share/catman/u_man/cat1 \
			-src zmore.mn.z $(G)zmore.z
	$(INSTALL_MAN) -F /usr/share/catman/u_man/cat1 \
			-src znew.mn.z $(G)znew.z
	$(INSTALL_MAN) -F /usr/share/catman/u_man/cat1 \
			-lns gzip.z gunzip.z
	$(INSTALL_MAN) -F /usr/share/catman/u_man/cat1 \
			-lns gzip.z ungzip.z
	$(INSTALL_MAN) -F /usr/share/catman/u_man/cat1 \
			-lns gzip.z $(ZCAT).z 
	$(INSTALL_MAN) -F /usr/share/catman/u_man/cat1 \
			-lns $(G)zdiff.z $(G)zcmp.z

installsrc: 
	$(INSTALL_SRC) -m 755 -dir $(INST_SRC_PATH) 
	$(INSTALL_SRC) -m 444 -F $(INST_SRC_PATH) $(DISTFILES) Makefile 

include $(COMMONRULES)
include $(ROOT)/usr/include/make/manrules

dist: $(DISTFILES) Makefile
	d=gzip-`sed -e '/VERSION/!d' -e 's/[^0-9.]*\([0-9.]*\).*/\1/' \
		    -e q revision.h` ; \
	rm -f ../$$d; \
	ln -s `pwd` ../$$d; \
	cd ..; \
	files=""; \
	for f in $(DISTFILES); do files="$$files $$d/$$f"; done; \
	GZIP=-9 $(TAR) chofz $$d/$$d.tar.gz $$files; \
	rm -f $$d

zipdist: $(DISTFILES) Makefile
	zip -u9T gzip`sed -e '/VERSION/!d' -e 's/[^0-9.]*\([0-9.]*\).*/\1/' \
			  -e s/[.]//g -e q revision.h` $(DISTFILES)

# Actual build-related targets

gzip:	$(OBJECTS)
	$(CCF) -o $@ $(OBJECTS) $(LDFLAGS)

$(G)zdiff: zdiff.in
	sed -e "$(SEDCMD)" -e "s|BINDIR|$(bindir)|" $(srcdir)/zdiff.in > $@
	chmod 755 $@

$(G)zgrep: zgrep.in
	sed -e "$(SEDCMD)" -e "s|BINDIR|$(bindir)|" $(srcdir)/zgrep.in > $@
	chmod 755 $@

$(G)zmore: zmore.in
	sed -e "$(SEDCMD)" -e "s|BINDIR|$(bindir)|" $(srcdir)/zmore.in > $@
	chmod 755 $@

$(G)znew: znew.in
	sed -e "$(SEDCMD)" -e "s|BINDIR|$(bindir)|" $(srcdir)/znew.in > $@
	chmod 755 $@

$(G)zforce: zforce.in
	sed -e "$(SEDCMD)" -e "s|BINDIR|$(bindir)|" $(srcdir)/zforce.in > $@
	chmod 755 $@

gzexe: gzexe.in
	sed -e "$(SEDCMD)" -e "s|BINDIR|$(bindir)|" $(srcdir)/gzexe.in > $@
	chmod 755 $@

# gross hack; since this makefile violates standard sgi rules,
# and includes both the common and the man page make include files
# we need to avoid the man rules being applied to libc.so.1...
${ROOT}/usr/lib32/libc.so.p ${ROOT}/usr/lib32/mips3/libc.so.p \
	${ROOT}/usr/lib32/libc.so ${ROOT}/usr/lib32/mips3/libc.so:
	echo ignore the .so.1 to .p rule.

# end of file
