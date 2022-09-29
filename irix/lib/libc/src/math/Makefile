#!smake

#ident "$Revision: 1.27 $"

DEPTH=..
include $(ROOT)/usr/include/make/libleafdefs

SUBDIR_ASINCS=-I$(TOP)inc

#
# Old 32 ABI needs longlong emulation
#
#if $(OBJECT_STYLE:M32*) != ""
LL_CFILES =  llbit.c  lldiv.c  llshift.c  
LL_ASFILES =  llcvt.s  lldivrem.s  lldivremi.s  llmul.s dwmultu.s
#else
#
# XXX removed dshiftv.c since that should only apply to 2.2 parts and doesn't
# seem like we'll need that..
#
LL_CFILES=
LL_ASFILES=dwmultu.s
#endif

#
# Theory is (10/3/94) that all future compilers will support long double
# so we will assume that if one is using anything but CFE, we'll have
# that support
#
#if $(VCC) != "CFE"
QUAD_WORD_CFILES= atold.c atoq.c dble_q.c fp_class_q.c ii_qint.c isnanq.c \
	ji_qint.c ji_quint.c ki_qint.c ki_quint.c \
	prodl.c qfpclass.c qfrexp.c qlogb.c qtenscale.c qmodf.c qldexp.c \
	qnextafter.c qscalb.c qtoa.c qunordered.c \
	q_div.c q_eq.c q_ext.c q_extd.c q_floti.c q_flotj.c q_flotju.c \
	q_flotk.c q_flotku.c q_ge.c q_gt.c q_le.c q_lt.c q_ne.c \
	q_neg.c sngl_q.c strtold.c
QUAD_WORD_ASFILES=qwmultu.s qfinite.s

#if $(OBJECT_STYLE) == "N32_M3" || $(OBJECT_STYLE) == "64_M3"
QUAD_WORD_CFILES += q_add.c q_mul.c q_sub.c
#endif

#if $(OBJECT_STYLE) == "N32_M4" || $(OBJECT_STYLE) == "64_M4"
QUAD_WORD_ASFILES += q_add_a.s q_mul_a.s q_sub_a.s
#endif

#else
QUAD_WORD_CFILES=
QUAD_WORD_ASFILES=
#endif


#---------------------------------------------------------------------
# When building n32 or 64 then we need to make sure that IEEE 
# comparison rules are obeyed and that no MADD instructions are
# generated -- this later is so that the code behaves the same
# on both T5 and TFP
#---------------------------------------------------------------------
#if $(OBJECT_STYLE:MN32*) != "" || $(OBJECT_STYLE:M64*) != ""
SUBDIR_CDEFS =  -OPT:IEEE_comparisons 
SUBDIR_CDEFS += -TARG:madd=off -OPT:roundoff=0
#endif

ABI_CFILES= \
	a64l.c atof.c atoi.c atol.c \
	dtoa.c drand48.c ecvt.c \
	frexp.c fp_data.c gcvt.c get_int_digits.c ilogb.c l64a.c llabsdiv.c \
	mixed_dtoa.c \
	nextafter.c \
	scalb.c strtod.c strtol.c strtoll.c strtoul.c swab.c \
	tenscalec.c \
	$(LL_CFILES)

NONABI_CFILES= \
	atod.c l3.c ltoa.c modff.c ${QUAD_WORD_CFILES}

ABI_ASFILES= \
	abs.s div.s isnand.s \
	labs.s ldexp.s ldiv.s logb.s modf.s \
	$(LL_ASFILES) 

NONABI_ASFILES= __infinity.s _infinity.s \
	fabs.s  sqrt.s \
	${QUAD_WORD_ASFILES}

#
# These aren't really part of the BB3.0 but really should be. We place them
# in the .so
#
ABITOBE_CFILES=\
	fpclass.c unordered.c

ABITOBE_ASFILES=\
	finite.s fp_class.s fpgetmask.s fpgetrnd.s \
	fpgetsticky.s fpsetmask.s fpsetrnd.s fpsetsticky.s \
	fp_control.s isnanf.s

#if $(LIBRARY) == "libc_32_abi.a" || \
    $(LIBRARY) == "libc_64_abi.a"
ASFILES=$(ABI_ASFILES) $(ABITOBE_ASFILES)
CFILES= $(ABI_CFILES) $(ABITOBE_CFILES)
#else
ASFILES=$(ABI_ASFILES) $(ABITOBE_ASFILES) $(NONABI_ASFILES)
CFILES= $(ABI_CFILES) $(ABITOBE_CFILES) $(NONABI_CFILES)
#endif

include $(LIBLEAFRULES)
