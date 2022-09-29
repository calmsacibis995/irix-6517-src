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
static char sccsid[] = "@(#)if.c	5.6 (Berkeley) 2/7/88 plus MULTICAST 1.2";
#endif /* not lint */

#include <sys/types.h>
#include <sys/sema.h>
#include <sys/hashing.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#include <netns/ns.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <signal.h>
#include <sys/param.h>
#include <ctype.h>
#include <stdarg.h>
#include <errno.h>
#include <curses.h>
#include <string.h>
#include <unistd.h>
#include <bstring.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <strings.h>

#include "netstat.h"

union ifaddr_union {
	struct ifaddr ifa;
	struct in_ifaddr in;
} ifaddr;

#define	MAX_NETFL	15
int net_fl;				/* length of netname field */

static	void catchalarm();
static int sidewaysintpr(unsigned int, ns_off_t);
static  char *etherprint(u_char*);

/*
 * Print a limited-length string, and indicate when it is truncated.
 */
static void
limpr(int lim, char *pad, char *str)
{
	char trimstr[200];

	if (strlen(str) > lim) {
		(void)strncpy(trimstr,str,lim-1);
		trimstr[lim-1] = '~';
		trimstr[lim] = '\0';
		str = trimstr;
	}
	(void)printf("%-*s%s", lim, str, pad);
}

/*
 * print interface name and MTU once
 */
static void
namepr(char *name, int mtu)
{
	char nmstr[IFNAMSIZ+1+16+1];


	if (name[0] != '\0') {
		(void)sprintf(nmstr, "%-4s %-5u", name, mtu);
	} else {
		nmstr[0] = '\0';
	}
	limpr(11, "", nmstr);
	name[0] = '\0';
}

/*
 * Print a description of an in_ifaddr structure
 */
void
print_alias(ns_off_t inifaddraddr, ns_off_t ifnetaddr, struct ifnet *ifnetp)
{
	struct in_ifaddr inifaddr;
	register char *cp;
	struct sockaddr_in *sin;
	int n;

	klseek(kmem, inifaddraddr, 0);
	read(kmem, (char *)&inifaddr, sizeof(struct in_ifaddr));

	if ((inifaddr.ia_addrflags & IADDR_PRIMARY) ||
	     ((ns_off_t)(inifaddr.ia_ifa.ifa_ifp) != ifnetaddr)) {
			return;
	}
	/*
	 * compute address in local buffer of the ifa_addr sockaddr structure
	 */
	cp = ((ns_off_t)inifaddr.ia_ifa.ifa_addr
			- inifaddraddr
			+ (char*)&inifaddr);
	sin = (struct sockaddr_in *)cp;

	switch (sin->sin_family) {

	case AF_UNSPEC:
		printf("%23s %4s\n", "", "none");
		break;

	case AF_INET:
		if (ifnetp->if_flags & IFF_POINTOPOINT) {
			/*
			 * compute address in local buffer of ifa_dstaddr
			 * sockaddr structure which has dst addr for PPP links
			 */
			cp = ((ns_off_t)inifaddr.ia_ifa.ifa_dstaddr
				- inifaddraddr
				+ (char*)&inifaddr);
			sin = (struct sockaddr_in *)cp;
		}
		printf("%*s %-19s\n", 27 + (net_fl - MAX_NETFL),
		       "", routename(sin->sin_addr.s_addr, 19));
		break;

	case AF_APPLETALK:
		printf("%23s atalk: ", "");
		{
			struct sockaddr *sa = (struct sockaddr *)sin;
			u_short *sp = (u_short *)sa->sa_data;
			u_char *cp = (u_char *)&sa->sa_data[2];
			printf("%04x     %02x",*sp, *cp);
		}
		putchar('\n');
		break;

	default:
		printf("%23s af %2d: ", "", sin->sin_family);
		for (cp = (char*)sin+sizeof(*sin)-1;
		     cp >= ((struct sockaddr*)sin)->sa_data;
		     --cp)
			if (*cp != 0)
				break;
		n = cp - ((struct sockaddr*)sin)->sa_data + 1;
		cp = ((struct sockaddr*)sin)->sa_data;
		if (n <= 7)
			while (--n)
				printf("%02d.", *cp++ & 0xff);
		else
			while (--n)
				printf("%02d", *cp++ & 0xff);
		printf("%02d ", *cp & 0xff);
		putchar('\n');
		break;
	}
}

