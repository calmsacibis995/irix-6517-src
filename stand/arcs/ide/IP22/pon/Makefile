DIAGDEPTH= ../..
include ${DIAGDEPTH}/Makedefs

LINCLUDES = -I${ROOT}/usr/include/gl

ASFILES= pon_addr.s pon_data.s pon_isdn.s \
	 pon_hndlr.s pon_io.s pon_subr.s pon_regs.s pon_mc.s
CFILES= pon_caches.c pon_more.c

# overide common COBJECTS here for core IDE
CASFILES= pon_io.s pon_subr.s
COBJECTS= ${CASFILES:.s=.o}

include ${DIAGDEPTH}/Makerules
