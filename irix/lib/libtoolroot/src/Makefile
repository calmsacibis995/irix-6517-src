#!smake
#ident "$Revision: 1.2 $"

DEPTH=.
include $(ROOT)/usr/include/make/libleafdefs

#
# the old_* files are only for shared C backward compat support
#

MATH_CFILES	= dcis.c rcis.c
MATH_ASFILES	= libm_vals.s 

CFILES		= attr.c $(MATH_CFILES)
ASFILES		= $(MATH_ASFILES)

include $(LIBLEAFRULES)

#--------------------------------------------------------------------------
#			Math library routines  
#--------------------------------------------------------------------------
#
#	These mathlibrary routines are included in libptools for
#	irix 6.5 development while still running on 6.2 - 6.4 systems.
#
#	We tone down the optimizations because these routines
#	are supplied for compatibility and not for performance.
#
#	I've encapulated everthing that is math library specific
#	into separate smake variables so that it will be easy
#	to rip it all out at a later time. 
#
#	Finally, the files dcis.c, rcis.c, libm.h and complex.h have
#	been modified from what is in the normal version of libm
#	in order to get them to build in this environment.  In particular,
#	I removed the code that sets errno and I removed any quad code.
#
#		bean 9/19/97
#
#--------------------------------------------------------------------------
MATH_LLDLIBS = -lm
MATH_SFILES=
MATH_FLAGS = 
MATH_OPTS  = -O1

#if	$(OBJECT_STYLE:MN32*) != "" 
# n32 builds for mathlib routines
MATH_FLAGS  = -DPIC -OPT:alias=restrict:alias=unnamed -TENV:X=1 -TARG:madd=off
MATH_FLAGS += -OPT:IEEE_comparisons -OPT:roundoff=0 -LNO:prefetch=0
#elif	$(OBJECT_STYLE:M64*) != "" 
#	n64 builds for mathlib routines
MATH_FLAGS  = -DPIC -OPT:alias=restrict:alias=unnamed -TENV:X=1 -TARG:madd=off
MATH_FLAGS += -OPT:IEEE_comparisons -OPT:roundoff=0 -LNO:prefetch=0
#elif	$(OBJECT_STYLE:M32*) != "" 
#	o32 builds for mathlib routines
MATH_FLAGS  = -DPIC -G 0 
#else
MATH_FLAGS =
#endif

LLDLIBS += $(MATH_LLDLIBS)

dcis.o	: dcis.c
	$(CCF) $(MATH_OPTS) $(MATH_FLAGS) -c $<

rcis.o	: rcis.c
	$(CCF) $(MATH_OPTS) $(MATH_FLAGS) -c $<

libm_vals.o	: libm_vals.s
	$(CCF) $(MATH_OPTS) $(MATH_FLAGS) -c $<

