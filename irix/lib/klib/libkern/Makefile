#!smake
#
# Makefile for libkern.a
#
#ident "$Header: "

BASELIB = libkern.a
ALTLIBS = libkern_n32_M3.a

include $(ROOT)/usr/include/make/librootdefs

TARGETS = libkern_n32_M3.a

include $(LIBROOTRULES)

default: $(TARGETS)

exports install: default
	$(INSTALL) -F /usr/lib32/internal -m 444 -idb "noship"  \
		-src libkern_n32_M3.a libkern.a


