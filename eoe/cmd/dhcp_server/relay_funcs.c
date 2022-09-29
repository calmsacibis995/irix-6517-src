/* relay_funcs.c - socket initialization and packet processing
 * many of the functions here are taken directly from dhcp_bootp.c
 * 7/4/95
 */
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <netdb.h>
#include <syslog.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <net/if.h>
#include <netinet/in.h>
#include <net/raw.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

#include <arpa/inet.h>

#include <sys/fddi.h>
#include <sys/llc.h>
#include <invent.h>

#include "dhcp.h"
#include "dhcpdefs.h"

/* globals */
#define	RELAY_TIMEOUT	120	/* number of idle seconds to wait */
#define	IFMAX	200		/* maximum number of interfaces supported */

extern	struct	ifreq ifreq[IFMAX];	/* holds interface configuration */
extern	struct	ifconf ifconf;		/* config. ioctl block */

static	u_char	buf[1024];	/* receive buffer */
static	int	rsockout_err = 0;

iaddr_t	myhostaddr;		/* (main) internet address of executing host */
int s;				/* the socket for reading input */
int rsockout;

/* flags */
extern int standalone_f;	/* standalone mode flag */
extern int ProclaimServer;
extern int debug_f;
extern int sleepmode_f;		/* sleepmode for debug */

/* extern globals */
extern int	relay_secs;	/* don't relay if secs field is smaller */
extern int	relay_hops;	/* don't relay if hops field is larger */
extern int	no_dhcp_servers; /* number od dhcp servers */
extern struct in_addr   dhcp_server_addr[];

struct netaddr* np_recv;	/* the net on which the last packet was recd */
struct netaddr* np_send;	/* the net on which to send a packet if it
				 * must be broadcast */
extern int ifcount;		/* the number of interfaces */

extern	struct netaddr	nets[];
extern	int		sr_initialize_nets(void);
extern	int		sr_initialize_broadcast_sockets(void);
extern	int		sr_initialize_single_broadcast_socket(void);
extern	struct netaddr	*sr_match_address(u_long);
extern  int             sr_create_ip_send_packet(char **, int *,
						 struct bootp *, int);
extern	int		sr_get_interface_type(char *);
extern	int		makenamelist(void);
extern	void		process_bootp_message(struct bootp *);
extern	void		setarp(iaddr_t *, u_char *, int);
extern	int		bestaddr(iaddr_t *, iaddr_t *);

void process_relay_messages(void);
void set_timeval(struct timeval *, long);
void sendreply(struct bootp *, int, int, int, int);

int
set_myhostaddr(void)
{
    register struct hostent *hp;
    char name[64];

    gethostname(name, sizeof(name));
    if ((hp = gethostbyname(name)) == 0) {
	syslog(LOG_ERR, "gethostbyname(%s) failed: %s", name, hstrerror(h_errno));
	exit(1);
    }
    myhostaddr.s_addr = *(u_long *)(hp->h_addr_list[0]);
    return 0;
}

int
initialize_socket(void)
{
    struct sockaddr_in sin;
    int	len;
    int	on = 1;
    
    /*
     * It is not necessary to create a socket for input.
     * The inet master daemon passes the required socket
     * to this process as fd 0.  Bootp takes control of
     * the socket for a while and gives it back if it ever
     * remains idle for the timeout limit.
     */
    s = 0;
    if (standalone_f) {
	s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s < 0) {
	    syslog(LOG_ERR,"Opening DGRAM stream socket(%m)");
	    exit(1);
	}
	dup2(s, 0);
	
	bzero((char *)&sin, sizeof(sin));
        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = INADDR_ANY;       
	sin.sin_port = htons(IPPORT_BOOTPS);
        if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
	    syslog(LOG_ERR,"Binding DGRAM stream socket(%m)");
	    exit(1);
	}
    }
    else {
	len = sizeof (sin);
	if (getsockname(s, &sin, &len) < 0) {
	    syslog(LOG_ERR, "getsockname failed (%m)");
	    exit(1);
	}
    }

    set_myhostaddr();

    np_recv = 0;
    
    /*
     * Save the name of the executing host
     */
    makenamelist();
    ifcount = sr_initialize_nets();

    if(MULTIHMDHCP) {
	rsockout_err = sr_initialize_broadcast_sockets();
    }
    else if(ProclaimServer) {
	rsockout_err = sr_initialize_single_broadcast_socket();
    }

    if(MULTIHMDHCP) {
	if(setsockopt(s, SOL_SOCKET, SO_PASSIFNAME, &on, sizeof(on)) < 0) {
	    syslog(LOG_ERR, "Cannot set socket option SO_PASSIFNAME:(%m)");
	    return 1;
	}
    }

    return 0;
}