/*
 * Print a description of an in_ifaddr structure
 */
static void
print_inifaddr(ns_off_t inifaddraddr, ns_off_t ifnetaddr,
	       struct ifnet *ifnetp, char *name, int stats_printed)
{
	struct in_ifaddr inifaddr;
	register char *cp;
	struct sockaddr_in *sin;
	int n;

	klseek(kmem, inifaddraddr, 0);
	read(kmem, (char *)&inifaddr, sizeof(struct in_ifaddr));

	/*
	 * compute address in local buffer of the ifa_addr sockaddr structure
	 */
	cp = ((ns_off_t)inifaddr.ia_ifa.ifa_addr
			- inifaddraddr
			+ (char*)&inifaddr);
	sin = (struct sockaddr_in *)cp;

	switch (sin->sin_family) {

	case AF_UNSPEC:
		namepr(name, ifnetp->if_data.ifi_mtu);
		limpr(net_fl, " ", "none");
		limpr(15,     " ", "none");
		break;

	case AF_INET:
		namepr(name, ifnetp->if_data.ifi_mtu);

		if (ifnetp->if_flags & IFF_POINTOPOINT) {
			/*
			 * compute address in local buffer of ifa_dstaddr
			 * sockaddr structure which has dst addr for PPP links
			 */
			cp = ((ns_off_t)inifaddr.ia_ifa.ifa_dstaddr
				- inifaddraddr
				+ (char*)&inifaddr);
			sin = (struct sockaddr_in *)cp;
			limpr(net_fl, " ", "(pt-to-pt)");
			n = 15;
		} else {
			n = printf("%-*s ", net_fl,
				   netname(htonl(inifaddr.ia_subnet),
					   inifaddr.ia_subnetmask,
					   Aflag ? net_fl : 20));
			n = MAX_NETFL - (n - net_fl - 1);
		}
		printf("%-*s ", n, routename(sin->sin_addr.s_addr,
					     !stats_printed ? 15 : 40));
		break;

	case AF_NS:
		namepr(name, ifnetp->if_data.ifi_mtu);
		{
		struct sockaddr_ns *sns = (struct sockaddr_ns *)sin;
		u_long net;
		char netnum[8];

		*(union ns_net *) &net = sns->sns_addr.x_net;
		sprintf(netnum, "%lxH", ntohl(net));
		upHex(netnum);
		printf("ns:%-8s ", netnum);
		printf("%-15s ", ns_phost(sns));
		}
		break;

	case AF_APPLETALK:
		printf("atalk: ");
		{
			struct sockaddr *sa = (struct sockaddr *)sin;
			u_short *sp = (u_short *)sa->sa_data;
			u_char *cp = (u_char *)&sa->sa_data[2];
			printf("%04x     %02x %12s",*sp, *cp, "");
		}
		break;

	default:
		namepr(name, ifnetp->if_data.ifi_mtu);
		printf("af%2d: ", sin->sin_family);
		for (cp = (char*)sin+sizeof(*sin)-1;
		     cp >= ((struct sockaddr*)sin)->sa_data;
		     --cp)
			if (*cp != 0)
				break;
		n = cp - ((struct sockaddr*)sin)->sa_data + 1;
		cp = ((struct sockaddr*)sin)->sa_data;
		if (n <= 7)
			while (--n)
				printf("%02d.", *cp++ & 0xff);
		else
			while (--n)
				printf("%02d", *cp++ & 0xff);
		printf("%02d ", *cp & 0xff);
		break;
	}
}

/*
 * Print a description of either an ifaddr structure OR an in_ifaddr structure
 * depending on the
 */
