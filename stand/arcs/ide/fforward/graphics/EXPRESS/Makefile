DIAGDEPTH= ../../..
SUBDIR = vdma
include ${DIAGDEPTH}/Makedefs

LINCLUDES = -I../ -I${ROOT}/usr/include/EXPRESS/gl

CFILES= RAMtools.c bdvers.c bitp.c bt457.c db.c dwnload.c err_report.c \
	ge7diag.c ge7dma.c hq2regs.c hwinv.c hqdiag.c vc1tools.c xmaptest1.c \
	dmasetup.c

LLWOFF=,1209

COMMONPREF = EXPRESS
_lib ${COMMONTARGS} _plib: ${COMMONPREF}$$@
	@for dirs in $(SUBDIR); do \
		echo "========\tcd $$dirs; $(MAKE) $@"; \
	(cd $$dirs; exec $(MAKE) $@) \
	done

# override common POBJECTS so only necessary files compile for PROM
PCFILES=  gr2_pon.c dmasetup.c
POBJECTS= ${PCFILES:.c=.o}

include ${DIAGDEPTH}/Makerules

