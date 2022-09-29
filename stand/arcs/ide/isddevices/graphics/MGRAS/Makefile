# Mgras makefile
#
DIAGDEPTH= ../../..
include ${DIAGDEPTH}/Makedefs


# illegal shift warning from ucode
LLWOFF=,848

IP22_X=\#
IP28_X=\#
IP30_X=\#
$(CPUBOARD)_X=
$(CPUBOARD)_$(COMPILATION_MODEL)X=

# I2 is HQ3, RACER HQ4
HQNUM=-DHQ3
$(IP30_X)HQNUM=-DHQ4

#include for extra msg_printf's
#LDEFINES=-DDEBUG
LDEFINES=$(HQNUM)
LINCLUDES=-I../../include

64_Z=\#
32_Z=\#
$(COMPILATION_MODEL)_Z=

BASE_LINCLUDES=-I../ -I../.. -I${ROOT}/usr/include/MGRAS/ -I../../include
$(32_Z)LINCLUDES= $(BASE_LINCLUDES)
$(64_Z)LINCLUDES= $(BASE_LINCLUDES)

CFILES= mgras_vc3_register.c mgras_vc3.c mgras_cmap.c mgras_dac.c mgras_xmap.c \
	mgras_pp1.c mgras_DacCrc.c mgras_info.c \
	mgras_re4.c mgras_tetr.c mgras_rdram_rac.c mgras_rdram_memtest_pio.c \
	mgras_hq3.c mgras_hq3_cp.c mgras_ge11.c mgras_ge11_main.c \
	mgras_hqpio_redma.c mgras_hqdma_redma.c mgras_host_hqdma.c \
	mgras_hq_download.c mgras_ge11_download.c \
	mgras_rss_utils.c mgras_re4func.c mgras_mg0tri.c mgras_linedraw.c \
	mgras_pp1func.c mgras_tetramfunc.c mgras_tex_3d.c mgras_tex_1d.c \
	mgras_tex_load.c mgras_tex_scistri.c mgras_tex_linegl.c \
	mgras_tex_persp.c mgras_tex_mag.c mgras_tex_detail.c \
	mgras_tex_bordertall.c mgras_tex_borderwide.c mgras_tex_lut4d.c \
	mgras_tex_mddma.c mgras_verif_mkImage.c mgras_gold_crc.c \
	mgras_util.c mgras_tex_terev.c mgras_alphablend.c \
	mgras_globals.c mgras_xbow.c mgras_xbowlink.c \
	mgras_rtdma.c mgras_retexlut.c mgras_i2c.c gfx_errorcode.c

ASFILES= dbl_ld.s

LLDIRT=mgras_ge11_ucode_utils mgras_ge.diag

COMMONPREF = MGRAS

_lib ${COMMONTARGS}: mgras_ge.diag ${COMMONPREF}$$@

_plib: mgras_ge.diag ${COMMONPREF}$$@

${COMMONTARGS}: ${COMMONPREF}$$@

default: _lib

# override common POBJECTS so only necessary files compile for PROM
PCFILES= mgras_pon.c 
POBJECTS= ${PCFILES:.c=.o}

include ${DIAGDEPTH}/Makerules

mgras_ge11_ucode_utils: mgras_ge11_ucode_utils.c
	$(TOOLROOT)/usr/bin/cc -g mgras_ge11_ucode_utils.c -o mgras_ge11_ucode_utils -I$(ROOT)/usr/include

mgras_ge.diag: mgras_ge11_ucode_utils mgras_ge.hex
	./mgras_ge11_ucode_utils mgras_ge.hex > mgras_ge.diag

mgras_tv:
	$(TOOLROOT)/usr/bin/cc -g $(LINCLUDES) -I$(WORKAREA)/gfx/KM/include/ -I$(WORKAREA)/usr/include mgras_tv.c -o mgras_tv
