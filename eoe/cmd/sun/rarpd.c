/*
 * Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
 *
 * NAME
 *	rarpd - Reverse Address Resolution Protocol daemon
 * SYNOPSIS
 *	rarpd [-dv] [interface ...]
 * AUTHOR
 *	Brendan Eich, 4/27/89
 */
#include <bstring.h>
#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/raw.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <net/if_types.h>
#include <sys/fddi.h>
#include <sys/capability.h>

/*
 * RARP protocol definitions.  A RARP packet looks like an ARP packet, but
 * with different Ethernet type and ARP operation codes.  Drain reception
 * requires padding before the Ethernet header to align subsequent headers
 * on natural boundaries.
 */
#define	ETHERTYPE_RARP	0x8035

#define	RARPOP_REQUEST	3
#define	RARPOP_REPLY	4

#define	ETHERHDRPAD	RAW_HDRPAD(sizeof(struct ether_header))
#define	ETHERHDRSIZE	(sizeof(struct ether_header) + ETHERHDRPAD)

#define	FDDIHDRSIZE	(sizeof(struct fddi))	/* includes pad/filler */

#define	ARP_ETHERSIZE	(ETHERHDRSIZE + sizeof(struct ether_arp))
#define	ARP_FDDISIZE	(FDDIHDRSIZE + sizeof(struct ether_arp))

/*
 * Union structure for ethernet and fddi interfaces.
 */
struct rarp_packet {
	union {
		struct {
			char			pad0[FDDIHDRSIZE-ETHERHDRSIZE];
			char			pad[ETHERHDRPAD];
			struct ether_header	header;
		} enet_hdr;

		struct fddi	fddi_hdr;	/* padding is included */
	} if_u;
	struct ether_arp	rarp;
};
#define	etherpad	if_u.enet_hdr.pad
#define	ether		if_u.enet_hdr.header
#define	fddihead	if_u.fddi_hdr

#define	ether_dst	ether.ether_dhost
#define	ether_src	ether.ether_shost
#define	rarp_hrd	rarp.arp_hrd
#define	rarp_pro	rarp.arp_pro
#define	rarp_hln	rarp.arp_hln
#define	rarp_pln	rarp.arp_pln
#define	rarp_op		rarp.arp_op
#define	rarp_sha	rarp.arp_sha
#define	rarp_spa	rarp.arp_spa
#define	rarp_tha	rarp.arp_tha
#define	rarp_tpa	rarp.arp_tpa

/*
 * Constants, some globals, and forward declarations.  Each interface listed
 * as an argument (or each live, broadcast IP interfaces if no args are given)
 * is represented by a struct interface, defined later.  There can be dynamic 
 * number of served interfaces.  The default buffer reservation for raw
 * sockets of 2048 is increased to RECVSPACE for each interface.
 *
 * To map a RARP client's Ethernet address to its IP address, the daemon maps
 * Ethernet to hostname, and then hostname to IP, wherefore HOSTNAMESIZE.
 */
/*
#define	MAXINTERFACES	8
*/
#define	RECVSPACE	(4 * 1024)
#define	HOSTNAMESIZE	64

char	*progname;	/* name invoked as, for error reporting */
int	debugging;	/* whether to stay in foreground for easy debugging */
FILE	*logfile;	/* if non-null, open log file pointer */

int	perrorf(char *, ...);
void	logprintf(char *, ...);
void	drainall(int);
void	drainone(int, int, char**);
void	drain(char *, struct in_addr);
void	serve(void);
void	reply(struct interface *, struct rarp_packet *, int);

struct interface {
	int		if_type;		/* enet/fddi; see if_types.h */
	char		name[RAW_IFNAMSIZ+1];	/* interface name */
	int		sock;			/* open drain socket */
	u_char		etheraddr[6];		/* hardware address */
	struct in_addr	ipaddr;			/* protocol address */
} *intp;

int numinterfaces;


/*
 * Main's job is to close open files, process arguments, background the
 * server, and finish severing the child's connection with its parent's
 * environment.  It opens an AF_INET datagram socket, because one must do
 * ioctls on a SOCK_DGRAM socket to operate on BSD network interfaces.
 */
