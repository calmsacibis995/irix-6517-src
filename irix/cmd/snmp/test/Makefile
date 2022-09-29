#!smake
#
# Makefile for SNMP test programs
#
#ident "$Revision: 1.2 $"

include ${ROOT}/usr/include/make/commondefs

C++FILES=lentest.c++ asn1test.c++ pdutest.c++ msgtest.c++ snmpwalk.c++
LC++INCS = -I../include
LLDLIBS = -lsnmp -lsnmpagent
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

include ${COMMONRULES}

install: default
	${INSTALL} ${INFLAGS} msgtest snmpwalk
