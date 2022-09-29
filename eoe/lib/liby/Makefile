#!smake
#
# Makefile for liby.
#
#ident	"$Revision: 1.34 $"

BASELIB=liby_32.a
ALTLIBS=liby_32_ns.a \
	liby_64.a liby_64_ns.a \
	liby_n32.a liby_n32_ns.a
NOSO=-no-thank-you

include	$(ROOT)/usr/include/make/librootdefs

default: $(TARGETS)

headers!
	$(INST_32) -m 444 yaccpar
	$(INST_64) -m 444 yaccpar
	$(INST_N32) -m 444 yaccpar

exports install! default
	$(INST_32) -m 444 yaccpar
	$(INST_32) -m 444 -src liby_32.a liby.a
	$(INST_32_NS) -m 444 -idb "nostrip" -src liby_32_ns.a liby.a
	$(INST_64) -m 444 yaccpar
	$(INST_64) -m 444 -src liby_64.a liby.a
	$(INST_64_NS) -m 444 -idb "nostrip" -src liby_64_ns.a liby.a
	$(INST_N32) -m 444 yaccpar
	$(INST_N32) -m 444 -src liby_n32.a liby.a
	$(INST_N32_NS) -m 444 -idb "nostrip" -src liby_n32_ns.a liby.a

include $(LIBROOTRULES)
