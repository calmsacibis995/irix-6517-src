#!smake
#
# Makefile for libklib.a
#
#ident "$Header: "

BASELIB = libklib.a
ALTLIBS = libklib_n32_M3.a

include $(ROOT)/usr/include/make/librootdefs

TARGETS = libklib_n32_M3.a

include $(LIBROOTRULES)

default: $(TARGETS)

exports install: default
	$(INSTALL) -F /usr/lib32/internal -m 444 -idb "noship" \
		-src libklib_n32_M3.a libklib.a