main(int argc, char **argv)
{
	int fd, opt, pid;

	progname = argv[0];
	for (fd = getdtablesize(); --fd > 2; )
		close(fd);
	fclose(stdout);
	fclose(stdin);

	while ((opt = getopt(argc, argv, "dl:")) != EOF) {
		switch (opt) {
		  case 'd':
			debugging = 1;
			break;
		  case 'l':
			logfile = fopen(optarg, "a");
			if (logfile == 0)
				exit(perrorf("cannot create %s", optarg));
			setvbuf(logfile, (char *) 0, _IOLBF, BUFSIZ);
			break;
		  default:
			fprintf(stderr,
				"usage: %s [-d] [-l logfile] [interface ...]\n",
				progname);
			exit(-1);
		}
	}
	argc -= optind;
	argv += optind;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
		exit(perrorf("cannot create socket"));
	if (argc == 0)
		drainall(fd);
	else
		drainone(fd, argc, argv);
	close(fd);

	if (!debugging) {
		pid = fork();
		if (pid < 0)
			exit(perrorf("cannot fork"));
		if (pid != 0)
			exit(0);
		fd = open("/dev/tty", 0);
		if (fd >= 0) {
			ioctl(fd, TIOCNOTTY, 0);
			close(fd);
		}
		fclose(stderr);
		openlog(progname, LOG_PID|LOG_NOWAIT, LOG_DAEMON);
	}
	serve();
	/* NOTREACHED */
}

/*
 * Given a struct ifreq pointer, get a struct sockaddr_in pointer to
 * its address member.
 */
#define	ifr_sin(ifr)	((struct sockaddr_in *) &(ifr)->ifr_addr)

/*
 * Use sock, an AF_INET datagram socket, to query the kernel for a list
 * of configured interfaces and to learn their IP address.  Drain for RARP
 * traffic on those which are IP, up, and broadcast-y.
 */
void
drainall(int sock)
{
	struct ifconf ifconf;
	char buf[1024];
	int count;
	struct ifreq *ifr;

	ifconf.ifc_len = sizeof buf;
	ifconf.ifc_buf = buf;
	if (ioctl(sock, SIOCGIFCONF, (char *) &ifconf) < 0)
		exit(perrorf("cannot get interface configuration"));
	count = ifconf.ifc_len / sizeof *ifr;

	intp = (struct interface *)malloc(count * sizeof(struct interface));
	if (intp == NULL) {
		perrorf("malloc failed");
		return;
	}

	for (ifr = ifconf.ifc_req; --count >= 0; ifr++) {
		struct in_addr ipaddr;

		if (ifr->ifr_addr.sa_family != AF_INET)
			continue;
		ipaddr = ifr_sin(ifr)->sin_addr;
		if (ioctl(sock, SIOCGIFFLAGS, (char *) ifr) < 0) {
			perrorf("cannot get interface flags from %s",
				ifr->ifr_name);
			continue;
		}
		if ((ifr->ifr_flags & IFF_UP)
		    && (ifr->ifr_flags & IFF_BROADCAST)) {
			drain(ifr->ifr_name, ipaddr);
		}
	}
}

/*
 * Drain for RARP traffic on the interface named by ifname, using the given
 * IP datagram socket to find ifname's IP address.
 */
void
drainone(int sock, int argc, char **argv)
{
	struct ifreq ifreq;
	char *ifname;

	intp = (struct interface *)malloc(argc * sizeof(struct interface));
	if (intp == NULL) {
		perrorf("malloc failed");
		return;
	}
	while (--argc >= 0) {
		bzero((char *)&ifreq, sizeof(ifreq));
		ifname = *argv++;
		(void) strncpy(ifreq.ifr_name, ifname, sizeof ifreq.ifr_name);
		if (ioctl(sock, SIOCGIFADDR, (char *) &ifreq) < 0)
			perrorf("cannot get address of %s", ifname);
		if (ifreq.ifr_addr.sa_family != AF_INET) {
			perrorf("cannot serve on non-IP interface %s", ifname);
			return;
		}
		drain(ifname, ifr_sin(&ifreq)->sin_addr);
	}
}

/*
 * RARP network interface table.  Each interface has a zero-terminated name,
 * an open drain socket, and its Ethernet and IP addresses.  The addresses
 * are used when formulating a reply to a RARP request.
 */


/*
 * Drain for RARP traffic on the named interface, and allocate an interface
 * table entry for this name, drain socket, and IP address.  Given a drain
 * socket bound to an interface, it's easy to find the interface's Ethernet
 * address.
 */
