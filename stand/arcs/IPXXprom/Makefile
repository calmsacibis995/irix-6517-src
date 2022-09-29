#ident "$Revision: 1.12 $"
#
# Makefile for IPXX PROM -- a prom simulator running under irix.
#
#
DEPTH= ..
TARGDEPTH= ../..
include ${DEPTH}/commondefs
include $(PRODUCTDEFS)
include $(CPUARCHDEFS)
include $(ENDIANDEFS)

# Revsion code for version string
REVISION=A

LLCDEFS= -D${CPUBOARD} ${CHIPDEFS}
LDV= /usr/bin/ld -o32 -elf -no_unresolved -g
TARGETDIR=.

#
# In the ASFILES list, csu.s must come first for proper load-order.
#
ASFILES= csu.s
CFILES= IPXX.c IPXXml.c IPXXnvram.c main.c glms.c gltp.c glkbd.c disk.c \
	tty.c stubs.c tests.c pon_more.c
CFFILES= IPXXconf.cf fsconf.cf
CFGO=IPXXconf.o

LLIBDEPTH=../
LIBPSA= $(DLIBSK) $(DLIBSC)
LLIBS= ${DEPTH}/*.ln

LDIRT= ${OBJECTS} vers.[co] pvers.[co] *.map *.nm *.ln ${ALLCFDIRT} promgl

STDTARGETS= prom promgl
TARGETS= ${STDTARGETS} ${OPTTARGETS}

# allow developers to have a different default target
# usually will be used to make dprom or lowprom the defaut
sinclude localdefault

default: directory ${STDTARGETS}

COMMONPREF= IPXXprom
include ${DEPTH}/commonrules

prom: ${OBJECTS} ${CFGO} pvers.o ${LIBPSA}
	${CCF} ${OBJECTS} ${CFGO} pvers.o ${LIBPSA} -o $@

promgl: promgl.c
	cc -g -I../include promgl.c -o promgl -lgl

$(LIBPSA):
	@echo "";echo "ERROR $@ is required"; echo""; exit 1;

directory: ${_FORCE}
	@if test ! -d ${TARGETDIR}; then mkdir ${TARGETDIR}; fi

pvers.c: ${OBJECTS} ${LIBIDE} ${LIBPSA}
	${NEWVERS} prom ${RELEASE} "Rev ${REVISION} ${CPUBOARD}, " > $@

lint: ${LOBJECTS}
	$(LINT) $(LINTCFLAGS) ${LOBJECTS} ${LLIBS}
	rm -f ${LOBJECTS}

clobber: clean ${COMMONPREF}clobber
	for d in $(EVERYPRODUCT); do rm -rf $$d.O; done

clean:	${COMMONPREF}clean
	@if test -d ${TARGETDIR}; then cd ${TARGETDIR}; rm -f ${LDIRT}; fi

incdepend:	${CFFILES:.cf=.c} ${COMMONPREF}incdepend
depend:		${CFFILES:.cf=.c} ${COMMONPREF}depend

tags: ${COMMONPREF}tags

fluff: ${_FORCE}
	${LINT} ${LINTFLAGS} ${CDEFS} ${CINCS} ${CFILES} ${LDLIBS}

Static="cvstatic.fileset"
static: ${LIBSA} ${ASFILES} ${CFILES}
	@echo "Making cvstatic data file"
	@echo ${ASFILES} ${CFILES} > ${Static}
	@find '../lib' -name '*.[chs]' -print >> ${Static}
	@echo "-I../include" >> ${Static}
	@echo "-I../../../include" >> ${Static}

headers: ${_FORCE}
	install -f /usr/include/make 4DXXdefs
	ln -s IPXXprom/RX000defs ../RX000defs
