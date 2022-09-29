#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <sys/param.h>
#include <sys/types.h>
#include <net/if.h>
#include <netinet/in.h>
#include "dhcp.h"
#include "dhcpdefs.h"
#include <strings.h>
#include <syslog.h>

#include <net/raw.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <netinet/udp.h>
#include <sys/fddi.h>

struct  netaddr	nets[IFMAX];	/* List of interfaces and other related data */
struct  ifreq	ifreq[IFMAX];	/* holds interface configuration */
struct  ifconf	ifconf;		/* int. config. ioctl block(points to ifreq) */

extern	iaddr_t		myhostaddr;
extern	struct netaddr	*np_recv;
extern	int 		ifcount;
extern	int 		ProclaimServer;
extern	int 		rsockout;
extern	int		max_addr_alloc;
extern	int		numaddrs;
extern	u_int		*myaddrs;
extern	int		s;

extern	struct ether_addr	*ether_aton(char *);

void	get_aliases(char *);

int	ifcount = 0;		/* the number of interfaces */

int
sr_initialize_nets(void)
{
    int ss, sss;
    register struct ifreq* ifrp;
    register struct netaddr* np;
    struct ifreq ifnetmask;
    int len;
    int first_alias;

    sss = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sss < 0) {
	syslog(LOG_ERR,"Opening DGRAM stream socket (%m)");
	exit(1);
    }

    ss = socket(AF_RAW, SOCK_RAW, 0);
    if (ss < 0) {
	syslog(LOG_ERR, "Opening RAW stream socket (%m)");
	exit(1);
    }


    ifconf.ifc_len = sizeof ifreq;
    ifconf.ifc_req = ifreq;
    if (ioctl(sss, SIOCGIFCONF, (caddr_t)&ifconf) < 0 || ifconf.ifc_len <= 0) {
	syslog(LOG_ERR, "error in ioctl SIOCGIFCONF (%m)");
	exit(1);
    }

    bzero(nets, sizeof nets);
    np = nets;
    ifcount = 0;
    len = ifconf.ifc_len;
    for (ifrp = &ifreq[0]; len > 0; len -= sizeof (struct ifreq), ifrp++) {
	if (SINPTR(&ifrp->ifr_addr)->sin_family == AF_INET) {
	    strncpy(ifnetmask.ifr_name, ifrp->ifr_name,
			sizeof(ifnetmask.ifr_name));
	    if (ioctl(sss, SIOCGIFFLAGS, (caddr_t)&ifnetmask) < 0) {
		syslog(LOG_ERR, "'get interface flags' ioctl failed (%m) ");
		exit(1);
	    }
	    if ((ifnetmask.ifr_flags & (IFF_UP|IFF_LOOPBACK)) != IFF_UP)
		continue;
	    if (ioctl(sss, SIOCGIFNETMASK, (caddr_t)&ifnetmask) < 0) {
		syslog( LOG_ERR, "'get interface netmask' ioctl failed (%m)");
		exit(1);
	    }
	    np->netmask = SINPTR(&ifnetmask.ifr_addr)->sin_addr.s_addr;
	    np->myaddr  = SINPTR(&ifrp->ifr_addr)->sin_addr;
	    np->net = np->netmask & np->myaddr.s_addr;
	    strncpy(np->ifname, ifrp->ifr_name, sizeof(np->ifname));

	    if (ioctl(ss, SIOCGIFADDR, (caddr_t)&ifnetmask) < 0) {
		syslog(LOG_ERR,"Error in ioctl(SIOCGIFADDR) (%m)");
		exit(1);
	    }
	    bcopy(ifnetmask.ifr_addr.sa_data, np->etheraddr, 6);
	    /* This call will add all the addresses associated
	     * with a given interfaceto the list of my
	     * addresses, including all the aliases
	     */
	    if (numaddrs >= max_addr_alloc) {
		max_addr_alloc *= 2;
		myaddrs = (u_int *)realloc((u_int *)myaddrs,
					max_addr_alloc*sizeof(u_int));
	    }
	    myaddrs[numaddrs++] = np->myaddr.s_addr;
	    first_alias = numaddrs;
	    get_aliases(ifrp->ifr_name);
	    if (numaddrs > first_alias) { /* some aliases added */
		np->first_alias = first_alias; /* note 0 is not a valid alias
						* index */
		np->last_alias = numaddrs -1;
	    }
	    np++;
	    ifcount++;
	}
    }
    close (sss);
    close (ss);
    return ifcount;
}

