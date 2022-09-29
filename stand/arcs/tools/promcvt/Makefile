#!smake
# Makefile for promcvt
#

include ${ROOT}/usr/include/make/commondefs

MKDEPFILE= Makedepend

CVERSION= -xansi
LLDLIBS= -lelf

LCOPTS= -fullwarn -D_STANDALONE -DDOING_COMPRESS

CFILES= promcvt.c s3.c compress.c

TARGETS= promcvt

default: ${TARGETS}

include ${COMMONRULES}

compress.c:
	ln -s ../../IO4prom/segldr/compress.c .

lzw.h:
	ln -s ../../IO4prom/segldr/lzw.h .

promcvt: lzw.h compress.c ${OBJECTS}
	${CCF} ${OBJECTS} ${LDFLAGS} -o promcvt
