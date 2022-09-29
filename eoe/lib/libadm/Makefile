#!smake

#ident	"$Revision: 1.7 $"

BASELIB=libadm_32.a
ALTLIBS=libadm_n32_M3.a

DSOEXPORTS=common.exports
DSOEXPORTS_N32=common.exports

include	$(ROOT)/usr/include/make/librootdefs
LLDLIBS=-lc

DSONAMEOPT=-soname libadm.so

default: $(TARGETS)

include $(LIBROOTRULES)

exports install: default
	$(INST_32) -m 444 -src libadm_32.so libadm.so
	$(INST_32) -m 444 -src libadm_32.a libadm.a
	$(INST_N32) -m 444 -src libadm_n32_M3.so libadm.so
	$(INST_N32) -m 444 -src libadm_n32_M3.a libadm.a
	$(INSTALL) -F /etc -m 444 device.tab