int
sr_initialize_broadcast_sockets(void)
{
    struct sockaddr_raw braw_addr;	/* raw socket addr struct */
    int sockbufsiz;
    int on = 1;
    int i;
    register struct netaddr *np;
    
    /* SEND SOCKET */
    np = nets;
    for (i=0; i < ifcount; i++, np++) {
	if ((np->rsockout = socket(AF_RAW, SOCK_RAW, RAWPROTO_SNOOP)) < 0) {
	    syslog(LOG_ERR, "Cannot create socket for broadcast rpc:(%m)");
	    return 1;
	}
	
	braw_addr.sr_family = AF_RAW;
	strncpy(braw_addr.sr_ifname, np->ifname, sizeof(braw_addr.sr_ifname));
	braw_addr.sr_port = 0;
	
	if(bind(np->rsockout, &braw_addr, sizeof(braw_addr))) {
	    syslog(LOG_ERR, "send bind error(%m)");
	    return 1;
	}
	if (setsockopt(np->rsockout, SOL_SOCKET, SO_BROADCAST,
		       &on, sizeof(on)) < 0) {
	    syslog(LOG_ERR, "Cannot set socket option SO_BROADCAST:(%m)");
	    return 1;
	}
	sockbufsiz = 8096;
	if(setsockopt(np->rsockout, SOL_SOCKET, SO_SNDBUF,
		      (caddr_t)&sockbufsiz, sizeof(sockbufsiz)) < 0) {
	    syslog(LOG_ERR, "Cannot set socket option SO_SNDBUF:(%m)");
	    return 1;
	}
    }
    return 0;
}

/* not_my_address - checks given address against all my address
 * return the net that matches, if does not match with any return 0
 */
struct netaddr*
sr_match_address(u_long addr)
{
    register struct netaddr *np;

    np = nets;
    while (np->myaddr.s_addr) {
	if (addr == np->myaddr.s_addr)
	    return np;
	np++;
    }
    return 0;
}

/*
** Simple routine to calculate the checksum.
*/
static int
in_cksum(u_short *addr, int len)
{
	register u_short *ptr;
	register int sum;
	u_short *lastptr;

	sum = 0;
	ptr = (u_short *)addr;
	lastptr = ptr + (len/2);
	for (; ptr < lastptr; ptr++) {
	    sum += *ptr;
	    if (sum & 0x10000) {
		sum &= 0xffff;
		sum++;
	    }
	}
	return (~sum & 0xffff);
}

int
sr_get_interface_type(char *i_name)
{
    if(!i_name || (*i_name == '\0'))
	return TYPE_UNSUPPORT;
    if( (strncmp(i_name, "ec", 2) == 0) || (strncmp(i_name, "enp", 3) == 0) ||
        (strncmp(i_name, "et", 2) == 0) || (strncmp(i_name, "ep", 2) == 0)  ||
	(strncmp(i_name, "ee", 2) == 0) || (strncmp(i_name, "ef", 2) == 0)  ||
	(strncmp(i_name, "fxp", 3) == 0) || (strncmp(i_name, "gfe", 3) == 0) ||
	(strncmp(i_name, "eg", 2) == 0) || (strncmp(i_name, "vfe", 3) == 0) )
	return TYPE_ETHER;
    if( (strncmp(i_name, "ipg", 3) == 0) || (strncmp(i_name, "xpi", 3) == 0) )
	return TYPE_FDDI;
    return TYPE_UNSUPPORT;
}

