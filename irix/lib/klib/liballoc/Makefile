#!smake
#
# Makefile for liballoc.a
#
#ident "$Header: "

BASELIB = liballoc.a
ALTLIBS = liballoc_n32_M3.a

include $(ROOT)/usr/include/make/librootdefs

TARGETS = liballoc_n32_M3.a

include $(LIBROOTRULES)

default: $(TARGETS)

headers: 
	$(INSTALL) -F /usr/include/klib -m 444 -idb "noship" \
		-src src/alloc_private.h alloc_private.h

exports install: default
	$(INSTALL) -F /usr/lib32/internal -m 444 -idb "noship" \
		-src liballoc_n32_M3.a liballoc.a

