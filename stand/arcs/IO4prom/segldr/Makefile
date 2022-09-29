#
# Makefile for the Everest IO4 PROM loader segment
#
# This Makefile builds the code which resides in the IO4 EPC's
# flash EPROM.  The code implicitly assumes that it is being 
# called by either the IP19, IP21 or IP25 PROM and relies on memory
# and certain data structure provided by these PROMS. 
#

PRODUCT=T5_EVEREST

DEPTH= ../..
TARGDEPTH= ../../..
include ${DEPTH}/commondefs
include $(PRODUCTDEFS)
include $(CPUARCHDEFS)
include $(ENDIANDEFS)

MIPSVERS=-mips3
DEF_CSTYLE=

# Revsion code for version string
RELEASE= 2.1
REVISION=A

64_Q=\#
${CPUARCH_PFIX}_Q=

#
# -N => data immediately follows text
#
LDV= ${LD} ${CPUARCH_MI_LDOPTS} -non_shared -N ${VLDFLAGS} ${LLDOPTS} ${ENDIAN}

# Not understood by ragnarok linker
#LLDOPTS=-m

# Prom Load Addresses
#	RSLADDRS specifies the text and bss segment bases for the
#	  real prom.
#	DSLADDRS specifies the text and bss segment bases for the
#	  debugging version of the segment loader.  We want to load debugging 
#	  versions of the segment loader, so we have to keep it out of the
#	  way of the primary PROM and segment loader
#
RSLADDRS=81b00000
DSLADDRS=81c00000

$(64_Q)RSLADDRS=0xA800000001b00000
$(64_Q)DSLADDRS=A800000001C00000

SLADDRS   = ${RSLADDRS}

#
# Command flag definitions
#
WORKAROUNDS = -DEAROM_CKSUM_WAR
LLCDEFS = -D$(CPUBOARD) ${SIMFLAGS} ${WORKAROUNDS} -DDECOMPRESS_ONLY -Wab,-tfprev10
LLASDEFS= -Wab,-tfprev10
TARGETDIR= ${PRODUCT}.O
VPATH= ${TARGETDIR}

#
# In the ASFILES list, slinit.s must come first for proper load-order.
#
ASFILES= slinit.s slasm.s ccuart.s epcuart.s
CFILES= main.c segment.c cons_io.c transfer.c pod_io.c sysctlr.c compress.c \
	r4000.c r8000.c r10000.c

LLIBDEPTH=../
LLIBS= ${DEPTH}/$(PRODUCT).O/*.ln

LDIRT= ${OBJECTS} ${DOBJECTS} vers.[co] pvers.[co] *.map *.nm *.ln

STDTARGETS= segldr
OPTTARGETS= dsegldr
TARGETS= ${STDTARGETS} ${OPTTARGETS}

# allow developers to have a different default target
# usually will be used to make dprom or lowprom the defaut
sinclude localdefault

default: directory ${STDTARGETS} 

COMMONPREF= segldr
include ${DEPTH}/commonrules
include ../Makerules

segldr: ${PRODUCT}segldr
	cd ${DEPTH}/lib ; make PRODUCT=EVEREST32
	cd ..; make PRODUCT=EVEREST32 EVEREST32prom
	cd ${DEPTH}/lib ; make PRODUCT=BB_EVEREST
	cd ..; make PRODUCT=BB_EVEREST BB_EVERESTprom
	cd ${DEPTH}/lib ; make PRODUCT=T5_EVEREST
	cd ..; make PRODUCT=T5_EVEREST T5_EVERESTprom
	../../tools/promcvt/promcvt -v $(VERSION) -V -b -o ../io4prom.bin \
		-M ${PRODUCT}segldr -R ../EVEREST32prom -T ../BB_EVERESTprom \
		-A ../T5_EVERESTprom

${PRODUCT}segldr ${PRODUCT}dsegldr ${PRODUCT}ssegldr: ${OBJECTS} pvers.o
	cd ${TARGETDIR}; ${LDV} -T ${SLADDRS} ${OBJECTS} pvers.o \
		 ${LIBPSA} -e entry -o ../$@ > ../$@.map
	${SIZE} -Ax $@; ${NM} -Bnx -n $@ | sort > $@.nm

dsegldr:
	make SLADDRS=${DSLADDRS} ${PRODUCT}dsegldr

ssegldr:
	make TARGETDIR=${PRODUCT}S.O SIMFLAGS=-DSABLE \
		SLADDRS=${DSLADDRS} ${PRODUCT}ssegldr

directory: ${_FORCE}
	@if test ! -d ${TARGETDIR}; then mkdir ${TARGETDIR}; fi

pvers.c: ${OBJECTS} ${LIBIDE} ${LIBPSA}
	${NEWVERS} segldr ${RELEASE} "Rev ${REVISION} MIPS3, " > $@

install: segldr
	$(INSTALL) -idb 'mach(CPUBOARD=${CPUBOARD})' \
		   -m 644 -F /usr/cpu/firmware ${PRODUCT}prom.bin

clobber: clean ${COMMONPREF}clobber
	rm -rf ${PRODUCT}.O ${PRODUCT}S.O ${PRODUCT}segldr
	for d in $(EVERYPRODUCT); do rm -rf $$d.O $$dS.O; done

clean:	${COMMONPREF}clean
	@if test -d ${TARGETDIR}; then cd ${TARGETDIR}; rm -f ${LDIRT}; fi

tags:	${COMMONPREF}tags

incdepend:	$(CFFILES:.cf=.c) ${COMMONPREF}incdepend
depend:		$(CFFILES:.cf=.c) ${COMMONPREF}depend

sable:	sable.o
	${LDV} -T bfc00000 -o sable -e sable BB_EVEREST.O/sable.o
