/* bootp_relay.c - these functions are borrowed from the dhcp_server.c file
 * and here only for handling the bootp requests
 */
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/file.h>

#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <errno.h>
#include <ctype.h>
#include <netdb.h>
#include <setjmp.h>
#include <syslog.h>

#include <net/raw.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

#include <sys/time.h>
#include <rpc/rpc.h>
#include <rpcsvc/ypclnt.h>

#include "dhcp.h"
#include "dhcpdefs.h"


/* other externs in relay_funcs.c */
extern int s;			/* primary input socket */
extern iaddr_t	myhostaddr;	/* save (main) internet address of executing host */
extern struct	ifreq ifreq[];	/* holds interface configuration */


char	reg_netmask[30], *reg_net;
char	reg_hostnet[30];
#define SINPTR(p)	((struct sockaddr_in *)(p))

/* flags */
extern int debug_f;
extern int forwarding_f;

extern struct netaddr nets[];

/*
 * Globals below are associated with the bootp database file (bootptab).
 */

char	*bootptab = "/etc/bootptab";
FILE	*fp;
char	line[256];	/* line buffer for reading bootptab */
char	*linep;		/* pointer to 'line' */
int	linenum;	/* current line number in bootptab */
char	homedir[64];	/* bootfile homedirectory */
char	defaultboot[64]; /* default file to boot */

#define	MHOSTS	512	/* max number of 'hosts' structs */

struct hosts {
	char	host[MAXHOSTNAMELEN+1];	/* host name (and suffix) */
	u_char	htype;		/* hardware type */
	u_char	haddr[6];	/* hardware address */
	iaddr_t	iaddr;		/* internet address */
	char	bootfile[32];	/* default boot file name */
} hosts[MHOSTS];

int	nhosts;		/* current number of hosts */
long	modtime;	/* last modification time of bootptab */

int	max_addr_alloc = 64;
int	numaddrs;
u_int	*myaddrs;	/* save addresses of executing host */
char	myname[MAXHOSTNAMELEN+1]; /* my primary name */

void forward(struct bootp *, iaddr_t *, iaddr_t *);
void readtab(void);
void getfield(char *, int);
void setarp(iaddr_t *, u_char *, int);
void makenamelist(void);
extern void sendreply(struct bootp *, int, int, int, int);
extern int ether_ntohost(char *, struct ether_addr *);
extern int registerinethost(char *, char *, char *, struct in_addr *, char *);
extern FILE *log_fopen(char *, char *);

char *
iaddr_ntoa(iaddr_t addr)
{
    struct hostent *hp;

    if (hp = gethostbyaddr(&addr, sizeof(addr), AF_INET)) {
	return hp->h_name;
    } else {
	return inet_ntoa(addr);
    }
}

/*
 * Perform Reverse ARP lookup using /etc/ether and /etc/hosts (or
 * their NIS equivalents)
 *
 * Returns 1 on success, 0 on failure.
 */
int
reverse_arp(struct ether_addr *eap, iaddr_t *iap)
{
    register struct hostent *hp;
    char host[512];

    /*
     * Call routine to access /etc/ethers or its NIS equivalent
     */
    if (ether_ntohost(host, eap))
	return (0);
	
    /*
     * Now access the hostname database.
     */
    if ((hp = gethostbyname(host)) == 0) {
	syslog(LOG_ERR, "gethostbyname(%s) failed: %s", host, hstrerror(h_errno));
	return (0);
    }

    /*
     * Return primary Internet address
     */
    iap->s_addr = *(u_long *)(hp->h_addr_list[0]);
    return (1);
}

/*
 * Check the passed name against the current host's addresses.
 *
 * Return value
 *	TRUE if match
 *	FALSE if no match
 */
int
matchhost(char *name)
{
    register struct hostent *hp;
    int i;
    u_int requested_addr;

    if (hp = gethostbyname(name)) {
	requested_addr =  *(u_long *)(hp->h_addr_list[0]);
	for (i = 0; i < numaddrs; i++)
	    if (requested_addr == myaddrs[i])
		return 1;
    }
    return 0;
}

/*
 * Check whether two passed IP addresses are on the same wire.
 * The first argument is the IP address of the requestor, so
 * it must be on one of the wires to which the server is
 * attached.
 *
 * Return value
 *	TRUE if on same wire
 *	FALSE if not on same wire
 */
