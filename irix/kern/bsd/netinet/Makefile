#!smake
#
# Makefile for Internet Protocol Suite (inet) code.
#
# $Revision: 3.45 $

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/bsd/netinet
.PATH:$(KPATH)

KCINCS=-I$(TOP)/bsd
KCOPTS=-signed -common

LINTNAME=bsdnetinet

HDRFILES=icmp_var.h if_ether.h igmp.h igmp_var.h in_pcb.h in_systm.h \
	in_var.h ip.h ip_icmp.h ip_mroute.h ip_var.h tcp.h \
	tcp_debug.h st.h \
	tcp_fsm.h tcp_seq.h tcp_timer.h tcp_var.h tcpip.h udp.h udp_var.h
#if defined(INET6_BUILD)
HDRFILES+= icmp6_var.h  in6_var.h    ip6_icmp.h   ip6_var.h    udp6_var.h \
	if_ether6.h  ip6.h        ip6_opts.h   tcp6_var.h ipsec.h if_ndp6.h \
	icmp6.h ip6_mroute.h
#endif


BSD_NAME=bsd.a
BSD_CSRCS=igmp.c if_ether.c in.c in_pcb.c in_proto.c ip_icmp.c ip_input.c \
	ip_output.c raw_ip.c tcp_debug.c tcp_input.c tcp_output.c \
	tcp_subr.c tcp_timer.c tcp_usrreq.c udp_usrreq.c rsvp_input.c
#if defined(INET6_BUILD)
BSD_CSRCS+= ip6_input.c in6_proto.c ip6_icmp.c ip6_output.c in6.c \
	if_ndp6.c ipsec.c udp6_usrreq.c raw_ip6.c in6_pcb.c \
	raw_ip6.c tcp6_input.c tcp6_output.c tcp6_usrreq.c \
	tcp6_subr.c ip6_mroute.c if_sit6.c if_cti6.c
#endif
BSD_CSRCS+= st_stubs.c

BSD_OBJS=$(BSD_CSRCS:.c=.o)
$(BSD_NAME):$(BSD_NAME)($(BSD_OBJS)) MAKELIB

FILTER_NAME=ipfilter.o
FILTER_CSRCS=ipfilter.c

MROUTE_NAME=ip_mroute.o
MROUTE_CSRCS=ip_mroute.c

TARGETS=$(MROUTE_NAME) $(BSD_NAME) $(FILTER_NAME)

LINTNAME=bsdnetinet
CFILES=$(MROUTE_CSRCS) $(BSD_CSRCS) $(FILTER_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}
#
# BSD_NAME gets installed at top level
#
$(KCOMMONPREF)install: $(KCOMMONPREF)default
	$(KINSTALL) -idb "std.sw.ipgateway" $(FILTER_NAME) $(MROUTE_NAME) $(IPSO_NAME)
	$(KDEVINSTALL) $(FILTER_NAME) $(MROUTE_NAME) $(IPSO_NAME)

headers!
	${INSTALL} -m 444 -F /usr/include/netinet $(HDRFILES)
	# ipfilter.h and in.h are needed by the ipfilter master.d file.
	${INSTALL} -m 444 -F /usr/include/netinet ipfilter.h in.h
