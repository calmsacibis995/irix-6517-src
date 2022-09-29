#!smake
#
# Makefile for sysctlrd, the system controller daemon
#
# $Revision: 1.3 $
#

# BASEVERSION=32bit

include $(ROOT)/usr/include/make/commondefs

TARGETS=    linkstat
LLCOPTS=
LLDOPTS=    -Wl,-I,$(ROOTRLDNAME)

LMKDEPFLAGS= $(NULLSUFFIX_MKDEPFLAG)

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -idb "std.sw.unix" -idb "mach(CPUBOARD=IP27)" \
		-F /usr/sbin linkstat

