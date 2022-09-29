#
# Makefile for cachefs tests
#
include	${ROOT}/usr/include/make/commondefs

CFILES= statfs.c corrupt.c getdents.c getdents64.c randomio.c statvfs.c \
			creat_test.c mmap.c truncate.c open_close.c

TARGETS= ${CFILES:.c=}

DIRS=	mmcp

LLDLIBS=

default: ${TARGETS}
	@for d in ${DIRS}; do \
		${ECHO} "====\tcd $$d; ${MAKE}"; \
		cd $$d; ${MAKE}; cd ..; \
	done

COMMONPREF=sun
include ${COMMONRULES}

${DIRS}: ${_FORCE}
	@${ECHO} "====\t$@"; cd $@; ${MAKE}

install: default
	${INSTALL} -F /usr/local/bin statfs
	${INSTALL} -F /usr/local/bin corrupt
	${INSTALL} -F /usr/local/bin getdents
	${INSTALL} -F /usr/local/bin getdents64
	${INSTALL} -F /usr/local/bin randomio
	${INSTALL} -F /usr/local/bin statvfs
	${INSTALL} -F /usr/local/bin creat_test
	${INSTALL} -F /usr/local/bin mmap
	${INSTALL} -F /usr/local/bin truncate
	${INSTALL} -F /usr/local/bin open_close
