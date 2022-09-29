#!smake
#
# Makefile for libnetman/protocols, libnetman's protocol modules.
#
LIBDEPTH=..
include ${LIBDEPTH}/Makedefs

NCINCS = -I../../../include -I../llc -I../pidlhacks
NCINCS = -I${ROOT}/usr/include/netman -I../llc -I../pidlhacks
LCINCS = -I${ROOT}/usr/include/netman -I../llc -I../pidlhacks

#
# Alphabetized and grouped source list.
#
# llc.c removed
CFILES= arp.c arp_ip.c \
	bootp.c \
	crayio.c \
	decnet.c decnet_addr.c decnet_host.c decnet_nsp.c \
	dns.c \
	dnhost.c \
	ds_arphdr.c ds_arpipseg.c ds_ip.c ds_ip_opt.c ds_tcphdr.c ds_udphdr.c \
	ether.c ether_host.c ether_type.c ether_vendor.c \
	fddi.c fddi_host.c \
	mac.c smt.c \
	hippi.c atm.c \
	ip.c ip_addr.c ip_checksum.c ip_host.c ip_icmp.c ip_igmp.c \
	ip_indexops.c ip_protoname.c ip_service.c ip_setport.c ip_tcp.c \
	ip_timestamp.c ip_udp.c \
	hello.c \
	loop.c \
	nfs.c \
	rip.c \
	snoop.c \
	sunrpc.c sunrpc_pmap.c sunrpc_port.c sunrpc_prog.c \
	tftp.c \
	xdr_data.c \
	xtp.c xtp_cksum.c

TARGETS= Marker

default: ${TARGETS}

include ${LIBDEPTH}/Makerules
