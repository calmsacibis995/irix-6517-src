#!smake
#
#	Makefile for SNMP agent containing the HP-UX MIB
#
#	$Revision: 1.4 $
#
#

DEPTH=../..
include ${DEPTH}/Makedefs

C++FILES= agent.c++ main.c++ remotesubagent.c++ \
	  hpsystemsa.c++ hpfssa.c++ hptrapdestsa.c++ hpprocsa.c++ \
	  hpsnmpdconfsa.c++ hpicmpsa.c++ hpauthfailsa.c++

#
# Note that all agents will include the SNMP, SYSTEM and SGI MIB groups
# System compilation flags are: MASTER or REMOTE
# MIB compilation flags are   : MIB2, RMON
#
#
# The _KMEMUSER option is to pick up process status flags and process flags
# in sys/proc.h
#
LC++OPTS= -DHPUX_MIB -I../libagent -I${ROOT}${HEADERSPATH} -I${ROOT}/usr/include/pcp -D_KMEMUSER
TARGETS= hpsnmpd
LC++INCS= -I../include

PMGENMAP= pmgenmap
NLDLIBS= -lmld -lm -lmalloc 
LLDLIBS= -lpcp_lite  -lsun  ../lib/libsnmp.a ../libagent/libsnmpagent.a
LLDOPTS= -L${ROOT}/usr/lib/pcp -rpath /usr/lib/pcp
LDIRT= hpsysmap.h hpfsmap.h hpprocmap.h

default: ${TARGETS} ${OBJECTS} ${LIBRARY} ${SNMPLIBRARY} ${AGENTLIBRARY}

include ${ROOT}/usr/include/make/commondefs
include ${ROOT}/usr/include/make/commonrules

hpsnmpd:	${OBJECTS} ${LIBRARY} ${SNMPLIBRARY} ${AGENTLIBRARY}
	${C++F} ${OBJECTS} ${LIBRARY} ${SNMPLIBRARY} ${AGENTLIBRARY} \
		${LDFLAGS} -o $@
	@${SIZE} -B $@

hpsystemsa.o:	hpsystemsa.c++ hpsystemsa.h hpsysmap.h
hpfssa.o:	hpfssa.c++ hpfssa.h hpfsmap.h
hptrapdestsa.o:	hptrapdestsa.c++ hptrapdestsa.h
hpprocsa.o:	hpprocsa.c++ hpprocsa.h hpprocmap.h
hpsnmpdconfsa.o:	hpsnmpdconfsa.c++ hpsnmpdconfsa.h

hpsysmap.h:	hpsysmap.spec
	${PMGENMAP} hpsysmap.spec > hpsysmap.h

hpfsmap.h:	hpfsmap.spec
	${PMGENMAP} hpfsmap.spec > hpfsmap.h

hpprocmap.h:	hpprocmap.spec
	${PMGENMAP} hpprocmap.spec > hpprocmap.h

install exports: default
	${INSTALL} -F /usr/etc -idb ${SNMP} hpsnmpd
	${INSTALL} -F /etc -idb "${SNMP} config(update)" \
	    -u root -g sys -m 0600 snmpd.trap.conf snmpd.remote.conf
	${INSTALL} -F /usr/IRIXpro/lib/mibs -idb ${SNMP} -m 0644 hp-ux_sgi.mib
	${INSTALL} -lns /usr/IRIXpro/lib/mibs/hp-ux_sgi.mib -idb ${SNMP} \
	    -F /usr/lib/netvis/mibs hp-ux_sgi.mib
	${INSTALL} -F /etc/config -idb "${SNMP} config(update)" \
	    -u root -g sys -m 0644 hpsnmpd.options
	${INSTALL} -F /etc/init.d -idb '${SNMP} config(update) preop("killall snmpd hpsnmpd; true") exitop("if `chkconfig snmpd`; then /usr/etc/snmpd `cat /etc/config/snmpd.options 2>/dev/null`; /usr/etc/hpsnmpd `cat /etc/config/hpsnmpd.options 2>/dev/null`; fi")' -m 0755 -src snmp.init snmp
	${INSTALL} -F /etc/rc2.d -lns ../init.d/snmp -idb ${SNMP} S32snmp