/* process_message - returns 1 if message is to be processed otherwise 0
 */
int
process_message(struct bootp* rq)
{
    if (!ProclaimServer)
	return 0;
    
    if (rq->bp_hops > relay_hops)
	return 0;

    if (rq->bp_secs < relay_secs)
	return 0;

    switch(rq->bp_op) {
	case BOOTREQUEST:
	    if (no_dhcp_servers == 0)	/* no one to forward to */
		return 0;
	    break;
	case BOOTREPLY:
	    if ((np_send = sr_match_address(rq->bp_giaddr.s_addr)) == 0)
		return 0;
	    break;
    }
    return 1;
}
    
int
request(struct bootp* bp)
{
    struct sockaddr_in to;
    int i;
    
    if (sleepmode_f)		/* allow debugging */
	sleep(10);
    /* check whether we should process this */
    if(bp->dh_magic != VM_DHCP) {
	process_bootp_message(bp); /* regular bootp messages */
	return 0;
    }
    /* dhcp messages are handled here */
    if (process_message(bp)) {
	/* must select based on the interface */
	if (bp->bp_giaddr.s_addr == 0) {
	    if (MULTIHMDHCP)
		bp->bp_giaddr.s_addr = (np_recv->myaddr).s_addr;
	    else
		bp->bp_giaddr.s_addr = myhostaddr.s_addr;
	}
	bp->bp_hops++;

	/* now relay the request to all of the destinations */
	to.sin_family = AF_INET;
	to.sin_port = htons(IPPORT_BOOTPS);
	for (i=0; i < no_dhcp_servers; i++) {
	    to.sin_addr.s_addr = dhcp_server_addr[i].s_addr;       
	    if (debug_f)
		syslog(LOG_DEBUG, "forwarding BOOTP request to %s",
		       inet_ntoa(dhcp_server_addr[i]));
	    if (sendto(s, (caddr_t)bp, sizeof *bp, 0, &to, sizeof to) < 0)
		syslog(LOG_ERR, "forwarding to %s failed (%m)", 
		       inet_ntoa(to.sin_addr));
	}
    }
    return 0;
}

int
reply(struct bootp* bp)
{
    if (debug_f)
	syslog(LOG_DEBUG, "Received BOOTREPLY");
    if (process_message(bp)) {
	if(bp->bp_flags & BROADCAST_BIT)
	    sendreply(bp, 1, 0, 1, DHCP_REPLY);
	else
	    sendreply(bp, 1, 0, 0, DHCP_REPLY);
    }
    return 0;
}

void
process_relay_messages(void)
{
    register struct bootp *bp;
    register int n;
    struct timeval tmout;
    fd_set readfds;
    struct sockaddr_in from;
    char sifname[IFNAMSIZ+1];
    int	fromlen;
    int i;

    bzero(buf, sizeof(buf));

    for (;;) {

	FD_ZERO(&readfds);
	FD_SET(s, &readfds);
	set_timeval(&tmout, RELAY_TIMEOUT);
	switch (select(s + 1, &readfds, (fd_set *)0, (fd_set *)0, &tmout)) {
	    case 0:
		/* Timeout happened */
		exit(0);		/* wait for inet to wake it up */
	    case -1:
		if (errno == EINTR)
		    continue;
		syslog(LOG_ERR, "select error (%m)");
		break;
	    default:
		fromlen = sizeof (from);
		n = recvfrom(s, buf, sizeof buf, 0, &from, &fromlen);
		if ( (n <= 0) || (n < BOOTPKTSZ) )
		    continue;

		if(MULTIHMDHCP) {
		    bzero(sifname, IFNAMSIZ);
		    np_recv = 0;
		    strncpy(sifname, (char *)buf, IFNAMSIZ);
		    if(*sifname == '\0')
			return;
		    for (i = 0; i < ifcount; i++) {
			if(strcmp(nets[i].ifname, sifname) == 0) {
			    np_recv = &nets[i];
			    break;
			}
		    }
		    if(!np_recv) {
			syslog(LOG_ERR, "Cannot find interface %s", sifname);
			continue;
		    }
		    bp = (struct bootp *)&buf[IFNAMSIZ];
		}
		else {
		    bp = (struct bootp *)buf;
		}
		
		if(bp) {
		    switch (bp->bp_op) {
			case BOOTREQUEST:
			    request(bp);
			    break;
			case BOOTREPLY:
			    reply(bp);
			    break;
		    }
		}
	} /* switch */
    } /* for */
}

void
set_timeval(struct timeval *tmo, long tval)
{
    tmo->tv_sec = tval;
    tmo->tv_usec = 0;
}

