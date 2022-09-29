#!smake
# Copyright 1996, Silicon Graphics Inc., Mountain View, CA. 
#

DEPTH?=../../../..
include ${DEPTH}/kcommondefs.fru
#KPATH=$(TOP)/ml/SN0/FRU/testing
KPATH=.
#FRU_PATH=$(TOP)/ml/SN0/FRU
FRU_PATH=..
.PATH:$(KPATH)
KCINCS=-I$(BOOTAREA)
KASINCS=-I$(BOOTAREA)
DBOPTS= -g -DFRU -DFRUTEST -DDEBUG -DFRU_DEBUG_RULES -DROUTER 
FRU_NAME=fru

FRU_CSRCS= $(KPATH)/sn0_fru_test.c $(FRU_PATH)/sn0_fru_node.c \
	$(FRU_PATH)/sn0_fru_analysis.c  $(FRU_PATH)/sn0_fru_conflib.c \
	$(FRU_PATH)/sn0_fru_tables.c  $(FRU_PATH)/sn0_fru_io.c
FRU_ASRCS=
FRU_OBJS=sn0_fru_test.o sn0_fru_node.o sn0_fru_analysis.o sn0_fru_conflib.o \
	sn0_fru_tables.o sn0_fru_io.o

sn0_fru_test.o: $(KPATH)/sn0_fru_test.c
sn0_fru_node.o: $(FRU_PATH)/sn0_fru_node.c
sn0_fru_analysis.o: $(FRU_PATH)/sn0_fru_analysis.c
sn0_fru_conflib.o: $(FRU_PATH)/sn0_fru_conflib.c
sn0_fru_tables.o: $(FRU_PATH)/sn0_fru_tables.c
sn0_fru_io.o: $(FRU_PATH)/sn0_fru_io.c

$(FRU_OBJS): $(FRU_PATH)/sn0_fru_analysis.h

$(FRU_NAME): $(FRU_OBJS)  

TARGETS=$(FRU_OBJS)
.PRECIOUS:$(TARGETS)

LINTNAME=sn0_fru
CFILES=$(FRU_CSRCS) $(MACHDEP_CSRCS)

LCDEFS+=-DVERSION='"K1.01"'


#$(KCOMMONPREF)
default:$(TARGETS)
	$(CCF)  -o $(FRU_NAME) $(FRU_OBJS)

clobber:
	rm -rf $(FRU_OBJS) $(FRU_NAME)
