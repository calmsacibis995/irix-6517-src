#!smake
#
#	Makefile for SNMP agent
#
#	$Revision: 1.18 $
#

SNMPDEPTH=..
include ../../Makedefs
include ${SNMPDEPTH}/Makedefs

C++FILES= agent.c++ \
	atsa.c++ \
	icmpsa.c++ \
	ifsa.c++ \
	ipsa.c++ \
	knlist.c++ \
	main.c++ \
	remotesubagent.c++ \
	rmonsa.c++ \
	tcpsa.c++ \
	udpsa.c++
LC++DEFS = -DMIB2
TARGETS= snmpd snmpd_64
DINFLAGS= -F /usr/etc
CIDB= -idb "config(noupdate)"
CINFLAGS= -F /etc ${CIDB} -u root -g sys -m 0600
SUBDIRS=snmpd_64
LLDLIBS = -lnetman -lsnmp -lsnmpagent -lelf -lm

default: ${TARGETS}

include ${SNMPDEPTH}/Makerules

snmpd:	${OBJECTS}
	${C++F} ${OBJECTS} ${LDFLAGS} -o $@

snmpd_64: ${LIBRARY} ${SNMPLIBRARY}
	@tlink -r -x '^.*\.[oai]$$' -x '^.*\.so$$' -x 'versiondefs' -x 'Makedepend' \
		-x 'snmpd' -x 'snmpd_64' . snmpd_64
	@ln -sf ../Makefile_64 snmpd_64/Makefile
	cd snmpd_64; smake

install exports: default
	${INSTALL} ${DINFLAGS} -idb "mach(MODE=32bit)" snmpd
	${INSTALL} ${CINFLAGS} snmpd.auth snmpd.trap.conf snmpd.remote.conf \
		snmpd.data
	${INSTALL} -F /etc/config ${CIDB} -m 0644 -src snmp.config snmpd
	$(SUBDIRS_MAKERULE)
