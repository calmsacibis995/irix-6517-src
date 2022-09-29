#!smake

#ident "$Revision: 1.4 $

BASELIB=tcpip.a
ALTLIBS=tcpip_n32_M3.a

include $(ROOT)/usr/include/make/librootdefs

include ../nametoaddrcommondefs

default:	${TARGETS}

include ${LIBROOTRULES}

install exports:	default
	$(INST_N32) -m 444 -src tcpip_n32_M3.so tcpip.so
	$(INST_32) -m 444 -src tcpip.so tcpip.so
