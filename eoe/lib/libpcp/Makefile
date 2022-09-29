#!smake
#
# libpcp.so - base PMAPI support
#
#ident "$Revision: 1.13 $"

IDBTAG		= PCP_EOE
IDBTAG64	= PCP_EOE_LIB64
IDBTAG_ROOT	= PCP_EOE_ROOT

BASELIB = libpcp_32.a
ALTLIBS = libpcp_n32.a libpcp_64.a libpcp_BUILD.a

DSOEXPORTS=common.exports
DSOEXPORTS_32_ABI=common.exports
DSOEXPORTS_64_ABI=common.exports

include $(ROOT)/usr/include/make/librootdefs

LIBPCP_VERSION = 2
DSONAMEOPT= -soname libpcp.so
DSOVERSION= sgi$(LIBPCP_VERSION).0
LIBPCP=libpcp.so.$(LIBPCP_VERSION)

LLDLIBS	= -lc -lgen
LLDOPTS  = -no_unresolved

default: $(TARGETS)

include $(LIBROOTRULES)

install: exports default
# pcp_eoe.sw.eoe
	$(INSTALL) -idb $(IDBTAG) -m 755 -dir /var/pcp/lib
	$(INST_32) -m 555 -idb "$(IDBTAG)" -src libpcp_32.so $(LIBPCP)
	$(INST_N32) -m 555 -idb "$(IDBTAG)" -src libpcp_n32.so $(LIBPCP)
	$(INST_64) -m 555 -idb "mach(MODE=64bit) $(IDBTAG)" -src libpcp_64.so $(LIBPCP)
	$(INST_32) -idb "$(IDBTAG)" -lns $(LIBPCP) libpcp.so
	$(INST_N32) -idb "$(IDBTAG)" -lns $(LIBPCP) libpcp.so
	$(INST_64) -idb "mach(MODE=64bit) $(IDBTAG)" -lns $(LIBPCP) libpcp.so
# pcp_eoe.sw64.eoe
	$(INST_64) -m 555 -idb "mach(MODE=32bit) $(IDBTAG64)" -src ./libpcp_64.so $(LIBPCP)
	$(INST_64) -idb "mach(MODE=32bit) $(IDBTAG64)" -lns $(LIBPCP) ./libpcp.so

exports: default
#	pcp_eoe_root
	$(INSTALL) -F /usr/lib -m 555 -idb "$(IDBTAG_ROOT)" libpcp_BUILD.so
	$(INST_32) -m 555 -idb "$(IDBTAG_ROOT)" -src libpcp_32.so $(LIBPCP)
	$(INST_N32) -m 555 -idb "$(IDBTAG_ROOT)" -src libpcp_n32.so $(LIBPCP)
	$(INST_64) -m 555 -idb "$(IDBTAG_ROOT)" -src ././libpcp_64.so $(LIBPCP)
	$(INST_32) -idb "$(IDBTAG_ROOT)" -lns $(LIBPCP) libpcp.so
	$(INST_N32) -idb "$(IDBTAG_ROOT)" -lns $(LIBPCP) libpcp.so
	$(INST_64) -idb "$(IDBTAG_ROOT)" -lns $(LIBPCP) ././libpcp.so

headers:
