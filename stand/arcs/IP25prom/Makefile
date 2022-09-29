
############################################################################
#
# Makefile for IP25 PROM
#
############################################################################

DEPTH= ..
TARGDEPTH= ..
include ${DEPTH}/commondefs
include $(PRODUCTDEFS)
include $(CPUARCHDEFS)

DEF_CSTYLE=

# Revision code for version string

NEWVERS=./ip25vers.sh

# Release is used by the PROM to determine whether all PROM revisions
#	on the system are up to date.  Before releasing a set of PROMs,
#	be sure to increment it past the last released prom's revision.
#	Always check it back in as zero so we don't get multiple proms
# 	with the same ID!
RELEASE	= 6
INFO	= 

PROMADDRS = -T 900000001fc00000
SPROMADDRS = -T ffffffffbfc00000

#
# Command flag definitions
#

LLDOPTS= -rom -non_shared -elf -G 0 -64 -mips4
LLCDEFS = -D$(CPUBOARD) -DPROM ${SIMFLAGS} ${WORKAROUNDS} ${CCTAG}
ASFLAGS_MIPS4= $(CPUARCH_MI_ASOPTS4) -DLOCORE ${CDEFS} ${CINCS} ${SIMFLAGS}
LDV= ${LD} ${VLDFLAGS} ${LLDOPTS} ${ENDIAN} 

TARGETDIR= IP25.O
VPATH= ${TARGETDIR}

#
# The IP25 does not contain an EAROM, instead, a ip25-<CACHE-SIZE> 
# file is linked in at address 0x400 (right after the exception vector).
# Following are the defines for the prom.
#
# SCS = 1MB Secondary cache Size
# PRM = Processor request MAX (2 outstanding reads) For historical reasons
#	only, this value -1 is the actual value used on the confrig cycle.
# SCD = System clock divide - 2.5 default 
# SCCD = Scache clock divide - 1.5 default
# SCCT = Cache tap

SCS	=	1
PRM	=	1
SCD	=	4
SCCD	=	2
SCCT	=	9

#
# DEFAULT_CONFIG is the suffix used for the above defines - this 
# allows us to generate the listing file only once.

DEFAULT_CONFIG=	-1MB-4-2-1_9

#
# PROM_SUF - is the suffix used in naming the prom.
#
CONFIG=		-${SCS}MB-${SCD}-${SCCD}-${PRM}_${SCCT}
CONFIG_FLAGS=	-DPRM=${PRM} -DSCS=${SCS} -DSCCD=${SCCD} -DSCCT=${SCCT} -DSCD=${SCD}

ASFILES = entry.s pod_cache.s ccio.s libasm.s bmarb.s slave.s \
	  sysctlr.s master.s pod_cc.s pod_ebus.s pod_ioasm.s epcuart.s \
	  podasm.s setjmp.s

CFILES  = pod.c pod_io.c nvram.c pod_parse.c pod_master.c epc_config.c \
	  pod_iaid.c diagval.c pod_mem.c disp_evcfg.c mc3_config.c \
	  config.c epc_regtest.c pod_iaram.c main.c init_epcuart.c \
	  transfer.c

LDIRT= ${OBJECTS} vector.o pvers.o

STDTARGETS=prom
COMMONPREF= ip25prom

# allow developers to have a different default target
# usually will be used to make dprom or lowprom the defaut
sinclude localdefault

############################################################################
#  Default Rule:
#
default: directory ${STDTARGETS} 

include ${DEPTH}/commonrules
include Makerules

#
# This rule allows use to list more that 1 prom to be built. Simply add
# "make ip25prom" with variable overrides for the configs above.
#
prom:	
	make ip25prom
	make SCS=1 PRM=1 SCD=4 SCCD=2 SCCT=9 ip25prom
	make SCS=1 PRM=2 SCD=4 SCCD=2 SCCT=9 ip25prom
	make SCS=2 PRM=2 SCD=4 SCCD=2 SCCT=8 ip25prom
	make SCS=4 PRM=2 SCD=4 SCCD=2 SCCT=9 ip25prom
	make ip25prom.lst

ip25prom: ip25prom${CONFIG}.hex

ip25prom${CONFIG}.hex:	ip25prom${CONFIG}
	cd ${TARGETDIR}; \
	${CONVERT} -z -f intelhex ip25prom${CONFIG} > $@;

ip25prom${CONFIG}: ip25config${CONFIG}.o ${OBJECTS} vector.o pvers.o
	(cd ${TARGETDIR}; \
	       ${LDV} ${PROMADDRS} vector.o ip25config${CONFIG}.o \
	              ${OBJECTS} pvers.o -e start -o ip25prom${CONFIG}); \
	${SIZE} -xA ${TARGETDIR}/$@ ;\
	${NM} -Bnx ${TARGETDIR}/$@ | sort > ${TARGETDIR}/$@.nm

ip25config${CONFIG}.o:	${_FORCE}
	${AS} ${ASFLAGS_MIPS4} ${CONFIG_FLAGS} \
	      -o ${TARGETDIR}/ip25config${CONFIG}.o ip25config.s 

ip25prom.lst:	ip25prom${DEFAULT_CONFIG}
	${TOOLROOT}/usr/bin/dis -S ${TARGETDIR}/ip25prom${DEFAULT_CONFIG} > ${TARGETDIR}/ip25prom.lst &

directory: ${_FORCE}
	@if test ! -d ${TARGETDIR}; then mkdir ${TARGETDIR}; fi

pvers.c: ${OBJECTS}
	${NEWVERS} prom ${RELEASE} " ${INFO}" > $@

clobber: clean 

clean:	
	rm -rf ${TARGETDIR}

