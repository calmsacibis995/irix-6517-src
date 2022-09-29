include ${ROOT}/usr/include/make/commondefs

CFILES=main.c mkdisc_900e.c
TARGETS=mkdisc_900e
LLDLIBS=-lds

default: ${TARGETS}

# no install, currently.

include ${COMMONRULES}

mkdisc_900e : ${OBJECTS}
	${CC} -o $@ ${OBJECTS} ${LDFLAGS}
