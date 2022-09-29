/*
 * Copyright (c) 1983,1988 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of California at Berkeley. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission. This software
 * is provided ``as is'' without express or implied warranty.
 */

#ifndef lint
static char sccsid[] = "@(#)inet.c	5.10 (Berkeley) 2/7/88 plus MULTICAST 1.0";
#endif /* not lint */

#include <stdio.h>
#include <bstring.h>
#define _KMEMUSER		/* get definition of "struct socket" */
#include <strings.h>

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/sysctl.h>

#include <sys/sysmp.h>

#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp_var.h>
#include <netinet/igmp_var.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_seq.h>
#define TCPSTATES
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_debug.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

#include <sys/tcpipstats.h>

#include <netdb.h>

#include <mls.h>
#include <sys/so_dac.h>
#include <pwd.h>
#include <sys/types.h>
#include <malloc.h>

#define MAX_UID_LINE 9
#define MAX_SOACL_LINE 256

#include <curses.h>
#include <stdarg.h>

#include "netstat.h"
#include "cdisplay.h"


struct printbuf {
	struct printbuf	*fwd;
	struct inpcb	*next;
	struct inpcb	inpcb;
	struct socket	sockb;
	struct tcpcb	tcpcb;
	mac_label	slabel;
	struct soacl	sacl;
};
struct	inpcb inpcb;
struct	tcpcb tcpcb;
struct	socket sockb;

int ipforwarding = -1;		/* let ipstats show state of flag */


extern int errno;

static void inetprint(struct in_addr*, u_short, char*);

#ifdef _SESMGR
extern mac_label plabel;
extern short havemac;
#endif

void
kread(int fd, void *buf, unsigned len)
{
	if (read(fd, buf, len) < 0) {
		perror("netstat: kread");
		exit(1);
	}
}

#ifdef _SESMGR
mac_label *
fetchlabel(mac_label *lblp, ns_off_t off)
{
	register int lblsize;
	register int hdrsize = (caddr_t)lblp->ml_list - (caddr_t)&lblp;

	klseek(kmem, off, 0);
	kread(kmem, lblp, hdrsize);
	lblsize = mac_size(lblp) - hdrsize;
	if (lblsize > 0)
		kread(kmem, (caddr_t)lblp + hdrsize, lblsize);
	return lblp;
}

char *
uidtostr(int uid)
{
    struct passwd *pwd;
    char *uidstring;

    uidstring = (char *)malloc(MAX_UID_LINE);
    if (pwd = getpwuid(uid))
	sprintf(uidstring, "%s", pwd->pw_name);
    else
	sprintf(uidstring, "%d", uid);
    return(uidstring);
}
void
souidpr(uid_t uid)
{
	register char *cp;

	cp = uidtostr(uid);
	printf("%-8.8s ", cp );
	if (cp)
		free(cp);
}

void
labelpr(mac_label *lblp)
{
	register char *cp;

	cp = mac_labeltostr(lblp, 1);
	printf("%-8.8s ", cp ? cp : "INVALID" );
	if (cp)
		free(cp);
}

char *
soacltostr(struct soacl *sp)
{
    char *buf;
    register int count;

    buf = (char *)malloc(MAX_SOACL_LINE);
    if (sp->so_uidcount == WILDACL)
	sprintf(buf, "wildacl");
    else {
	sprintf(buf, "%s", uidtostr(sp->so_uidlist[0]));
	for (count = 1; count < sp->so_uidcount; count++) {
	    sprintf(buf, "%s,%s", buf,
		    uidtostr(sp->so_uidlist[count]));
	}
    }
    return(buf);
}

void
soaclpr(struct soacl *soaclp)
{
	register char *cp;

	cp = soacltostr(soaclp);
	printf("%s ", cp);
	if (cp)
		free(cp);
}
#endif

/*
 * Print a summary of connections related to an Internet
 * protocol.  For TCP, also give state of connection.
 * Listening processes (aflag) are suppressed unless the
 * -a (all) flag is specified.
 */