int 
sr_create_ip_send_packet(char **bufp, int *bsz, struct bootp *bp, int itype)
{
    char *sbuf;
    int bufsize, cnt;
    int nbytes;
    struct ether_addr *eh;
    u_long val, mltaddr;
    /* not needed
    int bsum, isum, usum, psudpsum;
    */
    int isum;
    u_short *usp;

    struct ether_header	seh;
    struct ip		sip;
    struct udphdr	sudp;
    struct fddi		sfddi;

    if(itype == TYPE_ETHER)
	bufsize = sizeof(struct ether_header) + sizeof(struct ip) +
		  sizeof(struct udphdr) + sizeof(struct bootp);
    else if(itype == TYPE_FDDI)
	bufsize = sizeof(struct fddi) + sizeof(struct ip) +
		  sizeof(struct udphdr) + sizeof(struct bootp);
    else
	return 1;

    cnt = bufsize;
    sbuf = (char *)malloc(bufsize);

    usp = (u_short *)bp;
    nbytes = sizeof(struct bootp);
    /* not needed
    bsum = in_cksum(usp, nbytes);
    */
    cnt -= sizeof(struct bootp);
    bcopy(bp, &sbuf[cnt], sizeof(struct bootp));

    sip.ip_v = IPVERSION;
    sip.ip_hl = 5;
    sip.ip_tos = 0;
    sip.ip_len = sizeof(struct ip) +sizeof(struct udphdr) +sizeof(struct bootp);
    sip.ip_id = 2;	/* FOR NOW: LATER */
    sip.ip_off = 0;
    sip.ip_ttl = IPFRAGTTL;
    sip.ip_p = IPPROTO_UDP;
    sip.ip_sum = 0;
    mltaddr = MULTIHMDHCP ? np_recv->myaddr.s_addr : myhostaddr.s_addr;
    sip.ip_src.s_addr = mltaddr;
    val = inet_addr("255.255.255.255");
    sip.ip_dst.s_addr = val;
    /* calculate sip.ip_sum */
    usp = (u_short *)&sip;
    nbytes = sizeof(struct ip);
    isum = in_cksum(usp, nbytes);
    sip.ip_sum = isum;		/* Only contains the header checksum */

    sudp.uh_sport = htons((u_short)IPPORT_BOOTPS);
    sudp.uh_dport = htons((u_short)IPPORT_BOOTPC);
    sudp.uh_ulen = sizeof(struct udphdr) + sizeof(struct bootp);
    sudp.uh_sum = 0;
    /* calculate sudp.uh_sum */
    /* not needed
    usp = (u_short *)&sudp;
    nbytes = sizeof(struct udphdr);
    usum = in_cksum(usp, nbytes);
    psudpsum = in_cksum((u_short *)&sip.ip_src.s_addr, 4) +
	       in_cksum((u_short *)&sip.ip_dst.s_addr, 4) +
	       in_cksum((u_short *)&sip.ip_p, 1) +
	       in_cksum((u_short *)&sudp.uh_ulen, 2);
    sudp.uh_sum = usum + bsum + psudpsum;
    */
    cnt -= sizeof(struct udphdr);
    bcopy(&sudp, &sbuf[cnt], sizeof(struct udphdr));

    cnt -= sizeof(struct ip);
    bcopy(&sip, &sbuf[cnt], sizeof(struct ip));

    /* Broadcast Address */
    eh = (struct ether_addr *) ether_aton("ff:ff:ff:ff:ff:ff");
    if(itype == TYPE_ETHER) {
	if(MULTIHMDHCP)
	    bcopy(np_recv->etheraddr, seh.ether_shost, sizeof seh.ether_shost);
	else
	    bcopy(nets[0].etheraddr, seh.ether_shost, sizeof seh.ether_shost);
	bcopy(eh, seh.ether_dhost, sizeof seh.ether_dhost);
	seh.ether_type = ETHERTYPE_IP;
	bcopy(&seh, sbuf, sizeof(struct ether_header));
    }
    else if(itype == TYPE_FDDI) {
	bzero(&sfddi, sizeof(sfddi));
	/* The driver will bit swap the ether address */
	if(MULTIHMDHCP)
	    bcopy(np_recv->etheraddr, &sfddi.fddi_mac.mac_sa,
		  sizeof(sfddi.fddi_mac.mac_sa));
	else
	    bcopy(nets[0].etheraddr, &sfddi.fddi_mac.mac_sa,
		  sizeof(sfddi.fddi_mac.mac_sa));
	bcopy(eh, &sfddi.fddi_mac.mac_da, sizeof(sfddi.fddi_mac.mac_da));
	sfddi.fddi_mac.mac_fc = MAC_FC_ALEN|MAC_FC_LLC_FF|1;
	sfddi.fddi_mac.mac_bits = 0;
	sfddi.fddi_llc.llc_c1 = RFC1042_C1;
	sfddi.fddi_llc.llc_c2 = RFC1042_C2;
	sfddi.fddi_llc.llc_etype = htons(ETHERTYPE_IP);
	bcopy(&sfddi, sbuf, sizeof(struct fddi));
    }

    *bsz = bufsize;
    *bufp = sbuf;

    return 0;
}

