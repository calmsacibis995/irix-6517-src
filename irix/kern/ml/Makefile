#!smake
# Copyright 1989, Silicon Graphics Inc., Mountain View, CA.
#
#
# Makefile for machine-level kernel sources.
#
#ident	"$Revision: 3.179 $"

DEPTH?=..
include ${DEPTH}/kcommondefs

# Compile ksync sources multiple times to generate packages implementing
# ksync objects with statistics.
#
TLINKDIR_MAKERULE= \
	mkdir -p statistics; \
	echo "\tcd statistics; $(MAKE) $${RULE:=$@}"; \
	cd statistics; \
	ln -sf ../chardlocks.c ./chardlocks.c; \
	ln -sf ../llsclocks.s ./llsclocks.s; \
	ln -sf ../Makefile.statistics ./Makefile; \
	${MAKE} $${RULE:=$@}; \
	cd ..;

KDIRT+=$(KPATH)/statistics 

KPATH=$(TOP)/ml
.PATH:$(KPATH)
KCINCS=-I${BOOTAREA}
KASINCS=-I${BOOTAREA}

#
# Machine Dependent Info
#

MACHDEP_CSRCS=
MACHDEP_ASRCS=

SUBDIRS=LOCORE
#if defined(KERNTRACE)
SUBDIRS+=kerntrace
#endif

#if $(CPUBOARD) == "IP21" || $(CPUBOARD) =="IP19" \
	|| $(CPUBOARD) =="IP25" || $(CPUBOARD) =="IP27"
MACHDEP_ASRCS+=uliasm.s
MACHDEP_CSRCS+=mp_invent.c
#endif

#if $(CPUBOARD) == "IP33" 
#MACHDEP_ASRCS+=uliasm.s
MACHDEP_CSRCS+=mp_invent.c
#endif

#if $(CPUBOARD) == "IP19" || $(CPUBOARD) == "IP25"
MACHDEP_CSRCS+=arcs.c error.c clksupport.c
MACHDEP_ASRCS+=tlb.s arcsasm.s
#ifnmake (every)
SUBDIRS+=EVEREST soft_fp
#endif
#endif

#if $(CPUBOARD) == "IP25"
MACHDEP_CSRCS+=r10k_cacherr.c r10kperf.c
#endif

#if $(CPUBOARD) == "IP21"
MACHDEP_CSRCS+=arcs.c error.c clksupport.c
#ifnmake (every)
SUBDIRS+=EVEREST TFP soft_fp
#endif
#endif

#if $(CPUBOARD) == "IP20"
MACHDEP_CSRCS+=IP20.c arcs.c delay.c error.c timer.c \
	timer_8254.c pio.c mcparity.c upgraph.c
MACHDEP_ASRCS+=IP20asm.s tlb.s mcparasm.s spl.s
#if $(COMPILATION_MODEL) == "64" || $(COMPILATION_MODEL) == "N32"
MACHDEP_ASRCS+=arcsasm.s
#endif
#ifnmake (every)
SUBDIRS+=soft_fp
#endif
#endif

#if $(CPUBOARD) == "IP22"
MACHDEP_CSRCS+=IP22.c arcs.c delay.c error.c timer.c timer_r4000.c \
	timer_8254.c pio.c mcparity.c upgraph.c
MACHDEP_ASRCS+=IP22asm.s tlb.s mcparasm.s spl.s
#if $(COMPILATION_MODEL) == "64" || $(COMPILATION_MODEL) == "N32"
MACHDEP_ASRCS+=arcsasm.s
#endif
#ifnmake (every)
SUBDIRS+=soft_fp
#endif
#endif

#if $(CPUBOARD) == "IP26"
MACHDEP_CSRCS+=IP26.c arcs.c delay.c error.c timer.c timer_8254.c \
	pio.c upgraph.c
MACHDEP_ASRCS+=IP26asm.s spl.s
SUBDIRS+=TFP soft_fp
#endif

#if $(CPUBOARD) == "IP27"
MACHDEP_CSRCS+=arcs.c error.c clksupport.c r10kperf.c
#if $(CPUARCH) == "R10000"
MACHDEP_CSRCS+=r10k_cacherr.c
#endif
MACHDEP_ASRCS+=tlb.s cpucounter.s spl.s
SUBDIRS+= SN soft_fp
#endif

#if $(CPUBOARD) == "IP28"
MACHDEP_CSRCS+=IP28.c arcs.c delay.c error.c timer.c timer_8254.c \
	pio.c r10k_cacherr.c r10kperf.c upgraph.c
MACHDEP_ASRCS+=IP28asm.s tlb.s spl.s
SUBDIRS+=soft_fp
#endif