void
protopr(ns_off_t off,
	char *name)
{
	struct inpcb cb;
	register struct inpcb *prev, *next;
	int istcp;
	static int first = 1;
	mac_label slabel;
	struct soacl sacl;
	int pass = 0;
	struct printbuf *phead = 0;
	struct printbuf **ptail = &phead;
	struct printbuf *pnext;
	struct in_pcbhead hcb;
	ns_off_t hoff;
	int h;
	int tsize;
	ns_off_t table;
	struct inpcb *inp;

	if (off == 0)
		return;

	istcp = strcmp(name, "tcp") == 0;
	klseek(kmem, off, 0);
	read(kmem, (char *)&cb, sizeof (struct inpcb));
	inpcb = cb;
	tsize = inpcb.inp_tablesz;
	table = (ns_off_t)inpcb.inp_table;
	for (h = 0; h < tsize; h++) {
		hoff = table + (h * sizeof(hcb));
		prev = (struct inpcb *)hoff;
		klseek(kmem, hoff, 0);
		read(kmem, (char *)&hcb, sizeof (struct in_pcbhead));
		inp = (struct inpcb *)&hcb;
		if (inp->inp_next == (struct inpcb *)hoff)
			continue;
		while (inp->inp_next != (struct inpcb *)hoff) {

			next = inp->inp_next;
			klseek(kmem, (ns_off_t)next, 0);
			read(kmem, (char *)&inpcb, sizeof (inpcb));
			inp = &inpcb;
			if (inpcb.inp_prev != prev) {
				printf("???\n"); 
				break;
			}
			if (!aflag &&
			  inet_lnaof(inpcb.inp_laddr) == INADDR_ANY) {
				prev = next;
				continue;
			}
			klseek(kmem, (ns_off_t)inpcb.inp_socket, 0);
			read(kmem, (char *)&sockb, sizeof (sockb));
			if (istcp) {
				klseek(kmem, (ns_off_t)inpcb.inp_ppcb, 0);
				read(kmem, (char *)&tcpcb, sizeof (tcpcb));
			}
#ifdef _SESMGR
			if (havemac) {
				(void)fetchlabel(&slabel,
						 (ns_off_t)sockb.so_label);
				if (!mac_dom(&plabel, &slabel)) {
					prev = next;
					continue;
				}
				klseek(kmem, (ns_off_t)sockb.so_acl, 0);
				kread(kmem, &sacl, sizeof(struct soacl));
			}
#endif
			if (first) {
				printf("Active Internet connections");
				if (aflag)
					printf(" (including servers)");
				putchar('\n');
#ifdef _SESMGR
				if (lflag) {
					/* add label, uid, & acl headers */
					printf("%-8.8s %-8.8s %-5.5s %-20.20s %-20.20s %-11.11s %-8.8s %s\n",
					       "Label", "SndLabel", "Proto",
					       "Local Address", "Foreign Address",
					       "(state)", "Souid", "Soacl");
				} else {
#endif
#if _MIPS_SZLONG == 64
					if (Aflag)
					    printf("%-16.16s ", "PCB");
#else
					if (Aflag)
					    printf("%-8.8s ", "PCB");
#endif
					printf(Aflag ?
					       "%-5.5s %-6.6s %-6.6s  %-18.18s %-18.18s %s" :
					       "%-5.5s %-6.6s %-6.6s  %-22.22s %-22.22s %s",
					       "Proto", "Recv-Q", "Send-Q",
					       "Local Address", "Foreign Address", "(state)");
#ifdef _SESMGR
				}
#endif
				if (qflag) {
					printf("       Q0    Q Limit");
				}
				putchar('\n');
				first = 0;
			}
			/* read everything into a linked list before printing
			 * anything, to avoid the Heisenburg effects of doing
			 * address-to-name resolution.  That creates more PCBs.
			 */
			pnext = (struct printbuf*)malloc(sizeof(struct printbuf));
			if (!pnext) {
				(void)fprintf(stderr,"malloc(printbuf) failed\n");
				exit(1);
			}
			*ptail = pnext;
			ptail = &pnext->fwd;
			pnext->fwd = 0;
			pnext->inpcb = inpcb;
			pnext->sockb = sockb;
			pnext->tcpcb = tcpcb;
			pnext->slabel = slabel;
			pnext->sacl = sacl;
			phead->next = next;

			prev = next;
		}
	}	/* for */

	while (phead != 0) {
		inpcb = phead->inpcb;
		sockb = phead->sockb;
		tcpcb = phead->tcpcb;
		slabel = phead->slabel;
		sacl = phead->sacl;
		next = phead->next;
		phead = phead->fwd;

#ifdef _SESMGR
		if (lflag) {
			labelpr(&slabel);
			labelpr(fetchlabel(&slabel, (ns_off_t)sockb.so_sendlabel));
		} else {
#endif
#if _MIPS_SZLONG == 64
			if (Aflag)
				if (istcp)
					printf("%8llx ", inpcb.inp_ppcb);
				else
					printf("%8llx ", next);
#else
			if (Aflag)
				if (istcp)
					printf("%8x ", inpcb.inp_ppcb);
				else
					printf("%8x ", next);
#endif
#ifdef _SESMGR
		}
		if (lflag)
			printf("%-5.5s", name);
		else
#endif
			printf("%-5.5s %6d %6d ", name, sockb.so_rcv.sb_cc,
			       sockb.so_snd.sb_cc);
		inetprint(&inpcb.inp_laddr, inpcb.inp_lport, name);
		inetprint(&inpcb.inp_faddr, inpcb.inp_fport, name);
		if (istcp) {
			if (tcpcb.t_state < 0 || tcpcb.t_state >= TCP_NSTATES)
				printf("%-12.12d", tcpcb.t_state);
			else
				printf(" %-12.12s", tcpstates[tcpcb.t_state]);
			if (qflag) {
				printf("%4d %4d %5d",
				sockb.so_q0len, sockb.so_qlen,sockb.so_qlimit);
			}
		}
#ifdef _SESMGR
		if (lflag) {
			if (!istcp) {
				printf("%12s", "");
				if (qflag)
					printf("%4s %4s %5s", "","","");
			}
			souidpr(sockb.so_uid);
			soaclpr(&sacl);
		}
#endif
		putchar('\n');
		prev = next;
	}
}

static void
get_ipforwarding(void)
{
	int name[4];
	size_t len;

	if (ipforwarding < 0) {		/* only once */
		ipforwarding = 0;

		name[0] = CTL_NET;
		name[1] = AF_INET;
		name[2] = IPPROTO_IP;
		name[3] = IPCTL_FORWARDING;
		len = sizeof(ipforwarding);
		if (sysctl(name, 4, &ipforwarding, &len, 0, 0) < 0) {
			perror("netstat: cannot get ipforwarding");
			ipforwarding = 0;
		}
	}
}

/*
 * Get the TCP/IP statistics structure from the kernel
 * Currently we only support the option to obtain all the various TCP/IP
 * related statistics on each CPU and return them summed for each category.
 * Later additions could add support for obtaining individual CPU statistics.
 *
 * RETURNS:
 * 0 => Success
 * non-zero => Failure
 */
int
getkna(kna_p)
	struct kna *kna_p;
{
	int err;

	err = sysmp(MP_SAGET, MPSA_TCPIPSTATS, kna_p, sizeof(struct kna));
	return ((err < 0) ? errno : 0);
}

/*
 * Dump TCP statistics structure.
 */
void
tcp_stats(char *name)
{
	struct kna tcpipstats;

	if (getkna(&tcpipstats)) { /* failed */
		(void)fail("tcp_stats: failed sysmp MP_SAGET\n");
		return;
	}

	printf ("%s:\n", name);

#define	p(f, m)	    printf(m, tcpipstats.tcpstat.f,		\
			   plural(tcpipstats.tcpstat.f))
