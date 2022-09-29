#
# Makefile for the netdisk subsystem, which supports Sun diskless clients.
#
include ${ROOT}/usr/include/make/commondefs

CVERSION=-cckr

LCINCS= -I${INCLDIR}/sun -I${INCLDIR}/bsd
LLDLIBS= -lbsd

SCRIPTS= INSTALL convert_to_hex create_root extracting fix_bootparams \
	 setup_client setup_exec verify_tapevol_arch

HFILES= mktp.h toc.h xdr_custom.h
PGMFILES= opt_software.c xdrtoc.c
LIBFILES= toc.c xdr_custom.c xdrio.c
CFILES= ${PGMFILES} ${LIBFILES}

LIBOBJS= ${LIBFILES:.c=.o}
TARGETS= ${PGMFILES:.c=}

default: ${TARGETS}

include ${COMMONRULES}

${TARGETS}: $$@.o ${LIBOBJS}
	${CCF} $@.o ${LIBOBJS} ${LDFLAGS} -o $@

install: default
	${INSTALL} -F /usr/etc/netdisk ${SCRIPTS} ${TARGETS}
