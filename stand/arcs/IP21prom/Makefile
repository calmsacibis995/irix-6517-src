############################################################################
#
# Makefile for IP21 PROM
#
# Be default, makes with niblet included.  If you want to make without 
# niblet
#    1. add -DNO_NIBLET to the WORKAROUNDS define.
#    2. Remove niblet from the dependency list of the ip21prom target
#    3. Remove ${NIB_OBJS} from the action of the ip21prom target
#
############################################################################

DEPTH= ..
TARGDEPTH= ../..
include ${DEPTH}/commondefs
include $(PRODUCTDEFS)
include $(CPUARCHDEFS)

DEF_CSTYLE=

SUBDIRS=niblet
WORKAROUNDS=-DBOARD_TYPE_WAR
OPTIMIZER=-O1

# Flag for fpu compiling
FLAGS = -mips4 -non_shared -show -L/hosts/bongwater.engr/c/rag/64bit/nonshared -Yl,${TOOLROOT}/usr/lib -g

# Revision code for version string
NEWVERS=ip21vers.sh

# Release is used by the PROM to determine whether all PROM revisions
#	on the system are up to date.  Before releasing a set of PROMs,
#	be sure to increment it past the last released prom's revision.
#	Always check it back in as zero so we don't get multiple proms
# 	with the same ID!
RELEASE= 11
VCOPTS=
INFO=

POD_STACKADDR= a8000000000ec000
PROMADDRS = -T 800000041fc00000

NIBLET_TARGETDIR=niblet/PROM_LONG

NIB_OBJS=../${NIBLET_TARGETDIR}/nib_procs.o ../${NIBLET_TARGETDIR}/nib_code.o \
		../${NIBLET_TARGETDIR}/dcob.o ../${NIBLET_TARGETDIR}/nib_conf.o

#
# Command flag definitions
#
LLDOPTS= -rom -non_shared -elf -G 0 -64 -mips4
LLCDEFS = -D$(CPUBOARD) -DPROM -DPOD_STACKADDR=0x${POD_STACKADDR} ${SIMFLAGS} ${WORKAROUNDS}
ASFLAGS= $(CPUARCH_MI_ASOPTS) -DLOCORE ${CDEFS} ${CINCS} ${SIMFLAGS} ${WORKAROUNDS}
ASFLAGS_MIPS4= $(CPUARCH_MI_ASOPTS4) -DLOCORE ${CDEFS} ${CINCS} ${SIMFLAGS} ${WORKAROUNDS}
LDV= ${LD} ${VLDFLAGS} ${LLDOPTS} ${ENDIAN} 

TARGETDIR= IP21.O
VPATH= ${TARGETDIR}

#
# In the ASFILES list, entry.s must come first for proper load-order.
#
ASFILES = entry.s epcuart.s master.s slave.s ccio.s bmarb.s \
	  gparity.s libasm.s podasm.s pod_cc.s pod_cache.s \
	  pod_ebus.s setjmp.s sysctlr.s pod_bustags.s pod_ioasm.s \
	  block_div.s loop_block_div.s

CFILES  = config.c epc_config.c init_epcuart.c main.c \
	  mc3_config.c nvram.c pod.c pod_mem.c pod_parse.c \
	  pod_io.c transfer.c pod_master.c diagval.c \
	  disp_evcfg.c pod_iaid.c pod_iaram.c epc_regtest.c\
	  div2.c 


LDIRT= ${OBJECTS} pvers.o

STDTARGETS= prom
COMMONPREF= ip21prom

# allow developers to have a different default target
# usually will be used to make dprom or lowprom the defaut
sinclude localdefault



############################################################################
#  Default Rule:
#
default: directory ${STDTARGETS} 

NIBLET_TARGET=prom_niblet_long
include ${DEPTH}/commonrules
include Makerules

prom: ip21prom
	$(CONVERT) -f intelhex ip21prom >ip21prom.hex

sable sprom:
	make sableprom TARGETDIR=IP21SABLE.O SIMFLAGS=-DSABLE  \
	WORKAROUNDS= NIBLET_TARGET=prom_niblet_short \
	NIBLET_TARGETDIR=niblet/PROM_SHORT
 
sableprom dkprom ip21prom: ${OBJECTS} pvers.o niblet
	cd ${TARGETDIR}; ${LDV} ${PROMADDRS} ${OBJECTS} pvers.o ${NIB_OBJS} \
		-e start -o ../$@ ; \
	cd .. ; ${SIZE} -xA $@ ; ${NM} -Bnx $@ | sort > $@.nm &

niblet: ${_FORCE}
	cd niblet; make ${NIBLET_TARGET}; cd ..

ledprom: ledprom.o
	cd ${TARGETDIR}; ${LDV} ${PROMADDRS} ledprom.o \
		-e start -o ../$@ ; \
	cd .. ; ${SIZE} -xA $@ ; ${NM} -Bnx $@ | sort > $@.nm &
	$(CONVERT) -f intelhex ledprom >ledprom.hex

directory: ${_FORCE}
	@if test ! -d ${TARGETDIR}; then mkdir ${TARGETDIR}; fi

pvers.c: ${OBJECTS}
	${NEWVERS} prom ${RELEASE} " ${INFO}" > $@

block_div.o : block_div.s div2.h
	${AS} ${ASFLAGS_MIPS4} $< -o $(TARGETDIR)/$*.o

##	${ASF} $< -o $(TARGETDIR)/$*.o
loop_block_div.o : loop_block_div.s div2.h
	${AS} ${ASFLAGS_MIPS4} $< -o $(TARGETDIR)/$*.o

clobber: clean ${COMMONPREF}clobber
	for d in $(EVERYPRODUCT); do rm -rf $$d.O; done
	rm -rf IP21SABLE.O sableprom sableprom.nm
	rm -rf IP21.O ip21prom ip21prom.nm ip21prom.hex

clean:	
	if test -d ${TARGETDIR}; then cd ${TARGETDIR}; rm -f ${LDIRT}; fi