int
sr_initialize_single_broadcast_socket(void)
{
    struct sockaddr_raw braw_addr;
    int sockbufsiz;
    int on = 1;

    /* SEND SOCKET */

    if ((rsockout = socket(AF_RAW, SOCK_RAW, RAWPROTO_SNOOP)) < 0) {
	syslog(LOG_ERR, "Cannot create socket for broadcast rpc:(%m)");
	return 1;
    }

    braw_addr.sr_family = AF_RAW;
    strncpy(braw_addr.sr_ifname, nets[0].ifname, sizeof(braw_addr.sr_ifname));
    braw_addr.sr_port = 0;

    if(bind(rsockout, &braw_addr, sizeof(braw_addr))) {
	syslog(LOG_ERR, "send bind error(%m)");
	return 1;
    }
    if (setsockopt(rsockout, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) < 0) {
	syslog(LOG_ERR, "Cannot set socket option SO_BROADCAST:(%m)");
	return 1;
    }
    sockbufsiz = 8096;
    if(setsockopt(rsockout, SOL_SOCKET, SO_SNDBUF, (caddr_t)&sockbufsiz,
		  sizeof(sockbufsiz)) < 0) {
	syslog(LOG_ERR, "Cannot set socket option SO_SNDBUF:(%m)");
	return 1;
    }
    return 0;
}

void
get_aliases(char *ifn)
{
    struct ifaliasreq ifalias;
    struct sockaddr_in *sin;

    bzero (&ifalias, sizeof(struct ifaliasreq));
    strncpy(ifalias.ifra_name, ifn, sizeof ifalias.ifra_name); 

    ifalias.ifra_addr.sa_family = AF_INET;
    /*
     * A cookie of zero means to return the primary address first
     * and subsequent calls will return the aliases as the cookie
     * value increase linearly. When a -1 is returned we've exhausted
     * the alias'es for this interface.
     */
    ifalias.cookie = 1;

    for (;;) {
	if ((ioctl(s, SIOCLIFADDR, (caddr_t)&ifalias)) < 0) { 
	    syslog(LOG_ERR, "'get alias addr' ioctl failed (%m)");
	    break;
	}
	if (ifalias.cookie < 0) {
	    break;
	}
	if (numaddrs >= max_addr_alloc) {
	    max_addr_alloc *= 2;
	    myaddrs = (u_int *)realloc((u_int *)myaddrs,
					max_addr_alloc*sizeof(u_int));
	}
	sin = (struct sockaddr_in *)&ifalias.ifra_addr;
	myaddrs[numaddrs++] = sin->sin_addr.s_addr;
    }
}