void
print_linkaddr(ns_off_t ifaddraddr,
		ns_off_t ifnet,
		struct ifnet *ifnetp,
		char *name)
{
	union ifaddr_union ifaddr;
	register char *cp;
	struct sockaddr_in *sin;
	int n;

	klseek(kmem, ifaddraddr, 0);
	read(kmem, (char *)&ifaddr, sizeof ifaddr);

	if ((ns_off_t)(ifaddr.in.ia_ifp) != ifnet)
		return;
	/*
	 * compute address in local buffer of the
	 * ifa_addr sockaddr structure
	 */
	cp = ((ns_off_t)ifaddr.ifa.ifa_addr
		      - ifaddraddr
		      + (char*)&ifaddr);
	sin = (struct sockaddr_in *)cp;

	if (sin->sin_family == AF_LINK) {
		/*
		 * compute address in local buffer of the
		 * ifa_addr sockaddr structure
		 */
		cp = (char *)LLADDR((struct sockaddr_dl*)sin);
		n = ((struct sockaddr_dl*)sin)->sdl_alen;

		if (!Aflag || n == 0)
			return;
		namepr(name, ifnetp->if_data.ifi_mtu);
		if (n != 6) {
			int m = printf("<Link>");
			while (--n >= 0)
				m += printf("%x%c",
					    *cp++ & 0xff,
					    n>0 ? '.' : ' ');
			m = 28 - m;
			while (m-- > 0)
				putchar(' ');
		} else {
			printf("<Link>%21s ", etherprint((u_char*)cp));
		}
	}
}

/*
 * Print a description of an in_multi structure
 */
void
print_multiaddr(ns_off_t multiaddr, ns_off_t ifnetaddr)
{
	struct in_multi inm;

	klseek(kmem, multiaddr, 0);
	read(kmem, (char*)&inm, sizeof(struct in_multi));

	if (inm.inm_ifp == (struct ifnet *)ifnetaddr) {
		printf("%*s %-19s\n", 27 + (net_fl - MAX_NETFL), "",
		       routename(inm.inm_addr.s_addr, 19));
	}
	return;
}

/*
 * Print a description of the network interfaces.
 */
