#
# Makefile for unifdef(1).
#
include ${ROOT}/usr/include/make/commondefs

YFLAGS= -d

HFILES= unifdef.h unif_scan.h
CFILES= unif_expr.c unif_main.c unif_proc.c unif_stab.c
LFILES= unif_scan.l
YFILES= unif_gram.y

LDIRT= unif_gram.c [xy].tab.h
CTAGFINDARGS= ! -name 'unif_gram.[cy]' ! -name lex.yy.c
TARGETS= unifdef

default: ${TARGETS}

include ${COMMONRULES}

install: default
	${INSTALL} -F /usr/sbin -idb cc.sw.util unifdef

unifdef: ${OBJECTS}
	${CCF} ${OBJECTS} ${LDFLAGS} -o $@

unif_proc.o unif_scan.o: unif_scan.h

depend incdepend unif_scan.h: x.tab.h

x.tab.h: y.tab.h
	@cmp -s $? $@ || cp $? $@

y.tab.h: unif_gram.o