void
drain(char *ifname, struct in_addr ipaddr)
{
	int sock, space;
	struct sockaddr_raw sr;
	struct ifreq ifreq;
	struct interface *ifp;
	cap_t ocap;
	cap_value_t cap_priv_port = CAP_PRIV_PORT;
	static int toomany;

	ocap = cap_acquire(1, &cap_priv_port);
	sock = socket(AF_RAW, SOCK_RAW, RAWPROTO_DRAIN);
	cap_surrender(ocap);
	if (sock < 0) {
		perrorf("cannot create drain socket");
		return;
	}

	sr.sr_family = AF_RAW;
	(void) strncpy(sr.sr_ifname, ifname, sizeof sr.sr_ifname);
	sr.sr_port = htons(ETHERTYPE_RARP);
	if (bind(sock, (struct sockaddr *) &sr, sizeof sr) < 0) {
		perrorf("cannot bind to %s", ifname);
		close(sock);
		return;
	}

	(void) strncpy(ifreq.ifr_name, ifname, sizeof ifreq.ifr_name);
	if (ioctl(sock, SIOCGIFADDR, (char *) &ifreq) < 0) {
		perrorf("cannot get raw address of %s", ifname);
		close(sock);
		return;
	}

	space = RECVSPACE;
	if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char *) &space,
		       sizeof space) < 0) {
		perrorf("cannot reserve %d bytes for reception on %s",
			space, ifname);
	}

	ifp = (struct interface *)((char*)intp+numinterfaces * sizeof *ifp);
	numinterfaces++;

	/*
	 * XXX Set the type of the interface: ethernet vs fddi.
	 * Is looking at the interface name the only way to determine
	 * the type of interface? i.e imf/ipg/xpi implies fddi.
	 * Can read /dev/kmem for the interface list and check the
	 * if_type field. This way the dependency on interface name
	 * is removed.
	 */
	if (   strncmp(ifname, "imf", 3) == 0
	    || strncmp(ifname, "ipg", 3) == 0
	    || strncmp(ifname, "xpi", 3) == 0 )
		ifp->if_type = IFT_FDDI;
	else /* must be ethernet */
		ifp->if_type = IFT_ETHER;
	(void) strncpy(ifp->name, ifname, sizeof ifp->name);
	ifp->sock = sock;
	bcopy(ifreq.ifr_addr.sa_data, ifp->etheraddr, sizeof ifp->etheraddr);
	ifp->ipaddr = ipaddr;
}

/*
 * RARP server.  From here on, use syslog rather than stderr unless we
 * are debugging.
 *
 * Initialize a select-for-readability fd_set from the interface table's
 * drain socket descriptors and select forever.  For each readable socket,
 * grab the request packet and call reply.
 */
int	dosyslog;

void
serve()
{
	int count, max;
	fd_set readfds;
	struct interface *ifp;

	dosyslog = !debugging;

	count = numinterfaces;
	if (count == 0) {
		logprintf("no serviceable interfaces\n");
		exit(0);
	}
	max = 0;
	FD_ZERO(&readfds);
	for (ifp = intp; --count >= 0; ifp++) {
		if (ifp->sock + 1 > max)
			max = ifp->sock + 1;
		FD_SET(ifp->sock, &readfds);
	}


	for (;;) {
		fd_set rfds;
		struct rarp_packet rarp;

		rfds = readfds;
		count = select(max, &rfds, (fd_set *) 0, (fd_set *) 0,
			       (struct timeval *) 0);
		if (count < 0) {
			if (errno == EINTR)
				continue;
			exit(perrorf("select"));
		}

		for (ifp = intp; count > 0; ifp++) {
			int len;

			if (!FD_ISSET(ifp->sock, &rfds))
				continue;
			/*
			 * Drain(7P) prepends pad bytes for congruence with
			 * RAW_ALIGNGRAIN.
			 */
			if (ifp->if_type == IFT_ETHER)
				len = read(ifp->sock, (char *)&rarp.etherpad[0],
					   ARP_ETHERSIZE);
			else
				len = read(ifp->sock, (char *)&rarp.fddihead,
					   ARP_FDDISIZE);
			if (len < 0)
				perrorf("cannot read from %s", ifp->name);
			else
				reply(ifp, &rarp, len);
			--count;
		}
	}
}

/*
 * Validate a RARP request and reply to it if possible.
 */
