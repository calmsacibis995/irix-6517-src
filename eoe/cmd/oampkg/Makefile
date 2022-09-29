#!smake
#
#	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.
#	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T
#	  All Rights Reserved
#
#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF
#	UNIX System Laboratories, Inc.
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.
#
#ident	"@(#)oampkg:oampkg.mk	1.5"
#
# Makefile for pkgs
#
#	"$Revision: 1.1 $"

include $(ROOT)/usr/include/make/commondefs


SUBDIRS=\
	libinst pkgadd pkginstall pkgrm pkgremove \
	pkginfo pkgproto pkgchk pkgmk pkgscripts \
	installf pkgtrans

.PRECIOUS: $(SUBDIRS)


COMMONPREF=pkgs


default install $(COMMONTARGS):
	@for i in $(SUBDIRS); do\
		echo "====\tcd $$i; $(MAKE) $@"; \
		cd $$i; $(MAKE) $@; cd ..; \
	done


$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

include $(COMMONRULES)