#define	p_nos(f, m) printf(m, tcpipstats.tcpstat.f)
#define	p2(f1, f2, m)	printf(m, tcpipstats.tcpstat.f1,	\
			       plural(tcpipstats.tcpstat.f1),	\
			       tcpipstats.tcpstat.f2,		\
			       plural(tcpipstats.tcpstat.f2))
#define	p2_nos(f1, f2, m) printf(m, tcpipstats.tcpstat.f1,	\
			       plural(tcpipstats.tcpstat.f1),	\
			       tcpipstats.tcpstat.f2)

	p(tcps_sndtotal, "\t%llu packet%s sent\n");
	p2(tcps_sndpack,tcps_sndbyte,
	   "\t\t%llu data packet%s (%llu byte%s)\n");
	p2(tcps_sndrexmitpack, tcps_sndrexmitbyte,
	   "\t\t%llu data packet%s (%llu byte%s) retransmitted\n");
	p2_nos(tcps_sndacks, tcps_delack,
	   "\t\t%llu ack-only packet%s (%llu delayed)\n");
	p(tcps_sndurg, "\t\t%llu URG only packet%s\n");
	p(tcps_sndprobe, "\t\t%llu window probe packet%s\n");
	p(tcps_sndwinup, "\t\t%llu window update packet%s\n");
	p(tcps_sndctrl, "\t\t%llu control packet%s\n");
	p(tcps_rcvtotal, "\t%llu packet%s received\n");
	p2(tcps_rcvackpack, tcps_rcvackbyte,
	   "\t\t%llu ack%s (for %llu byte%s)\n");
	p(tcps_predack, "\t\t%llu ack prediction%s ok\n");
	p(tcps_rcvdupack, "\t\t%llu duplicate ack%s\n");
	p(tcps_rcvacktoomuch, "\t\t%llu ack%s for unsent data\n");
	p2(tcps_rcvpack, tcps_rcvbyte,
	   "\t\t%llu packet%s (%llu byte%s) received in-sequence\n");
	p(tcps_preddat, "\t\t%llu in-sequence prediction%s ok\n");
	p2(tcps_rcvduppack, tcps_rcvdupbyte,
	   "\t\t%llu completely duplicate packet%s (%llu byte%s)\n");
	p2(tcps_rcvpartduppack, tcps_rcvpartdupbyte,
	   "\t\t%llu packet%s with some dup. data (%llu byte%s duped)\n");
	p2(tcps_rcvoopack, tcps_rcvoobyte,
	   "\t\t%llu out-of-order packet%s (%llu byte%s)\n");
	p2(tcps_rcvpackafterwin, tcps_rcvbyteafterwin,
	   "\t\t%llu packet%s (%llu byte%s) of data after window\n");
	p(tcps_rcvwinprobe, "\t\t%llu window probe%s\n");
	p(tcps_rcvwinupd, "\t\t%llu window update packet%s\n");
	p(tcps_rcvafterclose, "\t\t%llu packet%s received after close\n");
	p(tcps_rcvbadsum, "\t\t%llu discarded for bad checksum%s\n");
	p(tcps_rcvbadoff,
	  "\t\t%llu discarded for bad header offset field%s\n");
	p_nos(tcps_rcvshort, "\t\t%llu discarded because packet too short\n");
	p(tcps_pawsdrop, "\t\t%llu discarded because of old timestamp%s\n");
	p(tcps_connattempt, "\t%llu connection request%s\n");
	p(tcps_accepts, "\t%llu connection accept%s\n");
	p(tcps_listendrop, "\t\t%llu listen queue overflow%s\n");
	p(tcps_badsyn, "\t\t%llu bad connection attempt%s\n");
	p(tcps_synpurge, "\t\t%llu drop%s from listen queue\n");
	p(tcps_connects,
	  "\t%llu connection%s established (including accepts)\n");
	p2(tcps_closed, tcps_drops,
	   "\t%llu connection%s closed (including %llu drop%s)\n");
	p(tcps_conndrops, "\t%llu embryonic connection%s dropped\n");
	p2(tcps_rttupdated, tcps_segstimed,
	   "\t%llu segment%s updated rtt (of %llu attempt%s)\n");
	p(tcps_rexmttimeo, "\t%llu retransmit timeout%s\n");
	p(tcps_timeoutdrop,
	  "\t\t%llu connection%s dropped by rexmit timeout\n");
	p(tcps_persisttimeo, "\t%llu persist timeout%s\n");
	p(tcps_persistdrop,
	  "\t\t%llu connection%s dropped by persist timeout\n");
	p(tcps_keeptimeo, "\t%llu keepalive timeout%s\n");
	p(tcps_keepprobe, "\t\t%llu keepalive probe%s sent\n");
	p(tcps_keepdrops, "\t\t%llu connection%s dropped by keepalive\n");
#undef p
#undef p2
}

/*
 * Dump UDP statistics structure.
 */
void
udp_stats(char *name)
{
	u_long delivered;
	struct kna tcpipstats;

	if (getkna(&tcpipstats)) { /* failed */
		(void)fail("und_stats: failed sysmp MP_SAGET\n");
		return;
	}
#define udpstat tcpipstats.udpstat

	printf("%s:\n\t%llu total datagrams received\n", name,
		udpstat.udps_ipackets);
	printf("\t%llu with incomplete header\n", udpstat.udps_hdrops);
	printf("\t%llu with bad data length field\n", udpstat.udps_badlen);
	printf("\t%llu with bad checksum\n", udpstat.udps_badsum);
	printf("\t%llu datagram%s dropped due to no socket\n",
	       udpstat.udps_noport, plural(udpstat.udps_noport));
	printf("\t%llu broadcast/multicast datagram%s"
	       " dropped due to no socket\n",
	       udpstat.udps_noportbcast, plural(udpstat.udps_noportbcast));
	printf("\t%llu datagram%s dropped due to full socket buffers\n",
	       udpstat.udps_fullsock, plural(udpstat.udps_fullsock));
	delivered = udpstat.udps_ipackets -
		    udpstat.udps_hdrops -
		    udpstat.udps_badlen -
		    udpstat.udps_badsum -
		    udpstat.udps_noport -
		    udpstat.udps_noportbcast -
		    udpstat.udps_fullsock;
	printf("\t%lu datagram%s delivered\n", delivered, plural(delivered));
	printf("\t%llu datagram%s output\n",
		udpstat.udps_opackets, plural(udpstat.udps_opackets));

#undef udpstat
}