/*
 * Send a reply packet to the client.  'forward' flag is set if we are
 * not the originator of this reply packet.
 */
void
sendreply(register struct bootp *bp, int forward,
	  int has_sname, int broadcast_it, int reply_type)
{
    iaddr_t dst;
    struct sockaddr_in to;
    char *bufp;
    int buflen;
    struct sockaddr_raw braw_addr;	/* raw socket addr struct */
    int itype = 0;

    to.sin_family = AF_INET;
    to.sin_port = htons(IPPORT_BOOTPC);
    /*
     * If the client IP address is specified, use that
     * else if gateway IP address is specified, use that
     * else make a temporary arp cache entry for the client's NEW 
     * IP/hardware address and use that.
     */
    if (bp->bp_ciaddr.s_addr) {
	dst = bp->bp_ciaddr;
	if (debug_f)
	    syslog(LOG_DEBUG, "reply ciaddr %s", inet_ntoa(dst));
	
    } else if (bp->bp_giaddr.s_addr && forward == 0) {
	dst = bp->bp_giaddr;
	to.sin_port = htons(IPPORT_BOOTPS);
	if (debug_f)
	    syslog(LOG_DEBUG, "reply giaddr %s", inet_ntoa(dst));
    }
    else {
	dst = bp->bp_yiaddr;
	if (debug_f)
	    syslog(LOG_DEBUG, "reply yiaddr %s", inet_ntoa(dst));
	if (dst.s_addr != 0) /* changed 6/22/95 */
	    setarp(&dst, bp->bp_chaddr, bp->bp_hlen);
    }
    
    if( (forward == 0) && (reply_type == BOOTSTRAP_REPLY) ) { /* plain bootp */
	/*
	 * If we are originating this reply, we
	 * need to find our own interface address to
	 * put in the bp_siaddr field of the reply.
	 * If this server is multi-homed, pick the
	 * 'best' interface (the one on the same net
	 * as the client).
	 */
	int gotmatch = 0;
	gotmatch = bestaddr(&dst, &bp->bp_siaddr);
	if (bp->bp_giaddr.s_addr == 0) {
	    if (gotmatch == 0) {
		syslog(LOG_ERR, "can't reply to %s (unknown net)", inet_ntoa(dst));
		return;
	    }
	    bp->bp_giaddr.s_addr = bp->bp_siaddr.s_addr;
	} else if (!gotmatch && has_sname) {
	    struct hostent *hp;
	    
	    /*
	     * Use the address corresponding to the name
	     * specified by the client.
	     */
	    if ((hp = gethostbyname((const char *)bp->bp_sname))) {
		bp->bp_siaddr = *(struct in_addr *)hp->h_addr;
	    }
	}
    }
    
    if (!(bp->bp_giaddr.s_addr && forward == 0)) { 
        if(ProclaimServer && (broadcast_it || (dst.s_addr == 0))) { 
	    if(rsockout_err)
		return;
	    if(MULTIHMDHCP) {
		np_send =
		    forward ? sr_match_address(bp->bp_giaddr.s_addr): np_recv;
		if (np_send) {
		    itype = sr_get_interface_type(np_send->ifname);
		    if(itype == TYPE_UNSUPPORT) {
			syslog(LOG_ERR, "Unsupported interface `%s`",
				np_send->ifname);
			return;
		    }
		    sr_create_ip_send_packet(&bufp, &buflen, bp, itype);
		    if ( (sendto(np_send->rsockout, bufp, buflen, 0,
				&braw_addr, sizeof (SCIN))) != buflen ) {
			syslog(LOG_ERR, "Cannot send broadcast packet (%m)");
		    }
	    	}
		else  {
		    syslog(LOG_ERR, "Cannot deliver broadcast (interface?)");
		}
	    }
	    else {
		itype = sr_get_interface_type(nets[0].ifname);
		if(itype == TYPE_UNSUPPORT) {
		    syslog(LOG_ERR, "Unsupported interface `%s`",
			   nets[0].ifname);
		    free(bufp);
		    return;
		}
		sr_create_ip_send_packet(&bufp, &buflen, bp, itype);
		if ( (sendto(rsockout, bufp, buflen, 0, &braw_addr,
				sizeof (SCIN))) != buflen ) {
		    syslog(LOG_ERR, "Cannot send broadcast packet (%m)");
		}
	    }
	    free(bufp);
	    return;
	}
    }
    
    to.sin_addr = dst;
    if (sendto(s, (caddr_t)bp, sizeof *bp, 0, &to, sizeof to) < 0)
	syslog(LOG_ERR, "send to %s failed (%m)", inet_ntoa(to.sin_addr));
}
