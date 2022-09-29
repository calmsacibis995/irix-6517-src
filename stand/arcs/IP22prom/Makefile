#ident "IP22prom/Makefile: $Revision: 1.32 $
#
# Makefile for IP22 PROM
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

4DACE1_O32_X=\#
4DACE1_64_X=\#
4DCT_X=\#
$(PRODUCT)_X=

# Revsion code for version string
$(4DACE1_64_X)REVISION=Proto 64 bit
$(4DACE1_O32_X)REVISION=Proto -o32
$(4DCT_X)REVISION=B4 R4X00

# override global opts to use -rom flag
LLDOPTS=-rom -m -non_shared

# RPROMADDRS == Real PROM Addresses
# DPROMADDRS == Debug PROM Addresses

# cached prom (normal build):
RPROMADDRS= -T 9fc00000 -B a8740000

# uncached prom (use for Logic Analyzer):
#RPROMADDRS= -T bfc00000 -B a8740000

#DPROMADDRS= -T a8400000 -B a8700000
DPROMADDRS= -T a8600000
UPROMADDRS= -T a8400000 -D 08480000 -B a84c0000
$(4DACE1_64_X)RPROMADDRS= -T 900000001fc00000 -B 9000000008740000
$(4DACE1_64_X)DPROMADDRS= -T 9000000008400000

$(4DACE1_64_X)RAGCDEFS=-DNOBOOTTUNE -DRAGNAROK
DTEXTADDR=a8400000
DBSSADDR=a8700000
$(4DACE1_64_X)DTEXTADDR=9000000008400000
$(4DACE1_64_X)DBSSADDR=9000000008480000

TEXTADDR=9fc00000
BSSADDR=a8700000
$(4DACE1_64_X)TEXTADDR=900000001fc00000
$(4DACE1_64_X)BSSADDR=9000000008700000

LLCDEFS= -D${CPUBOARD} -DBSSADDR=0x$(BSSADDR) -DDBSSADDR=0x$(DBSSADDR) ${CHIPDEFS} $(LLLCDEFS)
LLCOPTS=-use_readonly_const
LDV= ${LD} ${LLDOPTS} ${CPUARCH_MI_LDOPTS} ${VLDFLAGS} ${ENDIAN}

TARGETDIR= ${PRODUCT}.O
VPATH= ${TARGETDIR}

#
# In the ASFILES list, csu.s must come first for proper load-order.
#
ASFILES= csu.s lmem_conf.s IP22asm.s r4k_eerom.s
CFFILES= $(PSCFG).cf fsconf.cf
CFILES= IP22.c main.c $(PSCFG).c fsconf.c $(TUNEFILES)

LLIBDEPTH=../
LIBPSA= $(LIBSK) $(LIBSC)
LIBPON= ../${DEPTH}/ide/${PRODUCT}p.O/libpide.a
LLIBS= ${DEPTH}/$(PRODUCT).O/*.ln

LDIRT= ${OBJECTS} ${DOBJECTS} vers.[co] pvers.[co] *.map *.nm *.ln \
	$(ALLCFDIRT) deathprom ledprom *.tune

MKADPCM= ADPCM/make_adpcm_array
OPTTARGETS= prom 
STDTARGETS= dprom
OPTTARGETS= prom 
TARGETS= ${STDTARGETS} ${OPTTARGETS}

$(4DCT_X)TUNEFILES=hello_tune.c samps.c
$(4DACE1_O32_X)TUNEFILES=plucktune.c
$(4DACE1_64_X)TUNEFILES=plucktune.c

$(4DCT_X)TUNES=IP24start.tune IP24stop.tune IP24badgfx.tune
$(4DACE1_O32_X)TUNES=IP22start.tune IP22stop.tune IP22badgfx.tune
$(4DACE1_64_X)TUNES=IP22start.tune IP22stop.tune IP22badgfx.tune

# allow developers to have a different default target
# usually will be used to make dprom or lowprom the defaut
sinclude localdefault

default: directory ${STDTARGETS} 

COMMONPREF= IP22prom
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
	cd ${TARGETDIR}; ${LDV} -sgi_kernel ${DPROMADDRS} ${DOBJECTS} pvers.o \
	    ${LIBPON} ${LIBPSA} -o $@ > $@.map; \
	    ${SIZE} -Ax $@

# cd ${TARGETDIR}; ${LDV} -elspec elspec.$@ ${DOBJECTS} pvers.o

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

samps.o samps.d: ${TUNES}

tunes: ${TUNES}

${TUNES}: ${TUNES:.tune=.aiff} ${MKADPCM}

${MKADPCM}: ${MKADPCM}.c
	( cd ADPCM; $(MAKE) MAKEFLAGS=$(MAKEFLAGS) make_adpcm_array )

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

Static="cvstatic.fileset"
static: ${LIBSA} ${ASFILES} ${CFILES}
	@echo "Making cvstatic data file"
	@echo ${ASFILES} ${CFILES} > ${Static}
	@find '../lib' -name '*.[chs]' -print >> ${Static}
	@echo "-I../include" >> ${Static}
	@echo "-I../../../include" >> ${Static}