/*
 * Dump IP statistics structure.
 */
void
ip_stats(char *name)
{
	struct kna tcpipstats;

	if (getkna(&tcpipstats)) { /* failed */
		(void)fail("ip_stats: failed sysmp MP_SAGET\n");
		return;
	}
	get_ipforwarding();
#define ipstat tcpipstats.ipstat

	printf("%s:\n\t%llu total packets received\n", name,
	       ipstat.ips_total);
	printf("\t%llu bad header checksum%s\n",
	       ipstat.ips_badsum, plural(ipstat.ips_badsum));
	printf("\t%llu with size smaller than minimum\n", ipstat.ips_toosmall);
	printf("\t%llu with data size < data length\n", ipstat.ips_tooshort);
	printf("\t%llu with header length < data size\n", ipstat.ips_badhlen);
	printf("\t%llu with data length < header length\n", ipstat.ips_badlen);
	printf("\t%llu with bad options\n", ipstat.ips_badoptions);
	printf("\t%llu fragment%s received\n",
	       ipstat.ips_fragments, plural(ipstat.ips_fragments));
	printf("\t%llu fragment%s dropped (dup or out of space)\n",
	       ipstat.ips_fragdropped, plural(ipstat.ips_fragdropped));
	printf("\t%llu fragment%s dropped after timeout\n",
	       ipstat.ips_fragtimeout, plural(ipstat.ips_fragtimeout));
	printf("\t%llu packet%s for this host\n",
	       ipstat.ips_delivered, plural(ipstat.ips_delivered));
	printf("\t%llu packet%s recvd for unknown/unsupported protocol\n",
	       ipstat.ips_noproto, plural(ipstat.ips_noproto));

	printf("\t%llu packet%s forwarded",
	       ipstat.ips_forward, plural(ipstat.ips_forward));
	printf("  (forwarding %sabled)\n", ipforwarding ? "en" : "dis");
	printf("\t%llu packet%s not forwardable\n",
	       ipstat.ips_cantforward, plural(ipstat.ips_cantforward));
	printf("\t%llu redirect%s sent\n",
	       ipstat.ips_redirectsent, plural(ipstat.ips_redirectsent));

	printf("\t%llu packet%s sent from this host\n",
	       ipstat.ips_localout, plural(ipstat.ips_localout));
	printf("\t%llu output packet%s dropped due to no bufs, etc.\n",
	       ipstat.ips_odropped, plural(ipstat.ips_odropped));
	printf("\t%llu output packet%s discarded due to no route\n",
	       ipstat.ips_noroute, plural(ipstat.ips_noroute));
	printf("\t%llu datagram%s fragmented\n",
	       ipstat.ips_fragmented, plural(ipstat.ips_fragmented));
	printf("\t%llu fragment%s created\n",
	       ipstat.ips_ofragments, plural(ipstat.ips_ofragments));
	printf("\t%llu datagram%s that can't be fragmented\n",
		ipstat.ips_cantfrag, plural(ipstat.ips_cantfrag));
#undef ipstat
}

static	char *icmpnames[] = {
	"echo reply\t",
	"#1\t",
	"#2\t",
	"destination unreachable\t",
	"source quench\t",
	"routing redirect\t",
	"#6\t",
	"#7\t",
	"echo\t",
	"router advertisement\t",
	"router solicitation\t",
	"time exceeded\t",
	"parameter problem\t",
	"time stamp\t",
	"time stamp reply\t",
	"information request\t",
	"information request reply\t",
	"address mask request\t",
	"address mask reply\t",
};

/*
 * Dump ICMP statistics.
 */
void
icmp_stats(char *name)
{
	register int i, first;
	struct kna tcpipstats;

	if (getkna(&tcpipstats)) { /* failed */
		(void)fail("icmp_stats: failed sysmp MP_SAGET\n");
		return;
	}
#define icmpstat tcpipstats.icmpstat

	printf("%s:\n\t%llu call%s to icmp_error\n", name,
	       icmpstat.icps_error, plural(icmpstat.icps_error));
	printf("\t%llu error%s not generated 'cuz old message was icmp\n",
	       icmpstat.icps_oldicmp, plural(icmpstat.icps_oldicmp));
	for (first = 1, i = 0; i < ICMP_MAXTYPE + 1; i++)
		if (icmpstat.icps_outhist[i] != 0) {
			if (first) {
				printf("\tOutput histogram:\n");
				first = 0;
			}
			printf("\t\t%s: %llu\n", icmpnames[i],
			       icmpstat.icps_outhist[i]);
	}
	printf("\t%llu message%s with bad code fields\n",
	       icmpstat.icps_badcode, plural(icmpstat.icps_badcode));
	printf("\t%llu message%s < minimum length\n",
	       icmpstat.icps_tooshort, plural(icmpstat.icps_tooshort));
	printf("\t%llu bad checksum%s\n",
	       icmpstat.icps_checksum, plural(icmpstat.icps_checksum));
	printf("\t%llu message%s with bad length\n",
	       icmpstat.icps_badlen, plural(icmpstat.icps_badlen));
	/* Would be preferable to print this out iff kernel variable
	   icmp_nounsafe was set, but no easy way to query for that info */
	if (icmpstat.icps_dropped != 0) {
	  printf("\t%llu REDIRECT message%s dropped\n",
		 icmpstat.icps_dropped,
		 plural(icmpstat.icps_dropped));
	}
	for (first = 1, i = 0; i < ICMP_MAXTYPE + 1; i++)
		if (icmpstat.icps_inhist[i] != 0) {
			if (first) {
				printf("\tInput histogram:\n");
				first = 0;
			}
			printf("\t\t%s: %llu\n", icmpnames[i],
			       icmpstat.icps_inhist[i]);
	}
	printf("\t%llu message response%s generated\n",
	       icmpstat.icps_reflect, plural(icmpstat.icps_reflect));
#undef icmpstat
}

