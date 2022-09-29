#!smake
#
# Makefile for libgen.
#
#ident	"$Revision: 1.24 $"

BASELIB=libgen_32.a
ALTLIBS=libgen_32_ns.a libgen_32_abi.a \
	libgen_64.a libgen_64_ns.a libgen_64_abi.a \
	libgen_n32.a libgen_n32_ns.a

DSOEXPORTS=common.exports
# actually, libgen doesn't export a .so for ABI yet..
DSOEXPORTS_32_ABI=common.exports
DSOEXPORTS_64_ABI=common.exports

include	$(ROOT)/usr/include/make/librootdefs
LLDDSOOPTS=-LD_LAYOUT:segalign=0x4000
DSONAMEOPT= -soname libgen.so
LLDLIBS=-lc

default: $(TARGETS)

include $(LIBROOTRULES)

install: headers exports

exports: default $(_FORCE)
	$(INST_32) -m 444 -src libgen_32.a libgen.a
	$(INST_32) -m 444 -src libgen_32.so libgen.so
	$(INST_32) -lns libgen.a libPW.a
	$(INST_32) -lns libgen.so libPW.so
	$(INST_32_NS) -m 444 -idb "nostrip" -src libgen_32_ns.a libgen.a
	$(INST_32_NS) -lns libgen.a libPW.a
	# ABI version(s)
	$(INST_32_ABI) -m 444 -src libgen_32_abi.a libgen.a
	$(INST_64_ABI) -m 444 -src libgen_64_abi.a libgen.a
	# 64bit shared libgen
	$(INST_64) -m 444 -src libgen_64.a libgen.a
	$(INST_64) -m 444 -src libgen_64.so libgen.so
	$(INST_64) -lns libgen.a libPW.a
	$(INST_64) -lns libgen.so libPW.so
	# 64bit non_shared libgen
	$(INST_64_NS) -m 444 -idb "nostrip" -src libgen_64_ns.a libgen.a
	$(INST_64_NS) -lns libgen.a libPW.a
	# n32bit shared libgen
	$(INST_N32) -m 444 -src libgen_n32.a libgen.a
	$(INST_N32) -m 444 -src libgen_n32.so libgen.so
	$(INST_N32) -lns libgen.a libPW.a
	$(INST_N32) -lns libgen.so libPW.so
	# n32bit non_shared libgen
	$(INST_N32_NS) -m 444 -idb "nostrip" -src libgen_n32_ns.a libgen.a
	$(INST_N32_NS) -lns libgen.a libPW.a


headers: $(_FORCE)
	$(INSTALL) -F /usr/include -m 444 -src src/regexpr.h regexpr.h
	$(INSTALL) -F /usr/include -m 444 -src src/libgen.h libgen.h 
