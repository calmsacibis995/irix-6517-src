#!smake
#
# Makefile for SNMP test programs
#
#ident "$Revision: 1.5 $"

SNMPDEPTH=..
include ${SNMPDEPTH}/Makedefs

NC++INCS = -I../include

C++FILES=lentest.c++ asn1test.c++ pdutest.c++ msgtest.c++ snmpwalk.c++
TARGETS=lentest asn1test pdutest msgtest snmpwalk
INIDB= -idb netman.sw.netman
INFLAGS= -F /usr/sbin ${INIDB}

default: ${TARGETS} ${SNMPLIBRARY}

lentest:$@.o ${SNMPLIBRARY}
	${C++F} $@.o ${SNMPLIBRARY} ${LDFLAGS} -o $@
	@${SIZE} -B $@

asn1test:$@.o ${SNMPLIBRARY}
	${C++F} $@.o ${SNMPLIBRARY} ${LDFLAGS} -o $@
	@${SIZE} -B $@

pdutest:$@.o ${SNMPLIBRARY}
	${C++F} $@.o ${SNMPLIBRARY} ${LDFLAGS} -o $@
	@${SIZE} -B $@

msgtest:$@.o ${SNMPLIBRARY}
	${C++F} $@.o ${SNMPLIBRARY} ${LDFLAGS} -o $@
	@${SIZE} -B $@

snmpwalk:$@.o ${SNMPLIBRARY}
	${C++F} $@.o ${SNMPLIBRARY} ${LIBNETVIS} ${LDFLAGS} -o $@
	@${SIZE} -B $@

include ${SNMPDEPTH}/Makerules

install: default
	${INSTALL} ${INFLAGS} msgtest snmpwalk