void
intpr(int interval, ns_off_t ifnetaddr_start, ns_off_t hashtable_info_addr)
{
	struct ifnet ifnet;
	union ifaddr_union ifaddr;
	ns_off_t ifaddraddr, ifnetfound, ifnetaddr;
	char name[IFNAMSIZ+18+1+1];
	unsigned int hashtablesize;
	unsigned short i;
	ns_off_t h, head, hashtable_inaddr_addr;
	struct arpcom ac;
	struct hashinfo hashtable_info;
	struct hashbucket hb_entry;
	struct hashtable hashtable_inaddr[HASHTABLE_SIZE * 2];
	char *cp;
	char *p = (char *)hashtable_inaddr;
	int n;

	if (ifnetaddr_start == 0) {
		printf("ifnet: symbol not defined\n");
		return;
	}
	if (hashtable_info_addr == 0) {

		printf("hashtable_info: symbol not defined\n");
		return;
	}
	if (interval) {
		sidewaysintpr(interval, ifnetaddr_start);
		return;
	}

	/*
	 * Read address of first ifnet structure
	 */
	(void) klseek(kmem, ifnetaddr_start, 0);
	(void) read(kmem, (char *)&ifnetaddr, sizeof ifnetaddr);

	if (qflag) {
		net_fl = 11;
	} else if (tflag) {
		net_fl = 13;
	} else {
		net_fl = MAX_NETFL;
	}
	printf("%-5s%-5s %-*s %-15s %8s %5s %8s %5s",
	       "Name", "Mtu", net_fl, "Network", "Address",
	       "Ipkts", "Ierrs", "Opkts", "Oerrs");

	if (qflag) {
		printf(" %1s %3s %4s", "q", "max", "drop");
	} else {
		printf(" %5s", "Coll");
		if (tflag) {
			printf(" %4s", "Time");
		}
	}
	putchar('\n');

	while (ifnetaddr) {

		/* Read ifnet struct and name for the network interface */
		klseek(kmem, ifnetaddr, 0);
		read(kmem, (char *)&ifnet, sizeof ifnet);

		ifnetfound = ifnetaddr;
		ifnetaddr = (ns_off_t)ifnet.if_next;
		ifaddraddr = (ns_off_t)(ifnet.in_ifaddr);

		klseek(kmem, (ns_off_t)ifnet.if_name, 0);
		read(kmem, name, IFNAMSIZ);
		name[IFNAMSIZ] = '\0';

		if (interface != 0 &&
		 (strcmp(name, interface) != 0 || unit != ifnet.if_unit)) {
			continue;
		}

		cp = index(name, '\0');
		cp = cp + sprintf(cp, "%d", ifnet.if_unit);

		if ((ifnet.if_flags & IFF_UP) == 0) {
			*cp++ = '*';
			*cp = '\0';
		}

		/*
		 * See if inifaddr block set in ifnet structure
		 */
		if (ifaddraddr == 0) { /* none yet */
			namepr(name, ifnet.if_data.ifi_mtu);
			limpr(net_fl, " ", "none");
			limpr(15,     " ", "none");
		} else {
			print_inifaddr(ifaddraddr,
				       ifnetfound, &ifnet,
				       name,
				       0); /* stats not printed yet */
		}
		printf("%8u %5u %8u %5u ",
		       ifnet.if_data.ifi_ipackets, ifnet.if_data.ifi_ierrors,
		       ifnet.if_data.ifi_opackets, ifnet.if_data.ifi_oerrors);

		if (qflag) {
			printf(ifnet.if_snd.ifq_maxlen <= 99
			       ? "%2d %2d %4d"
			       : "%2d %3d %3d",
			       ifnet.if_snd.ifq_len,
			       ifnet.if_snd.ifq_maxlen,
			       ifnet.if_snd.ifq_drops);
		} else {
			printf("%5u", ifnet.if_data.ifi_collisions);
			if (tflag)
				printf(" %3d", ifnet.if_timer);
		}
		putchar('\n');

		/*
		 * print all internet alias addresses for this interface
		 * via enumerating the internet address hash table and looking
		 * for alias entries who match this interface.
		 */
		klseek(kmem, hashtable_info_addr, 0);
		read(kmem, (char*)&hashtable_info, sizeof(struct hashinfo));

		hashtable_inaddr_addr = (ns_off_t)hashtable_info.hashtable;
		hashtablesize = hashtable_info.hashtablesize;
		if (hashtablesize > HASHTABLE_SIZE)
			hashtablesize = HASHTABLE_SIZE;

		klseek(kmem, hashtable_inaddr_addr, 0);
		read(kmem, (char*)hashtable_inaddr,
			hashtablesize * hashtable_info.entrysize);

		for (i=0; i < hashtablesize; i++) {

			head = (ns_off_t)(hashtable_inaddr_addr +
				       i * hashtable_info.entrysize);
			klseek(kmem, head, 0);
			read(kmem, (char *)&hb_entry, sizeof(hb_entry));

			/*
			 * Regular C code for loop thru hash table.
			 * for (h = hashtable[i].next; h != head; h = h->next)
			 */
			for (h = (ns_off_t)((struct hashtable *)
				(p + i * hashtable_info.entrysize))->next;
					h != head;
				h = (ns_off_t)hb_entry.next) {

				klseek(kmem, h, 0);
				read(kmem, (char*)&hb_entry, sizeof(hb_entry));

				if (hb_entry.flags & HTF_INET) {
					print_alias((ns_off_t)h,
						    ifnetfound, &ifnet);
				}
			}
		}

		if (aflag) {
			/*
			 * print all internet multicast addresses for this
			 * interface by enumerating the internet address
			 * hash table and looking for multicast entries
			 * who match this interface.
			 */
			for (i=0; i < hashtablesize; i++) {

				head = (ns_off_t)(hashtable_inaddr_addr +
					       i * hashtable_info.entrysize);
				klseek(kmem, head, 0);
				read(kmem, (char *)&hb_entry,sizeof(hb_entry));

				for (h = (ns_off_t) ((struct hashtable *)
					(p + i *
					hashtable_info.entrysize))->next;
				     h != head;
				     h = (ns_off_t)hb_entry.next) {

					klseek(kmem, h, 0);
					read(kmem, (char*)&hb_entry,
						sizeof(hb_entry));

					if (hb_entry.flags & HTF_MULTICAST) {
						print_multiaddr((ns_off_t)h,
							ifnetfound);
					}
				}
			}
			/*
			 * print the link-level address for this interface
			 */
			if (ifnet.if_data.ifi_type == IFT_ETHER ||
			    ifnet.if_data.ifi_type == IFT_FDDI ||
			    ifnet.if_data.ifi_type == IFT_ISO88025 ||
			    ifnet.if_data.ifi_type == IFT_GSN) {

				klseek(kmem, ifnetfound, 0);
				read(kmem, (char *)&ac, sizeof ac);
				printf("%-*s %s\n", (27 + net_fl - MAX_NETFL),
				       "", etherprint(ac.ac_enaddr));
			}
		}
	}
	return;
}

