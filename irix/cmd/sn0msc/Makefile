#!smake
#
# Makefile for sn0msc, for accessing system controller functions
#
# $Revision: 1.1 $
#

# BASEVERSION=32bit

include $(ROOT)/usr/include/make/commondefs

TARGETS=    sn0msc
LLCOPTS=
LLDOPTS=    -Wl,-I,$(ROOTRLDNAME)

LMKDEPFLAGS= $(NULLSUFFIX_MKDEPFLAG)

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -idb "std.sw.unix" -idb "mach(CPUBOARD=IP27)" \
		-F /usr/sbin sn0msc