/*
 * Dump IGMP statistics.
 */
void
igmp_stats(char *name)
{
	struct kna tcpipstats;
	register int i, first;

	if (getkna(&tcpipstats)) { /* failed */
		(void)fail("igmp_stats: failed sysmp MP_SAGET\n");
		return;
	}
#define igmpstat tcpipstats.igmpstat

	printf("%s:\n", name );
	printf("\t%llu message%s received\n",
	       igmpstat.igps_rcv_total, plural(igmpstat.igps_rcv_total));
	printf("\t%llu message%s received with too few bytes\n",
	       igmpstat.igps_rcv_tooshort, plural(igmpstat.igps_rcv_tooshort));
	printf("\t%llu message%s received with bad checksum\n",
	       igmpstat.igps_rcv_badsum, plural(igmpstat.igps_rcv_badsum));
	printf("\t%llu membership quer%s received\n",
	       igmpstat.igps_rcv_queries, plural(igmpstat.igps_rcv_queries));
	printf("\t%llu membership quer%s received with invalid field(s)\n",
	       igmpstat.igps_rcv_badqueries,
	       plural(igmpstat.igps_rcv_badqueries));
	printf("\t%llu membership report%s received\n",
	       igmpstat.igps_rcv_reports,
	       plural(igmpstat.igps_rcv_reports));
	printf("\t%llu membership report%s received with invalid field(s)\n",
	       igmpstat.igps_rcv_badreports,
	       plural(igmpstat.igps_rcv_badreports));
	printf("\t%llu membership report%s received"
	       "for groups to which we belong\n",
	       igmpstat.igps_rcv_ourreports,
	       plural(igmpstat.igps_rcv_ourreports));
	printf("\t%llu membership report%s sent\n",
	       igmpstat.igps_snd_reports, plural(igmpstat.igps_snd_reports));

#undef igmpstat
}

/*
 * Pretty print an Internet address that might be a wildcard.
 */
static char*
inetname(__uint32_t x, int lim)
{
	if (x == 0)
		return "*";
	return routename(x,lim);
}

/*
 * Pretty print an Internet address (net address + port).
 * If the nflag was specified, use numbers instead of names.
 */
static void
inetprint(register struct in_addr *in,
	  u_short port,
	  char *proto)
{
	struct servent *sp = 0;
	char line[80], *cp;
	int width;
	static struct {
		u_short port;
		char proto[8];
		char portname[9];
	} cache[16];
	static int lru;
	int i;
#ifdef _SESMGR
	width = 16;
	if (lflag)
		width = 20;
	else if (Aflag)
		width = 18;
#else
	width = Aflag ? 18 : 16;
#endif
	sprintf(line, "%.*s.", width, inetname(in->s_addr, width));
	cp = index(line, '\0');
	for (i = 0; i < sizeof(cache)/sizeof(cache[0]); i++) {
		lru = (lru+1) % (sizeof(cache)/sizeof(cache[0]));
		if (!strcmp(proto, cache[lru].proto)
		    && port == cache[lru].port) {
			(void)strcat(cp,cache[lru].portname);
			goto goit;
		}
	}
	if (!nflag && port) {
		sp = getservbyport((int)port, proto);
	}
	if (sp || port == 0)
		sprintf(cp, "%.8s", sp ? sp->s_name : "*");
	else
		sprintf(cp, "%d", ntohs((u_short)port));
	cache[lru].port = port;
	strncpy(cache[lru].proto, proto, sizeof(cache[lru].proto)-1);
	strncpy(cache[lru].portname, cp, sizeof(cache[lru].portname)-1);
goit:
	lru = (lru+1) % (sizeof(cache)/sizeof(cache[0]));
#ifdef _SESMGR
	width = 22;
	if (lflag)
		width = 20;
	else if (Aflag)
		width = 18;
#else
	width = Aflag ? 18 : 22;
#endif
	printf(" %-*.*s", width, width, line);
}




static struct ipstat ipstat, zipstat;

void
initip(void)
{
	bzero((char*)&zipstat, sizeof(zipstat));
}

void
zeroip(void)
{
	zipstat = ipstat;
}


int
ck_y_off(int dsize, int y, int *y_off_ptr)
{
	int y_off = *y_off_ptr;
	y += dsize+1-BOS;
	if (y < 0)
		y = 0;
	if (y_off > y)
		*y_off_ptr = y_off = y;
	return y_off;
}


void
move_print(int *yp, int *y_off_ptr, int dy, int x, char *pat, ...)
{
	va_list args;
	int y;

	if (*y_off_ptr != 0) {
		y = MIN(dy, *y_off_ptr);
		dy -= y;
		*y_off_ptr -= y;
		if (dy == 0 || *y_off_ptr != 0)
			return;
	}

	y = *yp;
	if (pat != 0 && *pat != '\0' && y+MAX(dy,1) <= BOS) {
		move(y,x);
		va_start(args, pat);
		vwprintw(win,pat,args);
		va_end(args);
	}
	*yp = y + dy;
}


