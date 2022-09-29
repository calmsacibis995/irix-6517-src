#!smake
#
#ident	"$Revision: 1.1 $"
#

BASELIB=libhan_32.a
ALTLIBS=libhan_32_ns.a libhan_64.a libhan_64_ns.a \
	libhan_n32.a libhan_n32_ns.a

include	$(ROOT)/usr/include/make/librootdefs
TARGETS=$(LIBS)

default: $(TARGETS)

include $(LIBROOTRULES)

install: exports

exports: default
	$(INST_32) -m 444 -idb "nostrip" -src libhan_32.a libhan.a
	$(INST_32_NS) -m 444 -idb "nostrip" -src libhan_32_ns.a libhan.a
	$(INST_64) -m 444 -idb "nostrip" -src libhan_64.a libhan.a
	$(INST_64_NS) -m 444 -idb "nostrip" -src libhan_64_ns.a libhan.a
	$(INST_N32) -m 444 -idb "nostrip" -src libhan_n32.a libhan.a
	$(INST_N32_NS) -m 444 -idb "nostrip" -src libhan_n32_ns.a libhan.a
