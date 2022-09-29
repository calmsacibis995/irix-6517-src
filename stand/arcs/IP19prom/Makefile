############################################################################
#
# Makefile for IP19 (Everest) PROM
#
# This makefile normally builds the prom code.
#
############################################################################

DEPTH= ..
TARGDEPTH= ../..
include ${DEPTH}/commondefs
include $(PRODUCTDEFS)
include $(CPUARCHDEFS)

DEF_CSTYLE=

SUBDIRS= niblet

# Revision code for version string
NEWVERS=ip19vers.sh
# Release is used by the PROM to determine whether all PROM revisions
#	on the system are up to date.  Before releasing a set of PROMs,
#	be sure to increment it past the last released prom's revision.
#	Always check it back in as zero so we don't get multiple proms
# 	with the same ID!
RELEASE= 13
INFO=

#
# -N => data immediately follows text
#
LLDOPTS=-m

# Prom Load addresses.
#	RPROMADDRS specifies the text and bss segment bases for the
#	  real prom
#	DPROMADDRS specifies the text segment base for the debugging prom
#	UPROMADDRS specifies the text, data, and BSS seg bases for
#	  yet another kind of prom (Don't know what it is for, though)
#

POD_STACKADDR= 800fe000
 
RPROMADDRS= -T bfc00000 -B ${POD_STACKADDR} 
DPROMADDRS= -T a8400000
UPROMADDRS= -T a8400000 -D 08480000 -B a84c0000
NPROMADDRS= -T 80000000

PROMADDRS= ${RPROMADDRS}

NIB_OBJS= nib_procs.o nib_code.o dcob.o nib_conf.o

#
# Command flag definitions
CVERSION= -cckr
LLCOPTS= -Wf,-EN
LLCDEFS = -DIP19 -DEVEREST -DPROM -DBRINGUP -DPOD_STACKADDR=0x${POD_STACKADDR}
ASFLAGS= -coff -non_shared -Wa,-mips3 -32bit -Wa,-r4000 ${MKDEPOPT} -DLOCORE ${CDEFS} ${CINCS} -G 0
LDV= ${LD} ${CPUARCH_MI_LDOPTS} ${VLDFLAGS} ${LLDOPTS} ${ENDIAN}

TARGETDIR= IP19.O
VPATH= ${TARGETDIR}

#
# In the ASFILES list, entry.s must come first for proper load-order.
#
ASFILES = entry.s epcuart.s master.s slave.s ccio.s bmarb.s \
	  libasm.s podasm.s pod_cc.s pod_cache.s pod_ebus.s setjmp.s \
	  sysctlr.s pod_bustags.s pod_ioasm.s

CFILES  = config.c epc_config.c init_epcuart.c main.c \
	  mc3_config.c nvram.c pod.c pod_mem.c pod_parse.c \
	  pod_io.c transfer.c pod_master.c diagval.c \
	  disp_evcfg.c pod_iaid.c pod_iaram.c epc_regtest.c

LDIRT= ${OBJECTS} pvers.o

STDTARGETS= ip19prom
COMMONPREF= ip19prom

# allow developers to have a different default target
# usually will be used to make dprom or lowprom the defaut
sinclude localdefault



############################################################################
#  Default Rule:
#
default: directory ${STDTARGETS} 

include ${DEPTH}/commonrules
include Makerules

prom: ip19prom
	$(CONVERT) -z -f intelhex ip19prom >ip19prom.hex

newt:
	make TARGETDIR=IP19NEWT.O SIMFLAGS=-DNEWTSIM PROMADDRS="${NPROMADDRS}" newtprom
 
# newtprom ip19prom: ${OBJECTS} pvers.o
#	cd ${TARGETDIR}; ${LDV} ${PROMADDRS} ${OBJECTS} pvers.o \
#
newtprom ip19prom: ${OBJECTS} ${NIB_OBJS} pvers.o niblet
	cd ${TARGETDIR}; ${LDV} ${PROMADDRS} ${OBJECTS} pvers.o \
		${NIB_OBJS} \
		-e start -o ../$@ > ../$@.map; \
	cd .. ; ${SIZE} -x $@ ; ${NM} -Bnx $@ > $@.nm &

niblet: ${_FORCE}
	cd niblet; make niblet; cd ..

${NIB_OBJS}:
	cd niblet; make niblet; cd ..

ledprom: ledprom.o
	cd ${TARGETDIR}; ${LDV} ${PROMADDRS} ledprom.o -e start -o ../$@ 

newtearom_prom:
	make TARGETDIR=IP19NEWT.O SIMFLAGS=-DNEWTSIM earom_prom

earom_prom: earom_prom.o
	cd ${TARGETDIR}; ${LDV} ${PROMADDRS} earom_prom.o -e start -o ../$@

directory: ${_FORCE}
	@if test ! -d ${TARGETDIR}; then mkdir ${TARGETDIR}; fi

pvers.c: ${OBJECTS}
	${NEWVERS} prom ${RELEASE} " ${INFO}" > $@

clobber: clean ${COMMONPREF}clobber
	for d in $(EVERYPRODUCT); do rm -rf $$d.O; done

clean:	
	if test -d ${TARGETDIR}; then cd ${TARGETDIR}; rm -f ${LDIRT}; fi
	for d in $(SUBDIRS); do cd $$d; make clean; cd ..; done
	
