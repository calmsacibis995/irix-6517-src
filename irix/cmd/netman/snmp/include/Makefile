DEPTH= ../..
include ${DEPTH}/Makedefs

IFILES= asn1.h message.h oid.h packet.h pdu.h query.h snmp.h

default:

headers install: default
	${INSTALL} -m 444 -idb SNMPINC -F /usr/include/snmp ${IFILES}

exports:

include ${ROOT}/usr/include/make/commonrules
