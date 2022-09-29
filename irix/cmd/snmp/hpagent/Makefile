#!smake
#
#	Makefile for SNMP agent containing the HP-UX MIB
#
#	$Revision: 1.1 $
#
#

include ${ROOT}/usr/include/make/commondefs

C++FILES= agent.c++ main.c++ remotesubagent.c++ \
	  hpsystemsa.c++ hpfssa.c++ hptrapdestsa.c++ hpprocsa.c++ \
	  hpsnmpdconfsa.c++ hpicmpsa.c++ hpauthfailsa.c++
CFILES= pmns.c

#
# Note that all agents will include the SNMP, SYSTEM and SGI MIB groups
# System compilation flags are: MASTER or REMOTE
# MIB compilation flags are   : MIB2, RMON
#
#
# The _KMEMUSER option is to pick up process status flags and process flags
# in sys/proc.h
#
# LC++OPTS= -DHPUX_MIB -I${ROOT}/usr/include/netman -I${ROOT}/usr/include/pcp -I${ROOT}/usr/include/snmp -D_KMEMUSER
LC++OPTS= -DHPUX_MIB -I${ROOT}/usr/include/pcp -I${ROOT}/usr/include/snmp \
    -D_KMEMUSER
LCOPTS= -I${ROOT}/usr/include/pcp -D_KMEMUSER
TARGETS= hpsnmpd hpsnmpd_64
LC++INCS= -I../include -I../libagent/src
subdirs= hpsnmpd_64

PMGENMAP= ${TOOLROOT}/usr/sbin/pmgenmap
LLDLIBS= -lpcp_lite -lsnmp -lsnmpagent
LLDOPTS= -L${ROOT}/usr/lib/pcp -rpath /usr/lib/pcp
LDIRT= hpsysmap.h hpfsmap.h hpprocmap.h

default: ${TARGETS} ${OBJECTS} ${LIBRARY} ${SNMPLIBRARY} ${AGENTLIBRARY}

include ${ROOT}/usr/include/make/commonrules

hpsnmpd:	${OBJECTS} ${LIBRARY} ${SNMPLIBRARY} ${AGENTLIBRARY}
	${C++F} ${OBJECTS} ${LIBRARY} ${SNMPLIBRARY} ${AGENTLIBRARY} \
		${LDFLAGS} -o $@
	@${SIZE} -B $@

hpsnmpd_64: ${LIBRARY} ${SNMPLIBRARY}
	@tlink -r -x '^.*\.[oai]$$' -x '^.*\.so$$' -x 'versiondefs' -x 'Makedepend' -x 'hpsnmpd' -x 'hpsnmpd_64' . hpsnmpd_64
	@ln -sf ../Makefile_64 hpsnmpd_64/Makefile
	cd hpsnmpd_64; smake

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

install: default
	${INSTALL} -F /usr/etc -idb "${SNMP} mach(MODE=32bit)" hpsnmpd
	${INSTALL} -F /etc -idb "${SNMP} config(update)" \
	    -u root -g sys -m 0600 snmpd.trap.conf snmpd.remote.conf
	${INSTALL} -F /usr/provision/lib/mibs -idb ${SNMP} -m 0644 hp-ux_sgi.mib
	${INSTALL} -lns /usr/provision/lib/mibs/hp-ux_sgi.mib -idb ${SNMP} \
	    -F /usr/lib/netvis/mibs hp-ux_sgi.mib
	${INSTALL} -F /etc/config -idb "${SNMP} config(update)" \
	    -u root -g sys -m 0644 hpsnmpd.options
	${INSTALL} -F /etc/init.d -idb '${SNMP} config(update) preop("killall snmpd hpsnmpd; :") exitop("chroot $$rbase (if `chkconfig snmpd`; then /usr/etc/snmpd `cat /etc/config/snmpd.options`; /usr/etc/hpsnmpd `cat /etc/config/hpsnmpd.options`; fi; :) >/dev/null 2>&1")' -m 0755 -src snmp.init snmp
	${INSTALL} -F /etc/rc2.d -lns ../init.d/snmp -idb ${SNMP} S32snmp
	-cd hpsnmpd_64; smake install

exports: default
	${INSTALL} -F /usr/etc -idb ${SNMP} hpsnmpd
	-cd hpsnmpd_64; smake exports
