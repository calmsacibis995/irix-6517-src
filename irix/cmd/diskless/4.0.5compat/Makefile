#
# Makefile for cmd/diskless/4.0.5compat, diskless compatibility.
#
include ${ROOT}/usr/include/make/commondefs
MKDEPFILE= Makedepend

LCINCS=
LLDLIBS=

SHFILES= clinst.sh

TARGETS= inst4.0.5 ${SHFILES:.sh=}
DLOPTS= -F /var/boot/4.X -idb DSKLESS_SRV_405 -u root -g sys

default: ${TARGETS}

include ${COMMONRULES}

install: default
	${INSTALL} ${DLOPTS} -m 444 clinst.dat clinst.dat
	${INSTALL} ${DLOPTS} clinst
	${INSTALL} ${DLOPTS} inst4.0.5

inst4.0.5: inst4.0.5.uunc
	rm -f $@; uudecode inst4.0.5.uunc; chmod u+x $@
