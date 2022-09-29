#!smake

include ${ROOT}/usr/include/make/commondefs

CFILES= efsstat.c prex.c dilist.c getex.c efsdup.c efsbno.c efsdir.c prino.c\
	efsx.c efscopy.c efsinfo.c

TARGETS= ${CFILES:.c=}

LCINCS= -I ../nlib
LLDLIBS= ../nlib/libefs.a

all: ${TARGETS}

include ${COMMONRULES}

${TARGETS}: ../nlib/libefs.a

install: all
	${INSTALL} -F /usr/efs ${TARGETS}
