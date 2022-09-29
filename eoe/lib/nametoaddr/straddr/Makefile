#!smake

#ident "$Revision: 1.5 $

BASELIB=straddr.a
ALTLIBS=straddr_n32_M3.a

DSOEXPORTS=common.exports
DSOEXPORTS_32_ABI=common.exports
DSOEXPORTS_64_ABI=common.exports

include $(ROOT)/usr/include/make/librootdefs

include ../nametoaddrcommondefs

default:	${TARGETS}

include ${LIBROOTRULES}

install exports:	default
	$(INST_N32) -m 444 -src straddr_n32_M3.so straddr.so
	$(INST_32) -m 444 -src straddr.so straddr.so