int
samewire(register iaddr_t *src, register iaddr_t *dest)
{
    register struct netaddr *np;

    /*
     * It may well happen that src is zero, since the datagram
     * comes from a PROM that may not yet know its own IP address.
     * In that case the socket layer doesnt know what to fill in
     * as the from address.  In that case, we have to assume that
     * the source and dest are on different wires.  This means
     * we will be doing forwarding sometimes when it isnt really
     * necessary.
     */
    if (src->s_addr == 0 || dest->s_addr == 0)
	return 0;

    /*
     * In order to take subnetworking into account, one must
     * use the netmask to tell how much of the IP address
     * actually corresponds to the real network number.
     *
     * Search the table of nets to which the server is connected
     * for the net containing the source address.
     */
    for (np = nets; np->netmask != 0; np++)
	if ((src->s_addr & np->netmask) == np->net)
	    return ((dest->s_addr & np->netmask) == np->net);

    syslog(LOG_ERR, "can't find source net for %s", inet_ntoa(*src));
    return 0;
}

char	hostmap[] = "hosts.byname";
/*
 *	A new diskless workstation/autoreg is asking for
 *		initial boot/IPADDR
 */
int
handle_diskless(struct bootp *rq, struct bootp *rp)
{
    char	*t_host, *domain, *ypmaster, *str_match, *t_domain;
    char	hostname[256], alias[256];
    int	err = 0;

    if (yp_get_default_domain(&domain)) {
	syslog(LOG_ERR, "Autoreg: can't get domain");
	return(-1);
    }

    t_host = (char *)rq->vd_clntname;
    if (debug_f)
	syslog(LOG_DEBUG, "Autoreg: domain=%s, host=%s", domain, t_host);

    gethostname(hostname, sizeof(hostname));
    if (gethostbyname(t_host) != NULL) {
	/* play safe */
	rp->vd_flags = VF_RET_IPADDR;
	return(0);
    }

    if (!reg_netmask) {
	if (str_match = strrchr(reg_hostnet, '.'))
	    *str_match = 0;
    }

    /* get ypmaster ipaddr */
    if (yp_master(domain, hostmap, &ypmaster)) {
	if (debug_f)
	    syslog(LOG_DEBUG, "no \"%s\" YPmaster found for \"%s\" domain",
		   hostmap, domain);
	return(-1);
    }

    /* check and prevent broadcast storm */
    if (!matchhost(ypmaster)) {
	if (!forwarding_f) {
	    if (debug_f)
		syslog(LOG_DEBUG, "Autoreg: I am neither YPMASTER nor GATEWAY");
	    return(-1);
	}
	if (debug_f)
	    syslog(LOG_DEBUG, "Autoreg: I am GATEWAY");
	/* XXX
	 *	finding out if the requestor and YPMASTER are
	 *	at same sub-net is impossible. So let's
	 *	allow gateway issue an extra broadcast msg.
	 */
    } else if (debug_f)
	syslog(LOG_DEBUG, "Autoreg: I am YPMASTER");

    /* check/create full host name */
    if ((t_domain = strchr((const char *)rq->vd_clntname, '.')) != NULL) {
	/* if it not for the domain i belong, then ignore */
	if (strcmp(domain, t_domain+1)) {
	    if (debug_f)
		syslog( LOG_DEBUG, "Autoreg: req_domain(%s)<>domain(%s)",
			t_domain+1, domain);
	    return(-1);
	}
	strcpy(hostname, t_host);
	*t_domain = 0;
	strcpy(alias, t_host);
	*t_domain = '.';
    } else {
	strcpy(hostname, t_host);
	strcat(hostname, ".");
	strcat(hostname, domain);
	strcpy(alias, t_host);
    }

    /* can't create, other bootp may be creating at the same time
       or, there is no NIS */
    err = registerinethost(hostname,reg_hostnet,reg_netmask,0,alias);
    if ((u_long)err <= YPERR_BUSY) {
	syslog(LOG_ERR, "Autoreg: REGISTER failed(0x%x)", err);
	return(-1);
    }

    rp->vd_flags = VF_NEW_IPADDR;
    rp->bp_yiaddr.s_addr = (u_long)(err);

    if (debug_f)
	syslog( LOG_DEBUG, "Autoreg: %s REGISTERED as %s",
		hostname, inet_ntoa(*(struct in_addr *)&err));
    return(0);
}

