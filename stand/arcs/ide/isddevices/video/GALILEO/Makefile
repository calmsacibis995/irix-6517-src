DIAGDEPTH= ../../..
include ${DIAGDEPTH}/Makedefs

LINCLUDES = -I../ 

GALILEO_CFILES= ev1_init.c ev1_displaybus.c ab1_memtest.c cc1_memtest.c \
	cc1_dmatest.c extntrig.c regio.c
MGV_CFILES= mgv1_ev1_init.c mgv1_ev1_displaybus.c \
	mgv1_ab1_memtest.c mgv1_cc1_memtest.c mgv1_extntrig.c mgv1_regio.c \
	mgv1_paths.c mgv1_convert.c mgv1_cc1_dmatest.c mgv1_utils.c 

IP20_X=\#
IP22_X=\#
IP26_X=\#
IP28_X=\#
$(CPUBOARD)_X=

$(IP20_X)CFILES=$(GALILEO_CFILES)
$(IP22_X)CFILES=$(GALILEO_CFILES) $(MGV_CFILES)
$(IP26_X)CFILES=$(GALILEO_CFILES)
$(IP28_X)CFILES=$(GALILEO_CFILES) $(MGV_CFILES)

COMMONPREF = EXPRESS
_lib ${COMMONTARGS}: ${COMMONPREF}$$@

include ${DIAGDEPTH}/Makerules