void
reply(struct interface *ifp, struct rarp_packet *rarp, int len)
{
	char hostname[HOSTNAMESIZE];
	struct hostent *hp;
	char *start;
	int pktsize;

	pktsize = (ifp->if_type == IFT_ETHER) ? ARP_ETHERSIZE : ARP_FDDISIZE;

	if (len < pktsize
	    || rarp->rarp_hrd != ARPHRD_ETHER
	    || rarp->rarp_pro != ETHERTYPE_IP
	    || rarp->rarp_hln != sizeof rarp->rarp_sha
	    || rarp->rarp_pln != sizeof rarp->rarp_spa
	    || rarp->rarp_op != RARPOP_REQUEST) {
		logprintf("malformed request from %s\n",
			  ether_ntoa(rarp->ether_src));
		return;
	}

	if (bcmp(rarp->rarp_sha, rarp->rarp_tha, rarp->rarp_hln) == 0) {
		/*
		 * Huh?  The sender apparently doesn't want a reply...
		 * From RFC 903, on the format of a RARP request:
		 *
		 *  "In the case where the sender wishes to determine his own
		 *   protocol address, [arp_tha], like [arp_sha], will be the
		 *   hardware address of the sender."
		 */
		logprintf("idle request from %s\n",
			  ether_ntoa(rarp->ether_src));
		/*
		 * Used to just "return" at this point, but let's reply
		 * anyways cuz it shouldn't hurt.
		 *
		 * return;
		 */
	}

	/*
	 * Map Ethernet address to hostname, then hostname to IP address.
	 */
	if (ether_ntohost(hostname, rarp->rarp_tha) != 0) {
		logprintf("no ethers entry for %s\n",
			  ether_ntoa(rarp->ether_src));
		return;
	}
	hp = gethostbyname(hostname);
	if (hp == 0) {
		logprintf("no hosts entry for %s\n",
			  ether_ntoa(rarp->ether_src));
		return;
	}

	/*
	 * Turn the packet around and send it back to the client.
	 */
	rarp->rarp_op = RARPOP_REPLY;
	bcopy(ifp->etheraddr, rarp->rarp_sha, sizeof rarp->rarp_sha);
	bcopy((char *) &ifp->ipaddr, rarp->rarp_spa, sizeof rarp->rarp_spa);
	bcopy(hp->h_addr, rarp->rarp_tpa, sizeof rarp->rarp_tpa); /* answer */

	if (ifp->if_type == IFT_ETHER) {
		bcopy(rarp->ether_src, rarp->ether_dst, sizeof rarp->ether_dst);
		bcopy(ifp->etheraddr, rarp->ether_src, sizeof rarp->ether_src);
		if (rarp->ether.ether_type != htons(ETHERTYPE_RARP)) {
			logprintf("broken drain input: expected %#x, got %#x\n",
				ETHERTYPE_RARP, ntohs(rarp->ether.ether_type));
			rarp->ether.ether_type = htons(ETHERTYPE_RARP);
		}
	}
	else
	{
#define mac	if_u.fddi_hdr.fddi_mac
#define ftype	if_u.fddi_hdr.fddi_llc.ullc.c.etype
		bcopy(&rarp->mac.mac_sa.b[0], &rarp->mac.mac_da.b[0], 6);
		bcopy(ifp->etheraddr, &rarp->mac.mac_sa.b[0], 6);
			/* address should be in fddi bit order */
		/*
		 * XXX Check type like above? Does the type value,
		 * ETHERTYPE_RARP need to be bitswaped (0x8035 ==> 0x01AC)
		 * to fddi bit order?
		 *
		 * #define FDDI_RARP ?????
		 * if (rarp->ftype != htons(FDDI_RARP)) {
		 *	logprintf("broken drain input: expected %#x, got %#x\n",
		 *		FDDI_RARP, ntohs(rarp->ftype));
		 *	rarp->ftype = htons(FDDI_RARP);
		 * }
		 */
#undef mac
#undef ftype
	}

	if (ifp->if_type == IFT_ETHER) {
		start = (char *)&rarp->ether;
		len -= ETHERHDRPAD;	/* Remove drain socket padding */
	}
	else
	{
		start = (char *)&rarp->fddihead;
	}
	if (write(ifp->sock, start, len) != len)
		perrorf("cannot write to %s", ifp->name);
	logprintf("resolved %s to %s\n",
		  ether_ntoa(rarp->ether_src), hp->h_name);
}

/*
 * Formatted perror, with progname prefixing and syslog redirection.
 */
int
perrorf(char *format, ...)
{
	int error;
	va_list ap;
	char buf[BUFSIZ];

	error = errno;
	va_start(ap, format);
	if (!dosyslog) {
		fprintf(stderr, "%s: ", progname);
		vfprintf(stderr, format, ap);
		if (error)
			fprintf(stderr, ": %s.\n", strerror(error));
	} else {
		vsprintf(buf, format, ap);
		if (error)
			syslog(LOG_ERR, "%s: %m", buf);
		else
			syslog(LOG_ERR, buf);
	}
	va_end(ap);
	return error;
}

void
logprintf(char *format, ...)
{
	va_list ap;

	if (logfile == 0)
		return;
	va_start(ap, format);
	vfprintf(logfile, format, ap);
	va_end(ap);
}
