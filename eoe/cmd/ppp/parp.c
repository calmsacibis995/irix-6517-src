/* proxy-ARP for PPP
 */

#ident "$Revision: 1.7 $"

#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/raw.h>
#ifndef PPP_IRIX_53
#include <sys/sysctl.h>
#include <net/if_dl.h>
#include <net/route.h>
#endif

#include "ppp.h"

extern char *ether_ntoa(struct ether_addr *);

#define	ETHER_ISGROUP(addr) (*(u_char*)(addr) & 0x01)

#ifndef PPP_IRIX_53
struct rt_addrinfo rtinfo;
#define RTINFO_NETMASK	rtinfo.rti_info[RTAX_NETMASK]
#define RTINFO_IFA	rtinfo.rti_info[RTAX_IFA]

/* disassemble routing message
 */
void
rt_xaddrs(struct sockaddr *sa,
	  struct sockaddr *lim,
	  int addrs)
{
	int i;
#ifdef _HAVE_SA_LEN
	static struct sockaddr sa_zero;
#endif
#define ROUNDUP(a) ((a) > 0 ? (1 + (((a) - 1) | (sizeof(__uint64_t) - 1))) \
		    : sizeof(__uint64_t))


	bzero(rtinfo.rti_info, sizeof(rtinfo.rti_info));
	rtinfo.rti_addrs = addrs;

	for (i = 0; i < RTAX_MAX && sa < lim; i++) {
		if ((addrs & (1 << i)) == 0)
			continue;
#ifdef _HAVE_SA_LEN
		rtinfo.rti_info[i] = (sa->sa_len != 0) ? sa : &sa_zero;
		sa = (struct sockaddr *)((char*)(sa)
					 + ROUNDUP(sa->sa_len));
#else
		rtinfo.rti_info[i] = sa;
		sa = (struct sockaddr *)((char*)(sa)
					 + ROUNDUP(_FAKE_SA_LEN_DST(sa)));
#endif
	}
}


#endif


/* Given an IP address, find a suitable MAC address
 */
char *					/* 0 if bad or interface name */
if_mac_addr(u_long tgt_addr,		/* look for this IP address */
	    char *tgt_if,		/* or this interface name */
	    struct ether_addr *tgt_mac)	/* put MAC address here */
{
	static char cur_name[IFNAMSIZ+1];
	u_int cur_mask = 0;
	u_int new_mask;
#define S_ADDR(x) (((struct sockaddr_in *)(x))->sin_addr.s_addr)
#ifdef PPP_IRIX_53
	int rsoc, dsoc;
	int n;
	struct ifconf ifc;
	struct ifreq ifreq, *ifr;
	union {
		struct ifreq ifreq;
		char c[BUFSIZ];
	} buf;
	struct sockaddr_raw *sr;


