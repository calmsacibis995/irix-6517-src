#!smake
#
# Makefile for libtrace.a
#
#ident "$Header: "

BASELIB = libtrace.a
ALTLIBS = libtrace_n32_M3.a

include $(ROOT)/usr/include/make/librootdefs

TARGETS = libtrace_n32_M3.a

include $(LIBROOTRULES)

default: $(TARGETS)

exports install: default
	$(INSTALL) -F /usr/lib32/internal -m 444 -idb "noship" \
		-src libtrace_n32_M3.a libtrace.a

