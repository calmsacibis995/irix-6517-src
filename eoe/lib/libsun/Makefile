#!smake
#
# Makefile for a null libsun.
#
#ident	"$Revision: 1.6 $"

BASELIB=libsun.a
ALTLIBS=libsun_n32_M3.a
# Don't make a libsun.so
NOSO=no-thank-you

include	$(ROOT)/usr/include/make/librootdefs

default: $(TARGETS)

include $(LIBROOTRULES)

exports install: default
	$(INST_32) -m 444 libsun.a
	$(INST_N32) -m 444 -src libsun_n32_M3.a libsun.a
