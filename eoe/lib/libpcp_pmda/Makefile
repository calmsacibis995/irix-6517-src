#!smake
#
# libpcp_pmda_pmda.so - PCP PMDA support
#
#ident "$Revision: 1.16 $"

IDBTAG = PCP_EOE
IDBTAG64 = PCP_EOE_LIB64
IDBTAG_ROOT = PCP_EOE_ROOT

PREOP = `cat preop`

LIBPCP_PMDA_VERSION = 2
BASELIB	= libpcp_pmda_32.a
ALTLIBS = libpcp_pmda_n32.a libpcp_pmda_64.a libpcp_pmda_BUILD.a

DSOEXPORTS=common.exports
DSOEXPORTS_32_ABI=common.exports
DSOEXPORTS_64_ABI=common.exports

include $(ROOT)/usr/include/make/librootdefs
DSONAMEOPT= -soname libpcp_pmda.so
DSOVERSION= sgi$(LIBPCP_PMDA_VERSION).0

LLDLIBS=-lpcp -lc

default: $(TARGETS)

include $(LIBROOTRULES)

install: exports default
# pcp_eoe.sw.eoe
	$(INST_32) -m 555 -idb "$(IDBTAG)" -src libpcp_pmda_32.so libpcp_pmda.so.$(LIBPCP_PMDA_VERSION)
	$(INST_N32) -m 555 -idb "$(IDBTAG)" -src libpcp_pmda_n32.so libpcp_pmda.so.$(LIBPCP_PMDA_VERSION)
	$(INST_64) -m 555 -idb "mach(MODE=64bit) $(IDBTAG)" -src libpcp_pmda_64.so libpcp_pmda.so.$(LIBPCP_PMDA_VERSION)
	$(INST_32) -idb "$(IDBTAG) $(PREOP)" -lns libpcp_pmda.so.$(LIBPCP_PMDA_VERSION) libpcp_pmda.so
	$(INST_N32) -idb "$(IDBTAG) $(PREOP)" -lns libpcp_pmda.so.$(LIBPCP_PMDA_VERSION) libpcp_pmda.so
	$(INST_64) -idb "mach(MODE=64bit) $(IDBTAG)" -lns libpcp_pmda.so.$(LIBPCP_PMDA_VERSION) libpcp_pmda.so
# pcp_eoe.sw64.eoe
	$(INST_64) -m 555 -idb "mach(MODE=32bit) $(IDBTAG64)" -src ./libpcp_pmda_64.so libpcp_pmda.so.$(LIBPCP_PMDA_VERSION)
	$(INST_64) -idb "mach(MODE=32bit) $(IDBTAG64)" -lns libpcp_pmda.so.$(LIBPCP_PMDA_VERSION) ./libpcp_pmda.so

exports: default
	$(INSTALL) -F /usr/lib -m 555 -idb "$(IDBTAG_ROOT)" libpcp_pmda_BUILD.so
	$(INST_32) -m 555 -idb "$(IDBTAG_ROOT)" -src libpcp_pmda_32.so libpcp_pmda.so.$(LIBPCP_PMDA_VERSION)
	$(INST_N32) -m 555 -idb "$(IDBTAG_ROOT)" -src libpcp_pmda_n32.so libpcp_pmda.so.$(LIBPCP_PMDA_VERSION)
	$(INST_64) -m 555 -idb "$(IDBTAG_ROOT)" -src ././libpcp_pmda_64.so libpcp_pmda.so.$(LIBPCP_PMDA_VERSION)
	$(INST_32) -idb "$(IDBTAG_ROOT) $(PREOP)" -lns libpcp_pmda.so.$(LIBPCP_PMDA_VERSION) libpcp_pmda.so
	$(INST_N32) -idb "$(IDBTAG_ROOT) $(PREOP)" -lns libpcp_pmda.so.$(LIBPCP_PMDA_VERSION) libpcp_pmda.so
	$(INST_64) -idb "$(IDBTAG_ROOT)" -lns libpcp_pmda.so.$(LIBPCP_PMDA_VERSION) ././libpcp_pmda.so

headers:
