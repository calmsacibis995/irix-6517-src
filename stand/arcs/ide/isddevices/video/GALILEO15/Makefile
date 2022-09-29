DIAGDEPTH= ../../..
include ${DIAGDEPTH}/Makedefs

LINCLUDES = -I../ -I../../include
LLCOPTS=


CFILES= mgv_llbit.c		\
	mgv_globals.c 		\
	mgv_errcode.c		\
	mgv_init.c 		\
	mgv_ab1_mem.c		\
	mgv_cc1_mem.c		\
	mgv_vgi1_reg.c		\
	mgv_vgi1_int.c		\
	mgv_vgi1_dma.c		\
	mgv_gal15_L1.c		\
	mgv_allstds.c		\
	mgv_vid_util.c		\
	mgv_vin_L1.c		\
	mgv_ext_vin.c		\
	mgv_vout_L0.c		\
	mgv_vgi_vout_L0.c	\
	mgv_vout_L1.c		\
	mgv_vout_L2.c		\
	mgv_cc1tovgi1.c		\
	mgv_gal15_dcb.c		\
	mgv_cc1tovgi1x.c	\
	mgv_read_CC1.c		\
	mgv_csc_bus.c           \
	mgv_csc_outlut.c        \
	mgv_csc_coef.c          \
	mgv_csc_util.c          \
	mgv_edh_mem.c 		\
	mgv_i2c.c		\
	mgv_edh.c		\
	mgv_tmi_bus.c		\
	mgv_tmi_sram.c		\
	mgv_scl_reg.c		\
	mgv_tmi_bypass.c


ASFILES= mgv_llvgi1_asm.s


COMMONPREF = MGRAS
_lib ${COMMONTARGS}: ${COMMONPREF}$$@ mgv_llbit.c

include ${DIAGDEPTH}/Makerules
