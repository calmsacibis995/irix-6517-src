DIAGDEPTH= ../..
include ${DIAGDEPTH}/Makedefs

IP20_X=\#
IP22_X=\#
IP26_X=\#
IP28_X=\#
$(CPUBOARD)_X=

$(IP20_X)EXTRA_C_FILES= threebit.c checker.c parreg.c parity.c parcheck.c
$(IP20_X)EXTRA_AS_FILES= misc_s.s parasm.s

$(IP22_X)EXTRA_C_FILES= dmuxfix.c pargenlog.c march.c mats.c movi.c \
	altaccess.c blk.c dram2_diag.c shift_dram.c threebit.c butterfly.c \
	khdouble.c khlow.c new3bit.c tb_test.c checker.c \
	parreg.c parity.c parcheck.c
$(IP22_X)EXTRA_AS_FILES= khparity.s misc_s.s kh_dw.s khlow_diag.s parasm.s

$(IP26_X)EXTRA_C_FILES= dmuxfix.c march.c mats.c movi.c \
	altaccess.c blk.c dram2_diag.c shift_dram.c butterfly.c lkhtest.c
$(IP26_X)EXTRA_AS_FILES=

$(IP28_X)EXTRA_C_FILES= dmuxfix.c march.c mats.c movi.c \
	altaccess.c blk.c dram2_diag.c shift_dram.c butterfly.c lkhtest.c
$(IP28_X)EXTRA_AS_FILES=

HFILES= mem.h private.h
CFILES= addruniq.c binarybit.c dram_diag.c ldramtest.c memtest.c \
	private.c walkingbit.c diagram.c memutil.c \
	marchxy.c kh.c memtest2.c $(EXTRA_C_FILES)

ASFILES= ldram_diag.s $(EXTRA_AS_FILES)

# overide common MOBJECTS here for IDE modules
MASFILES= $(EXTRA_AS_FILES)
MCFILES= $(CFILES)
MOBJECTS= ${MASFILES:.s=.M} ${MCFILES:.c=.M}

# overide common COBJECTS here for core IDE
CASFILES= ldram_diag.s
COBJECTS= ${CASFILES:.s=.o}

include ${DIAGDEPTH}/Makerules
