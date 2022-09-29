#!smake
#
# Makefile for libmem.a
#
#ident "$Header: "

BASELIB = libmem.a
ALTLIBS = libmem_n32_M3.a

include $(ROOT)/usr/include/make/librootdefs

TARGETS = libmem_n32_M3.a

include $(LIBROOTRULES)

default: $(TARGETS)

exports install: default
	$(INSTALL) -F /usr/lib32/internal -m 444 -idb "noship" \
		-src libmem_n32_M3.a libmem.a

