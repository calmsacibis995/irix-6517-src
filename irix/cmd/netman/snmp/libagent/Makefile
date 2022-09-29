#!smake
#
# Makefile for libsnmpagent.a
#
#ident	"$Revision: 1.7 $"

BASELIB=libsnmpagent.a
ALTLIBS=libsnmpagent_64.a

include	$(ROOT)/usr/include/make/librootdefs

TARGETS=$(LIBS)

default: $(TARGETS)

include $(LIBROOTRULES)

install: exports

exports: default $(_FORCE)
	$(INST_N32) -m 444 -idb "nostrip" -src libsnmpagent_n32.a $(BASELIB)
	$(INST_64) -m 444 -idb "nostrip" -src libsnmpagent_64.a $(BASELIB)

headers: $(_FORCE)
	cd src; $(MAKE) headers
