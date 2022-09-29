#!smake
# Copyright 1992, Silicon Graphics Inc., Mountain View, CA. 
#

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/ml/LOCORE
.PATH:$(KPATH)
KCINCS=-I$(BOOTAREA)
KASINCS=-I$(BOOTAREA)

MACHDEP_ASRCS=

#if $(CPUBOARD) == "IP21" || $(CPUBOARD) =="IP19" \
	|| $(CPUBOARD) =="IP25" || $(CPUBOARD) =="IP27" \
	|| $(CPUBOARD) =="IP30"
MACHDEP_ASRCS+=ULIsystrap.s
#endif

#if $(CPUARCH) == "TFP"
ARCHDEP_ASRCS=tfp_special_int.s
#endif

#if $(CPUARCH) == "R4000"
ARCHDEP_ASRCS=vec_fpe.s
#if (! defined(_NO_R4000))
ARCHDEP_ASRCS+=R4Kvce.s
#endif
ARCHDEP_ASRCS+=ecc_exc.s ecc_vec.s
#endif

#if $(CPUARCH) == "R10000"
ARCHDEP_ASRCS=vec_fpe.s ecc_exc.s ecc_vec.s
#endif

#if defined(_R5000_CVT_WAR)
ARCHDEP_ASRCS+= R5Kcvt.s
#endif

ML_NAME=ml.a
ML_CSRCS=
ML_ASRCS=locore.s vec_tlbmiss.s vec_tlbmod.s vec_addrerr.s \
	vec_dbe.s vec_ibe.s vec_trap.s vec_cpfault.s \
	vec_int.s vec_unexp.s vec_nofault.s vec_breakpt.s \
	systrap.s backtouser.s exception_exit.s exc_common.s \
	exc_vec.s gen_exc.s intrnorm.s intrfast.s fullfault.s \
	kmiss.s \
	${ARCHDEP_ASRCS} ${MACHDEP_ASRCS}
ML_OBJS= ${ML_ASRCS:.s=.o}

$(ML_NAME):$(ML_NAME)($(ML_OBJS)) MAKELIB

TARGETS=$(ML_NAME)
.PRECIOUS:$(TARGETS)

LINTNAME=LOCORE
CFILES=$(ML_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

#
# 
# Since the RACER stuff goes into the ml lib - there is no reason to install
# it here
#
$(KCOMMONPREF)install: $(KCOMMONPREF)default
