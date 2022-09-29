#ident "$Revision: 1.33 $"
#
# Makefile for IP26 (teton) PROM
#
# This makefile normally builds the prom code.
#
DEPTH= ..
TARGDEPTH= ../..
include ${DEPTH}/commondefs
include $(PRODUCTDEFS)
include $(ENDIANDEFS)
include $(CPUARCHDEFS)

DEF_CSTYLE=

TETON_X=\#
TETON_E_X=\#
$(PRODUCT)_X=

# Revsion code for version string
REVISION=C

# get the cool mips 4 instructions
MIPSVERS=-mips4

# override global opts to use -rom flag
LLDOPTS=-rom -non_shared

# RPROMADDRS == Real PROM Addresses
# DPROMADDRS == Debug PROM Addresses
BSSADDR=9000000008f00000

# cached prom (should work now that kernel tunes fifos).  Use non-coherent
# cached mode like we are suppost to so it's easier recognise prom addrs.
# NOTE:  This will not work with 1.X IU due to a chip bug.
TEXTADDR=980000001fc00000

# semi-cached (chandra) prom:
#TEXTADDR=800000001fc00000

# uncached prom (use for Logic Analyzer):
#TEXTADDR=900000001fc00000

# mini-prom addresses (led, death and diag)
MTEXTADDR=900000001fc00000

# dprom load addresses:
DTEXTADDR=9000000008400000
DBSSADDR=9000000008480000

LLCDEFS= -D${CPUBOARD} -DBSSADDR=0x$(BSSADDR) -DDBSSADDR=0x$(DBSSADDR) ${CHIPDEFS} $(LLLCDEFS)
LLCOPTS=-use_readonly_const
LDV= ${LD} ${LLDOPTS} ${CPUARCH_MI_LDOPTS} ${VLDFLAGS} ${ENDIAN}

TARGETDIR= ${PRODUCT}.O
VPATH= ${TARGETDIR}

#
# In the ASFILES list, csu.s must come first for proper load-order.
#
ASFILES= csu.s lmem_conf.s cache.s
CFFILES= $(PSCFG).cf fsconf.cf
CFILES= IP26.c main.c $(PSCFG).c fsconf.c hello_tune.c

LLIBDEPTH=../
LIBPSA= $(LIBSK) $(LIBSC)
LIBPON= ../${DEPTH}/ide/${PRODUCT}p.O/libpide.a
LLIBS= ${DEPTH}/$(PRODUCT).O/*.ln

LDIRT= ${OBJECTS} ${DOBJECTS} vers.[co] pvers.[co] *.map *.nm *.ln \
	$(ALLCFDIRT) deathprom ledprom *.tune

MKADPCM= ADPCM/make_adpcm_array
STDTARGETS= prom
OPTTARGETS= dprom 
TARGETS= ${STDTARGETS} ${OPTTARGETS}

# allow developers to have a different default target
# usually will be used to make dprom or lowprom the defaut
sinclude localdefault

default: directory ${STDTARGETS} 

COMMONPREF=IP26prom
include ${DEPTH}/commonrules
include Makerules

deathprom ledprom: $$@.o elspec.template
	sed -e 's/TEXTADDR/0x${MTEXTADDR}/' -e 's/BSSADDR/0x${BSSADDR}/' \
		elspec.template > ${TARGETDIR}/elspec.$@
	cd ${TARGETDIR}; ${LDV} -elspec elspec.$@ $@.o -o $@ > $@.map; \
	${SIZE} -Ax $@; ${NM} -x -n $@ > $@.nm

prom: ${OBJECTS} pvers.o ${LIBPON} ${LIBPSA} elspec.template
	sed -e 's/TEXTADDR/0x${TEXTADDR}/' -e 's/BSSADDR/0x${BSSADDR}/' \
		elspec.template > ${TARGETDIR}/elspec.$@
	cd ${TARGETDIR}; ${LDV} -elspec elspec.$@ ${OBJECTS} pvers.o \
	    ${LIBPON} ${LIBPSA} -o $@ > $@.map; \
	    ${SIZE} -Ax $@

dprom: ${DOBJECTS} pvers.o ${LIBPON} ${LIBPSA} elspec.template
	sed -e 's/TEXTADDR/0x${DTEXTADDR}/' -e 's/BSSADDR/0x${DBSSADDR}/' \
		elspec.template > ${TARGETDIR}/elspec.$@
	cd ${TARGETDIR}; ${LDV} -elspec elspec.$@ ${DOBJECTS} pvers.o \
	    ${LIBPON} ${LIBPSA} -o $@ > $@.map; \
	    ${SIZE} -Ax $@

$(LIBPON):
	@echo "";echo "ERROR $(LIBPON) is required"; echo ""; exit 1;

$(LIBPSA):
	@echo "";echo "ERROR $@ is required"; echo""; exit 1;

directory: ${_FORCE}
	@if test ! -d ${TARGETDIR}; then mkdir ${TARGETDIR}; fi

pvers.o: $(TARGETDIR)/pvers.c
	$(CCF) $(TARGETDIR)/pvers.c -c -o $(TARGETDIR)/$@

$(TARGETDIR)/pvers.c: ${OBJECTS} ${LIBIDE} ${LIBPSA}
	${NEWVERS} prom "${RELEASE}" "Rev ${REVISION}" ${CPUBOARD} ${VERSCPU} > $@

lint: ${LOBJECTS}
	$(LINT) $(LINTCFLAGS) ${LOBJECTS} ${LLIBS}
	rm -f ${LOBJECTS}

clobber: clean ${COMMONPREF}clobber
	for d in $(EVERYPRODUCT); do rm -rf $$d.O; done

clean:	${COMMONPREF}clean
	@if test -d ${TARGETDIR}; then cd ${TARGETDIR}; rm -f ${LDIRT}; fi

tags:	${COMMONPREF}tags

incdepend:	$(CFFILES:.cf=.c) ${COMMONPREF}incdepend
depend:		$(CFFILES:.cf=.c) ${COMMONPREF}depend

fluff: ${_FORCE}
	${LINT} ${LINTFLAGS} ${CDEFS} ${CINCS} ${CFILES} ${LDLIBS}
