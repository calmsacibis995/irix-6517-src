DIAGDEPTH= ../..
include ${DIAGDEPTH}/Makedefs

HFILES= mem.h \
	private.h
#CFILES= addruniq.c binarybit.c \
#	dram_diag.c ldramtest.c memtest.c parreg.c	\
#	parity.c parcheck.c private.c diagram.c \
#	walkingpat.c walkingbit.c randompat.c \
#	dmuxfix.c \
#	pargenlog.c kh.c march.c mats.c  movi.c\
#	altaccess.c  memtest2.c  \
#	blk.c memutil.c dram2_diag.c shift_dram.c \
#	marchxy.c threebit.c butterfly.c khdouble.c khlow.c new3bit.c \
#	tb_test.c checker.c

CFILES= addruniq.c \
	parity.c diagram.c \
	walkingbit.c randompat.c \
	pargenlog.c kh.c march.c mats.c  movi.c\
	altaccess.c memtest.c memtest2.c  \
	blk.c memutil.c dram2_diag.c \
	marchxy.c threebit.c butterfly.c khdouble.c khlow.c new3bit.c \
	tb_test.c checker.c \
        walkingpat.c

#ASFILES= ldram_diag.s parasm.s khparity.s misc_s.s kh_dw.s khlow_diag.s
ASFILES= ldram_diag.s hdram_diag.s khparity.s misc_s.s kh_dw.s khlow_diag.s

include ${DIAGDEPTH}/Makerules

mfgstuff:
	$(MAKE) VCDEFS=-DMFG_USED
