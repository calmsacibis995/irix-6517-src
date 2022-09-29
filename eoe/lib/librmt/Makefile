#!smake
#
#ident	"$Revision: 1.12 $"
#

BASELIB=librmt.a
ALTLIBS=librmt_ns.a librmt_n32.a librmt_n32_ns.a librmt_64.a librmt_64_ns.a

include	$(ROOT)/usr/include/make/librootdefs
TARGETS=$(LIBS)

default: $(TARGETS)

exports install: default
	# o32 shared librmt
	$(INST_32) -m 444 -idb "nostrip" librmt.a
	# o32 nonshared librmt
	$(INST_32_NS) -m 444 -idb "nostrip" -src librmt_ns.a librmt.a
	# 64bit shared librmt
	$(INST_64) -m 444 -idb "nostrip" -src librmt_64.a librmt.a
	# 64bit nonshared librmt
	$(INST_64_NS) -m 444 -idb "nostrip" -src librmt_64_ns.a librmt.a
	# n32 shared librmt
	$(INST_N32) -m 444 -idb "nostrip" -src librmt_n32.a librmt.a
	# n32 non_shared librmt
	$(INST_N32_NS) -m 444 -idb "nostrip" -src librmt_n32_ns.a librmt.a



include $(LIBROOTRULES)
