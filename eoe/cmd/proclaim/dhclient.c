/*
   debugLevel = 0 -- normal operation
   debugLevel = 1 -- does not shut the network
   debugLevel > 1 -- does not start or shut the network
   */
/*
  An additional debugging option was added to allow a single host to simulate
  multiple MAC addresses. This option is the -M etherAddr:[IPaddr] option
  and can be used in conjunction with the "test-proclaim" perl script
  to test the server in a more thorough manner.
  With this option it is possible to make the dhcp server give out
  IP address leases for fictitious MAC addresses
  The variable debugmode is set when the -M option is given. debug_etheraddr
  stores the MAC address and InterfaceAddr stores the fictitious IP address
  if it was already assigned
  */
#include <stdio.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <net/soioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <net/raw.h>
#include <netinet/in_systm.h>
#include <netinet/if_ether.h>
#include <sys/fddi.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <time.h>
#include "dhcp.h"
#include "dhcp_common.h"

typedef struct sockaddr_in SCIN;
typedef struct sockaddr SCAD;

typedef struct {
    struct fddi f_fddi;
    struct ip   f_ip;
} fddiFilter;

#define DEFAULT_LEASE	0xfffffffe
#define USER_INPUT_TMOUT	120
#define DHCPPKTSZ		300     /* minimum size of the bootp pkt */

#define MAXADDRS		20
#define MAX_BROADCAST_SIZE	1400
#define MAX_INTERVAL            6 /* when inter-transmission delay becomes
				      more than this value we abort */
#define MIN_INTERVAL		2 /* first time wait before retransmit */
#define MAX_NEW_DELAY		5 /* maximum wait for new machine before
				     giving up */
#define INCR_INTERVAL		2 /* the increment of the wait */
#define NEW_INTERVAL		3 /* new machine interval */
#define DEFAULT_HNAME           "192.0.2.1"

#define DEFAULT_INVOCATIONS	2 /* number of invocations before setting
				   * autoconfig_ipaddress off */
char	VERS[] = "SGI dhcp client V2.0.2";
static int is_daemon = 0;
static int bootp_xid;
static int start_nis = 0;
static int bootp1533only = 0;

static char NFSOPT[] = "/etc/havenfs";
char * DHCP_CLIENT = "autoconfig_ipaddress";
char * useConfigDir  = "/etc/config";
char * clientInfoDir = "/var/adm";
char * CLIENT_CACHE = "proclaim.lease_info";
char * CLIENT_DATA = "proclaim.data";
char * CLIENT_CACHE_ASCII = "proclaim.lease_info_ascii";
char * CLIENT_OPTIONS = "proclaim.options";
int debugLevel = 0;
int debugmode = 0;		/* this is for debugging only */
static char  debug_etheraddr[8];

static struct ifconf ifc;
static struct ifreq ifr;
static struct ifreq ifreq;
static fd_set readfds;
static char interface[IFNAMSIZ];

int	isPrimary = 1;		/* configuration on primary interface */
char	*InterfaceName = (char *)0;		/* ec0, xpi0, .... */
char	*InterfaceHostName = (char *)0;
u_long	InterfaceAddr;		/* IP Address */
int	InterfaceType;		/* Ether, FDDI, ... */
int	InterfaceNum;		/* 0: primary, .... */
char	*cl_hostname = (char *)0;
u_long	cl_hostid;
u_long	deflt_hostid;
int	interactive = 1;	/* hack for use with CGI scripts */
int	datamode = 0;		/* flag to write CLIENT_DATA file */
char    ClientID[100];
int     ClientID_len;
char	reqHostname[MAXHOSTNAMELEN]; /* requested with -H option */
char    *client_fqdn;	/* use with CLIENT_FQDN */
static char	clientFQDN[512];
int	client_fqdn_flag = 1;	/* server updates default */
int     client_fqdn_option = 0;
char    vendor_class_str[64];

static char  cl_etheraddr[8], bsw_etheraddr[8];
static int max_delay_xmit = MAX_INTERVAL;
static int min_first_delay_xmit = MIN_INTERVAL;
static int xmit_interval = INCR_INTERVAL;


extern int dh0_decode_and_write_ascii_cache(FILE*, time_t, struct bootp*);
extern void bitswapcopy(u_char *, u_char *, int);
/* extern int dh0_encode_dhcp_client_packet(u_char (*)[], int, int, u_long,
					 u_int, char *, int, u_char, u_long,
					 char *, char *, int, char*);
*/
void daemon_init();
static int free_attrib(char *, char *, char *, char *, char *);


#define INVOCATIONS	"Invocations"
#define MAXTIMEOUT	"MaxTimeout"
#define SERVERADDRESS	"ServerAddress"
#define SHUTDOWNONEXPIRY "ShutdownOnExpiry"
#define LEASE		"Lease"
#define DHCPOPTIONS	"DHCPoptionsToGet"
#define VENDORTAG	"VendorClass"

#define TYPE_UNSUPPORT	0
#define TYPE_ETHER	1
#define TYPE_FDDI	2

static char addl_options_to_get[128]; /* these are options specified in the
				       * options file
				       */
static int addl_options_len = 0;
static int u_inp_tmout = 0;

static int
setInterfaceType()
{
    char *p = InterfaceName;

    if( (strncmp(p, "ec", 2) == 0) || (strncmp(p, "enp", 3) == 0) ||
	(strncmp(p, "et", 2) == 0) || (strncmp(p, "ep", 2) == 0)  ||
	(strncmp(p, "ee", 2) == 0) || (strncmp(p, "ef", 2) == 0)  ||
	(strncmp(p, "fxp", 3) == 0) || (strncmp(p, "vfe", 3) == 0) ||
	(strncmp(p, "gfe", 3) == 0) || (strncmp(p, "eg", 2) == 0) )
	InterfaceType = TYPE_ETHER;
    else if( (strncmp(p, "ipg", 3) == 0) || (strncmp(p, "xpi", 3) == 0) ||
	     (strncmp(p, "rns", 3) == 0) )
	InterfaceType = TYPE_FDDI;
    else
	InterfaceType = TYPE_UNSUPPORT;
    return 0;
}

static char *
get_ether_addr(char *eaddr, int get_ether)
{
    int ss;
    struct ifconf ifc;
    struct ifreq  ifr, *fir;
    char *buf;
    char xbuf[MAX_BROADCAST_SIZE];
    int  i;
    SCIN *sin1;
    struct hostent *hp;

    ss = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (ss < 0) {
	syslog(LOG_ERR,"Opening DGRAM stream socket(%m)");
	return (char *)0;
    }
    bzero(&ifc, sizeof(ifc));
    bzero(&ifr, sizeof(ifr));
    bzero(&fir, sizeof(fir));
    bzero(xbuf, sizeof(xbuf));
    ifc.ifc_len = MAX_BROADCAST_SIZE;
    ifc.ifc_buf = xbuf;
    if(ioctl(ss, SIOCGIFCONF, (char *)&ifc) < 0) {
	syslog(LOG_ERR,"Error in ioctl(SIOCGIFCONF) (%m)");
	return (char *)0;
    }
    close(ss);

    i = 0;
    fir = &ifc.ifc_req[i];
    while(strcmp(fir->ifr_name, "") ) {
	sin1 = (SCIN *)&fir->ifr_addr;
	if(sin1->sin_addr.s_addr == cl_hostid)
	    break;
	fir = &ifc.ifc_req[++i];
    }
    strncpy(interface, fir->ifr_name, IFNAMSIZ);
    if(strcmp(fir->ifr_name, "") == 0)
	return (char *)0;
    if(InterfaceName && strcmp(InterfaceName, interface)) {
	isPrimary = 0;
	i = 0;
	fir = &ifc.ifc_req[i];
	while(strcmp(fir->ifr_name, "")) {
	    if(strcmp(fir->ifr_name, InterfaceName) == 0) {
		InterfaceNum = i;
		break;
	    }
	    fir = &ifc.ifc_req[++i];
	}

	bzero(&ifr, sizeof(ifr));
	strncpy(ifr.ifr_name, InterfaceName, IFNAMSIZ);
	ss = socket(AF_INET, SOCK_DGRAM, 0);
	if(ss < 0) {
	    syslog(LOG_ERR,"Opening DGRAM stream socket(%m)");
	    return (char *)0;
	}
	if (ioctl(ss, SIOCGIFADDR, (caddr_t)&ifr) < 0) {
	    syslog(LOG_ERR,"Error in ioctl(SIOCGIFADDR) for %s (%m)",
		   InterfaceName);
	    return (char *)0;
	}
	sin1 = (SCIN *)&ifr.ifr_addr;
	if (InterfaceAddr == 0)
	    InterfaceAddr = sin1->sin_addr.s_addr;
	close(ss);
	hp = gethostbyaddr(&InterfaceAddr, sizeof(InterfaceAddr), AF_INET);
	if(InterfaceHostName)
	    free(InterfaceHostName);
	if(hp)
	    InterfaceHostName = strdup(hp->h_name);
	else
	    InterfaceHostName = strdup(cl_hostname);
    }
    else {
	if(InterfaceName)
	    free(InterfaceName);
	InterfaceName = strdup(interface);
	if (InterfaceAddr == 0)
	    InterfaceAddr = cl_hostid;
	if(InterfaceHostName)
	    free(InterfaceHostName);
	InterfaceHostName = strdup(cl_hostname);
	InterfaceNum = 1;
    }

    if(get_ether == 0)
	return (char *)0;

    ss = socket(AF_RAW, SOCK_RAW, 0);
    if (ss < 0) {
	syslog(LOG_ERR,"Opening RAW stream socket(%m)");
	return (char *)0;
    }

    strncpy(ifr.ifr_name, InterfaceName, IFNAMSIZ);

    ifr.ifr_addr.sa_family = AF_RAW;
    if (ioctl(ss, SIOCGIFADDR, (caddr_t)&ifr) < 0) {
	syslog(LOG_ERR,"Error in ioctl(SIOCGIFADDR) (%m)");
	return (char *)0;
    }
    close(ss);
    bcopy(ifr.ifr_addr.sa_data, eaddr, 6);
    bitswapcopy((u_char *)eaddr, (u_char *)bsw_etheraddr, 6);	/* for fddi */
    return (eaddr);
}