#define	MAXIF	128
struct	iftot {
	ns_off_t ifaddraddr;
	char	ift_name[1+IFNAMSIZ+1+3+1+1];	/* "(ifnameUNIT)*" */
	union if_addr {
		struct ifaddr ifa;
		struct in_ifaddr in;
	} ift_addr;
	u_int	ift_flags;		/* interface flags */
	u_int	ift_old_flags;		/* interface flags */
	u_int	ift_mtu;
	u_int	ift_ip;			/* input packets */
	u_int	ift_ie;			/* input errors */
	u_int	ift_ib;			/* input bytes */
	u_int	ift_op;			/* output packets */
	u_int	ift_oe;			/* output errors */
	u_int	ift_ob;			/* output bytes */
	u_int	ift_co;			/* collisions */
	u_int	ift_len;		/* queue length */
	u_int	ift_drop;		/* dropped from queue */
} iftot[MAXIF];

/*
 * Print a running summary of interface statistics.
 * Repeat display every interval seconds, showing statistics
 * collected over that interval.  Assumes that interval is non-zero.
 * First line printed at top of screen is always cumulative.
 */
static int
sidewaysintpr(unsigned int interval, ns_off_t off)
{
	struct ifnet ifnet;
	ns_off_t firstifnet;
	register struct iftot *ip, *total;
	register int line;
	struct iftot *lastif, *sum, *interesting;

	klseek(kmem, off, 0);
	read(kmem, (char *)&firstifnet, sizeof (ns_off_t));
	lastif = iftot;
	sum = iftot + MAXIF - 1;
	total = sum - 1;
	interesting = iftot;
	for (off = firstifnet, ip = iftot; off;) {
		char *cp;

		klseek(kmem, off, 0);
		read(kmem, (char *)&ifnet, sizeof ifnet);
		klseek(kmem, (ns_off_t)ifnet.if_name, 0);
		ip->ift_name[0] = '(';
		read(kmem, ip->ift_name + 1, IFNAMSIZ);
		ip->ift_name[IFNAMSIZ+1] = '\0';
		if (interface && strcmp(ip->ift_name + 1, interface) == 0 &&
		    unit == ifnet.if_unit)
			interesting = ip;
		cp = &ip->ift_name[strlen(ip->ift_name)];
		sprintf(cp, "%d)", ifnet.if_unit);
		ip++;
		if (ip >= iftot + MAXIF - 2)
			break;
		off = (ns_off_t) ifnet.if_next;
	}
	lastif = ip;

banner:
	printf("    input   %-6.6s    output       ", interesting->ift_name);
	if (lastif - iftot > 0)
		printf("     input  (Total)    output");
	for (ip = iftot; ip < iftot + MAXIF; ip++) {
		ip->ift_ip = 0;
		ip->ift_ie = 0;
		ip->ift_op = 0;
		ip->ift_oe = 0;
		ip->ift_co = 0;
	}
	putchar('\n');
	printf("%8.8s %5.5s %8.8s %5.5s %5.5s ",
		"packets", "errs", "packets", "errs", "colls");
	if (lastif - iftot > 0)
		printf("%8.8s %5.5s %8.8s %5.5s %5.5s ",
			"packets", "errs", "packets", "errs", "colls");
	putchar('\n');
	fflush(stdout);
	line = 0;
loop:
	sum->ift_ip = 0;
	sum->ift_ie = 0;
	sum->ift_op = 0;
	sum->ift_oe = 0;
	sum->ift_co = 0;
	for (off = firstifnet, ip = iftot; off && ip < lastif; ip++) {
		klseek(kmem, off, 0);
		read(kmem, (char *)&ifnet, sizeof ifnet);
		if (ip == interesting)
			printf("%8u %5u %8u %5u %5u ",
			       ifnet.if_data.ifi_ipackets - ip->ift_ip,
			       ifnet.if_data.ifi_ierrors - ip->ift_ie,
			       ifnet.if_data.ifi_opackets - ip->ift_op,
			       ifnet.if_data.ifi_oerrors - ip->ift_oe,
			       ifnet.if_data.ifi_collisions - ip->ift_co);
		ip->ift_ip = ifnet.if_data.ifi_ipackets;
		ip->ift_ie = ifnet.if_data.ifi_ierrors;
		ip->ift_op = ifnet.if_data.ifi_opackets;
		ip->ift_oe = ifnet.if_data.ifi_oerrors;
		ip->ift_co = ifnet.if_data.ifi_collisions;
		sum->ift_ip += ip->ift_ip;
		sum->ift_ie += ip->ift_ie;
		sum->ift_op += ip->ift_op;
		sum->ift_oe += ip->ift_oe;
		sum->ift_co += ip->ift_co;
		off = (ns_off_t) ifnet.if_next;
	}
	if (lastif - iftot > 0)
		printf("%8u %5u %8u %5u %5u ",
			sum->ift_ip - total->ift_ip,
			sum->ift_ie - total->ift_ie,
			sum->ift_op - total->ift_op,
			sum->ift_oe - total->ift_oe,
			sum->ift_co - total->ift_co);
	*total = *sum;
	putchar('\n');
	fflush(stdout);
	line++;
	 if (interval)
		sleep(interval);
	if (line == 21)
		goto banner;
	goto loop;
	/*NOTREACHED*/
}