#if $(CPUBOARD) == "IP30"
MACHDEP_CSRCS+=arcs.c error.c pio.c clksupport.c r10kperf.c \
	r10kperf.c r10k_cacherr.c
MACHDEP_ASRCS+=tlb.s cpucounter.s spl.s uliasm.s
#ifnmake (every)
SUBDIRS+=RACER soft_fp
#endif
#endif

#if $(CPUBOARD) == "IP32" || $(CPUBOARD) == "IP32SIM"
# IP32 shouldn't ultimately be using pio.c, when proper PCI
# support is added, it should be implemented for real.
MACHDEP_CSRCS+=arcs.c delay.c error.c r10kperf.c r10k_cacherr.c \
	timer.c timer_r4000.c pio.c upgraph.c
MACHDEP_ASRCS+=tlb.s spl.s
SUBDIRS+=MOOSEHEAD
#if $(COMPILATION_MODEL) == "64" || $(COMPILATION_MODEL) == "N32"
MACHDEP_ASRCS+=arcsasm.s
#endif
#ifnmake (every)
SUBDIRS+=soft_fp
#endif
#endif

#if $(CPUBOARD) == "IPMHSIM"
MACHDEP_CSRCS+=IPMHSIM.c delay.c error.c timer.c timer_r4000.c pio.c
MACHDEP_ASRCS+=IPMHSIMasm.s tlb.s spl.s
#if $(COMPILATION_MODEL) == "64" || $(COMPILATION_MODEL) == "N32"
MACHDEP_ASRCS+=arcsasm.s
#endif
#ifnmake (every)
SUBDIRS+=soft_fp
#endif
#endif

#if $(CPUBOARD) == "IP33"
MACHDEP_CSRCS+=arcs.c error.c clksupport.c
##if $(CPUARCH) == "BEAST"
#MACHDEP_CSRCS+=r10k_cacherr.c
##endif
MACHDEP_ASRCS+=cpucounter.s spl.s
SUBDIRS+= SN BEAST soft_fp
#endif

ARCHDEP_CSRCS =
ARCHDEP_ASRCS = atomic_ops.s

#if $(CPUARCH) == "R10000" || $(CPUBOARD) == "IP32"
ARCHDEP_ASRCS+=R10Kasm.s
#endif

#if $(CPUARCH) == "R4000"
ARCHDEP_ASRCS+=R4Kasm.s
#endif

#if $(COMPILATION_MODEL) != "64" && $(COMPILATION_MODEL) != "N32"
ARCHDEP_ASRCS += llfast.s
ARCHDEP_CSRCS += dshiftv.c llbit.c
#endif

ML_NAME=ml.a
ML_CSRCS=${MACHDEP_CSRCS} ${ARCHDEP_CSRCS} hook_exc.c tlbdump.c \
	splmeter.c ust_conv.c hwperf.c spldebug.c
ML_ASRCS= csu.s ust.s  process.s usercopy.s \
	  coproc_ctl.s hooks.s delayasm.s utlbmiss.s \
	  in_cksum_sub.s addrprobe.s asmsubr.s checkfp.s \
	  $(MACHDEP_ASRCS) $(ARCHDEP_ASRCS)
#if $(CPUARCH) != "BEAST"
ML_ASRCS+=cacheops.s
#endif
ML_OBJS=${ML_CSRCS:.c=.o} ${ML_ASRCS:.s=.o}
$(ML_NAME):$(ML_NAME)($(ML_OBJS)) MAKELIB

${ML_OBJS}: assym.h

#
# Production hardware locks
#
HARDLOCKS_NAME=hardlocks.a
HARDLOCKS_CSRCS=chardlocks.c
HARDLOCKS_ASRCS=llsclocks.s
HARDLOCKS_OBJS=${HARDLOCKS_CSRCS:.c=.o} ${HARDLOCKS_ASRCS:.s=.o}
$(HARDLOCKS_NAME):$(HARDLOCKS_NAME)($(HARDLOCKS_OBJS)) MAKELIB

#
# Nolocks for SP machines
#
NOLOCKS_NAME=nolocks.a
NOLOCKS_CSRCS=cnolocks.c
NOLOCKS_ASRCS=asmnolocks.s
NOLOCKS_OBJS=${NOLOCKS_CSRCS:.c=.o} ${NOLOCKS_ASRCS:.s=.o}
$(NOLOCKS_NAME):$(NOLOCKS_NAME)($(NOLOCKS_OBJS)) MAKELIB

#
# Debugging hardware locks
#
DHARDLOCKS_NAME=dhardlocks.a
DHARDLOCKS_CSRCS=dhardlocks.c
DHARDLOCKS_ASRCS=dhardlocksasm.s
DHARDLOCKS_OBJS=${DHARDLOCKS_CSRCS:.c=.o} ${DHARDLOCKS_ASRCS:.s=.o}
$(DHARDLOCKS_NAME):$(DHARDLOCKS_NAME)($(DHARDLOCKS_OBJS)) MAKELIB

