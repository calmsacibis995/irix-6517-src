#!smake
#
#	Makefile for SNMP agent
#
#	$Revision: 1.3 $
#

include ${ROOT}/usr/include/make/commondefs

C++FILES= agent.c++ \
	atsa.c++ \
	icmpsa.c++ \
	ifsa.c++ \
	ipsa.c++ \
	knlist.c++ \
	main.c++ \
	remotesubagent.c++ \
	tcpsa.c++ \
	udpsa.c++

#	rmonsa.c++

LC++DEFS = -DMIB2 -DMASTER
TARGETS= snmpd snmpd_64
DINFLAGS= -F /usr/etc
CIDB= -idb "config(noupdate)"
CINFLAGS= -F /etc ${CIDB} -u root -g sys -m 0600
SUBDIRS=snmpd_64
LLDLIBS = -lsnmp -lsnmpagent -lelf
LC++INCS= -I../include -I../libagent/src -g

default: ${TARGETS}

snmpd:	${OBJECTS}
	${C++F} ${OBJECTS} ${LDFLAGS} -o $@
	@${SIZE} -B $@

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
	${INSTALL} -f /etc/config ${CIDB} -m 0644 snmpd.options
	$(SUBDIRS_MAKERULE)

#new SGI base MIB files
	${INSTALL} -u root -g sys -m 0444 -F /usr/NetVis/lib/mibs sgi-base-smi.mib sgi-products-smi.mib
	${INSTALL} -lns /usr/NetVis/lib/mibs/sgi-base-smi.mib -F /usr/lib/netman/mibs sgi-base-smi.mib
	${INSTALL} -lns /usr/NetVis/lib/mibs/sgi-products-smi.mib -F /usr/lib/netman/mibs sgi-products-smi.mib

include ${COMMONRULES}
