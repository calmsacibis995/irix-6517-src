#!smake
#
# Makefile for libutil.a
#
#ident "$Header: "

BASELIB = libutil.a
ALTLIBS = libutil_n32_M3.a

include $(ROOT)/usr/include/make/librootdefs

TARGETS = libutil_n32_M3.a

include $(LIBROOTRULES)

default: $(TARGETS)

exports install: default
	$(INSTALL) -F /usr/lib32/internal -m 444 -idb "noship" \
		-src libutil_n32_M3.a libutil.a