# ml.a doesn't get shipped to customers, since it is part of kernel.o
ITARGETS=$(HARDLOCKS_NAME) $(NOLOCKS_NAME) $(DHARDLOCKS_NAME)
TARGETS=$(ML_NAME) $(ITARGETS)
.PRECIOUS:$(TARGETS)

#
# Local definitions
#
KDIRT=assym.h genassym *asm*locks.s elfassym elfdata32.c elfdata32.o \
	elfdata64.c elfdata64.o elfmain.o dhardlocks.s
#ifdef GFXBOARD
KCDEFS=-D$(GFXBOARD)
#endif
#ifnmake (clobber)
SUBDIRS_DEP+=doassym
#endif

#
# lint
# For lint just use dhardlocks since it has the most info
#
CFILES=$(ML_CSRCS) $(DHARDLOCKS_CSRCS)
LINTNAME=ml

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KINSTALL} ${ITARGETS}
	${KDEVINSTALL} ${TARGETS}

assym.h: elfassym
	@# use tmp file in case elfassym fails
	@# cd to BOOTAREA in case some says to make this from the ml dir
	cd $(BOOTAREA) ;\
	./elfassym genassym > TMPassym.h ;\
	mv TMPassym.h assym.h

#
# cd to BOOTAREA in case someone says to make this from the ml dir
#
#if $(COMPILATION_MODEL) == "64"
GENASYMLDOPTS=-nostdlib -L$(ROOT)/usr/lib64/mips3 -L$(ROOT)/usr/lib64
GENASYMCOPTS=-mips3 -64
#elif $(COMPILATION_MODEL) == "N32"
GENASYMLDOPTS=-nostdlib -L$(ROOT)/usr/lib32/mips3 -L$(ROOT)/usr/lib32
GENASYMCOPTS=-mips3 -n32
#else
GENASYMLDOPTS=-nostdlib -L$(ROOT)/usr/lib
GENASYMCOPTS=-mips1 -32
#endif
genassym: genassym.c
	cd $(BOOTAREA) ;\
	unset RLS_ID_OBJECT; $(CC) -D_GENASSYM $(GENASYMCOPTS) $(CINCS) $(CDEFS) \
		$(CVERSION) $(MKDEPOPT) $(KPATH)/genassym.c \
		$(GENASYMLDOPTS) -o genassym

elfdata32.c:	elfdata.c
	@touch $@
	@chmod u+w $@
	sed 's/_ELFSIZE_/32/g;s/LFMT//g' <$? >$@
	@chmod a-w $@

elfdata64.c:    elfdata.c
	@touch $@
	@chmod u+w $@
	sed 's/_ELFSIZE_/64/g;s/LFMT/ll/g' <$? >$@
	@chmod a-w $@

elfdata32.o:	elfdata32.c
	$(CC) -32 $(CINCS) $(CDEFS) $(CVERSION) $(MKDEPOPT) -c elfdata32.c

elfdata64.o:	elfdata64.c
	$(CC) -32 $(CINCS) $(CDEFS) $(CVERSION) $(MKDEPOPT) -c elfdata64.c

elfmain.o:	elfmain.c
	$(CC) -32 $(CINCS) $(CDEFS) $(CVERSION) $(MKDEPOPT) -c $(KPATH)/elfmain.c

r10k_cacherr.o:	r10k_cacherr.c
	$(CC) $(CINCS) $(CDEFS) ${CFLAGS} $(CVERSION) $(MKDEPOPT) -G0 -c $(KPATH)/r10k_cacherr.c

#
# elfassym always runs on HOST machine and is a 32 bit binary
#
elfassym: elfmain.o elfdata32.o elfdata64.o genassym
	cd $(BOOTAREA) ; \
	unset RLD_ID_OBJECT; \
	$(CC) -32 $(CINCS) $(CDEFS) $(CVERSION) $(MKDEPOPT) \
		elfmain.o elfdata32.o elfdata64.o -o elfassym \
		-nostdlib -L$(ROOT)/usr/lib -lelf

asmnolocks.s:assym.h snolocks.s
	cat assym.h $(KPATH)/snolocks.s > $(.TARGET)


#if !defined(DBOPTS)
STFLAG=-x
#else
#if $(DBOPTS) == ""
STFLAG=-x
#else
STFLAG=
#endif
#endif

IP20.o IP22.o IP26.o IP28.o: $(KPATH)/$(.PREFIX).c
	$(CCF) -c $(KPATH)/$*.c && \
	$(LDF) -r $(STFLAG) $(.TARGET:T) -o $$$$.o && \
	mv $$$$.o $(.TARGET:T)
