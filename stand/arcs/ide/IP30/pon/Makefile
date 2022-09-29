# power-on diagnostics makefile, for IP30
DIAGDEPTH= ../..
include ${DIAGDEPTH}/Makedefs

ASFILES= pon_addr.s pon_data.s pon_hndlr.s pon_io.s pon_heart.s pon_subr.s \
	 pon_regs.s pon_dbg.s
CFILES= pon_more.c pon_caches.c pon_scache.c pon_gfx.c pon_nic.c \
	pon_scsi.c pon_enet.c

# overide common COBJECTS here for core IDE
CASFILES= pon_io.s pon_subr.s
COBJECTS= ${CASFILES:.s=.o}

include ${DIAGDEPTH}/Makerules
