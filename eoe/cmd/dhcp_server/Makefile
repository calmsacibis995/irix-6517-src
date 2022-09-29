#!smake
#
# Makefile for dhcp bootp
#
#ident "$Revision: 1.32 $"

include	${ROOT}/usr/include/make/commondefs
sinclude ../Makedefs

DEPTH=..

TARGETS= dhcp_bootp dhcp_relay 
CHK_FILES=proclaim_server.chkconfig proclaim_relayagent.chkconfig
CONFIG_FILES=config.Default 
OPT_FILES=dhcp_bootp.options dhcp_relay.options bootp-rfc1533.options
CONFIG_OPT_FILES=vendor_options dhcp_option_types
GUI_FILES=ProclaimServerMgr ProclaimRelayMgr 
APPDEF_FILES=ProclaimServer ProclaimRelay
CONV_FILES=ethIPToDbm dbmToEthIP

NOUPDATE=-idb "${TCP} config(noupdate)"
IDBTAG=-idb "${TCP}"
LCOPTS= -fullwarn 
LLDLIBS=-lgen
# define LCDEFS=-DEDHCP for the enhanced server product


CFILES = dhcp_bootp.c dhcpfuncs.c nha0.c loc0.c common.c configure.c\
	 dhcp_relay.c relay_bootp.c relay_funcs.c sr_common.c dbm.c\
	 dbmToEthIP.c ethIPToDbm.c ping.c bootptab.c dhcptab.c dhcp_types.c \
	 dhcp_optvals.c
SOBJECTS = dhcp_bootp.o dhcpfuncs.o nha0.o loc0.o common.o \
	   configure.o  sr_common.o dbm.o ping.o bootptab.o dhcptab.o \
	   dhcp_types.o dhcp_optvals.o
ROBJECTS = dhcp_relay.o relay_bootp.o relay_funcs.o common.o sr_common.o
XOBJECTS = dbmToEthIP.o  
YOBJECTS = ethIPToDbm.o dbm.o 
LDAP_OBJECTS = ldap_init.o ldap_ops.o ldap_cach.o ldap_callback.o \
	 ldap_timeout.o dhcp_ldap_den.o 

DDNS_OBJECTS = dhcp_dnsupd.o

SLLDLIBS= -lelf
RLLDLIBS=

default:${TARGETS} dbmToEthIP ethIPToDbm

include ${COMMONRULES}

install:${CHK_FILES} ${TARGETS} ${CONFIG_FILES} ${OPT_FILES} ${CONV_FILES}
	${INSTALL} -m 644 -F /etc/config ${NOUPDATE} \
		   -src proclaim_server.chkconfig proclaim_server
	${INSTALL} -m 644 -F /etc/config ${NOUPDATE} \
		   -src proclaim_relayagent.chkconfig proclaim_relayagent
	${INSTALL} -F /usr/etc ${IDBTAG} ${TARGETS} ethIPToDbm
	${INSTALL} -m 644 -F /etc/config ${NOUPDATE} ${OPT_FILES}
	${INSTALL} -m 644 -F /var/dhcp/config ${IDBTAG} ${CONFIG_FILES}
	${INSTALL} -m 644 -F /var/dhcp/config ${NOUPDATE} \
		   dhcpmtab $(CONFIG_OPT_FILES)
	${INSTALL} -m 444 -F /usr/lib/X11/app-defaults ${IDBTAG} ${APPDEF_FILES}
	${INSTALL} -m 755 -F /usr/sbin ${IDBTAG} ${GUI_FILES}
	${INSTALL} -m 644 -F /var/dhcp/config ${NOUPDATE} dhcp_relay.servers
	${INSTALL} -F /usr/sbin ${IDBTAG} dbmToEthIP
	${INSTALL} -F /usr/sbin ${IDBTAG} dhcpdb

dhcp_relay: $(ROBJECTS)
	$(CCF) $(ROBJECTS) $(LDFLAGS) ${RLLDLIBS} -o $@

dhcp_bootp: $(SOBJECTS)
	$(CCF) $(SOBJECTS) $(LDFLAGS) ${SLLDLIBS} -o $@

dbmToEthIP: $(XOBJECTS)
	$(CCF) $(XOBJECTS) $(LDFLAGS) -o $@

ethIPToDbm: $(YOBJECTS) 
	$(CCF) $(YOBJECTS) $(LDFLAGS) -o $@









