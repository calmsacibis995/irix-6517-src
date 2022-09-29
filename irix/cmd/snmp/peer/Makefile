#!smake
#
#	Makefile for SNMP agent
#
#	$Revision: 1.3 $
#

include ${ROOT}/usr/include/make/commondefs

TARGETS=

default: ${TARGETS}

install exports: default
	${INSTALL} -u root -g sys -m 0644 -f /etc/config \
	    -idb 'config(noupdate)' peer_encaps.options peer_snmpd.options
	${INSTALL} -u root -g sys -m 0644 -f /etc -idb 'config(noupdate)' \
	    peer_encaps_config peer_snmpd_config
	${INSTALL} -u root -g sys -m 0755 -f /usr/etc peer_encaps peer_snmpd

include ${COMMONRULES}
