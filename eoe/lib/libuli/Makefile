#!smake
#
# Makefile for libuli.
#
#ident	"$Revision: 1.2 $"

BASELIB=libuli_32.a
ALTLIBS=libuli_n32.a libuli_64.a

NOSO=no-thank-you
include	$(ROOT)/usr/include/make/librootdefs

TARGETS=$(BASELIB) $(ALTLIBS)

default: $(TARGETS)

include $(LIBROOTRULES)

exports install! default
	$(INST_32) -m 444 -src libuli_32.a libuli.a
	$(INST_N32) -m 444 -src libuli_n32.a libuli.a
	$(INST_64) -m 444 -src libuli_64.a libuli.a
