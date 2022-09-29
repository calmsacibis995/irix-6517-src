#!smake
#
# Makefile for libsym.a
#
#ident "$Header: "

BASELIB = libsym.a
ALTLIBS = libsym_n32_M3.a

include $(ROOT)/usr/include/make/librootdefs

TARGETS = libsym_n32_M3.a

include $(LIBROOTRULES)

default: $(TARGETS)

exports install: default
	$(INSTALL) -F /usr/lib32/internal -m 444 -idb "noship" \
		-src libsym_n32_M3.a libsym.a

