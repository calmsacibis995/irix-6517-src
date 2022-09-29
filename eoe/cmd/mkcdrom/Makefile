include ${ROOT}/usr/include/make/commondefs

CFILES=main.c # common objects
CFILES_521=mkdisc_521.c
CFILES_900e=mkdisc_900e.c

OBJECTS_521=${CFILES_521:.c=.o}
OBJECTS_900e=${CFILES_900e:.c=.o}

TARGETS=mkdisc_521 mkdisc_900e burncd make_efs_image
LLDLIBS=-lds

default: ${TARGETS}

install: default
	${INSTALL} -m 755 -F /usr/etc mkdisc_521

include ${COMMONRULES}

mkdisc_521: ${OBJECTS} ${OBJECTS_521}
	${CC} -s -o $@ ${OBJECTS} ${OBJECTS_521} ${LDFLAGS}

mkdisc_900e: ${OBJECTS} ${OBJECTS_900e}
	${CC} -s -o $@ ${OBJECTS} ${OBJECTS_900e} ${LDFLAGS}
