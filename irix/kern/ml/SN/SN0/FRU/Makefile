#!smake
# Copyright 1992, Silicon Graphics Inc., Mountain View, CA. 
#

DEPTH?=../../../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/ml/SN/SN0/FRU
.PATH:$(KPATH)
KCINCS=-I$(BOOTAREA)
KASINCS=-I$(BOOTAREA)

FRU_NAME=fru.a
FRU_CSRCS=sn0_fru_analysis.c sn0_fru_node.c sn0_fru_io.c sn0_fru_tables.c 
FRU_ASRCS=
FRU_OBJS=${FRU_CSRCS:.c=.o} ${FRU_ASRCS:.s=.o}

$(FRU_NAME):$(FRU_NAME)($(FRU_OBJS)) MAKELIB

$(FRU_OBJS): $(KPATH)/sn0_fru_tables.c

TARGETS=$(FRU_NAME)
.PRECIOUS:$(TARGETS)

LINTNAME=fru
CFILES=$(FRU_CSRCS) $(MACHDEP_CSRCS)

LCDEFS+=-DVERSION='"K1.01"'

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

$(KPATH)/sn0_fru_tables.c: $(KPATH)/sn0_fru_parser.pl \
				$(KPATH)/sn0_fru_rules.txt
	$(KPATH)/sn0_fru_parser.pl $(KPATH)/sn0_fru_rules.txt $(KPATH)

#
# 
#
$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KDEVINSTALL} ${FRU_NAME}
	${KINSTALL} ${FRU_NAME}