static int
set_interface_state(int state)
{
    int af = AF_INET;
    struct ifreq gifr;
    int ss;
    int if_flags;

    /* if state = 1 means put interface up.
       return value of 0: interface was already up, left unchanged,
       value of 1: interface was down, flagged up.
       if state = 0 means put interface down.
       return value of 1: successful.
    */

    bzero(&gifr, sizeof(gifr));
    strncpy(gifr.ifr_name, InterfaceName, IFNAMSIZ);
    ss = socket(af, SOCK_DGRAM, 0);
    if(ss < 0) {
	syslog(LOG_ERR, "error in opening socket (%m)");
	return(-1);
    }

    if(ioctl(ss, SIOCGIFFLAGS, (caddr_t)&gifr) < 0) {
	syslog(LOG_ERR, "ioctl (SIOCGIFFLAGS) (%m)" );
	return(-1);
    }
    if_flags = gifr.ifr_flags;
    if(state == 0) {		/* down */
	if_flags &= ~IFF_UP;
	gifr.ifr_flags = if_flags;
    }
    else {
	if( (if_flags & IFF_UP) == 0) {
	    if_flags |= IFF_UP;		/* Up */
	    gifr.ifr_flags = if_flags;
	}
	else
	    return 0;
    }
    if (ioctl(ss, SIOCSIFFLAGS, (caddr_t)&gifr) < 0) {
	syslog(LOG_ERR, "ioctl (SIOCSIFFLAGS)" );
	return(-1);
    }
    close(ss);
    return 1;
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

/*
 * Make up a sort-of-random number.
 *
 * Note that this is not a general purpose routine, but is geared
 * toward the specific application of generating an integer in the
 * range of 1024 to 65k.
 */
static int
getrand(int lb, int ub)
{
    long secs;
    register int rand;

    /*
     * Read the real time clock which returns the number of
     * seconds from some epoch.
     */
    secs = time(0);

    /*
     * Combine different bytes of the word in various ways
     */
    rand  = (secs & 0xff00) >> 8;
    rand *= (secs & 0xff0000) >> 16;
    rand += (secs & 0xff);
    rand *= (secs & 0xff000000) >> 24;
    rand >>= (secs & 7);

    /*
     * Now fit the range
     */
    ub -= lb;
    rand = rand % ub;
    rand += lb;

    return (rand);
}

/*
 * Parameterized broadcast timeout backoff.
 */
void
firsttime(struct timeval *tv)
{
    tv->tv_sec = min_first_delay_xmit;
    tv->tv_usec = 0;
}

int
nexttime(struct timeval *tv, int rflag)
{
    if(rflag == RELEASE_LEASE)	/* Release */
	return 0;
	
    tv->tv_sec += xmit_interval;
    return tv->tv_sec <= max_delay_xmit;
}

/*
 * Format a bootp message for transmission
 */
static int
mk_bootp_msg(register struct bootp *bp, u_long useid)
{
    bzero(bp, sizeof *bp);
    bp->bp_op = BOOTREQUEST;
    bp->bp_htype = ARPHRD_ETHER;
    bp->bp_hlen = sizeof (struct ether_addr);
    bp->bp_hops = 0;
    /* Select a new bootp transaction id  */
    bootp_xid = getrand(1024, 65535);
    bp->bp_xid = bootp_xid;
    bp->bp_ciaddr.s_addr = useid;
    bp->bp_flags = 0;	/* The server does not need to broadcast the reply */
    if (debugmode)
	bp->bp_flags = 0x8000;
    if (debugmode) {
	bcopy(debug_etheraddr, bp->bp_chaddr, sizeof(struct ether_addr));
    }
    else {
	if(InterfaceType == TYPE_FDDI)
	    bcopy(bsw_etheraddr, bp->bp_chaddr, sizeof(struct ether_addr));
	else
	    bcopy(cl_etheraddr, bp->bp_chaddr, sizeof(struct ether_addr));
    }
    bp->dh_magic = VM_DHCP;
    return(0);
}

/*
 * Verify a received message is valid dhcp reply
 */
static int
dhcp_ok(register char *bb, register struct bootp *rbp, int count, int ucast)
{
    struct ip ipp;
    struct udphdr uudp;
    struct ether_header eeh;
    struct ether_addr *eh;
    struct fddi	fdd;
    int msgtype;

    if (!ucast) {		/* ucast packets are just bootp */
	if(InterfaceType == TYPE_ETHER) {
	    bb += RAW_BUFPAD(sizeof eeh);
	    count -= RAW_BUFPAD(sizeof eeh);

	    bcopy(bb, &eeh, sizeof(eeh));
	    if(eeh.ether_type != ETHERTYPE_IP)
		return(1);
	    eh = (struct ether_addr *) ether_aton("ff:ff:ff:ff:ff:ff");
	    if( (bcmp(eeh.ether_dhost, cl_etheraddr,
			sizeof(struct ether_addr)) != 0) &&
		(bcmp(eeh.ether_dhost, eh, sizeof(struct ether_addr)) != 0) )
		return(2);

	    bb += sizeof(struct ether_header);
	}
	else if(InterfaceType == TYPE_FDDI) {
	    bb += RAW_BUFPAD(sizeof fdd);
	    count -= RAW_BUFPAD(sizeof fdd);

	    bcopy(bb, &fdd, sizeof(fdd));
	    if(fdd.fddi_llc.llc_etype != ETHERTYPE_IP)
		return(3);
	    eh = (struct ether_addr *) ether_aton("ff:ff:ff:ff:ff:ff");
	    if( (bcmp(&fdd.fddi_mac.mac_da, cl_etheraddr,
			sizeof(struct ether_addr)) != 0) &&
		(bcmp(&fdd.fddi_mac.mac_da, eh,
		      sizeof(struct ether_addr)) != 0) )
		return(4);

	    bb += sizeof(struct fddi);
	}
	bcopy(bb, &ipp, sizeof(ipp));
	if(ipp.ip_p != IPPROTO_UDP)
	    return(5);
	bb += sizeof(struct ip);
	bcopy(bb, &uudp, sizeof(uudp));
	if(uudp.uh_dport != htons((u_short)IPPORT_BOOTPC))
	    return(6);
	bb += sizeof(struct udphdr);
    }
    bcopy(bb, rbp, sizeof(struct bootp));

    if (rbp->bp_op != BOOTREPLY) {
	return (7);
    }
    if (rbp->bp_xid != bootp_xid) {
	return (8);
    }
    if (debugmode) {
	if (bcmp(rbp->bp_chaddr, debug_etheraddr,
		 sizeof (struct ether_addr)) != 0)
	    return (9);
    }
    else {
	if(InterfaceType == TYPE_FDDI) {
	    if (bcmp(rbp->bp_chaddr, bsw_etheraddr,
		     sizeof (struct ether_addr)) != 0)
	    return (10);
	}
	else {
	    if (bcmp(rbp->bp_chaddr, cl_etheraddr,
		     sizeof (struct ether_addr)) != 0)
		return (11);
	}
    }
    
    if (rbp->dh_magic != VM_DHCP ) {
	return(12);
    }

    if (bootp1533only) {
	return(0);
    }

    msgtype = dh0_get_dhcp_msg_type(rbp->dh_opts);
    if( (msgtype == DHCPDISCOVER) || (msgtype == DHCPREQUEST) ||
	(msgtype == DHCPDECLINE) || (msgtype == DHCPRELEASE) ||
	(msgtype == DHCPINFORM) || (msgtype == DHCPPOLL) )
	return(13);
    return(0);
}
void
timerdiff(struct timeval *ta, struct timeval *tb, struct timeval *tc)
{
    int tmp;

    tmp = (ta->tv_sec-tb->tv_sec)*1000000+
	(ta->tv_usec-tb->tv_usec);
    tc->tv_usec = tmp%1000000;
    tc->tv_sec = tmp/1000000;
}

int
do_unicast_dhcp(struct bootp *bp, struct bootp *rp, int rflag, u_long unicast_to)
{
    int sock, sock1, fromlen;
    struct timeval t, tbegin, tnow, tprev;
    SCIN baddr, raddr;
    int raddrlen;
    int ret;
    int on = 1;
    int found = 0;
    int sockbufsiz;
    int packetsiz;
    char *p;

    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
	syslog(LOG_ERR, "Cannot create socket for unicast rpc (%m)");
	return(PROCLAIM_SOCK_ERR);
    }
    bzero(&baddr, sizeof (baddr));
    baddr.sin_family = AF_INET;
    baddr.sin_port = htons((u_short)IPPORT_BOOTPS);
    baddr.sin_addr.s_addr = unicast_to;

    if ((sock1 = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
	syslog(LOG_ERR, "Cannot create socket for broadcast rpc(%m)");
	return(PROCLAIM_SOCK_ERR);
    }
    sockbufsiz = 8096;
    if(setsockopt(sock1, SOL_SOCKET, SO_RCVBUF, (caddr_t)&sockbufsiz,
		  sizeof(sockbufsiz)) < 0) {
	syslog(LOG_ERR, "Cannot set socket option SO_RCVBUF(%m)");
	return(PROCLAIM_SOCKOPT_ERR);
    }
    bzero(&raddr, sizeof (raddr));
    raddr.sin_family = AF_INET;
    raddr.sin_port = htons((u_short)IPPORT_BOOTPC);
    if(bind(sock1, &raddr, sizeof(raddr))) {
	syslog(LOG_ERR, "bind error(%m)");
	return(PROCLAIM_BIND_ERR);
    }
    if(InterfaceType == TYPE_ETHER)
	packetsiz = sizeof(struct bootp) + sizeof(struct ether_header) +
		    sizeof(struct ip) + sizeof(struct udphdr) +
		    RAW_BUFPAD(sizeof(struct ether_header));
    else if(InterfaceType == TYPE_FDDI)
	packetsiz = sizeof(struct bootp) + sizeof(struct fddi) +
		    sizeof(struct ip) + sizeof(struct udphdr);
    else
	return(PROCLAIM_UNSUPPORTED_IF);

    if(packetsiz % sizeof(long))
	packetsiz += sizeof(long) - (packetsiz % sizeof(long));
    p = (char *) malloc(packetsiz);

    firsttime(&t);
    tprev = t;
    do {
	if ( (ret = sendto(sock, bp, sizeof(struct bootp), 0, &baddr,
			sizeof (SCIN))) != sizeof(struct bootp)) {
		syslog(LOG_ERR,"Cannot send packet: sendto(%m)");
		exit(1);
	}
	if (gettimeofday(&tbegin) == -1)
	    syslog(LOG_ERR, "gettimeofday  error: (%m)");
	t = tprev;
    u_select_label:
	FD_ZERO(&readfds);
	FD_SET(sock1, &readfds);
	switch(select(sock1+1, &readfds, (fd_set *)NULL, (fd_set *)NULL, &t)) {
	    case 0:
		continue;
	    case -1:
		if(errno == EINTR)
		    continue;
		syslog(LOG_ERR, "select (%m)");
		break;
	    default:
		raddrlen = sizeof(raddr);
		fromlen = recvfrom(sock1, p, packetsiz, 0, &raddr, &raddrlen);
		if (fromlen < 0) {
		    syslog(LOG_ERR, "recvfrom (%m)");
		    exit(1);
		}
		if(fromlen < DHCPPKTSZ)
		    continue;
		if((ret = dhcp_ok(p, rp, fromlen,1)) == 0) { 
		    ++found;
		    break;
		} else if (debugLevel > 2) {
		    syslog(LOG_DEBUG, "1st dhcp_ok returned %d, (%m)", ret);
		}
	}
	if(found) {
	    break;
	}
	if (gettimeofday(&tnow) == -1)
	    syslog(LOG_ERR, "gettimeofday error: (%m)");
	timerdiff(&tnow, &tbegin, &tnow);
	if (timercmp(&tnow, &t, >))
	    continue;
	else
	    timerdiff(&t, &tnow, &t);
	goto u_select_label;
    } while (nexttime(&tprev, rflag));

    close(sock);
    close(sock1);

    free(p);

    if(!found) {
        if (rflag == POLL_SERVER)
	  return(3);
	else
	  return(1);
    }
    return(ret);
}

static int 
create_ip_send_packet(char **bufp, int *bsz, struct bootp *bp)
{
    char *sbuf;
    int bufsize, i, cnt;
    int nbytes;
    struct ether_addr *eh;
    u_long val;
    int bsum, isum, usum, psudpsum;
    u_short *usp;

    struct ether_header	seh;
    struct ip		sip;
    struct udphdr	sudp;
    struct fddi		sfddi;

    if(InterfaceType == TYPE_ETHER)
    	bufsize = sizeof(struct ether_header) + sizeof(struct ip) +
		  sizeof(struct udphdr) + sizeof(struct bootp);
    else if(InterfaceType == TYPE_FDDI)
	bufsize = sizeof(struct fddi) + sizeof(struct ip) +
		  sizeof(struct udphdr) + sizeof(struct bootp);
    else	/* Should never get to this case */
	return 1;

    cnt = bufsize;
    sbuf = (char *)malloc(bufsize);

    usp = (u_short *)bp;
    nbytes = sizeof(struct bootp);
    bsum = in_cksum(usp, nbytes);
    cnt -= sizeof(struct bootp);
    bcopy(bp, &sbuf[cnt], sizeof(struct bootp));

    sip.ip_v = IPVERSION;
    sip.ip_hl = 5;
    sip.ip_tos = 0;
    sip.ip_len = sizeof(struct ip) + sizeof(struct udphdr) + sizeof(struct bootp);
    sip.ip_id = 1;	/* FOR NOW: LATER */
    sip.ip_off = 0;
    sip.ip_ttl = IPFRAGTTL;
    sip.ip_p = IPPROTO_UDP;
    sip.ip_sum = 0;
    val = inet_addr("0.0.0.0");
    sip.ip_src.s_addr = val;
    val = inet_addr("255.255.255.255");
    sip.ip_dst.s_addr = val;
    /* calculate sip.ip_sum */
    usp = (u_short *)&sip;
    nbytes = sizeof(struct ip);
    isum = in_cksum(usp, nbytes);
    sip.ip_sum = isum;		/* Only contains the header checksum */

    sudp.uh_sport = htons((u_short)IPPORT_BOOTPC);
    sudp.uh_dport = htons((u_short)IPPORT_BOOTPS);
    sudp.uh_ulen = sizeof(struct udphdr) + sizeof(struct bootp);
    sudp.uh_sum = 0;
    /* calculate sudp.uh_sum */
    /*
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

    if(InterfaceType == TYPE_ETHER) {
	bcopy(cl_etheraddr, seh.ether_shost, sizeof seh.ether_shost);
	/* Broadcast Addr */
	eh = (struct ether_addr *) ether_aton("ff:ff:ff:ff:ff:ff");
	bcopy(eh, seh.ether_dhost, sizeof seh.ether_dhost);
	seh.ether_type = ETHERTYPE_IP;
	bcopy(&seh, sbuf, sizeof(struct ether_header));
    }
    else if(InterfaceType == TYPE_FDDI) {
	bzero(&sfddi, sizeof(sfddi));
	/* The driver will bit swap this */
	bcopy(cl_etheraddr, &sfddi.fddi_mac.mac_sa,
	      sizeof(sfddi.fddi_mac.mac_sa));
	/* Broadcast Addr */
	eh = (struct ether_addr *) ether_aton("ff:ff:ff:ff:ff:ff");
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
do_dhcp(struct bootp *bp, struct bootp *rp, int rflag, u_long unicast_to)
{
    struct in_addr addrs[MAXADDRS];
    char *bufp;
    SCIN baddr;
    struct sockaddr_raw braw_addr, rraw_addr;	/* broadcast and receive addresses */
    struct snoopfilter sf, sfb;
    int rsockout, cnt, fromlen, rsockin;
    int raddrlen;
    struct timeval t, tbegin, tnow, tprev;
    int found = 0;
    int ret;
    int on = 1;
    int sockbufsiz;
    int packetsiz, buflen;
    char *p;

    int	val;
    struct ether_addr *eh;
    int interface_was_dn = 0;
    u_char cudp = IPPROTO_UDP;

    if( (rflag == RENEW_LEASE) || (rflag == RELEASE_LEASE) || 
	(rflag == POLL_SERVER) ) {
	ret = do_unicast_dhcp(bp, rp, rflag, unicast_to);
	return(ret);
    }

    ret = set_interface_state(INTERFACE_UP);
    if(ret == -1)
	return(PROCLAIM_SOCK_ERR);
    if(ret == 1)
	interface_was_dn = 1;

    /*
     * initialization: create a socket, a broadcast address, and
     * preserialize the arguments into a send buffer.
     */
    /* RECEIVE SOCKET */

    if ((rsockin = socket(AF_RAW, SOCK_RAW, RAWPROTO_SNOOP)) < 0) {
	syslog(LOG_ERR, "Cannot create raw socket for broadcast (%m)");
	return(PROCLAIM_SOCK_ERR);
    }
    rraw_addr.sr_family = AF_RAW;
    strncpy(rraw_addr.sr_ifname, InterfaceName, sizeof(rraw_addr.sr_ifname));
    rraw_addr.sr_port = 0;

    if(bind(rsockin, &rraw_addr, sizeof(rraw_addr))) {
	syslog(LOG_ERR, "recieve bind error(%m)");
	if(interface_was_dn)
	    set_interface_state(INTERFACE_DOWN);
	return(PROCLAIM_BIND_ERR);
    }
    bzero(&sf, sizeof(sf));
    if(InterfaceType == TYPE_FDDI) {
	/* Filter against the un bit swapped address as the driver would
	 * have bit swapped it back.
	 */
	bcopy(cl_etheraddr,
	      &RAW_HDR(sf.sf_match, fddiFilter)->f_fddi.fddi_mac.mac_da, 6);
	memset(&RAW_HDR(sf.sf_mask, fddiFilter)->f_fddi.fddi_mac.mac_da,
	       0xff, 6);
	bcopy(&cudp, &RAW_HDR(sf.sf_match, fddiFilter)->f_ip.ip_p, 1);
	memset(&RAW_HDR(sf.sf_mask, fddiFilter)->f_ip.ip_p, 0xff, 1);
    }
    else {
	bcopy(cl_etheraddr,
		RAW_HDR(sf.sf_match, struct ether_header)->ether_dhost, 6);
	memset(RAW_HDR(sf.sf_mask, struct ether_header)->ether_dhost, 0xff, 6);
    }
    if(ioctl(rsockin, SIOCADDSNOOP, &sf) < 0) {
	syslog(LOG_ERR,"Error in ioctl(SIOCADDSNOOP) (%m)");
	if(interface_was_dn)
	    set_interface_state(INTERFACE_DOWN);
	return(PROCLAIM_SNOOP_ERR);
    }
    bzero(&sfb, sizeof(sfb));
    if(InterfaceType == TYPE_FDDI) {
	eh = (struct ether_addr *) ether_aton("ff:ff:ff:ff:ff:ff");
	bcopy(eh,
	      &RAW_HDR(sfb.sf_match, fddiFilter)->f_fddi.fddi_mac.mac_da, 6);
	memset(&RAW_HDR(sfb.sf_mask, fddiFilter)->f_fddi.fddi_mac.mac_da,
	       0xff, 6);
	bcopy(&cudp, &RAW_HDR(sfb.sf_match, fddiFilter)->f_ip.ip_p, 1);
	memset(&RAW_HDR(sfb.sf_mask, fddiFilter)->f_ip.ip_p, 0xff, 1);
    }
    else {
	eh = (struct ether_addr *) ether_aton("ff:ff:ff:ff:ff:ff");
	bcopy(eh, RAW_HDR(sfb.sf_match, struct ether_header)->ether_dhost, 6);
	memset(RAW_HDR(sfb.sf_mask, struct ether_header)->ether_dhost, 0xff, 6);
    }
    if(ioctl(rsockin, SIOCADDSNOOP, &sfb) < 0) {
	syslog(LOG_ERR,"Error in ioctl(SIOCADDSNOOP) (%m)");
	if(interface_was_dn)
	    set_interface_state(INTERFACE_DOWN);
	return(PROCLAIM_SNOOP_ERR);
    }

    if(ioctl(rsockin, SIOCSNOOPING, &on) < 0) {
	syslog(LOG_ERR,"Error in ioctl(SIOCSNOOPING) (%m)");
	if(interface_was_dn)
	    set_interface_state(INTERFACE_DOWN);
	return(PROCLAIM_SNOOP_ERR);
    }
    sockbufsiz = 8096;
    if(setsockopt(rsockin, SOL_SOCKET, SO_RCVBUF, (caddr_t)&sockbufsiz,
		  sizeof(sockbufsiz)) < 0) {
	syslog(LOG_ERR, "Cannot set socket option SO_RCVBUF:(%m)");
	if(interface_was_dn)
	    set_interface_state(INTERFACE_DOWN);
	return(PROCLAIM_SOCKOPT_ERR);
    }

    /* SEND SOCKET */

    if ((rsockout = socket(AF_RAW, SOCK_RAW, RAWPROTO_SNOOP)) < 0) {
	syslog(LOG_ERR, "Cannot create socket for broadcast rpc:(%m)");
	if(interface_was_dn)
	    set_interface_state(INTERFACE_DOWN);
	return(PROCLAIM_SOCK_ERR);
    }
    braw_addr.sr_family = AF_RAW;
    strncpy(braw_addr.sr_ifname, InterfaceName, sizeof(braw_addr.sr_ifname));
    braw_addr.sr_port = 0;

    if(bind(rsockout, &braw_addr, sizeof(braw_addr))) {
	syslog(LOG_ERR, "send bind error(%m)");
	if(interface_was_dn)
	    set_interface_state(INTERFACE_DOWN);
	return(PROCLAIM_BIND_ERR);
    }
    if (setsockopt(rsockout, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) < 0) {
	syslog(LOG_ERR, "Cannot set socket option SO_BROADCAST:(%m)");
	if(interface_was_dn)
	    set_interface_state(INTERFACE_DOWN);
	return(PROCLAIM_SOCKOPT_ERR);
    }
    if(setsockopt(rsockout, SOL_SOCKET, SO_SNDBUF, (caddr_t)&sockbufsiz,
		  sizeof(sockbufsiz)) < 0) {
	syslog(LOG_ERR, "Cannot set socket option SO_SNDBUF:(%m)");
	if(interface_was_dn)
	    set_interface_state(INTERFACE_DOWN);
	return(PROCLAIM_SOCKOPT_ERR);
    }

    create_ip_send_packet(&bufp, &buflen, bp);

    if(InterfaceType == TYPE_ETHER)
	packetsiz = sizeof(struct bootp) + sizeof(struct ether_header) +
		sizeof(struct ip) + sizeof(struct udphdr) +
		RAW_BUFPAD(sizeof(struct ether_header));
    else if(InterfaceType == TYPE_FDDI)
	packetsiz = sizeof(struct bootp) + sizeof(struct fddi) +
		sizeof(struct ip) + sizeof(struct udphdr);
    else
	return(PROCLAIM_UNSUPPORTED_IF);
    if(packetsiz % sizeof(long))
	packetsiz += sizeof(long) - (packetsiz % sizeof(long));
    p = (char *) malloc(packetsiz);

    firsttime(&t);
    tprev = t;
    do {
	if ( (ret = sendto(rsockout, bufp, buflen, 0, &braw_addr,
			   sizeof (SCIN))) != buflen ) {
	    syslog(LOG_ERR, "Cannot send broadcast packet (%m)");

	    free(bufp); /* malloced in create_ip_send_packet */
	    free(p);

	    if(interface_was_dn)
		set_interface_state(INTERFACE_DOWN);
	    return(PROCLAIM_SENDTO_ERR);
	}

	t = tprev;
	if (gettimeofday(&tbegin) == -1)
	    syslog(LOG_ERR, "gettimeofday returned error: (%m)");
    select_label:
	FD_ZERO(&readfds);
	FD_SET(rsockin, &readfds);
	switch(select(rsockin+1, &readfds, (fd_set *)NULL, (fd_set *)NULL, &t)) {
	    case 0:		/* Timer expired */
		continue;
	    case -1:
		if(errno == EINTR)
		    continue;
		syslog(LOG_ERR, "select (%m)");
		break;
	    default:
		raddrlen = sizeof(rraw_addr);
		fromlen = recvfrom(rsockin, p, packetsiz, 0, &rraw_addr, &raddrlen);
		if (fromlen < 0) {
		    syslog(LOG_ERR, "recvfrom (%m)");
		    exit(1);
		}
		if(fromlen < DHCPPKTSZ)
		    goto select_label;
		if((ret = dhcp_ok(p, rp, fromlen,0)) == 0) {
		    ++found;
		    break;
		} else if (debugLevel > 2) {
		    syslog(LOG_DEBUG, "2nd dhcp_ok returned %d, (%m)", ret);
		}
	}
	if(found) {
	    break;
	}
	if (gettimeofday(&tnow) == -1)
	    syslog(LOG_ERR, "gettimeofday returned error: (%m)");
	timerdiff(&tnow, &tbegin, &tnow);
	if (timercmp(&tnow, &t, >))
	    continue;
	else
	    timerdiff(&t, &tnow, &t);
	goto select_label;
    } while (nexttime(&tprev, rflag));

    if(interface_was_dn)
	set_interface_state(INTERFACE_DOWN);
    close(rsockout);
    close(rsockin);

    free(bufp); /* malloced in create_ip_send_packet */
    free(p);

    if(!found) {
	return(1);
    }
    return(ret);
}

int
NfsOption()
{
    FILE *fip;

    if( access(NFSOPT, 1) )
	return(1);

    fip = popen(NFSOPT, "r");
    if(pclose(fip))
	return(1);
    return(0);
}

static void
client_init(int ethget)
{
    char hname[MAXHOSTNAMELEN+1];
    char *eaddr;

    gethostname(hname, MAXHOSTNAMELEN);
    if(cl_hostname)
	free(cl_hostname);
    cl_hostname = strdup(hname);
    cl_hostid = gethostid();
    eaddr = get_ether_addr(cl_etheraddr, ethget);
    if(ethget && (eaddr == 0) ) {
	syslog( LOG_ERR, "Cannot get ethernet address for interface %s.",
		InterfaceName);
	exit(1);
    }
}

void
Usage(char *invocation)
{
    syslog(LOG_ERR,
    "Usage: %s [-s server_addr [-r LeaseRenewTime][-d]][-b LeaseRebindTime][-l LeaseTime][-i][-D debug-level][-e interface-name][-B]",
		invocation);
}

/* CHANGES */
static    u_int rq_lease, rq_renew_lease, rq_rebind_lease, rq_poll_server;
static    int LeaseSurrender, InitReboot;
static    int rq_inform;
static    int rq_shutdown_on_expiry;
static    int rq_no_invocations;
static	  int rq_report = 0;
static    u_long rq_server_addr;
static    time_t rv_lease_start, save_rv_lease_start;
static    int interrupt, interrupt_alarm;
static    int first_time;
static    char *rq_hsname;  /* hostname and IP requested with -w */
static    u_long rq_haddr = 0;
static    u_long req_addr_discover = 0;	/* address requested in DISCOVER */
static    u_long poll_addr = INADDR_NONE;/* address requested in DHCPPOLL */


void
dhclient(char* invoc)
{
    static	  char fname[64];
    int retval;

    struct bootp bxp;
    struct bootp rxp;	/* bootp reply */

    int rv_sgi, rv_msgtype;
    u_long rv_smask, rv_ipaddr, rv_server, save_rv_server, rv_sdist_server;
    u_int rv_lease, rv_lease_t1, rv_lease_t2;

    char *rv_hname, *rv_nisdm, *rv_dnsdm;
    char *rv_server_name, *rv_msg, *rv_off_hsname, *req_hsname;

    u_long req_haddr;
    char rhsname[MAXHOSTNAMELEN+2];
    int  interface_is_down;

    int reqParams, if_number;
    int typ;
    int mtype;  /* for -w */
    char mb[2];

    struct in_addr ipz;

    char *ui0_confirm_hostname();

    rv_hname = rv_nisdm = rv_dnsdm = (char *) 0;
    rv_server_name = rv_msg = rv_off_hsname = req_hsname = (char *) 0;

    rv_ipaddr = 0;
    save_rv_server = 0;
    if_number = 1;
    typ = 0;

    
    /* CHANGES */
    alarm(0);                   /* disable alarms if any */

    if (*ClientID == 0){
	if (debugmode){
	    strncpy(ClientID, debug_etheraddr, sizeof(debug_etheraddr));
	    ClientID_len = strlen(ClientID);
	}
	else{
	    if(InterfaceType == TYPE_FDDI)
		bcopy(bsw_etheraddr, ClientID, sizeof(struct ether_addr));
	    else
		bcopy(cl_etheraddr, ClientID, sizeof(struct ether_addr));
	    ClientID_len = 6; /*For ethernet and fddi only*/	
	}
    }

    for (;;) {			/* large for */
    reqParams = 0;
    if(bootp1533only) {
	mk_bootp_msg(&bxp, InterfaceAddr); /* CHANGE to set ciaddr */
	reqParams |= (GET_SUBNETMASK | GET_DNSDOMAIN);
	if(start_nis)
	    reqParams |= GET_NISDOMAIN;
	dh0_encode_dhcp_client_packet(&bxp.dh_opts, BOOTP1533,
				      0, InterfaceAddr, 0, 0, 0,
				      bxp.bp_htype, 0, 0, 0, 0, 0, 0);
	retval = do_dhcp(&bxp, &rxp, 0, 0);
	if(retval) {
	    syslog(LOG_ERR, "No response to the BOOTP1533 broadcast.");
	    syslog(LOG_ERR, "See man pages for proclaim(1M), dhcp_bootp(1M).");
            exit(1);
	}
    }
    else if(LeaseSurrender) {
	if(rq_server_addr == 0) {
	    Usage(invoc);
	    exit(1);
	}
	mk_bootp_msg(&bxp, InterfaceAddr); /* CHANGE to set ciaddr */
	dh0_encode_dhcp_client_packet(&bxp.dh_opts, DHCPRELEASE,
				      0, 0, 0, ClientID, ClientID_len,
				      bxp.bp_htype, rq_server_addr, 
				      0, 0, 0, 0, 0);
	retval = do_dhcp(&bxp, &rxp, RELEASE_LEASE, rq_server_addr);
	syslog(LOG_DEBUG, "Shutting down network: Address lease surrendered.");
	if (debugLevel == 0)
	    dhc0_shutdownNetwork();
	sprintf(fname,"%s/%s.%s", clientInfoDir, CLIENT_CACHE, InterfaceName);
	if (unlink(fname))	/* delete file if it exists */
	    if (errno != ENOENT)
		syslog(LOG_ERR, "Unable to remove file %s: (%m)", fname);
	exit(0);
    }
    else if(rq_renew_lease) {
	if (rq_server_addr == 0) { 
	    Usage(invoc);
	    rq_renew_lease = 0;
	    continue;
	}
	mk_bootp_msg(&bxp, InterfaceAddr);
	save_rv_server = rq_server_addr;
	reqParams |= GET_IPLEASE_TIME;
	dh0_encode_dhcp_client_packet(&bxp.dh_opts, DHCPREQUEST,
				      reqParams, 0, rq_renew_lease,ClientID,
				      ClientID_len, bxp.bp_htype,
				      0, 0, 0, 0, 0, 0);
	if (time(&rv_lease_start) == (time_t)-1) {
	    syslog(LOG_ERR, "time failed for lease renewal: %m");
	    exit(1);
	}
	    
	
	retval = do_dhcp(&bxp, &rxp, RENEW_LEASE, rq_server_addr);
	if(retval) {
	    syslog(LOG_ERR, "No response to the DHCPREQUEST unicast for renewal.");
	    retval = wait_renew_again(save_rv_lease_start, rv_lease, rv_lease_t2);
	    continue;
	}
    }
    else if(rq_rebind_lease) {
	mk_bootp_msg(&bxp, InterfaceAddr);
	reqParams |= (GET_IPLEASE_TIME | GET_DHCP_SERVER);
	dh0_encode_dhcp_client_packet(&bxp.dh_opts, DHCPREQUEST,
				      reqParams, 0, rq_rebind_lease,ClientID,
				      ClientID_len, bxp.bp_htype, 
				      0, 0, 0, 0, 0, 0);
	if (time(&rv_lease_start) == (time_t)-1) {
	    syslog(LOG_ERR, "time failed for rebinding: %m");
	    exit(1);
	}
	    
	
	retval = do_dhcp(&bxp, &rxp, REBIND_LEASE, 0);
	if(retval) {
	    syslog(LOG_ERR, "No response to the DHCPREQUEST broadcast for rebind.");
	    retval = wait_rebind_again(save_rv_lease_start, rv_lease);
	    continue;
	}
    }
    else if(InitReboot) {
	retval = read_client_cache(&save_rv_lease_start, &rxp);
	if (retval) {		/* error reading cache - do init */
	    if (InterfaceAddr == deflt_hostid) {
		InitReboot = 0;
		continue;		/* do regular INIT */
	    }
	}
	else  {
	    rv_msgtype = dh0_decode_dhcp_server_packet(rxp.dh_opts,
					&rv_lease, &rv_msg, &rv_server,
					&rv_hname, &rv_nisdm, &rv_dnsdm,
					&rv_smask, &rv_sdist_server,
					&rv_lease_t1, &rv_lease_t2,
					&rv_off_hsname);
	    free_attrib(rv_msg, rv_hname, rv_nisdm, rv_dnsdm, rv_off_hsname);

	    if (rv_msgtype == -1) {
				/* something wrong with saved cache */
		if (InterfaceAddr == deflt_hostid) {
		    InitReboot = 0;
		    continue;
		}
	    }
	    else {
		if( (save_rv_server) && (rv_server) &&
		   (save_rv_server != rv_server) ) {
		    syslog(LOG_WARNING, "Server mismatch with saved lease_info.");
		}
		if (!save_rv_server)
		    save_rv_server = rq_server_addr = rv_server;
		if (rv_ipaddr == 0)
		    rv_ipaddr = rxp.bp_yiaddr.s_addr;
	    }
	}
	mk_bootp_msg(&bxp, 0); 
	reqParams |= (GET_IPADDRESS | GET_IPLEASE_TIME | GET_DHCP_SERVER);
	dh0_encode_dhcp_client_packet(&bxp.dh_opts, DHCPREQUEST,
				      reqParams, InterfaceAddr, rq_lease,
				      ClientID, ClientID_len, bxp.bp_htype,
				      0, 0, addl_options_to_get,
				      addl_options_len, rv_hname, 
				      vendor_class_str);
	if (time(&rv_lease_start) == (time_t)-1) {
	    syslog(LOG_ERR, "time failed in InitReboot: %m");
	    exit(1);
	}


	retval = do_dhcp(&bxp, &rxp, 0, 0);
	if(retval) {
	    syslog(LOG_ERR,
		   "No response to the DHCPREQUEST broadcast for init-reboot.");
	    InitReboot = 0; 
	    continue; 
	}
    }
    else if (rq_inform) {	/* send DHCPINFORM */
	mk_bootp_msg(&bxp, InterfaceAddr);
	reqParams |= (GET_SUBNETMASK | GET_SDIST_SERVER | GET_DHCP_SERVER |
		      GET_DNSDOMAIN | GET_HOSTNAME);
	if(start_nis)
	    reqParams |= GET_NISDOMAIN;
	dh0_encode_dhcp_client_packet(&bxp.dh_opts, DHCPINFORM,
				      reqParams, 0, 0, ClientID, ClientID_len,
				      bxp.bp_htype, 0, 0, addl_options_to_get,
				      addl_options_len, 0, 0);
	retval = do_dhcp(&bxp, &rxp, 0, 0);
	if(retval) {
	    syslog(LOG_ERR, "No response to the DHCPINFORM broadcast.");
	    syslog(LOG_ERR, "See man pages for proclaim(1M), dhcp_bootp(1M).");
	    autoconfig_ipaddress_off(0);
            exit(1);
	}
    }
    else if (rq_poll_server) {
	mk_bootp_msg(&bxp, InterfaceAddr); /* CHANGE to set ciaddr */
	dh0_encode_dhcp_client_packet(&bxp.dh_opts, DHCPPOLL,
				      0, 0, 0, 0, 0,
				      0, 0, 
				      0, 0, 0, 0, 0);
	retval = do_dhcp(&bxp, &rxp, POLL_SERVER, poll_addr);
	exit(retval);
    }
    else {
	rv_ipaddr = 0;		/* invalidate old address */
	mk_bootp_msg(&bxp, 0);
	reqParams |= (GET_IPADDRESS | GET_IPLEASE_TIME | GET_SUBNETMASK | GET_HOSTNAME);
	reqParams |= (GET_SDIST_SERVER | RESOLVE_HOSTNAME | GET_DHCP_SERVER |
		      GET_DNSDOMAIN);
	if(start_nis)
	    reqParams |= GET_NISDOMAIN;
	dh0_encode_dhcp_client_packet(&bxp.dh_opts, DHCPDISCOVER,
				      reqParams, req_addr_discover, rq_lease,
				      ClientID, ClientID_len, bxp.bp_htype, 0,
				      0,addl_options_to_get, addl_options_len,
				      reqHostname, vendor_class_str);
	retval = do_dhcp(&bxp, &rxp, 0, 0);
	if(retval) {
	    syslog(LOG_ERR, "No response to the DHCPDISCOVER broadcast.");
	    syslog(LOG_ERR, "See man pages for proclaim(1M), dhcp_bootp(1M).");
	    autoconfig_ipaddress_off(0);
	    sprintf(fname,"%s/%s.%s", clientInfoDir, CLIENT_CACHE, InterfaceName);
	    if (unlink(fname))	/* delete file if it exists */
		if (errno != ENOENT)
		    syslog(LOG_ERR, "Unable to remove file %s: (%m)", fname);
            exit(1);
	}
    }

    for(;;) {
	rv_msg = rv_hname = rv_nisdm = rv_dnsdm = rv_off_hsname = (char *) 0;
	rv_msgtype = dh0_decode_dhcp_server_packet(rxp.dh_opts, &rv_lease,
					&rv_msg, &rv_server, &rv_hname,
					&rv_nisdm, &rv_dnsdm, &rv_smask,
					&rv_sdist_server, &rv_lease_t1,
					&rv_lease_t2, &rv_off_hsname);
	if( (save_rv_server) && (rv_server) && (save_rv_server != rv_server) ) {
	    syslog(LOG_WARNING,
		   "Server mismatch to the DHCPREQUEST broadcast.");
	    /* Redo everything here : LATER */
	    rq_renew_lease = rq_rebind_lease = InitReboot = LeaseSurrender = 0;
	    break;
	    /*	    exit(1);*/
	}
 	if (rv_ipaddr == 0)
	    rv_ipaddr = rxp.bp_yiaddr.s_addr;
	if(rv_msgtype == DHCPOFFER) {
	    mk_bootp_msg(&bxp, 0); 
	    bxp.dh_magic = VM_DHCP;
	    if((!is_daemon) && rv_off_hsname) {	/* not when daemon */
		req_hsname = (char *)0;
		/* ADDED THIS */
		req_haddr = 0;
		if (interactive) {
		    alarm(0);
		    u_inp_tmout = 1;
		    alarm(USER_INPUT_TMOUT);
		    rv_ipaddr = rxp.bp_yiaddr.s_addr;
		    req_hsname = ui0_confirm_hostname(rv_off_hsname, rv_msg,
						      rv_ipaddr, rv_smask,
						      &req_haddr, &typ);
		    alarm(0);	/* Disable the alarm */
		    u_inp_tmout = 0;
		}
                else { /* non-interactive mode for use with CGI scripts */
                    if(rq_hsname) {                    /* specified hostname */
                        req_hsname = strdup(rq_hsname);
                        if(rv_msg) {
                            if(rv_msg[1] == ':') {
                                mb[0] = rv_msg[0]; mb[1] = '\0'; mtype = atoi(mb);
                            }
                        }
                        if(mtype == NAME_INUSE) {         /* hostname exists */
                            if(!strcmp(req_hsname, InterfaceHostName))
                                typ = KEEP_OLD_NAMEADDR;
                            else {
                                fprintf(stdout, "Bad Hostname\n"); exit(4);
                            }
                        }
                        else {                     /* hostname doesn't exist */
                            if (rq_haddr) {          /* specified IP address */
                                if((rv_ipaddr&rv_smask) != (rq_haddr&rv_smask)) {
                                    fprintf(stdout, "Bad Subnet\n"); exit(4);
                                }
                                req_haddr = rq_haddr;
                                typ = NEW_NAMEADDR;
                            }
                            else typ = SELECTED_NAME; /*no address specified */
                        }
                    }
                    else typ = OFFERED_NAME;        /* no hostname specified */
                }
		/* DONE ADDING HERE */

		sprintf(rhsname, "%d:", typ);
		switch(typ) {
		    case OFFERED_NAME:
			break;
		    case SELECTED_NAME:
			strcat(rhsname, req_hsname);
    			dh0_encode_dhcp_client_packet(&bxp.dh_opts,
				DHCPREQUEST, reqParams, rv_ipaddr,
				rq_lease,ClientID,ClientID_len, bxp.bp_htype,
			        rv_server, rhsname,addl_options_to_get,
			        addl_options_len, 0, vendor_class_str);
			break;
		    case KEEP_OLD_NAMEONLY:
			strcat(rhsname, InterfaceHostName);
    			dh0_encode_dhcp_client_packet(&bxp.dh_opts,
				DHCPREQUEST, reqParams, rv_ipaddr,
				rq_lease, ClientID, ClientID_len, bxp.bp_htype,
				rv_server, rhsname, addl_options_to_get,
				addl_options_len, 0, vendor_class_str);
			break;
		    case KEEP_OLD_NAMEADDR:
			strcat(rhsname, InterfaceHostName);
    			dh0_encode_dhcp_client_packet(&bxp.dh_opts,
				DHCPREQUEST, reqParams, InterfaceAddr,
				rq_lease,ClientID,ClientID_len,bxp.bp_htype,
				rv_server, rhsname,addl_options_to_get,
				addl_options_len, 0, vendor_class_str);
			rv_ipaddr = InterfaceAddr;
			break;
		    case NEW_NAMEADDR:
			strcat(rhsname, req_hsname);
    			dh0_encode_dhcp_client_packet(&bxp.dh_opts,
				DHCPREQUEST, reqParams, req_haddr,
				rq_lease,ClientID,ClientID_len,bxp.bp_htype,
				rv_server, rhsname, addl_options_to_get,
				addl_options_len, 0, vendor_class_str);
			rv_ipaddr = req_haddr;
			break;
		    case NO_DHCP:
			/* This is a special case where the client just
			 * wants to exit and manage its own configuration.
			 * The Server should recognize this and clean up
			 */
    			dh0_encode_dhcp_client_packet(&bxp.dh_opts,
				DHCPREQUEST, reqParams, req_haddr,
				rq_lease,ClientID,ClientID_len,bxp.bp_htype,
				rv_server, rhsname,addl_options_to_get,
				addl_options_len, 0, vendor_class_str);
			/* Need to send the packet */
			/* XXX */
			autoconfig_ipaddress_off(1);
			syslog(LOG_WARNING, "Disabling DHCP Client");
			exit(0);
		    default:
	    		syslog( LOG_ERR, "User Response: Internal Error(%d).",
				typ);
			exit(1);
		}
		if(req_hsname)
		    free(req_hsname);
		if(typ != OFFERED_NAME) {
		    if (time(&rv_lease_start) == (time_t)-1) {
			syslog(LOG_ERR, "time failed : %m");
			exit(1);
		    }
		    
		    retval = do_dhcp(&bxp, &rxp, 0, 0);
		    if(retval) {
	    		syslog(LOG_ERR,
				"No response to the DHCPREQUEST broadcast.");
			rq_renew_lease = rq_rebind_lease =
			    InitReboot = LeaseSurrender = 0;
			break;
		    }
		    if(save_rv_server == 0)
			save_rv_server = rv_server;
		    free_attrib(rv_msg, rv_hname, rv_nisdm,
				  rv_dnsdm, rv_off_hsname);
		    continue;
		}
	    }
    	    dh0_encode_dhcp_client_packet(&bxp.dh_opts, DHCPREQUEST,
				  reqParams, rv_ipaddr, rq_lease,
				  ClientID, ClientID_len, bxp.bp_htype,
				  rv_server, 0, addl_options_to_get,
				  addl_options_len, rv_hname, 
					  vendor_class_str);
	    if (time(&rv_lease_start) == (time_t)-1) {
		syslog(LOG_ERR, "time failed : %m");
		exit(1);
	    }
	    
	    retval = do_dhcp(&bxp, &rxp, 0, 0);
	    if(retval) {
	    	syslog(LOG_ERR, "No response to the DHCPREQUEST broadcast.");
		rq_renew_lease = rq_rebind_lease = InitReboot
		    = LeaseSurrender = 0;
		
		break;
	    }
	    if(save_rv_server == 0)
		save_rv_server = rv_server;
	}
	else if(rv_msgtype == DHCPACK) {
	    /* Same server as the one with the DHCPOFFER response */
	    if( (save_rv_server == rv_server) || (rq_rebind_lease) ||
	       (rq_inform) ) {
		break;
	    }
	    else if(InitReboot) {
	    	syslog(LOG_DEBUG, "Continuing the Init-reboot Successfully.");
		break;
	    }
	}
	else if(rv_msgtype == DHCPNAK) {
	    ipz.s_addr = rv_server;
	    if(save_rv_server == rv_server) {
	    	syslog(LOG_ERR, "Address unavailable: DHCPNAK from server %s",
			inet_ntoa(ipz) );
		/* exit(2); */
		rq_renew_lease = rq_rebind_lease =
		    InitReboot = LeaseSurrender = 0;

		break;		/* just restart */
	    }
	    else if(InitReboot) {
	    	syslog(LOG_ERR, "Address invalid: DHCPNAK from server %s",
			inet_ntoa(ipz) );
		/* exit(2);	 change to init and try again ???? */
		rq_renew_lease = rq_rebind_lease =
		    InitReboot = LeaseSurrender = 0;

		break;
	    }
	    else {
		/* when its not Initreboot and some other server answers */
	    	syslog(LOG_ERR, "Address invalid: DHCPNAK from server %s",
			inet_ntoa(ipz) );
		/* exit(2);	 change to init and try again ???? */
		rq_renew_lease = rq_rebind_lease =
		    InitReboot = LeaseSurrender = 0;

		break;
	    }
	}
	else if(rv_msgtype == BOOTP1533) {
	    break;
	}
	else {
	    syslog(LOG_ERR,
		   "Unknown response (%d) to the DHCPREQUEST broadcast.",
		   rv_msgtype);
	    rq_renew_lease = rq_rebind_lease =
	    InitReboot = LeaseSurrender = 0;
	    break;
	}
	free_attrib(rv_msg, rv_hname, rv_nisdm, rv_dnsdm, rv_off_hsname);
    }
    if(rv_msgtype == DHCPACK) {
	rv_ipaddr = rxp.bp_yiaddr.s_addr;
	rv_server_name = strdup((char *)rxp.bp_sname);
	ipz.s_addr = rv_server;
	if( (rv_hname == 0) || (*rv_hname == '\0') ) {
	    syslog(LOG_WARNING,
		   "Server %s returned null hostname, using name %s",
		   inet_ntoa(ipz), InterfaceHostName);
	    rv_hname = strdup(InterfaceHostName);
	}

	retval = write_client_cache(rv_lease_start, &rxp);
	if (retval) {		/* error writing the cache */
	    syslog(LOG_ERR, "Unable to write to client cache: %m.");
	    exit(1);
	}
	if(debugLevel > 1) {
	    printf("==================  Accepted Parameters ==================\n");
	    printf("Server Address:\t%s\n", inet_ntoa(ipz));
	    printf("Server Name:\t%s\n", rv_server_name);
	    printf("Lease Duration:\t%d\n", rv_lease);
	    if(InitReboot == 0) {
		printf("Hostname:\t%s\n", rv_hname);
		ipz.s_addr = rv_ipaddr;
		printf("IP Address:\t%s\n", inet_ntoa(ipz));
		printf("Subnet Mask:\t0x%x\n", rv_smask);
		printf("NIS domainname:\t%s\n", rv_nisdm);
		ipz.s_addr = rv_sdist_server;
		printf("Propel Server:\t%s\n", inet_ntoa(ipz));
	    }
	    exit(0);
	}
	else
	    syslog(LOG_DEBUG,
		   "Got lease for %d secs, starting %s from Server %s",
		   rv_lease, ctime(&rv_lease_start), rv_server_name);

	if ((debugLevel == 0) && (!rq_renew_lease) && (!rq_rebind_lease)
	    && (!InitReboot) && (!rq_inform)) {
	    ipz.s_addr = rv_ipaddr;
	    syslog(LOG_DEBUG, "Configuring with IP Address %s Hostname %s",
		   inet_ntoa(ipz), rv_hname);
	
	    if(InterfaceType == TYPE_FDDI)
		dhc0_setupTheConfiguration(rv_hname, rv_ipaddr, bsw_etheraddr,
					   rv_smask, rv_server_name, rv_server,
					   rv_sdist_server, rv_nisdm,start_nis,
					   rv_dnsdm);
	    else
		dhc0_setupTheConfiguration(rv_hname, rv_ipaddr, cl_etheraddr,
					   rv_smask, rv_server_name, rv_server,
					   rv_sdist_server, rv_nisdm,start_nis,
					   rv_dnsdm);
	    client_init(0); 
	}
	if ((rq_inform) || (rv_lease == -1))
	    exit(0);		/* infinite lease - no renewal */
	if (first_time && debugLevel == 0) {
	    first_time = 0;
	    daemon_init();
	}
	rq_server_addr = rv_server;
	save_rv_lease_start = rv_lease_start;
	retval = wait_renew_rebind(rv_lease_start,
				   rv_lease, &rv_lease_t1, &rv_lease_t2);
	if (retval) {		/* error */
	    syslog(LOG_ERR, "Error in wait_renew_rebind");
	    exit(1);
	}
	max_delay_xmit = MAX_INTERVAL;
	min_first_delay_xmit = MIN_INTERVAL;
	xmit_interval = INCR_INTERVAL;
	free_attrib(rv_msg, rv_hname, rv_nisdm, rv_dnsdm, rv_off_hsname);
	free(rv_server_name);
	continue;
    }
    else if(rv_msgtype == BOOTP1533) {
	FILE *fd;
	char fname[80];
	rv_ipaddr = rxp.bp_yiaddr.s_addr;
	rv_server_name = strdup((char *)rxp.bp_sname);
	ipz.s_addr = rv_server;
	if( (rv_hname == 0) || (*rv_hname == '\0') ) {
	    syslog(LOG_WARNING,
		   "Server %s returned null hostname, using name %s",
		   inet_ntoa(ipz), InterfaceHostName);
	    rv_hname = strdup(InterfaceHostName);
	}

	/* retval = write_client_cache(0, &rxp); */
	sprintf(fname, "%s/%s", clientInfoDir, CLIENT_DATA);
	if ((fd = fopen(fname, "w")) == (FILE*)NULL) {
	    syslog(LOG_ERR, "Error in open client data %s for write: %m", fname);
	    exit(1);
	}
	dh0_decode_and_write_ascii_cache(fd, 0, &rxp);
	fclose(fd);
	if(debugLevel > 1) {
	    printf("==================  Accepted Parameters ==================\n");
	    printf("Server Address:\t%s\n", inet_ntoa(ipz));
	    printf("Server Name:\t%s\n", rv_server_name);
	    printf("Hostname:\t%s\n", rv_hname);
	    ipz.s_addr = rv_ipaddr;
	    printf("IP Address:\t%s\n", inet_ntoa(ipz));
	    printf("Subnet Mask:\t0x%x\n", rv_smask);
	    printf("NIS domainname:\t%s\n", rv_nisdm);
	}
	exit(0);
    }
    else {
	syslog(LOG_WARNING,
	       "Response (%d) to the DHCPREQUEST broadcast. Restarting configuration",
		rv_msgtype);
	/* exit(1); */
	rq_renew_lease = rq_rebind_lease = InitReboot = LeaseSurrender = 0;
	free_attrib(rv_msg, rv_hname, rv_nisdm, rv_dnsdm, rv_off_hsname);
	continue;
    }

  } /* larger for ends */
}

/* handlers for signals */
/* handle SIGUSR1 - for surrendering a lease */
void
h_surrender_lease(int signo)
{
    interrupt = 1;
    LeaseSurrender = 1;
    return;
}

/* handle SIGUSR2 - for reboot/verify lease */
void
h_verify_lease(int signo)
{
    interrupt = 1;
    rq_renew_lease = 0;
    InitReboot = 1;
    return;
}

/* handle SIGTERM and SIGINT */
void
h_cleanup(int signo)
{
    syslog(LOG_DEBUG, "Proclaim exiting: Signal received %d", signo);
    /* if (debugLevel == 0)
	dhc0_shutdownNetwork(); */
    exit(0);
}    

/* handle ALARM */
void
h_renew_rebind(int signo)
{
    if(u_inp_tmout) {
	syslog(LOG_ERR,
	    "Proclaim exiting: User input timeout, configuration unchanged");
	exit(1);
    }
    interrupt_alarm = 1;
    return;
}

int
main(int argc, char **argv)
{
    int ch;
    long tloc;
    struct sigaction act;
    char *tmp;
    
    rq_lease = THREE_YEAR_LEASE;
    rq_renew_lease = rq_rebind_lease = 0;
    rq_server_addr = 0;
    rq_poll_server = 0;
    LeaseSurrender = InitReboot = 0;
    rq_inform = 0;
    rq_shutdown_on_expiry = 0;
    InterfaceName = (char *)0;
    InterfaceHostName = (char *)0;
    InterfaceAddr = 0;
    InterfaceType = TYPE_UNSUPPORT;
    rq_no_invocations = DEFAULT_INVOCATIONS;
    first_time = 1;
    deflt_hostid = inet_addr(DEFAULT_HNAME);
    reqHostname[0] = '\0';
    bzero(clientFQDN, sizeof(clientFQDN));
    
    bzero(ClientID, 100);
    bzero(vendor_class_str, sizeof(vendor_class_str));
    /* process options from file */
    process_proclaim_options();
    
    /*
     * Process command line args
     */
    while ((ch = getopt(argc, argv, "b:de:x:il:r:s:ID:pEt:h:a:wqA:BM:C:H:P:F:")) != -1) {
	switch(ch) {
	  case 'b':               /* Rebinding the Lease: time */
	    rq_rebind_lease = atoi(optarg);
	    break;
	  case 'd':               /* Relinquish the IP address and lease */
	    LeaseSurrender = 1;
	    break;
	  case 'e':		  /* Interface name */
	    InterfaceName = strdup(optarg);
	    break;
	  case 'x':
	    max_delay_xmit = atoi(optarg);
	    if (max_delay_xmit < MAX_NEW_DELAY)
		max_delay_xmit = MAX_NEW_DELAY;
	    break;
	  case 'i':		    /* INIT-REBOOT - client verifying its IP address */
	    InitReboot = 1;
	    break;
	  case 'l':               /* Initial Lease: time */
	    rq_lease = atoi(optarg);
	    break;
	  case 'r':               /* Renewing the Lease: time */
	    rq_renew_lease = atoi(optarg);
	    break;
	  case 's':               /* Server address in dot notation */
	    rq_server_addr = inet_addr(optarg);
            if (rq_server_addr == INADDR_NONE) {
                fprintf(stdout, "Bad Server Address\n"); exit(4);
            }
	    break;
	  case 'I':
	    rq_inform = 1;
	    break;
	  case 'D':
	    debugLevel = atoi(optarg);
	    break;
	  case 'p':		/* for reporting status */
	    rq_report = 1;
	    break;
	  case 't':
	    rq_no_invocations = atoi(optarg);
	    break;
	  case 'E':
	    rq_shutdown_on_expiry = 1;
	    break;
	  /* ADDED -w, -h, -a */
          case 'h':
            rq_hsname = strdup(optarg);
            break;
          case 'a':                /* ignored if using old hostname */
            rq_haddr = inet_addr(optarg);
            if (rq_haddr == INADDR_NONE) {
                fprintf(stdout, "Bad Address\n"); exit(4);
            }
            break;
          case 'w':                /* for use with CGI scripts */
            interactive = 0;
            break;
	  case 'q':
	    datamode = 1;
	    interactive = 0;
	    debugLevel = 2;
	    break;
	  case 'A':
	    req_addr_discover = inet_addr(optarg);  
            if (req_addr_discover == INADDR_NONE) {
                fprintf(stdout, "Bad Address (-A option)\n"); exit(4);
            }
            break;
	  case 'M':
	    debugmode = 1;
	    tmp = strtok(optarg, ":");
	    strncpy(debug_etheraddr, tmp, sizeof(debug_etheraddr));
	    if (tmp = strtok(NULL,":")) {
		InterfaceAddr = inet_addr(tmp);
		if (InterfaceAddr == INADDR_NONE) {
		    fprintf(stdout, "Bad Address (-M option)\n"); exit(4);
		}
	    }
	    break;
	case 'C':
	    strcpy(ClientID, optarg);
	    ClientID_len = strlen(ClientID);
	    break;
	case 'H':
	    strcpy(reqHostname, optarg);
	    break;
	case 'B':
	    bootp1533only = 1;
	    break;
	case 'P':
	    rq_poll_server = 1;
	    poll_addr = inet_addr(optarg);  
            if (poll_addr == INADDR_NONE) {
                fprintf(stdout, "Bad Address (-P option)\n"); exit(4);
            }
            break;
	case 'F':		/* client fqdn tag 81 testing */
	    client_fqdn_option = 1;
	    strcpy(clientFQDN, optarg);
	    /* if 1st character is a number it i sth flags field
	     * 0 means client updates, 1 means server updates DNS */
	    if (isdigit(clientFQDN[0])) {
		client_fqdn_flag = clientFQDN[0] - '0';
		client_fqdn = &clientFQDN[1];
	    }
	    else 
		client_fqdn = clientFQDN;
	    break;
	default:
	    Usage(argv[0]);
	    exit(1);
	}
    }
    if (rq_report) {
	client_init(0);  
	exit(report_status());
    }
    
    
    if ((interactive && rq_hsname) || (!rq_hsname && rq_haddr)) {
        Usage(argv[0]);
        exit(1);
    }
    
    atexit(closelog);
    time(&tloc);
    openlog("proclaim", LOG_CONS, LOG_USER);
    syslog(LOG_DEBUG, "%s starting at %s", VERS, ctime(&tloc));

    bzero(cl_etheraddr, 8);
    bzero(bsw_etheraddr, 8);
    client_init(1);
    if(cl_etheraddr == (char *)0) {
	syslog(LOG_ERR, "unable to get ether address");
	exit(1);
    }
    setInterfaceType();
    if(InterfaceType == TYPE_UNSUPPORT) {
	syslog(LOG_ERR, "Currently only Ethernet and FDDI is supported");
	exit(1);
    }
    if(NfsOption() == 0)	/* NFS & NIS Option is installed */
	start_nis = 1;

    /* check whether this is a new machine or existing one being rebooted */
    if (InterfaceAddr == deflt_hostid) { /* new machine */
	min_first_delay_xmit = MIN_INTERVAL;
	max_delay_xmit = MAX_NEW_DELAY;
	xmit_interval = NEW_INTERVAL;
	InitReboot = rq_renew_lease = rq_rebind_lease = 0;
    }
	
    /* setup the signal handlers */
    act.sa_handler = h_surrender_lease;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_flags |= SA_RESTART;
    if (sigaction(SIGUSR1, &act, NULL) == -1) {
	syslog(LOG_ERR, "unable to set handler for SIGUSR1: %m");
	exit(1);
    }
    act.sa_handler = h_verify_lease;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_flags |= SA_RESTART;
    if (sigaction(SIGUSR2, &act, NULL) == -1) {
	syslog(LOG_ERR, "unable to set handler for SIGUSR2: %m");
	exit(1);
    }
    act.sa_handler = h_cleanup;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_flags |= SA_RESTART;
    if (sigaction(SIGTERM, &act, NULL) == -1) {
	syslog(LOG_ERR, "unable to set handler for SIGTERM: %m");
	exit(1);
    } 
    /* nothing to do for SIGINT - don't handle it
       act.sa_handler = h_cleanup; 
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_flags |= SA_RESTART;
    if (sigaction(SIGINT, &act, NULL) == -1) {
	syslog(LOG_ERR, "unable to set handler for SIGINT: %m");
	exit(1);
    }
    */
    act.sa_handler = h_renew_rebind;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_flags |= SA_RESTART;
    if (sigaction(SIGALRM, &act, NULL) == -1) {
	syslog(LOG_ERR, "unable to set handler for SIGALRM: %m");
	exit(1);
    }

    dhclient(argv[0]);			/* becomes a daemon */
}



int
wait_renew_rebind(time_t lease_start,
		  u_int lease, u_int *lease_t1, u_int *lease_t2)
{
    time_t curr_time;
    struct sigaction act;
    sigset_t newmask, oldmask, zeromask;
    int rnd;
    
    if(*lease_t1 == 0)	
	*lease_t1 = lease*0.5;

    if(*lease_t2 == 0)	
	*lease_t2 = lease*0.875;
    
    /* Add a fuzz factor of 0 to 31 secs */
    srand(time(0)); 
    rnd = rand();
    rnd &= 0x1f;
    *lease_t1 = *lease_t1 + (rnd*0.5);
    *lease_t2 = *lease_t2 + (rnd*0.875);

    if (time(&curr_time) == (time_t)-1) {
	syslog(LOG_ERR, "time failed (wait_renew): %m");
	exit(1);
    }

    if (lease_start + lease <= curr_time) {
	rq_rebind_lease = rq_renew_lease = InitReboot = 0;
	return 0;		/* already expired */
    }
    else if (lease_start + *lease_t2 <= curr_time) {
	rq_rebind_lease = lease;
	return 0;
    }
    else if (lease_start + *lease_t1 <= curr_time) {
	rq_renew_lease = lease;
	return 0;
    }

    /* set up the alarm handler */
    interrupt_alarm = 0;
    alarm(0);			/* reinitialize */
    sigemptyset(&zeromask);

    sigemptyset(&newmask);
    sigaddset(&newmask, SIGALRM);
    if (sigprocmask(SIG_BLOCK, &newmask, &oldmask) == -1) {
	syslog(LOG_ERR, "unable to block SIGALRM: %m");
	exit(1);
    }
    alarm(lease_start+(*lease_t1) - curr_time);
    if (sigsuspend(&zeromask) != -1) {
	syslog(LOG_ERR, "unable to suspend process: %m");
	exit(1);
    }	
    if (sigprocmask(SIG_SETMASK, &oldmask, NULL) == -1) {
	syslog(LOG_ERR, "unable to block SIGALRM: %m");
	exit(1);
    }
    alarm(0);
    /* return after alarm takes place */
    if (interrupt_alarm == 1) {
	InitReboot = rq_rebind_lease = 0;
	rq_renew_lease = lease;
    }
    return 0;
}

int
wait_renew_again(time_t lease_start, u_int lease, u_int lease_t2)
{
    time_t curr_time;
    struct sigaction act;
    sigset_t newmask, oldmask, zeromask;
    int timer = 0;
    
    if (time(&curr_time) == (time_t)-1) {
	syslog(LOG_ERR, "time failed (wait_renew): %m");
	exit(1);
    }

    if(lease_t2 == 0)	
	lease_t2 = lease*0.875;
    
    if (lease_start + lease <= curr_time) {
	rq_rebind_lease = rq_renew_lease = InitReboot = 0;
	return 0;		/* already expired */
    }
    else if (lease_start + lease_t2 <= curr_time) {
	rq_renew_lease = 0;
	rq_rebind_lease = lease;
	return 0;
    }

    /* set up the alarm handler */
    interrupt_alarm = 0;
    alarm(0);			/* reinitialize */
    sigemptyset(&zeromask);

    sigemptyset(&newmask);
    sigaddset(&newmask, SIGALRM);
    if (sigprocmask(SIG_BLOCK, &newmask, &oldmask) == -1) {
	syslog(LOG_ERR, "unable to block SIGALRM: %m");
	exit(1);
    }
    timer = lease_start + lease_t2 - curr_time;
    if (timer > 60)
	timer /= 2;
    alarm(timer);
    if (sigsuspend(&zeromask) != -1) {
	syslog(LOG_ERR, "unable to suspend process: %m");
	exit(1);
    }	
    if (sigprocmask(SIG_SETMASK, &oldmask, NULL) == -1) {
	syslog(LOG_ERR, "unable to block SIGALRM: %m");
	exit(1);
    }
    alarm(0);
    /* return after alarm takes place */
    if (interrupt_alarm == 1) {
	InitReboot = rq_rebind_lease = 0;
	rq_renew_lease = lease;
    }
    return 0;
}
    

int
wait_rebind_again(time_t lease_start, u_int lease)
{
    time_t curr_time;
    struct sigaction act;
    sigset_t newmask, oldmask, zeromask;
    int timer = 0;
    
    if (time(&curr_time) == (time_t)-1) {
	syslog(LOG_ERR, "time failed (wait_renew): %m");
	exit(1);
    }

    if (lease_start + lease <= curr_time) {
	rq_rebind_lease = rq_renew_lease = InitReboot = 0;
	syslog(LOG_DEBUG, "Shutting down network: Lease has expired");
	if (debugLevel == 0 && rq_shutdown_on_expiry)
	    dhc0_shutdownNetwork(); /* sectopn 3.8 */
	return 0;		/* already expired */
    }

    /* set up the alarm handler */
    interrupt_alarm = 0;
    alarm(0);			/* reinitialize */
    sigemptyset(&zeromask);

    sigemptyset(&newmask);
    sigaddset(&newmask, SIGALRM);
    if (sigprocmask(SIG_BLOCK, &newmask, &oldmask) == -1) {
	syslog(LOG_ERR, "unable to block SIGALRM: %m");
	exit(1);
    }
    timer = lease_start + lease - curr_time;
    if (timer > 60)
	timer /= 2;
    alarm(timer);
    if (sigsuspend(&zeromask) != -1) {
	syslog(LOG_ERR, "unable to suspend process: %m");
	exit(1);
    }	
    if (sigprocmask(SIG_SETMASK, &oldmask, NULL) == -1) {
	syslog(LOG_ERR, "unable to block SIGALRM: %m");
	exit(1);
    }
    alarm(0);
    /* return after alarm takes place */
    if (interrupt_alarm == 1) {
	InitReboot = rq_renew_lease = 0;
	rq_rebind_lease = lease;
    }
    return 0;
}

/* functions to read and write the client cache used for InitReboot */
int
write_client_cache(time_t lease_start, struct bootp* rxp)
{
    FILE *fd;
    char fname[80];
    char cmd[256];

    sprintf(fname,"%s/%s.%s", clientInfoDir, CLIENT_CACHE, InterfaceName);
    if ((fd = fopen(fname, "w")) == (FILE*)NULL) {
	syslog(LOG_ERR, "Error opening the client cache %s for write: %m", fname);
	exit(1);
    }

    if (fwrite(&lease_start, sizeof(lease_start), 1, fd) != 1) {
	syslog(LOG_ERR, "Error writing the client cache %s: %m", fname);
	exit(1);
    }

    if (fwrite(rxp, sizeof(*rxp), 1, fd) != 1) {
	syslog(LOG_ERR, "Error writing the client cache %s: %m", fname);
	exit(1);
    }

    fclose(fd);
    if (chmod(fname, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) != 0)
	syslog(LOG_ERR, "Error in chmod: (%m)");
    if (!(rq_renew_lease || rq_rebind_lease || LeaseSurrender)) {
	sprintf(fname, "%s/%s.%s", clientInfoDir, CLIENT_CACHE_ASCII,
		InterfaceName);
	if ((fd = fopen(fname, "w")) == (FILE*)NULL) {
	    syslog(LOG_ERR, "Error in open ascii cache %s for write: %m", fname);
	    return 1;
	}
	dh0_decode_and_write_ascii_cache(fd, lease_start, rxp);
	fclose(fd);
	if (chmod(fname, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) != 0)
	    syslog(LOG_ERR, "Error in chmod: (%m)");
	if (datamode) {
	    sprintf(cmd, "cp %s %s/%s", fname, clientInfoDir, CLIENT_DATA);
	    if (system(cmd))
		syslog(LOG_ERR, "Error creating datafile: (%m)");
	}
    }
    return 0;

}

int
read_client_cache(time_t *lease_start, struct bootp *rxp)
{
    FILE* fd;
    char fname[80];
    
    sprintf(fname,"%s/%s.%s", clientInfoDir, CLIENT_CACHE, InterfaceName);
    if ((fd = fopen(fname, "r")) == (FILE*)NULL) {
	return -1;
    }

    if (fread(lease_start, sizeof(*lease_start), 1, fd) != 1) {
	syslog(LOG_ERR, "Error reading the client cache %s: %m", fname);
	return -1;
    }

    if (fread(rxp, sizeof(*rxp), 1, fd) != 1) {
	syslog(LOG_ERR, "Error reading the client cache %s: %m", fname);
	return -1;
    }

    return(fclose(fd));

}

int
autoconfig_ipaddress_off(int do_it)
{
    char buf[256];
    FILE *fd, *fdt;
    char fname[80];
    char tname[80];
    int times_invoked = 1;
    int max_times;
    int found_invocations = 0;

    if (!isPrimary)
	return 0;
    sprintf(fname,"%s/%s", useConfigDir, CLIENT_OPTIONS);
    sprintf(tname, "%s/%s%s", useConfigDir, CLIENT_OPTIONS, ".tmp");
    if ((fd = fopen(fname, "r")) != (FILE*)NULL) {
	if ((fdt = fopen(tname, "w")) == (FILE*)NULL) {
	    syslog(LOG_ERR, "Error creating file %s:(%m)", tname);
	    return -1;
	}
	while (fgets(buf, 256, fd) != NULL) {
	    if (found_invocations) {
		fputs(buf, fdt);
		continue;
	    }
	    if (sscanf(buf, "Invocations:\t%d\t%d\n",&max_times,
		       &times_invoked) > 0) {
		found_invocations = 1;
		times_invoked++;

		if ( do_it || (times_invoked >= rq_no_invocations) )
		    times_invoked = 0;
		if (fprintf(fdt, "Invocations:\t%d\t%d\n",
			    max_times, times_invoked) <= 0) {
		    syslog(LOG_ERR, "Error writing %s: (%m)", tname);
		    fclose(fd);
		    return -1;
		}
	    }
	    else
		fputs(buf, fdt);
	}
	fclose(fd);
	fclose(fdt);
	rename(tname, fname);
    }
    else {
	/* options file does not exist */
	if ( do_it || (times_invoked >= rq_no_invocations) )
	    times_invoked = 0;
	if ((fd = fopen(fname, "w")) == (FILE*)NULL) {
	    syslog(LOG_ERR, "Error creating %s: (%m)", fname);
	    return -1;
	}
	if (fprintf(fd, "Invocations:\t%d\t%d\n", rq_no_invocations,
		    times_invoked) <= 0) {
	    syslog(LOG_ERR, "Error writing %s: (%m)", fname);
	    fclose(fd);
	    return -1;
	}
	fclose(fd);
    }
    if (times_invoked == 0) {
	sprintf(buf, "/etc/chkconfig autoconfig_ipaddress off");
	system(buf);
    }
    return 0;
}

/*
   report_status - returns 0 if okay
   */
#define SECSPERYEAR	31536000
#define SECSPERDAY	86400
#define SECSPERHOUR	3600
#define SECSPERMIN      60

void
secstostr(char *rbuf, int secs)
{
    int years, days, hours, minutes, seconds;
    char buf[80];

    *rbuf = '\0';
    if (secs <= 0) {
	strcpy(rbuf, "0 secs");
	return;
    }

    if (years = secs / SECSPERYEAR) {
	sprintf(buf, "%d years, ", years);
	strcat(rbuf, buf);
	secs -= years*SECSPERYEAR;
    }

    if (days = secs / SECSPERDAY) {
	sprintf(buf, "%d days, ", days);
	strcat(rbuf, buf);
	secs -= days*SECSPERDAY;
    }

    if (hours = secs / SECSPERHOUR) {
	sprintf(buf, "%d hours, ", hours);
	strcat(rbuf, buf);
	secs -= hours*SECSPERHOUR;
    }

    if (minutes = secs / SECSPERMIN) {
	sprintf(buf, "%d minutes, ", minutes);
	strcat(rbuf, buf);
	secs -= minutes*SECSPERMIN;
    }

    sprintf(buf, "%d seconds.", secs);
    strcat(rbuf, buf);
}
	
int
report_status()
{
    int retval;
    struct bootp rxp;
    time_t lease_start, lease_end, tnow;
    u_long rv_smask, rv_ipaddr, rv_server, rv_sdist_server;
    u_int rv_lease, rv_lease_t1, rv_lease_t2;
    char *rv_hname, *rv_nisdm, *rv_dnsdm;
    char *rv_msg;
    char *rv_off_hsname;
    char hname[256];
    char rbuf[128];
    struct in_addr ipz;
    
    rv_hname = rv_nisdm = rv_dnsdm = (char *) 0;
    rv_msg = rv_off_hsname = (char *) 0;

    retval = read_client_cache(&lease_start, &rxp);
    if (retval) {		/* error reading cache - do init */
	printf(" Lease Information Unavailable\n");
	return retval;
    }
    else  {
	retval = dh0_decode_dhcp_server_packet(rxp.dh_opts,
					&rv_lease, &rv_msg, &rv_server,
					&rv_hname, &rv_nisdm,
				   	&rv_dnsdm, &rv_smask,
					&rv_sdist_server,
				   	&rv_lease_t1, &rv_lease_t2,
					&rv_off_hsname);
	if (retval == -1) 
	    return 1;
	lease_end = lease_start+rv_lease;
	tnow = time(0);
	
	if (rv_lease == -1)
	    printf(" This lease does not expire\n");
	else {
	    secstostr(rbuf, (lease_end - tnow));
	    printf(" The Lease ends on %s in %s\n", ctime(&lease_end),
		   rbuf);
	    if (rv_lease_t1) {
		secstostr(rbuf, (lease_start+rv_lease_t1 - tnow));
		printf(" Renewal scheduled in %s\n",rbuf);
	    }
	    else {
		secstostr(rbuf, (lease_start+(0.5* rv_lease) - tnow));
		printf(" Renewal scheduled in approximately %s\n",rbuf);
	    }
	}
	gethostname(hname, 256);
	ipz.s_addr = rxp.bp_yiaddr.s_addr;
	printf(" IP address: %s\tHost Name: %s\n",
	       inet_ntoa(ipz), hname);
	ipz.s_addr = rv_server;
	printf(" DHCP Server: %s\tServer Name: %s\n",
	       inet_ntoa(ipz), rxp.bp_sname);
	if (rv_nisdm && *rv_nisdm)
	    printf(" NIS Domain: %s\n", rv_nisdm);
	if (rv_dnsdm && *rv_dnsdm)
	    printf(" DNS Dmain: %s\n", rv_dnsdm);
	if (rv_sdist_server) {
	    ipz.s_addr = rv_sdist_server;
	    printf(" Propel Server: %s\n", inet_ntoa(ipz));
	}
    }

    free_attrib(rv_msg, rv_hname, rv_nisdm, rv_dnsdm, rv_off_hsname);

    return 0;
}


void
daemon_init()
{
  pid_t pid;
  int i;

  if ((pid = fork()) < 0)
    return;
  else
    if (pid != 0)
      exit(0);
  is_daemon = 1;
  setsid();
/*  chdir("/");
  umask(22);*/
  closelog();
  for (i=0; i < 32; i++)
    close(i);
  openlog("proclaim", LOG_CONS, LOG_USER);
}

int
process_proclaim_options()
{
    char buf[256];
    FILE *fd;
    char fname[80];
    char *pstr;
    int tmp;
    char tmpbuf[32];
    
    sprintf(fname,"%s/%s", useConfigDir, CLIENT_OPTIONS);
    if ((fd = fopen(fname, "r")) != (FILE*)NULL) {
	while ((fgets(buf, 256, fd) != NULL) && (*buf != '\0')) {
	    if (*buf == '#')
		continue;
	    if (strstr(buf, INVOCATIONS)) {
		if (sscanf(buf, "Invocations:\t%d\t%d\n",&rq_no_invocations,
			   &tmp) <= 0) {
		    rq_no_invocations = DEFAULT_INVOCATIONS;
		    syslog(LOG_ERR, "Error in %s: Invocations", fname);
		}
		continue;
	    }
	    else if (strstr(buf, MAXTIMEOUT)) {
		if (sscanf(buf, "MaxTimeout:\t%d\n",&max_delay_xmit) <= 0) {
		    max_delay_xmit = MAX_NEW_DELAY;
		    syslog(LOG_ERR, "Error in %s: Max Timeout", fname);
		}
		continue;
	    }
	    else if (strstr(buf, SERVERADDRESS)) {
		if (sscanf(buf, "ServerAddress:\t%s\n", tmpbuf) > 0) 
		    rq_server_addr = inet_addr(tmpbuf);
		continue;
	    }
	    else if (strstr(buf, SHUTDOWNONEXPIRY)) {
		if (sscanf(buf, "ShutdownOnExpiry:\t%d\n",&rq_shutdown_on_expiry) <= 0) {
		    syslog(LOG_ERR, "Error in %s: Shutdown On Expiry", fname);
		    rq_shutdown_on_expiry = 0;
		}
		continue;
	    }
	    else if (strstr(buf, LEASE)) {
		if (sscanf(buf, "Lease:\t%d\n",&rq_lease) <= 0) {
		    syslog(LOG_ERR, "Error in %s: Lease", fname);
		    rq_lease = THREE_YEAR_LEASE;
		}
		continue;
	    }
	    else if (strstr(buf, DHCPOPTIONS)) {
		pstr = strchr(buf, ':');
		pstr = strtok(pstr+1, " \t,");
		addl_options_len = 0;
		while (pstr) {
		    sscanf(pstr, " %d", &tmp);
		    addl_options_to_get[addl_options_len] = (char)tmp;
		    addl_options_len++;
		    pstr = strtok(0, " \t,");
		}
		continue;
	    }
	    else if (strstr(buf, VENDORTAG)) {
		if (sscanf(buf, "VendorClass:\t%s\n", vendor_class_str) > 0) 
		    ;
		continue;
	    }
	    continue;
	} /* for each line in file */
	fclose(fd);
    } /* file was found */
    return 0;
}

static int
free_attrib(char *a, char *b, char *c, char *d, char *e)
{
    if(a) free(a);
    if(b) free(b);
    if(c) free(c);
    if(d) free(d);
    if(e) free(e);
    return 0;
}

