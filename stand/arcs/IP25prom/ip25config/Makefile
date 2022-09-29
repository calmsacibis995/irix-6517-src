############################################################################
#
# Makefile for IP25 PROM Configurations
#
############################################################################

DEPTH= ../..
TARGDEPTH= ..
include ${DEPTH}/commondefs
include $(PRODUCTDEFS)
include $(CPUARCHDEFS)

DEF_CSTYLE=

OPTIMIZER=-O3

# Flag for fpu compiling
#FLAGS = -mips4 -non_shared -show -L/hosts/bongwater.engr/c/rag/64bit/nonshared -Yl,${TOOLROOT}/usr/lib -g
FLAGS = -mips4 -non_shared -show 

#
# Command flag definitions
#

LLDOPTS= -rom -non_shared -elf -G 0 -64 -mips4
LLCDEFS = -D$(CPUBOARD) -DPROM ${SIMFLAGS} ${WORKAROUNDS}
ASFLAGS= $(CPUARCH_MI_ASOPTS) -DLOCORE ${CDEFS} ${CINCS} ${SIMFLAGS} ${WORKAROUNDS}
ASFLAGS_MIPS4= $(CPUARCH_MI_ASOPTS4) -DLOCORE ${CDEFS} ${CINCS} ${SIMFLAGS} ${WORKAROUNDS}
LDV= ${LD} ${VLDFLAGS} ${LLDOPTS} ${ENDIAN} 

TARGETDIR= ../IP25.O
VPATH= ${TARGETDIR}

#
# The IP25 does not contain an EAROM, instead, a r10000-<MHZ>-<CACHE-SIZE> 
# file is linked in at address 0x400 (right after the exception vector).
#
ASFILES = r10000-${IP25_CONFIG}.s
CFILES  = 

LDIRT= ${OBJECTS}

STDTARGETS= 
COMMONPREF= 

# allow developers to have a different default target
# usually will be used to make dprom or lowprom the defaut
#include localdefault



############################################################################
#  Default Rule:
#
default: ${STDTARGETS}

include ${DEPTH}/commonrules
include ../Makerules

ip25config-${SCACHE}.o: ip25config-${SCACHE}.s
 
clobber: clean ${COMMONPREF}clobber
	for d in $(EVERYPRODUCT); do rm -rf $$d.O; done
	rm -rf IP25SABLE.O sableprom sableprom.nm
	rm -rf IP25.O ip25prom ip25prom.nm ip25prom.hex

clean:	
	if test -d ${TARGETDIR}; then cd ${TARGETDIR}; rm -f ${LDIRT}; fi
