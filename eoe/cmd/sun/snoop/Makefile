#!smake
#
# $Header: /proj/irix6.5.7m/isms/eoe/cmd/sun/snoop/RCS/Makefile,v 1.4 1999/03/18 20:57:25 sherwood Exp $

include	${ROOT}/usr/include/make/commondefs

LCINCS += -I.
#LLDLIBS = /usr/lib/libaudio.a

HFILES= snoop.h
CFILES = snoop.o snoop_arp.o snoop_bparam.o \
	snoop_capture.o snoop_dhcp.o snoop_display.o snoop_ether.o \
	snoop_filter.o snoop_icmp.o snoop_ip.o snoop_ipaddr.o \
	snoop_mount.o snoop_nfs.o snoop_nfs3.o snoop_nfs_acl.o \
	snoop_nis.o snoop_nisplus.o snoop_nlm.o snoop_ntp.o \
	snoop_pmap.o snoop_rip.o snoop_rpc.o snoop_rpcprint.o \
	snoop_rport.o snoop_rquota.o snoop_rstat.o snoop_solarnet.o \
	snoop_tcp.o snoop_tftp.o snoop_udp.o

LDIRT=nfs_prot.h

TARGETS= snoop

default: ${TARGETS}

include	${COMMONRULES}

$(OBJECTS): nfs_prot.h

snoop: ${OBJECTS}
	${CCF} ${OBJECTS} ${LCDEFS} ${LDFLAGS} -o $@

nfs_prot.h: nfs_prot.x
	${TOOLROOTSAFE} $(TOOLROOT)/usr/bin/rpcgen -C "${CCF} -E" -h -o $@ nfs_prot.x

install: default
	${INSTALL} -F /usr/bin snoop
