#!smake
# Make the compress library
#
# $Revision: 1.2 $

BASELIB = libcomp_32.a
ALTLIBS=libcomp_64.a libcomp_n32.a
# Don't make a libcomp.so
NOSO=no-thank-you

include $(ROOT)/usr/include/make/librootdefs

LCOPTS = -signed

default! $(TARGETS)

include $(LIBROOTRULES)

install exports: default $(_FORCE)
	$(INST_32) -m 444 -idb "nostrip" -src libcomp_32.a internal/libcomp.a
	$(INST_64) -m 444 -idb "nostrip" -src libcomp_64.a internal/libcomp.a
	$(INST_N32) -m 444 -idb "nostrip" -src libcomp_n32.a internal/libcomp.a
