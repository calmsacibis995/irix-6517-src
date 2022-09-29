DIAGDEPTH= ../../..
include ${DIAGDEPTH}/Makedefs

LINCLUDES = -I../ 
LWOFF=,315
VCDEFS=-DNO_NETWORK

CFILES=mco_utils.c \
	mco_probe.c \
	mco_fpga.c \
	mco_7162.c \
	mco_1572.c \
	mco_473.c \
	mco_errorcode.c \
	mco_info.c \
	mco_diag_timing.c \
	mco_vc2_register.c \
	mco_vc2.c \
	mco_swizzletests.c  \
	mco_DacCrc.c


COMMONPREF = MGRAS
#COMMONPREF = EXPRESS
_lib ${COMMONTARGS}: ${COMMONPREF}$$@

include ${DIAGDEPTH}/Makerules

