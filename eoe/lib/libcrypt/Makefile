#!smake
#
# Makefile for libcrypt.
#
#ident	"$Revision: 1.31 $"

BASELIB=libcrypt_32.a
ALTLIBS=libcrypt_32_ns.a libcrypt_64.a libcrypt_n32_M3.a

DSOEXPORTS=common.exports
DSOEXPORTS_32_ABI=common.exports
DSOEXPORTS_64_ABI=common.exports

include	$(ROOT)/usr/include/make/librootdefs
LLDDSOOPTS=-LD_LAYOUT:segalign=0x4000
LLDLIBS=-lc
DSONAMEOPT=-soname libcrypt.so

#
# WARNING 85: definition of XXX preempts that definition in  ...
# WARNING 69: Definition of XXX in .so's interface section appears at least once before
#
LLDOPTS_64_M3=-woff 85,69

default: $(TARGETS)

include $(LIBROOTRULES)

exports install: default
	$(INST_32) -m 444 -src libcrypt_32.a libcrypt.a
	$(INST_32) -m 444 -src libcrypt_32.so libcrypt.so
	$(INST_32_NS) -m 444 -idb "nostrip" -src libcrypt_32_ns.a libcrypt.a
	$(INST_64) -m 444 -src libcrypt_64.a libcrypt.a
	$(INST_64) -m 444 -src libcrypt_64.so libcrypt.so
	$(INST_N32) -m 444 -src libcrypt_n32_M3.a libcrypt.a
	$(INST_N32) -m 444 -src libcrypt_n32_M3.so libcrypt.so
