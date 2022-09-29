#!smake
#
# Makefile for cmd/clshm, the support routines for inter-partition
# shared-memory communication over CrayBlink
#
# $Revision: 1.2 $

include $(ROOT)/usr/include/make/commondefs

SUBDIRS=
TARGETS=clshmctl clshmd clshm_diag
LCOPTS=-fullwarn -g

MACHTAGS=-idb "mach(CPUBOARD=IP27)"

default: $(TARGETS)

COMMONPREF=clshm
include $(COMMONRULES)

#clshm_diag: clshm_diag.c
#	$(CCF) clshm_diag.c $(LDFLAGS) -o clshm_diag -lclshm

install $(COMMONTARGS): $(COMMONPREF)$$@

$(COMMONPREF)install: default 
	$(INSTALL) $(MACHTAGS) -F /usr/etc $(TARGETS)
#	$(INSTALL) -idb "std.sw.unix" $(MACHTAGS) -F /usr/etc $(TARGETS)
#	$(INSTALL) -idb "std.sw.unix" -idb "config(noupdate)" \
#		$(MACHTAGS) -m 644 -F /etc/config -src clshmd.config clshmd


$(SUBDIRS): $(_FORCE)
	@$(ECHO) "====\t$@"; cd $@; $(MAKE)