/*
 * Return a printable string representation of an Ethernet address.
 */
static char *
etherprint(u_char *en)
{
	static char string[18];

	sprintf(string, "%02x:%02x:%02x:%02x:%02x:%02x",
		en[0], en[1], en[2], en[3], en[4], en[5] );
	string[17] = '\0';
	return(string);
}

/*----------------------------------------------------------------------*/
#include "cdisplay.h"

static struct ifnet ifnet;
static struct iftot ziftot[MAXIF];
static struct iftot niftot[MAXIF];

void
initif(void)
{
	bzero(ziftot, sizeof(ziftot));
}

void
zeroif(void)
{
	bcopy(iftot, ziftot, sizeof(ziftot));
}

void
sprif(int y, int *y_off_ptr, ns_off_t ifnetaddr)
{
	register struct iftot *nip, *oip;
	int num_ifs;
	struct sockaddr_in *sin;
	register char *cp;
	char *netstr, *addrstr, *tmpcp;
	int n, nif, y_off;
	char netary[64], addrary[64];
	char line[200];
	union if_addr ifaddr;


	klseek(kmem, ifnetaddr, 0);
	read(kmem, (char *)&ifnetaddr, sizeof ifnetaddr);
	nip = niftot;
	for (num_ifs= 0; ifnetaddr != 0 && num_ifs < MAXIF; nip++, num_ifs++) {
		klseek(kmem, ifnetaddr, 0);
		read(kmem, (char *)&ifnet, sizeof ifnet);

		klseek(kmem, (ns_off_t)ifnet.if_name, 0);
		read(kmem, nip->ift_name, IFNAMSIZ);

		nip->ift_name[IFNAMSIZ] = '\0';
		ifnetaddr = (ns_off_t)ifnet.if_next;
		cp = &nip->ift_name[strlen(nip->ift_name)];
		cp = index(nip->ift_name, '\0');
		cp = cp + sprintf(cp, "%d", ifnet.if_unit);
		if ((ifnet.if_flags&IFF_UP) == 0)
			*cp++ = '*';
		*cp = '\0';

		nip->ift_ip = ifnet.if_data.ifi_ipackets;
		nip->ift_ie = ifnet.if_data.ifi_ierrors;
		nip->ift_ib = ifnet.if_data.ifi_ibytes;
		nip->ift_op = ifnet.if_data.ifi_opackets;
		nip->ift_oe = ifnet.if_data.ifi_oerrors;
		nip->ift_ob = ifnet.if_data.ifi_obytes;
		nip->ift_co = ifnet.if_data.ifi_collisions;
		nip->ift_len = ifnet.if_snd.ifq_len;
		nip->ift_drop = ifnet.if_snd.ifq_drops;

		nip->ift_mtu = ifnet.if_data.ifi_mtu;
		nip->ift_flags = ifnet.if_flags;

		nip->ifaddraddr = (ns_off_t)(ifnet.in_ifaddr);
	}

	y_off = ck_y_off(num_ifs*2+3, y, y_off_ptr);
	move_print(&y, &y_off, 1, 0,
		   "%-5s%-5.5s %-16.16s %s",
		   "Name", "Mtu", "Network", "Address");

	for (nif = 0, nip = niftot; nif < num_ifs; nif++, nip++) {
		if (nip->ifaddraddr == 0) {
			netstr = "none";
			addrstr = "none";
		} else {
			klseek(kmem, nip->ifaddraddr, 0);
			read(kmem, &ifaddr, sizeof(ifaddr));

			cp = ((ns_off_t)ifaddr.in.ia_ifa.ifa_addr
			      - nip->ifaddraddr
			      + (char*)&ifaddr);
			sin = (struct sockaddr_in *)cp;

			/* if nothing has changed, do not bother looking
			 * up names or displaying them.
			 */
			if (!Cfirst
			    && nip->ift_old_flags == nip->ift_flags
			    && !bcmp(&ifaddr.in.ia_ifa, &(nip->ift_addr),
				     sizeof(nip->ift_addr))) {
				netstr = 0;

			} else switch (sin->sin_family) {
			case AF_UNSPEC:
			case AF_LINK:
				netstr = "none";
				addrstr = "none";
				break;

			case AF_INET:
				if ((nip->ift_flags & IFF_POINTOPOINT) != 0) {
					/*
					 * compute offset in ifaddr structure
					 * for the destination sockaddr
					 */
				     cp = ((ns_off_t)ifaddr.in.ia_ifa.ifa_dstaddr
					      - nip->ifaddraddr
					      + (char*)&ifaddr);
					sin = (struct sockaddr_in *)cp;
					netstr = "(pt-to-pt)";
				} else {
				    netstr= netname(htonl(ifaddr.in.ia_subnet),
						     ifaddr.in.ia_subnetmask,
						     16);
				}
				addrstr = routename(sin->sin_addr.s_addr,0);
				break;
			case AF_NS:
				{
				struct sockaddr_ns *sns =
					(struct sockaddr_ns *)sin;
				u_long net;
				char netnum[8];

				*(union ns_net *) &net = sns->sns_addr.x_net;
				sprintf(netnum, "%lxH", ntohl(net));
				upHex(netnum);
				netstr = netnum;
				addrstr = ns_phost(sns);
				}
				break;
			case AF_APPLETALK:
				{
				struct sockaddr *sa = (struct sockaddr *)sin;
				u_short *sp = (u_short *)sa->sa_data;
				u_char *cp = (u_char *)&sa->sa_data[2];
				netstr = "atalk: ";
				sprintf(addrstr = addrary,
					"%04x     %02x %12s",
					*sp, *cp, "");
				
				}
				break;
			default:
				sprintf(netary, "af%2d: ", sin->sin_family);
				for (cp = (char*)sin+sizeof(*sin)-1;
				     cp >= ((struct sockaddr*)sin)->sa_data;
				     --cp)
					if (*cp != 0)
						break;
				n = cp - ((struct sockaddr*)sin)->sa_data + 1;
				cp = ((struct sockaddr*)sin)->sa_data;
				tmpcp = addrary;
				if (n <= 7) {
					while (--n)
					    tmpcp += sprintf(tmpcp, "%02d.",
							*cp++ & 0xff);
				} else {
					while (--n)
					    tmpcp += sprintf(tmpcp, "%02d",
							*cp++ & 0xff);
				}
				netstr = netary;
				addrstr = addrary;
				break;
			}
			nip->ift_old_flags = nip->ift_flags;
		}
		move_print(&y, &y_off, 1, 0,
			   netstr == 0 ? "" : "%-5s%-5u %-16.16s %-53.53s",
			   nip->ift_name, nip->ift_mtu,
			   netstr, addrstr);
	}

	move_print(&y, &y_off, 2, 0,
		   "\n%-5s %10s %6s %10s %10s %6s %10s %8s %2s %4s",
		   "Name", "Ipkts", "Ierrs", "Ibytes",
		   "Opkts", "Oerrs", "Obytes",
		   "Coll", "q", "drop");

	oip =  (cmode == DELTA) ? iftot : ziftot;
	nip = niftot;
	for (n = 0; n < num_ifs; n++) {
		sprintf(line,
			"%-5s %10u %6u %10u %10u %6u %10u %8u %2u %4u",
			nip->ift_name,
			nip->ift_ip - oip->ift_ip,
			nip->ift_ie - oip->ift_ie,
			nip->ift_ib - oip->ift_ib,
			nip->ift_op - oip->ift_op,
			nip->ift_oe - oip->ift_oe,
			nip->ift_ob - oip->ift_ob,
			nip->ift_co - oip->ift_co,
			nip->ift_len,
			nip->ift_drop - oip->ift_drop);
		/* do not write past edge of screen,
		 * and indicate clipping */
		if (strlen(line) > 80) {
			line[79] = '~';
			line[80] = '\0';
		}
		move_print(&y, &y_off, 1, 0, line);

		bcopy(nip, &iftot[n], sizeof(iftot[n]));
		oip++;
		nip++;
	}
}
