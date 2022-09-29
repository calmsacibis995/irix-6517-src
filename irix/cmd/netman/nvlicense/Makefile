#!smake
#Tag 256
#
#	Makefile for nvlicense
#
#	$Revision: 1.4 $
#
DEPTH=..
include ${DEPTH}/Makedefs

C++FILES= nvlicense.c++
NC++OPTS= +a1 +w +p -float -Wf,-XNl8192
NLDLIBS= -l${LIBNETVIS} -l${LIBNETVISUI} -lhelpmsg -ltu -lX11 -lm
#NLDLIBS= -l${LIBNETVIS} -l${LIBNETVISUI} -lhelpmsg -ltu -lX11 -lgl -lm

TARGETS= nvlicense
HELPCARDS= NVLicense.general.help

default: ${TARGETS}

include ${COMMONRULES}

nvlicense: ${OBJECTS}
	${C++F} ${OBJECTS} ${LDFLAGS} -o $@
	tag 0x010 $@
	@${SIZE} -B $@

install: default
	${INSTALL} -F ${NV_INSTALL_BIN} -idb ${DISPLAYIDB} nvlicense
	${INSTALL} -lns ${NV_INSTALL_BIN}/nvlicense -idb \
		${DISPLAYLINKSIDB} -F ${NV_TARGET_BIN} nvlicense
	${INSTALL} -F ${NV_INSTALL_HELP} -idb ${HELPIDB} ${HELPCARDS}
	for f in ${HELPCARDS}; do \
		${INSTALL} -lns ${NV_INSTALL_HELP}/$$f -idb \
			${DISPLAYLINKSIDB} -F ${NV_TARGET_HELPCARDS} $$f; \
	done
