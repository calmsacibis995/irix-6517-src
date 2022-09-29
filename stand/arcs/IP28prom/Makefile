#ident "IP28prom/Makefile: $Revision: 1.12 $
#
# Makefile for IP38 PROM
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

# Revsion code for version string
REVISION=A

# override global opts to use -rom flag
LLDOPTS=-rom -non_shared

# prom BSS, uncached
BSSADDR=9000000020f00000

# cached prom in compat K0 for cache diag purposes.
TEXTADDR=ffffffff9fc00000
DATAADDR=ffffffffbfc50000

# uncached prom (use for Logic Analyzer):
#TEXTADDR=ffffffffbfc00000

# mini-prom addresses (led, death and diag)
MTEXTADDR=ffffffffbfc00000

# dprom load addresses:
DTEXTADDR=9000000020600000
DDATAADDR=9000000020650000
DBSSADDR=9000000020690000

LLCDEFS= -D${CPUBOARD} -DBSSADDR=0x$(BSSADDR) -DDBSSADDR=0x$(DBSSADDR) ${CHIPDEFS} $(LLLCDEFS)
LLCOPTS=-use_readonly_const
LDV= ${LD} ${LLDOPTS} ${CPUARCH_MI_LDOPTS} ${VLDFLAGS} ${ENDIAN}

TARGETDIR= ${PRODUCT}.O
VPATH= ${TARGETDIR}

#
# In the ASFILES list, csu.s must come first for proper load-order.
#
ASFILES= csu.s lmem_conf.s IP28asm.s
CFFILES= $(PSCFG).cf fsconf.cf
CFILES= IP28.c main.c $(PSCFG).c fsconf.c hello_tune.c

LLIBDEPTH=../
LIBPSA= $(LIBSK) $(CPULIBSC)
LIBPON= ../${DEPTH}/ide/${PRODUCT}p.O/libpide.a
LLIBS= ${DEPTH}/$(PRODUCT).O/*.ln

LDIRT= ${OBJECTS} ${DOBJECTS} vers.[co] pvers.[co] *.map *.nm *.ln \
	$(ALLCFDIRT) deathprom ledprom *.tune

STDTARGETS= prom
OPTTARGETS= dprom 
TARGETS= ${STDTARGETS} ${OPTTARGETS}

# allow developers to have a different default target
# usually will be used to make dprom or lowprom the defaut
sinclude localdefault

default: directory ${STDTARGETS} 

COMMONPREF=IP28prom
include ${DEPTH}/commonrules
include Makerules

deathprom ledprom: $$@.o elspec.template
	sed -e 's/TEXTADDR/0x${MTEXTADDR}/' -e 's/DATAADDR/0x${DATAADDR}/' -e 's/BSSADDR/0x${BSSADDR}/' \
		elspec.template > ${TARGETDIR}/elspec.$@
	cd ${TARGETDIR}; ${LDV} -elspec elspec.$@ $@.o -o $@ > $@.map; \
	${SIZE} -Ax $@; ${NM} -x -n $@ > $@.nm

prom: ${OBJECTS} pvers.o ${LIBPON} ${LIBPSA} elspec.template
	sed -e 's/TEXTADDR/0x${TEXTADDR}/' -e 's/DATAADDR/0x${DATAADDR}/' -e 's/BSSADDR/0x${BSSADDR}/' \
		elspec.template > ${TARGETDIR}/elspec.$@
	cd ${TARGETDIR}; ${LDV} -elspec elspec.$@ ${OBJECTS} pvers.o \
	    ${LIBPON} ${LIBPSA} -o $@ > $@.map; \
	    ${SIZE} -Ax $@

dprom: ${DOBJECTS} pvers.o ${LIBPON} ${LIBPSA} elspec.template
	sed -e 's/TEXTADDR/0x${DTEXTADDR}/' -e 's/DATAADDR/0x${DDATAADDR}/' -e 's/BSSADDR/0x${DBSSADDR}/' \
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