	/* look for the target IP address or interface name */
	if ((rsoc = socket(AF_INET, SOCK_RAW, 0)) < 0) {
		log_errno("if_mac_addr socket(SOCK_RAW)","");
		return 0;
	}
	if ((dsoc = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		log_errno("if_mac_addr socket(SOCK_DGRAM)","");
		(void)close(rsoc);
		return 0;
	}
	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf.c;
	if (ioctl(rsoc, SIOCGIFCONF, &ifc) < 0) {
		log_errno("if_mac_addr GIFCONF","");
		(void)close(rsoc);
		(void)close(dsoc);
		return 0;
	}
	ifr = ifc.ifc_req;
	cur_name[0] = '\0';
	for (n = ifc.ifc_len/sizeof(ifreq); n > 0; n--, ifr++) {
		ifreq = *ifr;
		if (ioctl(rsoc, SIOCGIFFLAGS, &ifreq) < 0) {
			log_errno("if_mac_addr GIFFLAGS ", ifr->ifr_name);
			continue;
		}
		if (ifreq.ifr_flags & (IFF_POINTOPOINT | IFF_LOOPBACK))
			continue;
		if ((ifreq.ifr_flags & (IFF_UP | IFF_RUNNING))
		    != (IFF_UP | IFF_RUNNING))
			continue;
		if (ifr->ifr_addr.sa_family != AF_INET)
			continue;

		if (ioctl(dsoc, SIOCGIFNETMASK, &ifreq) < 0) {
			log_errno("if_mac_addr SIOCGIFNETMASK ",
				  ifr->ifr_name);
			continue;
		}
		new_mask = S_ADDR(&ifreq.ifr_addr);

		/* stop on the target interface name */
		if (tgt_if != 0) {
			if (strcmp(ifr->ifr_name, tgt_if))
				continue;
			cur_mask = new_mask;
			(void)strcpy(cur_name, ifr->ifr_name);
			break;
		}

		/* Stop if the first interface is good enough, as when looking
		 * for any MAC address for a PPP Endpoint Discriminator.
		 */
		if (tgt_addr == 0) {
			cur_mask = new_mask;
			(void)strcpy(cur_name, ifr->ifr_name);
			break;
		}

		/* Look for the best fitting interface address and mask,
		 * provided the PPP link is not to a remote network.
		 * If the netmask has been explicitly configured to anything
		 * other than -1, then the link is to a remote network
		 * instead of to a host.
		 */
		if (((ntohl(tgt_addr) & new_mask)
		     == (ntohl(S_ADDR(&ifr->ifr_addr)) & new_mask))
		    && cur_mask < new_mask
		    && (netmask.sin_addr.s_addr == 0
			|| netmask.sin_addr.s_addr == -1)) {
			cur_mask = new_mask;
			(void)strcpy(cur_name, ifr->ifr_name);
		}
	}

	if (cur_name[0] == '\0') {
		(void)close(rsoc);
		(void)close(dsoc);
		return 0;
	}

	bcopy(cur_name,ifreq.ifr_name,sizeof(ifreq.ifr_name));
	if (ioctl(rsoc, SIOCGIFADDR, &ifreq) < 0) {
		log_errno("if_mac_addr SIOCGIFADDR","");
		(void)close(rsoc);
		(void)close(dsoc);
		return 0;
	}
	(void)close(rsoc);
	(void)close(dsoc);

	sr = (struct sockaddr_raw*)&ifreq.ifr_addr;
	if (sr->sr_family != AF_RAW
	    || ETHER_ISGROUP(sr->sr_addr)) {
		log_complain("","if_mac_addr: bad MAC address %s"
			     " for interface %s",
			     ether_ntoa(((struct ether_addr *)sr->sr_addr)),
			     cur_name);
		return 0;
	}

	bcopy(sr->sr_addr, tgt_mac, sizeof(*tgt_mac));
#else
	struct ether_addr *ea;
	char *sysctl_buf;
	size_t needed;
	int mib[6];
	struct if_msghdr *ifm;
	struct ifa_msghdr *ifam, *ifam_lim, *ifam2;
	struct sockaddr_dl *sdl;


	/* fetch the interface list
	 */
	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET;
	mib[4] = NET_RT_IFLIST;
	mib[5] = 0;
	if (sysctl(mib, 6, 0, &needed, 0, 0) < 0) {
		log_errno("","sysctl(NET_RT_IFLIST) 1");
		return 0;
	}
	sysctl_buf = malloc(needed);
	if (sysctl(mib, 6, sysctl_buf,&needed, 0, 0) < 0) {
		log_errno("","sysctl(NET_RT_IFLIST) 2");
		return 0;
	}
	cur_name[0] = '\0';
	ifam_lim = (struct ifa_msghdr *)(sysctl_buf + needed);
	for (ifam = (struct ifa_msghdr *)sysctl_buf;
	     ifam < ifam_lim;
	     ifam = ifam2) {

		ifam2 = (struct ifa_msghdr*)((char*)ifam + ifam->ifam_msglen);

		if (ifam->ifam_type == RTM_IFINFO) {
			ifm = (struct if_msghdr *)ifam;
			sdl = (struct sockaddr_dl *)(ifm + 1);
			continue;
		}

		if (ifam->ifam_type != RTM_NEWADDR) {
			log_complain("","if_mac_addr out of sync");
			return 0;
		}

		if ((ifm->ifm_flags & IFF_POINTOPOINT)
		    || (ifm->ifm_flags & IFF_LOOPBACK)
		    || !(ifm->ifm_flags & IFF_BROADCAST)
		    || (ifm->ifm_flags & IFF_NOARP)
		    || !(ifm->ifm_flags & IFF_UP)
		    || !(ifm->ifm_flags & IFF_RUNNING))
			continue;

		rt_xaddrs((struct sockaddr *)(ifam+1),
			  (struct sockaddr *)ifam2,
			  ifam->ifam_addrs);
		if (RTINFO_IFA == 0
		    || RTINFO_IFA->sa_family != AF_INET)
			continue;

		new_mask = ntohl(S_ADDR(RTINFO_NETMASK));

		/* stop on the target interface name */
		if (tgt_if != 0) {
			if (strncmp(sdl->sdl_data, tgt_if, sdl->sdl_nlen))
				continue;
			cur_mask = new_mask;
			strncpy(cur_name, sdl->sdl_data,
				MIN(sizeof(cur_name)-1, sdl->sdl_nlen));
			ea = (struct ether_addr *
			      )&sdl->sdl_data[sdl->sdl_nlen];
			break;
		}

		/* Stop if the first interface is good enough, as when looking
		 * for any MAC address for a PPP Endpoint Discriminator.
		 */
		if (tgt_addr == 0) {
			cur_mask = new_mask;
			strncpy(cur_name, sdl->sdl_data,
				MIN(sizeof(cur_name)-1, sdl->sdl_nlen));
			ea = (struct ether_addr *
			      )&sdl->sdl_data[sdl->sdl_nlen];
			break;
		}

		/* look for the best fitting interface address and mask,
		 * provided the PPP link is not to a remote network.
		 * If the netmask has been explicitly configured to anything
		 * other than -1, then the link is to a remote network
		 * instead of to a host.
		 */
		if (((ntohl(tgt_addr) & new_mask)
		     == (ntohl(S_ADDR(RTINFO_IFA)) & new_mask))
		    && cur_mask < new_mask
		    && (netmask.sin_addr.s_addr == 0
			|| netmask.sin_addr.s_addr == -1)) {
			cur_mask = new_mask;
			strncpy(cur_name, sdl->sdl_data,
				MIN(sizeof(cur_name)-1, sdl->sdl_nlen));
			ea = (struct ether_addr *
			      )&sdl->sdl_data[sdl->sdl_nlen];
		}
	}

	if (cur_name[0] == '\0')
		return 0;
	if (ETHER_ISGROUP(ea)) {
		log_complain("","if_mac_addr: bad MAC address %s"
			     " for interface %s",
			     ether_ntoa(((struct ether_addr *)ea)),
			     cur_name);
		return 0;
	}
	bcopy(ea, tgt_mac, sizeof(*tgt_mac));
#endif
	return cur_name;
}


#define ARP_COM "exec 1>&2; HOSTRESORDER=local export HOSTRESORDER; "
#define ARP_DEL " arp -d %s"
#if defined(PPP_IRIX_53) || defined(PPP_IRIX_62) || defined(PPP_IRIX_63) || defined(PPP_IRIX_64)
#define ARP_ADD " arp -s %s %s pub"
#else
#define ARP_ADD " arp -s %s %s proxy-only"
#endif

/* try to add proxy-ARP table entry
 */
void
proxy_arp(void)
{
	char *ifname;
	struct ether_addr eaddr;
	char eaddr_a[sizeof("xx:xx:xx:xx:xx:xx:xx")+2];
	char arp_cmd[sizeof(ARP_COM ARP_ADD
			    "111.222.333.444 xx:xx:xx:xx:xx:xx:xx")];
	int st;

	ifname = if_mac_addr(remhost.sin_addr.s_addr, proxy_arp_if, &eaddr);
	if (ifname == 0) {
		/* Failed to find a usable MAC address.
		 *
		 * No problem if running on automatic,
		 * but complain if the specified interface was wrong.
		 */
		if (conf_proxy_arp > 0) {
			if (proxy_arp_if)
				log_complain("",
					     "interface %s not"
					     " suitable for proxy-ARP",
					     proxy_arp_if);
			else
				log_complain("",
					     "no interface"
					     " suitable for proxy-ARP");
		}
		conf_proxy_arp = 0;
		return;
	}

	/* Since there is a suitable local interface, the default value
	 * of the netmask is -1.
	 */
	if (netmask.sin_addr.s_addr == 0)
		netmask.sin_addr.s_addr = -1;

	if (!conf_proxy_arp)
		return;

	(void)strcpy(eaddr_a, ether_ntoa(&eaddr));

	log_debug(1,"","add proxy-ARP table entry at %s for %s via %s",
		  eaddr_a, remhost_str, ifname);

	kludge_stderr();
	sprintf(arp_cmd, ARP_COM ARP_ADD, remhost_str, eaddr_a);
	st = system(arp_cmd);
	if (st < 0) {
		log_errno("proxy-", arp_cmd+sizeof(ARP_COM));
		conf_proxy_arp = 0;
	} else if ((st = WEXITSTATUS(st)) != 0) {
			log_complain("", "`"ARP_ADD"` exit value=%d",
				     remhost_str, eaddr_a, st);
			conf_proxy_arp = 0;
	} else {
		conf_proxy_arp = 2;
	}
}


/* remove proxy-ARP entry
 */
void
proxy_unarp(void)
{
	char arp_cmd[sizeof(ARP_COM ARP_ADD)+15+2];
	int st;

	if (conf_proxy_arp > 1) {
		log_debug(1,"","delete ARP table entry for %s", remhost_str);

		kludge_stderr();
		sprintf(arp_cmd, ARP_COM ARP_DEL, remhost_str);
		st = system(arp_cmd);
		if (st < 0) {
			log_errno("proxy-", arp_cmd+sizeof(ARP_COM));
		} else if ((st = WEXITSTATUS(st)) != 0) {
			log_complain("", "`"ARP_DEL"` exit value=%d",
				     inet_ntoa(remhost.sin_addr), st);
		}
	}
}
