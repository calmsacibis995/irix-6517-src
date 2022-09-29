#ident  "ide/godzilla/mem/Makefile:  $Revision: 1.10 $"
#
#  This makes the diagnostics memory tests.
#  This Makefile was originally ported from fforward/mem, then stripped of 
#   the other board options (IP20, IP22, IP26)
#  read the README file in this directory for an overview.
#

DIAGDEPTH= ../..

include ${DIAGDEPTH}/Makedefs

IP30_X=\#
$(CPUBOARD)_X=

$(IP22_X)EXTRA_C_FILES= khdouble.c
$(IP22_X)EXTRA_AS_FILES= kh_dw.s

$(IP30_X)EXTRA_C_FILES= checker.c
$(IP30_X)EXTRA_AS_FILES= 

HFILES = 
CFILES= addruniq.c ldramtest.c walkingbit.c diagram.c memutil.c \
        marchxy.c kh.c march.c mats.c movi.c altaccess.c \
	butterfly.c dram_diag.c dram2_diag.c blk.c  patterns.c \
	shift_dram.c memtest.c lkhtest.c new3bit.c threebit.c \
	fasttest.c $(EXTRA_C_FILES)
ASFILES= ldram_diag.s khparity.s misc_s.s $(EXTRA_AS_FILES)

include ${DIAGDEPTH}/Makerules

# place all diag header files in ide/godzilla/include dir
# LINCLUDES is used in stand/arcs/ide/Makedefs
LINCLUDES = -I../include  