void
sprip(int y, int *y_off_ptr)
{
	int y_off = ck_y_off(20, y, y_off_ptr);
	struct kna tipstat;
	struct ipstat *ipp;
#define COL0 1
#define ST tipstat.ipstat
#define MP(p,v) move_print(&y, &y_off, 1, COL0, p, v)

	if (getkna(&tipstat)) {
		(void)fail("sprip: failed sysmp MP_SAGET\n");
		return;
	}
	get_ipforwarding();

	ipp = (cmode == DELTA) ? &ipstat : &zipstat;

	move(y,COL0);
	printw("%10u total packets received\t",
	       ST.ips_total-ipp->ips_total);
	MP("%10u with bad checksum",
	   ST.ips_badsum-ipp->ips_badsum);
	MP("%10u with size smaller than minimum",
	   ST.ips_toosmall-ipp->ips_toosmall);
	MP("%10u with data size < data length",
	   ST.ips_tooshort-ipp->ips_tooshort);
	MP("%10u with header length < data size",
	   ST.ips_badhlen-ipp->ips_badhlen);
	MP("%10u with data length < header length",
	   ST.ips_badlen-ipp->ips_badlen);
	MP("%10u with bad options",
	   ST.ips_badoptions-ipp->ips_badoptions);
	MP("%10u fragments received",
	   ST.ips_fragments-ipp->ips_fragments);
	MP("%10u fragments dropped (dup or out of space)",
	   ST.ips_fragdropped-ipp->ips_fragdropped);
	MP("%10u fragments dropped after timeout",
	   ST.ips_fragtimeout-ipp->ips_fragtimeout);
	MP("%10u packets for this host",
	   ST.ips_delivered-ipp->ips_delivered);
	MP("%10u packets recvd for unknown/unsupported protocol",
	   ST.ips_noproto-ipp->ips_noproto);

	MP((ipforwarding
	    ? "%10u packets forwarded (forwarding enabled)"
	    : "%10u packets forwarded (forwarding disabled)"),
	   ST.ips_forward-ipp->ips_forward);
	MP("%10u packets not forwardable",
	   ST.ips_cantforward-ipp->ips_cantforward);
	MP("%10u redirects sent",
	   ST.ips_redirectsent-ipp->ips_redirectsent);

	MP("%10u packets sent from this host",
	   ST.ips_localout-ipp->ips_localout);
	MP("%10u output packets dropped due to no bufs, etc.",
	   ST.ips_odropped-ipp->ips_odropped);
	MP("%10u output packets discarded due to no route",
	   ST.ips_noroute-ipp->ips_noroute);
	MP("%10u datagrams fragmented",
	   ST.ips_fragmented-ipp->ips_fragmented);
	MP("%10u fragments created",
	   ST.ips_ofragments-ipp->ips_ofragments);
	MP("%10u datagrams that can't be fragmented",
	   ST.ips_cantfrag-ipp->ips_cantfrag);

	ipstat = ST;
#undef ST
#undef MP
#undef COL0
}



static struct icmpstat icmpstat, zicmpstat;
static unsigned char in_shown[ICMP_MAXTYPE];
static int total_in_shown = 1;
static unsigned char out_shown[ICMP_MAXTYPE];
static int total_out_shown = 1;

void
initicmp(void)
{
	bzero(&zicmpstat, sizeof(zicmpstat));
}

void
zeroicmp(void)
{
	zicmpstat = icmpstat;
}

void
spricmp(int y, int *y_off_ptr)
{
	int y_off = ck_y_off(total_in_shown/2+total_out_shown/2+9, y,
			     y_off_ptr);
	int i, x;
	struct kna ticmpstat;
	struct icmpstat *icmpp;
#define ST ticmpstat.icmpstat
#define COL0 5
#define COL1 1
#define COL2 33
#define MP(dy,x,p,v) move_print(&y, &y_off, dy, x, p, v)

	if (getkna(&ticmpstat)) {
		(void)fail("spricmp: failed sysmp MP_SAGET\n");
		return;
	}

	icmpp = (cmode == DELTA) ? &icmpstat : &zicmpstat;

	MP(1, COL0, "%10u messages with bad code fields",
	   ST.icps_badcode-icmpp->icps_badcode);
	MP(1, COL0,"%10u messages < minimum length",
	   ST.icps_tooshort-icmpp->icps_tooshort);
	MP(1, COL0,"%10u messages with bad checksum",
	   ST.icps_checksum-icmpp->icps_checksum);
	MP(1, COL0,"%10u messages with bad length",
	   ST.icps_badlen-icmpp->icps_badlen);
	MP(1, COL0,"%10u calls to icmp_error",
	   ST.icps_error-icmpp->icps_error);
	MP(1, COL0,"%10u errors not generated 'cuz old message was icmp",
	   ST.icps_oldicmp-icmpp->icps_oldicmp);
	MP(1, COL0,"%10u message responses generated",
	   ST.icps_reflect-icmpp->icps_reflect);
	for (x = 0, i = 0; i < ICMP_MAXTYPE+1; i++) {
		if (ST.icps_inhist[i] != 0 || in_shown[i]) {
			if (x == 0)
				MP(1, x = COL1, "Input histogram:",0);
			move_print(&y, &y_off, x==COL1 ? 0 : 1, x,
				   "%7u %s",
				   ST.icps_inhist[i] - icmpp->icps_inhist[i],
				   icmpnames[i]);
			x = (x==COL1) ? COL2 : COL1;
			if (! in_shown[i]) {
				in_shown[i] = 1;
				total_in_shown++;
			}
		}
	}
	if (x == COL2)
		MP(1, x, "", 0);
	for (x = 0, i = 0; i < ICMP_MAXTYPE+1; i++) {
		if (ST.icps_outhist[i] != 0 || out_shown[i]) {
			if (x == 0)
				MP(1, x = COL1, "Output histogram:",0);
			move_print(&y, &y_off, x==COL1 ? 0 : 1, x,
				   "%7u %s",
				   ST.icps_outhist[i] - icmpp->icps_outhist[i],
				   icmpnames[i]);
			x = (x==COL1) ? COL2 : COL1;
			if (!out_shown[i]) {
				out_shown[i] = 1;
				total_out_shown++;
			}
		}
	}
	icmpstat = ST;

#undef COL0
#undef COL1
#undef COL2
#undef ST
#undef MP
}


