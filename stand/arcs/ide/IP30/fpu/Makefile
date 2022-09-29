DIAGDEPTH= ../..
include ${DIAGDEPTH}/Makedefs

R4000_X=\#
TFP_X=\#
$(CPUARCH)_X=

# use mips4 on TFP
$(TFP_X)MIPSVERS=-mips4
$(TFP_X)OPTIMIZER=-O1

ASFILES= lfpu.s sfpu.s overflow.s underflow.s fabs.s getfp.s setfp.s \
	dtoa.s dwmultu.s tenscale.s
 
CFILES= fpregs.c faddsubd.c faddsubs.c fmuldivd.c fmuldivs.c fpmem.c \
	fdivzero.c foverflow.c funderflow.c finexact.c finvalid.c \
	fmulsubs.c fmulsubd.c fpcmput.c fptest.c lpack.c

include ${DIAGDEPTH}/Makerules
