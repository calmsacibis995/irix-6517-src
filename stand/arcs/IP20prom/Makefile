#ident "IP20prom/Makefile: $Revision: 1.32 $
#
# Makefile for IP20 PROM
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
REVISION=B
#
# -N => data immediately follows text
#
LLDOPTS=-non_shared -coff -m -N
LLCOPTS=

# RPROMADDRS == Real PROM Addresses
# DPROMADDRS == Debug PROM Addresses
RPROMADDRS= -T bfc00000 -B a8740000
DPROMADDRS= -T a8400000
UPROMADDRS= -T a8400000 -D 08480000 -B a84c0000

LLCDEFS= -D${CPUBOARD} ${CHIPDEFS} $(LLLCDEFS)
LDV= ${LD} ${CPUARCH_MI_LDOPTS} ${VLDFLAGS} ${LLDOPTS} ${ENDIAN}

TARGETDIR= ${PRODUCT}.O
VPATH= ${TARGETDIR}

#
# In the ASFILES list, csu.s must come first for proper load-order.
#
ASFILES= csu.s lmem_conf.s IP20asm.s r4k_eerom.s
CFFILES= $(SCFG).cf fsconf.cf
CFILES= IP20.c main.c $(SCFG).c fsconf.c hello_tune.c

LLIBDEPTH=../
LIBPSA= $(LIBSK) $(LIBSC)
LIBPON= ../${DEPTH}/ide/${PRODUCT}p.O/libpide.a
LLIBS= ${DEPTH}/$(PRODUCT).O/*.ln

LDIRT= ${OBJECTS} ${DOBJECTS} vers.[co] pvers.[co] *.map *.nm *.ln \
	$(ALLCFDIRT) deathprom ledprom \
	hello_tune.lod hello_tune.lst hello_tune.h

STDTARGETS= prom
OPTTARGETS= dprom 
TARGETS= ${STDTARGETS} ${OPTTARGETS}

# allow developers to have a different default target
# usually will be used to make dprom or lowprom the defaut
sinclude localdefault

default: directory ${STDTARGETS} 

COMMONPREF= IP20prom
include ${DEPTH}/commonrules
include Makerules

deathprom ledprom:	$$@.o
	cd ${TARGETDIR}; ${LDV} ${RPROMADDRS} $@.o -o $@ > $@.map; \
	${SIZE} -x $@; ${NM} -x -n $@ > $@.nm &

prom: ${OBJECTS} pvers.o ${LIBPON} ${LIBPSA}
	cd ${TARGETDIR}; ${LDV} ${RPROMADDRS} ${OBJECTS} pvers.o \
		${LIBPON} ${LIBPSA} -o $@ > $@.map; \
	${SIZE} -x $@; ${NM} -x -n $@ > $@.nm &

dprom: ${DOBJECTS} pvers.o ${LIBPON} ${LIBPSA}
	cd ${TARGETDIR}; ${LDV} ${DPROMADDRS} ${DOBJECTS} pvers.o \
		${LIBPON} ${LIBPSA} -o $@ > $@.map; \
	${SIZE} -x $@; ${NM} -x -n $@ > $@.nm &

uprom: ${DOBJECTS} pvers.o ${LIBPON} ${LIBPSA}
	cd ${TARGETDIR}; ${LDV} ${UPROMADDRS} ${DOBJECTS} pvers.o \
		${LIBPON} ${LIBPSA} -o $@ > $@.map; \
	${SIZE} -x $@; ${NM} -x -n $@ > $@.nm &

$(LIBPON):
	@echo "";echo "ERROR $(LIBPON) is required"; echo ""; exit 1;

$(LIBPSA):
	@echo "";echo "ERROR $@ is required"; echo""; exit 1;

directory: ${_FORCE}
	@if test ! -d ${TARGETDIR}; then mkdir ${TARGETDIR}; fi

pvers.c: ${OBJECTS} ${LIBIDE} ${LIBPSA}
	${NEWVERS} prom ${RELEASE} "Rev ${REVISION} ${CPUBOARD}, " > $@

hello_tune.h: hello_tune.lod
	$(TOOLROOT)/usr/bin/nawk -f M56001.nawk hello_tune.lod > hello_tune.h

hello_tune.lod: hello_tune.lod.uue
	uudecode hello_tune.lod.uue

hello_tune.d hello_tune.o: hello_tune.c hello_tune.h

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