static struct udpstat udpstat, zudpstat;
void
initudp(void)
{
	bzero((char*)&zudpstat, sizeof(zudpstat));
}

void
zeroudp(void)
{
	zudpstat = udpstat;
}

void
sprudp(int y, int *y_off_ptr)
{
	int y_off = ck_y_off(9, y, y_off_ptr);
	u_long delivered;
	struct kna tudpstat;
	struct udpstat *udpp;
#define COL0 1
#define ST tudpstat.udpstat
#define MP(p,v) move_print(&y, &y_off, 1, COL0, p, v)

	if (getkna(&tudpstat)) {
		(void)fail("sprudp: failed sysmp MP_SAGET\n");
		return;
	}

	udpp = (cmode == DELTA) ? &udpstat : &zudpstat;

	MP("%10u total datagrams received",
	   ST.udps_ipackets-udpp->udps_ipackets);
	MP("%10u with incomplete header",
	   ST.udps_hdrops-udpp->udps_hdrops);
	MP("%10u with bad data length field",
	   ST.udps_badlen-udpp->udps_badlen);
	MP("%10u with bad checksum",
	   ST.udps_badsum-udpp->udps_badsum);
	MP("%10u datagrams dropped due to no socket",
	   ST.udps_noport-udpp->udps_noport);
	MP("%10u broadcast/multicast datagrams dropped due to no socket",
	   ST.udps_noportbcast-udpp->udps_noportbcast);
	MP("%10u datagrams dropped due to full socket buffers",
	   ST.udps_fullsock-udpp->udps_fullsock);
	delivered = ((ST.udps_ipackets - udpp->udps_ipackets)
		     - (ST.udps_hdrops - udpp->udps_hdrops)
		     - (ST.udps_badlen - udpp->udps_badlen)
		     - (ST.udps_badsum - udpp->udps_badsum)
		     - (ST.udps_noport - udpp->udps_noport)
		     - (ST.udps_noportbcast - udpp->udps_noportbcast)
		     - (ST.udps_fullsock - udpp->udps_fullsock));

	MP("%10u datagrams delivered", delivered);
	MP("%10u datagrams output",
	   ST.udps_opackets-udpp->udps_opackets);

	udpstat = ST;
#undef ST
#undef MP
#undef COL0
}


static struct tcpstat tcpstat, ztcpstat;
void
inittcp(void)
{
	bzero((char*)&ztcpstat, sizeof(ztcpstat));
}

void
zerotcp()
{
	ztcpstat = tcpstat;
}

void
sprtcp(int y, int *y_off_ptr)
{
	int y_off = ck_y_off(42, y, y_off_ptr);
	struct kna ttcpstat;
	struct tcpstat *tcpp;
#define COL0 1
#define P(f,p) move_print(&y,&y_off,1,COL0, p,ttcpstat.tcpstat.f - tcpp->f)
#define	P2(f1, f2, p) move_print(&y,&y_off,1,COL0, p,		\
				 ttcpstat.tcpstat.f1 - tcpp->f1,\
				 ttcpstat.tcpstat.f2 - tcpp->f2)

	if (getkna(&ttcpstat)) {
		(void)fail("sprtcp: failed sysmp MP_SAGET\n");
		return;
	}

	tcpp = (cmode == DELTA) ? &tcpstat : &ztcpstat;

	P(tcps_sndtotal, "%10u packets sent\t");
	P2(tcps_sndpack, tcps_sndbyte,
	   "%18u data packets (%u bytes)\t\t");
	P2(tcps_sndrexmitpack, tcps_sndrexmitbyte,
	   "%18u retransmitted data packets (%u bytes)\t\t");
	P2(tcps_sndacks, tcps_delack,
	   "%18u ack-only packets (%u delayed)\t\t");
	P(tcps_sndurg, "%18u URG only packets\t\t");
	P(tcps_sndprobe, "%18u window probe packets\t\t");
	P(tcps_sndwinup, "%18u window update packets\t\t");
	P(tcps_sndctrl, "%18u control packets\t\t");
	P(tcps_rcvtotal, "%10u packets received\t");
	P2(tcps_rcvackpack,tcps_rcvackbyte, "%18u acks (for %u bytes)\t\t");
	P(tcps_predack, "%18u ack predictions ok\t");
	P(tcps_rcvdupack, "%18u duplicate acks\t");
	P(tcps_rcvacktoomuch, "%18u ACKs for unsent data\t\t");
	P2(tcps_rcvpack, tcps_rcvbyte,
	   "%18u packets (%u bytes) received in-sequence\t\t");
	P(tcps_preddat, "%18u in-sequence predictions ok\t");
	P2(tcps_rcvduppack, tcps_rcvdupbyte,
	   "%18u completely duplicate packets (%u bytes)\t\t");
	P2(tcps_rcvpartduppack, tcps_rcvpartdupbyte,
	   "%18u packets with some dup. data (%u bytes duped)\t\t");
	P2(tcps_rcvoopack, tcps_rcvoobyte,
	   "%18u out-of-order packets (%u bytes)\t\t");
	P2(tcps_rcvpackafterwin, tcps_rcvbyteafterwin,
	   "%18u packets (%u bytes) of data after window\t\t");
	P(tcps_rcvwinprobe, "%18u window probes\t\t");
	P(tcps_rcvwinupd, "%18u window update packets\t\t");
	P(tcps_rcvafterclose, "%18u packets received after close\t\t");
	P(tcps_rcvbadsum, "%18u discarded for bad checksums\t");
	P(tcps_rcvbadoff, "%18u discarded for bad header offset fields\t");
	P(tcps_rcvshort, "%18u discarded because packet too short\t");
	P(tcps_pawsdrop, "%18u discarded because of old timestamp\t\t");
	P2(tcps_connattempt, tcps_accepts,
	   "%10u connection requests, %u connection accepts\t");
	P(tcps_listendrop, "%18u listen queue overflows\t\t");
	P(tcps_badsyn, "%18u bad connection attempt\t\t");
	P(tcps_synpurge, "%18u drops from listen queue\t\t");

	P(tcps_connects,
	  "%10u connections established (including accepts)\t");
	P2(tcps_closed, tcps_drops,
	   "%10u connections closed (including %u drops)\t");
	P(tcps_conndrops, "%10u embryonic connections dropped\t\t");
	P2(tcps_rttupdated, tcps_segstimed,
	   "%10u segments updated rtt (of %u attempts)\t\t");
	P2(tcps_rexmttimeo, tcps_keeptimeo,
	   "%10u retransmit timeouts, %u keepalive timeouts\t");
	P(tcps_timeoutdrop, "%18u connection dropped by rexmit timeoutt\t");
	P(tcps_persisttimeo, "%10u persist timeouts\t\t");
	P(tcps_persistdrop,"%18u connections dropped by persist timeout\t\t");
	P(tcps_keeptimeo, "%10u keepalive timeouts\t\t");
	P(tcps_keepprobe, "%18u keepalive probes sent\t\t");
	P(tcps_keepdrops, "%18u connections dropped by keepalive\t\t");

	tcpstat = ttcpstat.tcpstat;
