#
# Makefile for mmcp(1)
#
#ident	"$Revision: 1.1 $"

include	$(ROOT)/usr/include/make/commondefs

# NOTE: we do NOT want mv/cp/ln compiled with shared libraries,
# because it makes it impossible to recover from moving shared
# libraries targets, dynamic linkers, etc. to a different place,
# perhaps for testing. Without at least one program that doesn't
# use shared librariesl, you have to boot a miniroot to recover.
# SO, do NOT change this back to shared libs.

CFILES= mmcp.c
TARGETS= mmcp
GLDOPTS=
#LCOPTS=-non_shared
# Link text and data close together.
#LLDOPTS=-nostdlib -L$(ROOT)/usr/lib/nonshared -Wl,-T,10000000,-D,10040000
#LLDLIBS=-lc_nomp

default: $(TARGETS)

include $(COMMONRULES)

install: default
