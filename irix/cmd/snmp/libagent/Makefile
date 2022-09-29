#!smake
#
# Makefile for libsnmpagent.a
#
#ident	"$Revision: 1.3 $"

BASELIB=libsnmpagent.a
ALTLIBS=libsnmpagent_n32.a libsnmpagent_64.a
NOSO=no-thank-you

include	$(ROOT)/usr/include/make/librootdefs

TARGETS=$(LIBS)

default: $(TARGETS)

include $(LIBROOTRULES)

install exports: default $(_FORCE)
	$(INST_32) -m 444 -idb "nostrip" -src libsnmpagent.a $(BASELIB)
	$(INST_N32) -m 444 -idb "nostrip" -src libsnmpagent_n32.a $(BASELIB)
	$(INST_64) -m 444 -idb "nostrip" -src libsnmpagent_64.a $(BASELIB)
	cd src; $(MAKE) $@

headers: $(_FORCE)
	cd src; $(MAKE) headers