/* process_bootp_message  - handle regular bootp messages
 */
void
process_bootp_message(struct bootp *rq)
{
    struct bootp rp;
    char path[MAXPATHLEN], file[128];
    iaddr_t fromaddr;
    register struct hosts *hp;
    register struct hostent *hostentp;
    register n;
    int has_sname;	/* does rq have nonempty bp_sname? */
    struct ifreq ifnetmask; 

    rp = *rq;	/* copy request into reply */
    rp.bp_op = BOOTREPLY;

    readtab();			/* (re)read bootptab */
    
    strncpy(ifnetmask.ifr_name, ifreq[0].ifr_name,
	    sizeof(ifnetmask.ifr_name));
    if (ioctl(s, SIOCGIFNETMASK, (caddr_t)&ifnetmask) < 0) {
	syslog(LOG_ERR, "netmask ioctl failed (%m)");
	return;
    }
    reg_net = inet_ntoa(SINPTR(&ifnetmask.ifr_addr)->sin_addr);
    strcpy(reg_netmask, reg_net);
    if (ioctl(s, SIOCGIFADDR, (caddr_t)&ifnetmask) < 0) {
	syslog(LOG_ERR, "netaddr ioctl failed (%m)");
	return;
    }
    reg_net = inet_ntoa(SINPTR(&ifnetmask.ifr_addr)->sin_addr);
    strcpy(reg_hostnet, reg_net);
    
    /* This is a non dhcp, regular bootp packet */
    /*
     * Let's resolve client's ipaddr first.
     */
    if (rq->bp_yiaddr.s_addr != 0 && rq->bp_giaddr.s_addr != 0) { 
	/*
	 * yiaddr has already been filled in by forwarding bootp
	 */
	hp = (struct hosts *) 0;
    } else if (rq->bp_ciaddr.s_addr == 0) {
	/*
	 * client doesnt know his IP address, 
	 * search by hardware address.
	 */
	for (hp = &hosts[0], n = 0 ; n < nhosts ; n++,hp++)
	    if (rq->bp_htype == hp->htype &&
		bcmp(rq->bp_chaddr, hp->haddr, 6) == 0)
		break;
	if (n == nhosts) {
	    /*
	     * The requestor isn't listed in bootptab.
	     */
	    hp = (struct hosts *) 0;
	    
	    if( (rp.vd_magic == VM_SGI) && (rp.vd_flags == VF_GET_IPADDR) ) {
		if ((hostentp = 	
		     gethostbyname((const char *)rp.vd_clntname)) == 0) {
		    rp.bp_yiaddr.s_addr = 0;
		} else {
		    rp.bp_yiaddr.s_addr= *(u_long *)(hostentp->h_addr_list[0]);
		    rp.vd_flags = VF_RET_IPADDR;
		}
	    }
	    
	    /*
	     * Try Reverse ARP using /etc/ethers or NIS before
	     * giving up.
	     */
	    if (!reverse_arp((struct ether_addr *)rq->bp_chaddr, &rp.bp_yiaddr)) {
		/*
		 * Don't trash the request at this point,
		 * since it may be for another server with
		 * better tables than we have.
		 */
		if (debug_f)
		    syslog(LOG_DEBUG, "no Internet address for %s",
			   ether_ntoa((struct ether_addr *)rq->bp_chaddr));
		/* Play it safe */
		rp.bp_yiaddr.s_addr = 0;
	    }
	} else {
	    rp.bp_yiaddr = hp->iaddr;
	}
    } else {
	/* search by IP address */
	for (hp = &hosts[0], n = 0 ; n < nhosts ; n++,hp++)
	    if (rq->bp_ciaddr.s_addr == hp->iaddr.s_addr)
		break;
	if (n == nhosts) {
	    /*
	     * The requestor knows his Internet address already,
	     * but he isn't listed in bootptab.  Try to satisfy
	     * the request anyway.
	     */
	    hp = (struct hosts *) 0;
	}
    }
    
    fromaddr = rq->bp_ciaddr.s_addr ? rq->bp_ciaddr : rp.bp_yiaddr;
    
    /*
     * Check whether the requestor specified a particular server.
     * If not, fill in the name of the current host.  If a
     * particular server was requested, then don't answer the
     * request unless the name matches our name or one of our
     * aliases.
     */
    has_sname = rq->bp_sname[0] != '\0';
    if (!has_sname)
	strncpy((char *)rp.bp_sname, myname, sizeof(rp.bp_sname));
    else if (!matchhost((char *) rq->bp_sname)) {
	iaddr_t destaddr;
	
	if (debug_f)
	    syslog(LOG_DEBUG, "%s: request for server %s",
		   iaddr_ntoa(fromaddr), rq->bp_sname);
	/*
	 * Not for us.
	 */
	if (!forwarding_f)
	    return;
	/*
	 * Look up the host by name and decide whether
	 * we should forward the message to him.
	 */
	if ((hostentp = gethostbyname((const char *)rq->bp_sname)) == 0) {
	    syslog(LOG_INFO, "%s: request for unknown server %s",
		   iaddr_ntoa(fromaddr), rq->bp_sname);
	    return;
	}
	destaddr.s_addr = *(u_long *)(hostentp->h_addr_list[0]);
	/*
	 * If the other server is on a different cable from the
	 * requestor, then forward the request.  If on the same
	 * wire, there is no point in forwarding.  Note that in
	 * the case that we don't yet know the IP address of the
	 * client, there is no way to tell whether the client and
	 * server are actually on the same wire.  In that case
	 * we forward regardless.  It's redundant, but there's
	 * no way to get around it.
	 */
	if (!samewire(&fromaddr, &destaddr)) {
	    /*
	     * If we were able to compute the client's Internet
	     * address, pass that information along in case the
	     * other server doesn't have the info.
	     */
	    rq->bp_yiaddr = rp.bp_yiaddr;
	    forward(rq, &fromaddr, &destaddr);
	}
	return;
    }
    if ( (fromaddr.s_addr == 0) && (rq->vd_magic == VM_AUTOREG) &&
         rq->vd_clntname && rq->vd_clntname[0] ) {
	if (debug_f)
	    syslog(LOG_DEBUG, "Autoreg starts");
	if (handle_diskless(rq, &rp))
	    return;
	fromaddr = rp.bp_yiaddr;
    }
    /*
     * If we get here and the 'from' address is still zero, that
     * means we don't recognize the client and we can't pass the buck.
     * So we have to give up.
     */
    if (fromaddr.s_addr == 0)
	return;
    
    if (rq->bp_file[0] == 0) {
	/* client didnt specify file */
	if (hp == (struct hosts *)0 || hp->bootfile[0] == 0)
	    strcpy(file, defaultboot);
	else
	    strcpy(file, hp->bootfile);
    } else {
	/* client did specify file */
	strcpy(file, (const char *)rq->bp_file);
    }
    if (file[0] == '/')	/* if absolute pathname */
	strcpy(path, file);
    else {			/* else look in boot directory */
	strcpy(path, homedir);
	strcat(path, "/");
	strcat(path, file);
    }
    /* try first to find the file with a ".host" suffix */
    n = strlen(path);
    if (hp != (struct hosts *)0 && hp->host[0] != 0) {
	strcat(path, ".");
	strcat(path, hp->host);
    }
    if (access(path, R_OK) < 0) {
	path[n] = 0;	/* try it without the suffix */
	if (access(path, R_OK) < 0) {
	    /*
	     * We don't have the file.  Don't respond unless
	     * the client asked for us by name, in case some
	     * other server does have the file.  If he asked
	     * for us by name, send him back a null pathname
	     * so that he knows we don't have his boot file.
	     */
	    if (rq->bp_sname[0] == 0)
		return;		/* didnt ask for us */
	    syslog(LOG_ERR, "%s requested boot file %s: access failed: %m",
		   iaddr_ntoa(fromaddr), path);
	    path[0] = '\0';
	}
    }
    if (path[0] != '\0') {
	if (strlen(path) > sizeof(rp.bp_file)-1) {
	    syslog(LOG_ERR, "%s: reply boot file name %s too long",
		   iaddr_ntoa(fromaddr), path);
	    path[0] = '\0';
	} else
	    syslog(LOG_INFO, "reply to %s: boot file %s", 
		   iaddr_ntoa(fromaddr), path);
    }
    strcpy((char *)rp.bp_file, path);
    sendreply(&rp, 0, has_sname, 0, BOOTSTRAP_REPLY);
}

