#!smake
#
#ident	"$Revision: 1.1 $"
#

BASELIB=libattr_32.a
ALTLIBS=libattr_32_ns.a libattr_64.a libattr_64_ns.a \
	libattr_n32.a libattr_n32_ns.a

include	$(ROOT)/usr/include/make/librootdefs
TARGETS=$(LIBS)

default: $(TARGETS)

include $(LIBROOTRULES)

install: exports

exports: default
	$(INST_32) -m 444 -idb "nostrip" -src libattr_32.a libattr.a
	$(INST_32_NS) -m 444 -idb "nostrip" -src libattr_32_ns.a libattr.a
	$(INST_64) -m 444 -idb "nostrip" -src libattr_64.a libattr.a
	$(INST_64_NS) -m 444 -idb "nostrip" -src libattr_64_ns.a libattr.a
	$(INST_N32) -m 444 -idb "nostrip" -src libattr_n32.a libattr.a
	$(INST_N32_NS) -m 444 -idb "nostrip" -src libattr_n32_ns.a libattr.a
