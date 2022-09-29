#!smake
#
# Makefile for gifts
#
#ident "$Revision: 1.13 $"

DEPTH=..
MAKEDEFS= ${DEPTH}/Makedefs
#if ${IRIX} == "SunOS"
MAKEDEFS= ${DEPTH}/Makedefs.sun
#endif
include ${MAKEDEFS}

GIDB= -idb ${GIFTSIDB}
GOWN= -u guest -g guest
GPATH= /usr/people/4Dgifts/examples/netvis
#if ${IRIX} == "SunOS"
GPATH=	${SUNPATH}/gifts
#endif
SCRIPTS=cpapack loadncp
TEXTS= README tospread
TUTOR= tutorial
TIDB= -idb ${TUTORIALIDB}
TOWN= -u root -g sys -m 444

MKFILE=	Makefile.gift

include ${MAKERULES}

default:

install:
	@if [ "${IRIX}" = "IRIS-4D operating system" ]; then \
		${INSTALL} ${GIDB} ${GOWN} -m 0755 -dir ${NV_INSTALL_EXAMPLES}; \
		${INSTALL} ${GIDB} ${GOWN} -m 0644 -F ${NV_INSTALL_EXAMPLES} \
			-src ${MKFILE} Makefile; \
		${INSTALL} -lns ${NV_INSTALL_EXAMPLES}/Makefile \
			-idb ${DISPLAYLINKSIDB} \
			-F ${NV_TARGET_EXAMPLES} Makefile; \
		${INSTALL} ${GIDB} ${GOWN} -m 0644 \
			-F ${NV_INSTALL_EXAMPLES} ${TEXTS} ${SCRIPTS}; \
		for f in ${TEXTS} ${SCRIPTS}; do \
			${INSTALL} -lns ${NV_INSTALL_EXAMPLES}/$$f \
				-idb ${DISPLAYLINKSIDB} \
				-F ${NV_TARGET_EXAMPLES} $$f; \
		done; \
		${INSTALL} ${TIDB} ${TOWN} -F ${NV_INSTALL_BIN}	${TUTOR}; \
		${INSTALL} -lns ${NV_INSTALL_BIN}/${TUTOR} \
			-idb ${DISPLAYLINKSIDB} \
			-F ${NV_TARGET_EXAMPLES} ${TUTOR}; \
	else \
		${INSTALL} -d ${SUNOWN} -m 0755 ${GPATH}; \
		${INSTALL} ${SUNOWN} -m 0644 ${TEXTS} ${GPATH}; \
		${INSTALL} ${SUNOWN} -m 0755 ${SCRIPTS} ${GPATH}; \
	fi
