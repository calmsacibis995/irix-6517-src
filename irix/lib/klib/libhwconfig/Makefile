#!smake
#
# Makefile for libhwconfig.a
#
#ident "$Header: "

BASELIB = libhwconfig.a
ALTLIBS = libhwconfig_n32_M3.a

include $(ROOT)/usr/include/make/librootdefs

TARGETS = libhwconfig_n32_M3.a

include $(LIBROOTRULES)

default: $(TARGETS)

exports install: default
	$(INSTALL) -F /usr/lib32/internal -m 444 -idb "noship" \
		-src libhwconfig_n32_M3.a libhwconfig.a

