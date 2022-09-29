#!smake

# Makefile for E-Plex EEPROM utility
#
#	$Revision: 1.13 $

OBJECT_STYLE=N32
include	$(ROOT)/usr/include/make/commondefs

SUBDIRS	=firm

# to avoid making the firmware simultaneously
.NOTPARALLEL:

LMKDEPFLAGS=$(NULLSUFFIX_MKDEPFLAG)

LCOPTS	=-fullwarn

CFILES	=epfirm.c epdump.c
TARGETS	=epfirm epdump

# build the firmware first
#ifndef SRC_PRD
default:sub_epfirm $(TARGETS)
#else
default:
#endif

COMMONPREF=epfirm
include $(COMMONRULES)

# Nothing in the subdirectory is installed.  If that ever changes,
# move the "install" target up to here.
$(COMMONTARGS) hex!$(COMMONPREF)$$@ .RECURSIVE
	@for d in $(SUBDIRS); do \
		$(ECHO) "\tcd $$d; $(MAKE) $@"; \
		cd $$d; $(MAKE) $@; cd ..; \
	done

# Build the firmware first, but do not rebuild epfirm unnecessarily.
epfirm:./firm/ep_b.firm ./firm/ep_c.firm ./firm/ep_flash.firm
./firm/ep_b.firm ./firm/ep_c.firm ./firm/ep_flash.firm:sub_epfirm
sub_epfirm $(SUBDIRS)!.RECURSIVE
	@for d in $(SUBDIRS); do \
		$(ECHO) "\tcd $$d; $(MAKE)"; \
		cd $$d; $(MAKE); cd ..; \
	done

install $(COMMONPREF)install:default
	$(INSTALL) -F /usr/etc 	\
		-idb "mach(CPUBOARD=IP19 CPUBOARD=IP21 CPUBOARD=IP25)" epfirm

$(COMMONPREF)hex:
	@#no hex files here