/*
 * Select the address of the interface on this server that is
 * on the same net as the 'from' address.
 *
 * Return value
 *	TRUE if match
 *	FALSE if no match
 */
int
bestaddr(iaddr_t *from, iaddr_t *answ)
{
    register struct netaddr *np;
    int match = 0;

    if (from->s_addr == 0) {
		answ->s_addr = myhostaddr.s_addr;
    } else {
	/*
	 * Search the table of nets to which the server is connected
	 * for the net containing the source address.
	 */
	for (np = nets; np->netmask != 0; np++)
	    if ((from->s_addr & np->netmask) == np->net) {
		answ->s_addr = np->myaddr.s_addr;
		match = 1;
		break;
	    }

	/*
	 * If no match in table, default to our 'primary' address
	 */
	if (np->netmask == 0)
	    answ->s_addr = myhostaddr.s_addr;
    }
    return match;
}


/*
 * Forward a BOOTREQUEST packet to another server.
 *
 *	RFC 951 (7.3) implies no-forward in case
 *		bp_ciaddr = 0.
 */
void
forward(register struct bootp *bp, register iaddr_t *from, register iaddr_t *dest)
{
    struct sockaddr_in to;

    /*
     * If hop >= 3, just discard(RFC951 says so).
     */
    if (bp->bp_hops >= 3)
		return;
    bp->bp_hops++;

    /*
     * If giaddr is 0, then I am the immediate gateway.
     * So, set myaddr for successing forwarders to send
     * reply to me directly.
     */
    if (!bp->bp_giaddr.s_addr) {
	(void) bestaddr(from, &bp->bp_giaddr);
    }

    to.sin_family = AF_INET;
    to.sin_port = htons(IPPORT_BOOTPS);
    to.sin_addr.s_addr = dest->s_addr;	/* already in network order */

    if (debug_f)
	syslog(LOG_DEBUG, "forwarding BOOTP request to %s(%s)",
		bp->bp_sname, iaddr_ntoa(*dest));

    if (sendto(s, (caddr_t)bp, sizeof *bp, 0, &to, sizeof to) < 0)
	syslog(LOG_ERR, "forwarding to %s failed (%m)", 
		iaddr_ntoa(to.sin_addr));
}


