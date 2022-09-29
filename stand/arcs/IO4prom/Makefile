#
# Makefile for the Everest IO4 PROM
#
# This Makefile builds the code which resides in the IO4 EPC's
# flash EPROM.  The code implicitly assumes that it is being 
# called by either the IP19 or IP21 PROM and relies on memory
# and certain data structure provided by these PROMS. 
#

PRODUCT?=EVEREST32
DEPTH= ..
TARGDEPTH= ../..
include ${DEPTH}/commondefs
include $(PRODUCTDEFS)
include $(CPUARCHDEFS)
include $(ENDIANDEFS)

DEF_CSTYLE=

# Revsion code for version string
RELEASE= 4.19
REVISION=A
VERSION=419

#
# -N => data immediately follows text
#
# A hack for the merged redwood/ucode compilers. -N in the 3.18 and 3.19
# compilers forces -coff, which is unfortunately not supported by
# subsequent tools, such as size, or promcvt.
# The workaround implemented was '-sgi_kernel', which creates a -N type
# output file, but in elf.
# -sgi_kernel is only recognized by the 32 bit ld, so have to special
# case the option.
# The reason that this hack is implemented is that subsequent tools, such
# as size, nm and promcvt, can not deal with coff files.

64_Q=\#
${CPUARCH_PFIX}_Q=

LD_N_FLAG=-sgi_kernel
${64_Q}LD_N_FLAG=-N

LDV= ${LD} ${CPUARCH_MI_LDOPTS} -non_shared ${LD_N_FLAG} ${VLDFLAGS} ${LLDOPTS} ${ENDIAN}
LLDOPTS=-m

# Prom Load Addresses
#	RPROMADDRS specifies the text and bss segment bases for the
#	  real prom.
#	DPROMADDRS specifies the text and bss segment bases for the
#	  debugging version of the PROM.  We want to load debugging 
#	  versions of the PROM, so we have to keep it out of the
#	  way of the primary PROM
#	SPROMADDRS specifies the text and bss segment bases for the
#	  sable version of the prom. Sable only supports 8 mb of main
#	  memory, so we have to load the prom at a lower address.
#
RPROMADDRS=81800000
DPROMADDRS=81c00000
SPROMADDRS=80400000

$(64_Q)RPROMADDRS=A800000001800000
$(64_Q)DPROMADDRS=A800000001C00000
$(64_Q)SPROMADDRS=A800000000400000

PROMADDRS   = ${RPROMADDRS}

#
# Command flag definitions
#
LLCOPTS =
LLCDEFS = -D$(CPUBOARD) ${SIMFLAGS}
TARGETDIR= ${PRODUCT}.O
VPATH= ${TARGETDIR}

VMECFILES=pod_cmd.c pod_fvutils.c pod_lpbk.c pod_errors.c pod_iaid.c \
	pod_msgs.c pod_vmecc.c pod_fchip.c pod_iaram.c

VMEHFILES=pod_fvdefs.h pod_fvmsgs.h 
VMEASFILES= 

#
# In the ASFILES list, sysasm.s must come first for proper load-order.
#
ASFILES= sysasm.s loop_block_div.s ${VMEASFILES}
CFFILES= io4conf.cf fsconf.cf
CFILES= main.c io4conf.c sysinit.c fsconf.c flashprom.c inventory.c \
        io4_config.c serial.c sysctlr.c div2.c margin.c ${VMECFILES} \
	ip25mon.c

