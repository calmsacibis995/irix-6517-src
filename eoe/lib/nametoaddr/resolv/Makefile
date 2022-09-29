#!smake

#ident "$Revision: 1.4 $

BASELIB=resolv.a
ALTLIBS=resolv_n32_M3.a

DSOEXPORTS=common.exports
DSOEXPORTS_32_ABI=common.exports
DSOEXPORTS_64_ABI=common.exports

include $(ROOT)/usr/include/make/librootdefs

include ../nametoaddrcommondefs

default:	${TARGETS}

include ${LIBROOTRULES}

install exports:	default
	$(INST_N32) -m 444 -src resolv_n32_M3.so resolv.so
	$(INST_32) -m 444 -src resolv.so resolv.so
