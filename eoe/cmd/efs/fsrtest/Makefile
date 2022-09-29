#!smake
#
include ${ROOT}/usr/include/make/commondefs

CVERSION=-cckr

CFILES= rsf.c exop.c fop.c indir.c testfsctl.c random.c movex.c mkdup.c hog.c
TARGETS= rsf exop fop indir testfsctl random movex hog 

OPTIMIZER=
LCOPTS= -g

LLDOPTS = -L${KTST} -L../lib

LCINCS= -I ../fsr -I ../nlib
LLDLIBS= iprintf.o ../fsr/lib.o ../nlib/libefs.a -lmalloc


all: iprintf.o ${TARGETS}

include ${COMMONRULES}

#libc is all that's needed for these guys
fop:	fop.c
	${CCF} fop.c ${LDOPTS} -o $@
random:	random.c
	${CCF} random.c ${LDOPTS} -o $@
hog:	hog.c
	${CCF} hog.c ${LDOPTS} -o $@
