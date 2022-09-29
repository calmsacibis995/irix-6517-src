#!smake
#
# Makefile for pidl, the Protocol Interpreter Description Languange compiler.
#
DEPTH=..

MAKEDEFS= ${DEPTH}/Makedefs
#if ${IRIX} == "SunOS"
MAKEDEFS= ${DEPTH}/Makedefs.sun
#endif

include ${MAKEDEFS}
include ${ROOT}/usr/include/make/commondefs

CFILES= pidl.c scope.c type.c \
	eval.c treenode.c analyze.c basicblock.c bitset.c \
	generate.c cg_endian.c

HFILES= pidl.h scope.h type.h \
	treenode.h analyze.h basicblock.h bitset.h \
	generate.h

LFILES= scan.l
YFILES= parse.y

# Need extensions for scan.l setup of yyin and yyout.
NCDEFS= -D__EXTENSIONS__
YFLAGS= -d
CTAGFINDARGS= ! -name 'parse.[cy]' ! -name lex.yy.c
LINTFLAGS= -woff 2,86,102,302

# Work around Sun optimizer bug
#if ${IRIX} == "SunOS"
OPTIMIZER=-O1
#endif

LDIRT= x.tab.h parse.c
TARGETS= pidl

LCINCS=-I../include

default: ${TARGETS}

include ${MAKERULES}

pidl: ${OBJECTS} ${LIBDIR}/Marker
	${CCF} ${OBJECTS} ${LIBRARY} ${LDFLAGS} -o $@
	@${SIZE} -B $@;

${LIBDIR}/Marker:
	cd ${LIBDIR}; ${MAKE} Marker

install exports: default
	@if [ "${IRIX}" = "IRIS-4D operating system" ]; then \
		${INSTALL} -F /usr/sbin -idb ${PIDLIDB} pidl; \
	fi

depend incdepend scan.o: x.tab.h

x.tab.h: y.tab.h
	@cmp -s $? $@ || cp $? $@

y.tab.h: parse.o

print: .printstamp
.printstamp: ${SOURCES}
	enscript $?
	@touch $@
