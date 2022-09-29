#!smake
#
# Makefile for libl.
#
#ident	"$Revision: 1.35 $"

BASELIB=libl_32.a
ALTLIBS=libl_32_ns.a libl_32_abi.a \
	libl_64.a libl_64_ns.a \
	libl_n32.a libl_n32_ns.a

DSOEXPORTS=common.exports
DSOEXPORTS_N32=common.exports
DSOEXPORTS_64=common.exports

include	$(ROOT)/usr/include/make/librootdefs

LLDLIBS=-lc
# turn off -no_unresolved for this library
LD_NO_UNRESOLVED=
DSONAMEOPT=-soname libl.so
LDIRT=so_locations

default: $(TARGETS)

headers!
	$(INSTALL) -F /usr/lib/lex ncform nrform

exports install: default
	$(INSTALL) -F /usr/lib/lex ncform nrform
	$(INST_32) -m 444 -src libl_32.a libl.a
	$(INST_32_NS) -m 444 -idb "nostrip" -src libl_32_ns.a libl.a
	$(INST_32_ABI) -m 444 -src libl_32_abi.a libl.a
	$(INST_64) -m 444 -src libl_64.a libl.a
	$(INST_64_NS) -m 444 -idb "nostrip" -src libl_64_ns.a libl.a
	$(INST_N32) -m 444 -src libl_n32.a libl.a
	$(INST_N32_NS) -m 444 -idb "nostrip" -src libl_n32_ns.a libl.a

include $(LIBROOTRULES)
