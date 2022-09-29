#!smake
#
#ident	"$Id: Makefile,v 1.2 1996/11/15 19:35:55 lguo Exp $"
#

BASELIB=libdpipe_32.a
ALTLIBS=libdpipe_32_ns.a libdpipe_64.a libdpipe_64_ns.a libdpipe_n32.a libdpipe_n32_ns.a

include	$(ROOT)/usr/include/make/librootdefs

TARGETS=$(LIBS)
default: $(TARGETS)

include $(LIBROOTRULES)

install: exports

exports!default
	$(INST_32) -m 444 -idb "nostrip" -src libdpipe_32.so libdpipe.so
	$(INST_32_NS) -m 444 -idb "nostrip" -src libdpipe_32_ns.a libdpipe.a
	$(INST_32) -m 444 -idb "nostrip" -src libdpipe_32.a libdpipe.a
	# 64bit shared libdpipe
	$(INST_64) -m 444 -idb "nostrip" -src libdpipe_64.so libdpipe.so
	$(INST_64) -m 444 -idb "nostrip" -src libdpipe_64.a libdpipe.a
	# 64bit nonshared libdpipe
	$(INST_64_NS) -m 444 -idb "nostrip" -src libdpipe_64_ns.a libdpipe.a
	# n32 shared libdpipe
	$(INST_N32) -m 444 -idb "nostrip" -src libdpipe_n32.so libdpipe.so
	$(INST_N32) -m 444 -idb "nostrip" -src libdpipe_n32.a libdpipe.a
	# n32 non_shared libdpipe
	$(INST_N32_NS) -m 444 -idb "nostrip" -src libdpipe_n32_ns.a libdpipe.a
