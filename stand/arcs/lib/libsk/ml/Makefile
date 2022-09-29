#!${TOOLROOT}/bin/make
#
# This makefile won't work with smake because smake doesn't understand VPATH
#
# Makefile libsk/ml
#
# This directory contains the CPU board-specific components of the standalone
# kernel.  The files in this directory are machine dependent.
#
# $Revision: 1.92 $

SKDEPTH= ..
include $(SKDEPTH)/libskdefs

# Set up to build machine dependent objects
LLCDEFS = -D$(CPUBOARD)

# when we begin using pmake, this will work:
# LLCDEFS += -D$(CPUBOARD)

OBJECTDIR=$(TARGETDIR)
LIBPATH=$(LIBNAME)

#
# Machine Dependent Info
#
IP19_X=\#
IP20_X=\#
IP21_X=\#
IP22_X=\#
IP25_X=\#
IP26_X=\#
IP27_X=\#
IP27XXL_X=\#
IP28_X=\#
IP30_X=\#
IP32_X=\#
4DXX_X=\#
IPMHSIM_X=\#
$(CPUBOARD)_X=
$(4DXX)_X=

EVEREST_ASRCS= EVERESTasm.s r4kasm.s r4kcache.s
EVEREST_CSRCS= EVEREST.c EVERESTnvram.c r4k.c evslave.c s1chip.c badaddr.c \
	       dshiftv.c sysctlr.c delay.c

#if $(CPUBOARD) == "IP19"
$(IP19_X)MACHDEP_ASRCS= $(EVEREST_ASRCS)
$(IP19_X)MACHDEP_CSRCS= $(EVEREST_CSRCS) k64prom32.c k64p32restart.c
#endif

#if $(CPUBOARD) == "IP20"
$(IP20_X)MACHDEP_ASRCS=IP20asm.s delayasm.s r4kasm.s  r4kcache.s
$(IP20_X)MACHDEP_CSRCS=IP20.c IP12nvram.c dp8573.c hpc.c hpc_scsi.c r4k.c badaddr.c wbadaddr.c k64prom32.c k64p32restart.c delay.c
#endif

BB_EVEREST_ASRCS= EVERESTasm.s tfpasm.s tfpcache.s tfptrap.s
BB_EVEREST_CSRCS= EVEREST.c EVERESTnvram.c tfp.c tfphook.c evslave.c s1chip.c \
		  badaddr.c dshiftv.c sysctlr.c delay.c

#if $(CPUBOARD) == "IP21"
$(IP21_X)MACHDEP_ASRCS= $(BB_EVEREST_ASRCS)
$(IP21_X)MACHDEP_CSRCS= $(BB_EVEREST_CSRCS)
#endif

#if $(CPUBOARD) == "IP22"
$(IP22_X)MACHDEP_ASRCS=IP22asm.s delayasm.s r4kasm.s r4kcache.s r4k_eerom.s
$(IP22_X)MACHDEP_CSRCS=IP22.c IP12nvram.c ds1286.c hpc.c hpc_scsi.c r4k.c badaddr.c wbadaddr.c dallas.c IP22nvram.c k64prom32.c k64p32restart.c delay.c sflash.c
#endif

#if $(CPUBOARD) == "IP25"
$(IP25_X)MACHDEP_ASRCS= EVERESTasm.s r10kcache.s r4kasm.s
$(IP25_X)MACHDEP_CSRCS= EVEREST.c EVERESTnvram.c evslave.c s1chip.c badaddr.c dshiftv.c sysctlr.c delay.c r4k.c
#endif

#if $(CPUBOARD) == "IP26"
$(IP26_X)MACHDEP_ASRCS=IP26asm.s tfpasm.s tfpcache.s tfptrap.s
$(IP26_X)MACHDEP_CSRCS=IP26.c IP12nvram.c ds1286.c hpc.c hpc_scsi.c tfp.c tfphook.c badaddr.c wbadaddr.c
#endif