/*
 * Read bootptab database file.  Avoid rereading the file if the
 * write date hasnt changed since the last time we read it.
 */
void
readtab(void)
{
    struct stat st;
    register char *cp;
    int v;
    register u_long i;
    char temp[64], tempcpy[64];
    register struct hosts *hp;
    int skiptopercent;

    if (fp == 0) {
	if ((fp = log_fopen(bootptab, "r")) == NULL) {
	    syslog(LOG_ERR, "can't open %s", bootptab);
	    exit(1);
	}
    }
    fstat(fileno(fp), &st);
    if (st.st_mtime == modtime && st.st_nlink)
	return;	/* hasnt been modified or deleted yet */
    fclose(fp);
    if ((fp = log_fopen(bootptab, "r")) == NULL) {
	syslog(LOG_ERR, "can't open %s", bootptab);
	exit(1);
    }
    fstat(fileno(fp), &st);
    if (debug_f)
	syslog(LOG_DEBUG, "(re)reading %s", bootptab);
    modtime = st.st_mtime;
    homedir[0] = defaultboot[0] = 0;
    nhosts = 0;
    hp = &hosts[0];
    linenum = 0;
    skiptopercent = 1;

    /*
     * read and parse each line in the file.
     */
    for (;;) {
	if (fgets(line, sizeof line, fp) == NULL)
	    break;	/* done */
	if ((i = strlen(line)))
	    line[i-1] = 0;	/* remove trailing newline */
	linep = line;
	linenum++;
	if (line[0] == '#' || line[0] == 0 || line[0] == ' ')
	    continue;	/* skip comment lines */
	/* fill in fixed leading fields */
	if (homedir[0] == 0) {
	    getfield(homedir, sizeof homedir);
	    continue;
	}
	if (defaultboot[0] == 0) {
	    getfield(defaultboot, sizeof defaultboot);
	    continue;
	}
	if (skiptopercent) {	/* allow for future leading fields */
	    if (line[0] != '%')
		continue;
	    skiptopercent = 0;
	    continue;
	}
	/* fill in host table */
	getfield(hp->host, sizeof hp->host);
	getfield(temp, sizeof temp);
	sscanf(temp, "%d", &v);
	hp->htype = v;
	getfield(temp, sizeof temp);
	strcpy(tempcpy, temp);
	cp = tempcpy;
	/* parse hardware address */
	for (i = 0 ; i < sizeof hp->haddr ; i++) {
	    char *cpold;
	    char c;
	    cpold = cp;
	    while (*cp != '.' && *cp != ':' && *cp != 0)
		cp++;
	    c = *cp;	/* save original terminator */
	    *cp = 0;
	    cp++;
	    if (sscanf(cpold, "%x", &v) != 1)
		goto badhex;
	    hp->haddr[i] = v;
	    if (c == 0)
		break;
	}
	if (hp->htype == 1 && i != 5) {
badhex:	    syslog(LOG_ERR, "bad hex address: %s at line %d of bootptab",
		   temp, linenum);
	    continue;
	}
	getfield(temp, sizeof temp);
	i = inet_addr(temp);
	if (i == -1) {
	    register struct hostent *hep;
	    hep = gethostbyname(temp);
	    if (hep != 0 && hep->h_addrtype == AF_INET)
		i = *(int *)(hep->h_addr_list[0]);
	}
	if (i == -1 || i == 0) {
	    syslog(LOG_ERR, "bad internet address: %s at line %d of bootptab",
		   temp, linenum);
	    continue;
	}
	hp->iaddr.s_addr = i;
	getfield(hp->bootfile, sizeof hp->bootfile);
	if (++nhosts >= MHOSTS) {
	    syslog(LOG_ERR, "bootptab exceeds max. number of hosts (%d)", MHOSTS);
	    exit(1);
	}
	hp++;
    }
}


