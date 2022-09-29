#!smake
#
# Makefile for libswconfig.a
#
#ident "$Header: "

BASELIB = libswconfig.a
ALTLIBS = libswconfig_n32_M3.a

include $(ROOT)/usr/include/make/librootdefs

TARGETS = libswconfig_n32_M3.a

include $(LIBROOTRULES)

default: $(TARGETS)

exports install: default
	$(INSTALL) -F /usr/lib32/internal -m 444 -idb "noship" \
		-src libswconfig_n32_M3.a libswconfig.a