#undef COL0
#undef P
#undef P2
}


static int total_shown_sockstat;
static struct sockstat shown_sockstat, sockstat, zsockstat;
static struct socket_types {
	int	st_type;
	char	*st_name;
} socket_types[] = {
	{ 0,			"Zero/Illegal sockets" },
	{ SOCK_DGRAM,		"DATAGRAM/TPI_CLTS sockets" },
	{ SOCK_STREAM,		"STREAM sockets" },
	{ _NC_TPI_COTS_ORD,	"TPI_COTS_ORD sockets" },
	{ SOCK_RAW,		"RAW/TPI_RAW sockets" },
	{ SOCK_RDM,		"RDM sockets" },
	{ SOCK_SEQPACKET,	"SEQPACKET sockets" },
	{ TCPSTAT_TPISOCKET,	"TPI_COTS sockets" },
	{ 0, 0 }
};
static struct tcp_states {
	int	tcp_type;
	char	*tcp_name;
} tcp_states[] = {
	{ 0,	"CLOSED state" }, /* refer to tcp_fsm.h for actual symbols */
	{ 1,	"LISTEN state" },
	{ 2,	"SYN_SENT state" },
	{ 3,	"SYN_RECEIVED state" },
	{ 4,	"ESTABLISHED state" },
	{ 5,	"CLOSE_WAIT state" },
	{ 6,	"FIN_WAIT_1 state" },
	{ 7,	"CLOSING state" },
	{ 8,	"LAST_ACK state" },
	{ 9,	"FIN_WAIT_2 state" },
	{ 10,	"TIME_WAIT state" },
	{ 0, 0 }
};

void
initsock(void)
{
	bzero ((char*)&zsockstat, sizeof(zsockstat));
	bzero ((char *)&shown_sockstat, sizeof(zsockstat));
}

void
zerosock()
{
	zsockstat = sockstat;
}

void
sprsock(int y, int *y_off_ptr)
{
	int y_off = ck_y_off(2+total_shown_sockstat, y, y_off_ptr);
	struct sockstat seen_sockstat, tsockstat;
	struct socket_types *sp;
	struct tcp_states *tp;
	u_long active_socks;
	int err, i;
#define COL0 1

	bzero (&seen_sockstat, sizeof(struct sockstat));
	err = sysmp(MP_SAGET1, MPSA_SOCKSTATS, &tsockstat,
		    sizeof(struct sockstat), 0);
	if (err < 0) {
		(void)fail("sprsock: failed sysmp MPSA_SOCKSTATS; errno %d\n",
			   errno);
		return;
	}
	for (i=0; i < TCPSTAT_NUM_SOCKTYPES; i++) {
		if (tsockstat.open_sock[i] > 0) seen_sockstat.open_sock[i] = 1;
	}
	for (i=0; i < TCPSTAT_NUM_TCPSTATES; i++) {
		if (tsockstat.tcp_sock[i] > 0) seen_sockstat.tcp_sock[i] = 1;
	}

	active_socks = 0;
	for (i=0; i < TCPSTAT_NUM_SOCKTYPES; i++) {
		active_socks += tsockstat.open_sock[i];
	}
	move_print(&y,&y_off,1,COL0, "%10u Active socket types\t\t",
		   active_socks);
	for (sp = socket_types; sp->st_name; sp++) {
		seen_sockstat.open_sock[sp->st_type] = 1;
		if (tsockstat.open_sock[sp->st_type] == 0
		    && !shown_sockstat.open_sock[sp->st_type]) {
			continue;
		}
		move_print(&y,&y_off,1,COL0, "%18u %s\t\t",
			   tsockstat.open_sock[sp->st_type], sp->st_name);
		if (! shown_sockstat.open_sock[sp->st_type]) {
			shown_sockstat.open_sock[sp->st_type] = 1;
			total_shown_sockstat++;
		}
	}

	move_print(&y,&y_off,1,COL0, "%10u Total TCP connections\t\t",
		   tsockstat.open_sock[2]);
	for (tp = tcp_states; tp->tcp_name && y < BOS-1; tp++) {
		seen_sockstat.tcp_sock[tp->tcp_type] = 1;
		if (tsockstat.tcp_sock[tp->tcp_type] == 0
		    && !shown_sockstat.tcp_sock[tp->tcp_type]) {
			continue;
		}
		move_print(&y,&y_off,1,COL0, "%18u %s\t\t",
			tsockstat.tcp_sock[tp->tcp_type], tp->tcp_name);
		shown_sockstat.tcp_sock[tp->tcp_type] = 1;
	}
	sockstat = tsockstat;
#undef COL0
}

