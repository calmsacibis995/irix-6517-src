#!smake
#
# libpcp_dev_dev.a - noship/dev PMAPI support
#
#ident "$Revision: 1.9 $"

IDBTAG = PCP_EOE_ROOT
BASELIB	= libpcp_dev.a
ALTLIBS = libpcp_dev_n32.a libpcp_dev_64.a
NOSO=no-thank-you

include $(ROOT)/usr/include/make/librootdefs

LLDLIBS=-lc -lpcp

default: $(TARGETS)

include $(LIBROOTRULES)

install exports: default
#	pcp_eoe_root is not to be shipped to customers
	$(INST_32) -m 555 -idb "$(IDBTAG)" -src src/libpcp_dev_32.a libpcp_dev.a
	$(INST_N32) -m 555 -idb "$(IDBTAG)" -src libpcp_dev_n32/libpcp_dev_n32.a libpcp_dev.a
	$(INST_64) -m 555 -idb "$(IDBTAG)" -src libpcp_dev_64/libpcp_dev_64.a libpcp_dev.a

headers:
