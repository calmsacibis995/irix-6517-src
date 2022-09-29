DIAGDEPTH= ../..
include ${DIAGDEPTH}/Makedefs

IP20_X=\#
IP22_X=\#
IP26_X=\#
IP28_X=\#
$(CPUBOARD)_X=

$(IP22_X)EXTRA_C_FILES=wd95a_regs.c

OPTIMIZER=-O

CFILES= ex.c \
	scsi.c \
	scsi_diag.c \
	scsi_xcise.c \
	$(EXTRA_C_FILES)

# overide common COBJECTS here for core IDE
CCFILES= ex.c scsi.c scsi_diag.c scsi_xcise.c
COBJECTS= ${CCFILES:.c=.o}

include ${DIAGDEPTH}/Makerules
