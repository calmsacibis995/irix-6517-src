#!smake
# Generate BSD compatibility library.
#
#ident	"$Revision: 1.30 $"

BASELIB=libbsd_32.a
ALTLIBS=libbsd_32_ns.a libbsd_64.a libbsd_64_ns.a libbsd_n32_M3.a libbsd_n32_M3_ns.a
# Don't make a libbsd.so
NOSO=no-thank-you

include	$(ROOT)/usr/include/make/librootdefs

default! $(TARGETS)

include $(LIBROOTRULES)

install exports:default
	$(INST_32) -m 444 -src libbsd_32.a libbsd.a
	$(INST_32_NS) -m 444 -idb "nostrip" -src libbsd_32_ns.a libbsd.a
	$(INST_64) -m 444 -src libbsd_64.a libbsd.a
	$(INST_64_NS) -m 444 -idb "nostrip" -src libbsd_64_ns.a libbsd.a
	$(INST_N32) -m 444 -src libbsd_n32_M3.a libbsd.a
	$(INST_N32_NS) -m 444 -idb "nostrip" -src libbsd_n32_M3_ns.a libbsd.a
