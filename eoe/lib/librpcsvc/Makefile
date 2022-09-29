#!smake
#
# Makefile for /usr/lib/librpcsvc.a, the RPC servers' interface library.
#
# ident  "$Revision: 1.19 $
#

BASELIB=librpcsvc_32.a
ALTLIBS=librpcsvc_32_ns.a \
	librpcsvc_64.a librpcsvc_64_ns.a \
	librpcsvc_n32.a librpcsvc_n32_ns.a

DSOEXPORTS=common.exports
DSOEXPORTS_32_ABI=common.exports
DSOEXPORTS_64_ABI=common.exports

include	$(ROOT)/usr/include/make/librootdefs
DSONAMEOPT=-soname librpcsvc.so
LLDLIBS=-lc

default: $(TARGETS)

include $(LIBROOTRULES)

exports install: default
	$(INST_32) -m 444 -src librpcsvc_32.a librpcsvc.a
	$(INST_32) -m 444 -src librpcsvc_32.so librpcsvc.so
	$(INST_32_NS) -m 444 -idb "nostrip" -src librpcsvc_32_ns.a librpcsvc.a
	$(INST_64) -m 444 -src librpcsvc_64.a librpcsvc.a
	$(INST_64) -m 444 -src librpcsvc_64.so librpcsvc.so
	$(INST_64_NS) -m 444 -idb "nostrip" -src librpcsvc_64_ns.a librpcsvc.a
	$(INST_N32) -m 444 -src librpcsvc_n32.a librpcsvc.a
	$(INST_N32) -m 444 -src librpcsvc_n32.so librpcsvc.so
	$(INST_N32_NS) -m 444 -idb "nostrip" -src librpcsvc_n32_ns.a librpcsvc.a