#if $(CPUBOARD)$(SUBPRODUCT) == "IP27"
$(IP27_X)MACHDEP_ASRCS= SN0asm.s r10kcache.s r4kasm.s
$(IP27_X)MACHDEP_CSRCS= SN0.c SN0nvram.c badaddr.c \
	dshiftv.c delay.c r4k.c bridge.c xbow.c \
	klgetdrv.c graph.c hwgraph.c strtbl.c promgraph.c pg.c pglib.c\
	pgdrv.c stubs.c nic.c diag_enet.c diag_scsi.c diag_lib.c
#elif $(CPUBOARD)$(SUBPRODUCT) == "IP27XXL"
$(IP27XXL_X)MACHDEP_ASRCS= SN0asm.s r10kcache.s r4kasm.s
$(IP27XXL_X)MACHDEP_CSRCS= SN0.c SN0nvram.c badaddr.c \
	dshiftv.c delay.c r4k.c bridge.c xbow.c \
	klgetdrv.c graph.c hwgraph.c strtbl.c promgraph.c pg.c pglib.c\
	pgdrv.c stubs.c nic.c diag_enet.c diag_scsi.c diag_lib.c
#endif

#if $(CPUBOARD) == "IP28"
$(IP28_X)MACHDEP_ASRCS=IP28asm.s delayasm.s r4kasm.s r10kcache.s
$(IP28_X)MACHDEP_CSRCS=IP28.c IP12nvram.c ds1286.c hpc.c hpc_scsi.c r4k.c badaddr.c wbadaddr.c delay.c
#endif

#if $(CPUBOARD) == "IP30"
R4000_SR=\#
R10000_SR=\#
$(CPUARCH)_SR=
$(R4000_SR)$(IP30_X)MACHDEP_ASRCS=IP30asm.s delayasm.s r4kasm.s r4kcache.s 
$(R10000_SR)$(IP30_X)MACHDEP_ASRCS=IP30asm.s delayasm.s r4kasm.s r10kcache.s 
$(IP30_X)MACHDEP_CSRCS=IP30.c IP30nvram.c r4k.c nic.c badaddr.c wbadaddr.c \
	delay.c ds1687clk.c bridge.c ioc3.c heart.c xbow.c \
	sflash.c sa.c xb.c
#endif

#if $(CPUBOARD) == "IP32"
$(IP32_X)DEF_CSTYLE =
$(IP32_X)LLCDEFS= -D$(CPUBOARD) -D__STDC__=1 -D_KERNEL
$(CPUARCH)_Y=
$(IP32_X)MACHDEP_ASRCS=IP32tci.s IP32asm.s delayasm.s r4kasm.s IP32_cacheops.s usecdelay.s
$(IP32_X)MACHDEP_CSRCS=IP32.c r4k.c badaddr.c wbadaddr.c delay.c \
			ds1685.c env.c flash.c flashwrite.c \
			ds2502.c stubs.c 
#endif

#if $(CPUBOARD) == "IPMHSIM"
$(IPMHSIM_X)DEF_CSTYLE =
$(IPMHSIM_X)LLCDEFS= -D$(CPUBOARD) -D__STDC__=1 -D_KERNEL
$(CPUARCH)_Y=
$(IPMHSIM_X)MACHDEP_ASRCS=IPMHSIMasm.s delayasm.s r4kasm.s IP32_cacheops.s usecdelay.s
$(IPMHSIM_X)MACHDEP_CSRCS=IPMHSIM.c r4k.c badaddr.c wbadaddr.c delay.c \
			nvram.c 
#endif

# override for 4DXX unix prom
$(4DXX_X)MACHDEP_ASRCS=
$(4DXX_X)MACHDEP_CSRCS=

ASFILES = $(MACHDEP_ASRCS) faultasm.s csu.s genasm.s callarcsprom.s
CFILES = $(MACHDEP_CSRCS) nvram.c nvcompat.c \
	freq.c clogo.c spinlock.c startup.c businfo.c \
	setpage.c membig.c ${SKDEPTH}/${PRODUCT}.O/pvers.c

#
include $(LIBSKRULES)
MKDEPRULE= $(EVERYPRODUCT_MKDEPRULE)

${SKDEPTH}/${PRODUCT}.O/pvers.c: ${_FORCE}
	${NEWVERS} libsk ${RELEASE} "Rev ${REVISION} ${CPUBOARD}, " "" "" "${CFLAGS}" libsk > $@