LLIBDEPTH=../
LIBPSA= $(LIBSK) $(LIBSC)
LLIBS= ${DEPTH}/$(PRODUCT).O/*.ln

LDIRT= ${OBJECTS} ${DOBJECTS} vers.[co] ${PRODUCT}pvers.[co] *.map *.nm *.ln \
	$(ALLCFDIRT)

STDTARGETS= prom
OPTTARGETS= dprom
TARGETS= ${STDTARGETS} ${OPTTARGETS}

# allow developers to have a different default target
# usually will be used to make dprom or lowprom the defaut
sinclude localdefault

default: directory ${STDTARGETS} 

COMMONPREF= IO4prom
include ${DEPTH}/commonrules
include Makerules

prom: ${PRODUCT}prom segldr_dir 

${PRODUCT}prom ${PRODUCT}dprom ${PRODUCT}sprom: ${OBJECTS} ${PRODUCT}pvers.o \
		${LIBPSA}
	cd ${TARGETDIR}; ${LDV} -T ${PROMADDRS} ${OBJECTS} ${PRODUCT}pvers.o \
		 ${LIBPSA} -e entry -o ../$@ > ../$@.map
	${SIZE} -Ax $@; ${NM} -Bnx -n $@ | sort > $@.nm

dprom:
	make PROMADDRS=${DPROMADDRS} ${PRODUCT}dprom
dproms:
	make PRODUCT=EVEREST32 dprom 
	make PRODUCT=BB_EVEREST dprom 
	make PRODUCT=T5_EVEREST dprom 

sprom:
	make TARGETDIR=${PRODUCT}S.O SIMFLAGS=-DSABLE \
		PROMADDRS=${SPROMADDRS} ${PRODUCT}sprom

segldr_dir:
	cd segldr; make VERSION=${VERSION}

$(LIBPSA):
	@echo "";echo "ERROR $@ are required; build them first"; echo""; exit 1

directory: ${_FORCE}
	@if test ! -d ${TARGETDIR}; then mkdir ${TARGETDIR}; fi

${PRODUCT}pvers.c: ${OBJECTS} ${LIBIDE} ${LIBPSA}
	${NEWVERS} prom ${RELEASE} "Rev ${REVISION} ${CPUBOARD}, " > $@
#
# The real prom is from ajc (allan christie)
# This prom will be from his work area ...
# It currently is version 311 and is need for the
# The builds (ie. io4prom, and eoe1 images).
#
# To change this you need to check in a uuencoded
# file call io4prom.uunc.
#
install: prom
	rm -rf io4prom.bin
	uudecode io4prom.uunc > io4prom.bin
	$(INSTALL) -idb 'mach(CPUBOARD=${CPUBOARD})' \
		   -m 644 -F /usr/cpu/firmware io4prom.bin

lint: ${LOBJECTS}
	$(LINT) $(LINTCFLAGS) ${LOBJECTS} ${LLIBS}
	rm -f ${LOBJECTS}

clobber: clean ${COMMONPREF}clobber
	cd segldr ; make clobber
	rm -f ${PRODUCT}prom.bin ${PRODUCT}dprom.bin ${PRODUCT}sprom.bin
	rm -f ${PRODUCT}prom.map ${PRODUCT}dprom.map ${PRODUCT}sprom.map
	rm -f ${PRODUCT}prom.nm  ${PRODUCT}dprom.nm  ${PRODUCT}sprom.nm
	rm -f ${PRODUCT}prom     ${PRODUCT}dprom     ${PRODUCT}sprom
	rm -rf ${PRODUCT}.O      ${PRODUCT}S.O
	rm -f io4prom.bin
	for d in $(EVERYPRODUCT); do rm -rf $$d.O $$dS.O; done

clean:	${COMMONPREF}clean
	cd segldr ; make clean
	@if test -d ${TARGETDIR}; then cd ${TARGETDIR}; rm -f ${LDIRT}; fi

tags:	${COMMONPREF}tags

incdepend:	$(CFFILES:.cf=.c) ${COMMONPREF}incdepend
depend:		$(CFFILES:.cf=.c) ${COMMONPREF}depend

fluff: ${_FORCE}
	${LINT} ${LINTFLAGS} ${CDEFS} ${CINCS} ${CFILES} ${LDLIBS}

loop_block_div.o : loop_block_div.s
	${AS} ${CPUARCH_DEFS} ${CPUARCH_MI_ASOPTS4} $< -o $(TARGETDIR)/$*.o
