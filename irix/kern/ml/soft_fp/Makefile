#!smake
#
# Makefile for os which contains generic kernel objects.
#
#ident	"$Revision: 1.4 $"

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/ml/soft_fp
.PATH:$(KPATH)
KCINCS=-I${BOOTAREA} -I$(TOP)/ml/soft_fp
KASINCS=-I${BOOTAREA} -I$(TOP)/ml/soft_fp

ML_NAME=ml.a

ML_CSRCS = abs_sd.c add.c add_sd.c comp_sd.c cvtd_s.c \
	cvtl_sd.c cvts_d.c cvtsd_l.c cvtsd_w.c cvtw_sd.c \
	div_sd.c fpglobals.c fputil.c madd.c \
	madd_sd.c msub_sd.c mul_sd.c neg_sd.c nmadd_sd.c \
	nmsub_sd.c renorm_d.c renorm_s.c round_d.c round_s.c \
	rsqrt_sd.c softfp_dispatch.c sqrt_sd.c sub_sd.c


ML_ASRCS = \
	fpload_d.s fpstore_d.s mul64.s \
	fpload_s.s fpstore_s.s

ML_OBJS=${ML_CSRCS:.c=.o} ${ML_ASRCS:.s=.o}
$(ML_NAME):$(ML_NAME)($(ML_OBJS)) MAKELIB


TARGETS=$(ML_NAME)
.PRECIOUS:$(TARGETS)

# lint
LINTNAME=ml
CFILES=$(ML_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

# This default rule is always invoked in BOOTAREA
$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

$(KCOMMONPREF)install: $(KCOMMONPREF)default