/*
 * Get next field from 'line' buffer into 'str'.  'linep' is the 
 * pointer to current position.
 */
void
getfield(char *str, int len)
{
    register char *cp = str;

    for ( ; *linep && (*linep == ' ' || *linep == '\t') ; linep++)
	;	/* skip spaces/tabs */
    if (*linep == 0) {
	*cp = 0;
	return;
    }
    len--;	/* save a spot for a null */
    for ( ; *linep && *linep != ' ' && *linep != '\t' ; linep++) {
	*cp++ = *linep;
	if (--len <= 0) {
	    *cp = 0;
	    syslog(LOG_ERR, "string truncated: %s, on line %d of bootptab",
		   str, linenum);
	    return;
	}
    }
    *cp = 0;
}

/*
 * Setup the arp cache so that IP address 'ia' will be temporarily
 * bound to hardware address 'ha' of length 'len'.
 */
void
setarp(iaddr_t *ia, u_char *ha, int len)
{
    struct sockaddr_in *si;
    struct	arpreq arpreq;	/* arp request ioctl block */

    arpreq.arp_pa.sa_family = AF_INET;
    si = (struct sockaddr_in *)&arpreq.arp_pa;
    si->sin_addr = *ia;
    arpreq.arp_ha.sa_family = AF_UNSPEC;
    bcopy(ha, arpreq.arp_ha.sa_data, len);
    if (ioctl(s, SIOCSARP, (caddr_t)&arpreq) < 0)
	syslog(LOG_ERR, "set arp ioctl failed (%m)");
}

/*
 * Build a list of all the names and aliases by which
 * the current host is known on all of its interfaces.
 */
void
makenamelist(void)
{
    register struct hostent *hp;
    char name[64];

    numaddrs = 0;
    myaddrs = (u_int *)malloc(max_addr_alloc*sizeof(u_int));

    /*
     * Get name of host as told to the kernel and look that
     * up in the hostname database.
     */
    gethostname(name, sizeof(name));
    if ((hp = gethostbyname(name)) == 0) {
	syslog(LOG_ERR, "gethostbyname(%s) failed: %s", name, hstrerror(h_errno));
	exit(1);
    }
    strcpy(myname, name);

    /*
     * Remember primary Internet address
     */
    myhostaddr.s_addr = *(u_long *)(hp->h_addr_list[0]);
}
