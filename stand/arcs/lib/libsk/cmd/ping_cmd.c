#ident	"lib/libsc/cmd/ping_cmd.c:  $Revision: 1.12 $"

/*
 * ping server at NETADDR
 */
#include <arcs/io.h>
#include <arcs/errno.h>
#include <arcs/eiob.h>
#include <arcs/time.h>

struct mbuf;		/* local definitions to quiet irix net headers */
struct ifnet;

#include <sys/param.h>
#include <net/in_systm.h>
#include <net/in.h>
#include <net/ip.h>
#include <netinet/ip_icmp.h>
#include <net/ip_var.h>
#include <net/udp.h>
#include <net/udp_var.h>
#include <saio.h>
#include <saioctl.h>
#include <net/socket.h>
#include <net/arp.h>
#include <net/ei.h>

#include <libsc.h>
#include <libsk.h>
#ifdef SN0
#include <pgdrv.h>
#endif

#define TRUE	1
#define FALSE	0

#define DATALEN 64-8				/* UDP DATA */
#define PACKLEN DATALEN+60+76			/* MAXIP + MAXICMP */

#define FLOOD_INTERVAL	10000			/* 10 ms, for us_delay () */
#define	ONE_SECOND	1000000			/* 1 second, for us_delay() */

#define DEFAULT_COUNT	4			/* 4 pings, no more */

static unsigned ping_interval;
static unsigned floodmode;
static unsigned ping_count;

/* socket and file related variables */
#define cei(x)  ((struct ether_info *)(x->iob.DevPtr))

static struct sockaddr_in whereto;
static char obuf[PACKLEN];
static char ibuf[PACKLEN];

#if SN0
#ifdef SN_PDI
static char *NetworkName =
"/dev/network/0" ;
#else
static char *NetworkName = 
"/xwidget/alias_bridge0/pci/master_ioc3/ef/" ;
#endif
#else
static char *NetworkName = "network(0)";
#endif

extern char netaddr_default[];

int in_cksum(u_short *addr, int len);

/*ARGSUSED*/
int
ping(int argc, char **argv, char **envp, struct cmd_table *bunk2)
{
    struct sockaddr_in *to = (struct sockaddr_in *) &whereto;
    register char *bp, *cp;
    struct in_addr myiaddr, piaddr;
    ULONG fd, err;
    struct icmp *icp;
    int flags = 0;
    int iter = 1;
    int cc;
    struct eiob *io;
    FSBLOCK * fsb;

    ping_interval = ONE_SECOND;
    ping_count = DEFAULT_COUNT;
    floodmode = FALSE;

    while (--argc > 0) {
	argv++;
	if (!*argv)             /* allow empty argv slots */
	    continue;

	if ((*argv)[0] == '-') /* flag variables */
	{
	    flags++;
	    switch ((*argv)[1]) {

		case 'f':	/* enable flood mode */
		    floodmode = TRUE;
		    ping_interval = FLOOD_INTERVAL;
		    break;

		case 'c':	/* change default number of pings */
		    if (--argc <= 0)
			return(1);
		    atobu(*++argv, &ping_count);
		    break;

		case 'i':	/* change default interval in seconds */
		    if (--argc <= 0)
			return(1);
		    atobu(*++argv, &ping_interval);
		    ping_interval *= ONE_SECOND;
		    break;

		default:	/* exit if bad flag */
		    printf("%s is an unknown flag!!!\n", flags, argv[0]);
		    return(1);
	    }
	    printf("flag %d was %s\n", flags, argv[0]);
	}
	else			/* no more flags */
	{
	    break;
	}
    }

    if (argc != 1)	/* must be a net address, and only one, left */
	return(1);

    bp = *argv;		/* should be ID of target machine */

    /*
     * figure out if target has a valid IA address
     * these routines adapted from libsc/cmd/bootp.c
     */
    piaddr.s_addr = 0;			/* clear address */
    piaddr = inet_addr(bp);		/* parse string */
    if (ntohl(piaddr.s_addr) == (unsigned)-1)
    {
	printf("%s is not a valid Internet address\n", bp);
	return(1);
    }

    /*
     * now figure out the IA address of this machine
     */

    myiaddr.s_addr = 0;
    if ((cp = getenv("netaddr")) != NULL && *cp != '\0')
    {
	/*
	 * If Internet address is set in the environment, then
	 * configure the interface for that IP address.
	 */
	myiaddr = inet_addr(cp);
	if (!strcmp(cp, netaddr_default))
	{
	    printf("%s%s%s",
"Warning: 'netaddr' is set to the default address ", netaddr_default,
".\nUse 'setenv' to reset it to an Internet address on your network.\n");
	}
	if (ntohl(myiaddr.s_addr) == (unsigned)-1)
	{
	    printf("$netaddr is not a valid Internet address\n");
	    return(1);
	}
    }

    /*
     * both target and this machine have valid IA addresses, so do
     * whatever setup necessary
     */

    bzero((char *)&whereto, sizeof(struct sockaddr));

    /*
    printf("Opening %s ...\n", NetworkName);
    */
    if  ((err = Open(NetworkName, OpenReadWrite, &fd)) != ESUCCESS) {
	printf("Error in opening file %s (err = %d)\n",
		err, NetworkName);
	goto ExitPt;
    }

    /* get pointer to ioblock */
    io = get_eiob(fd);	
    fsb = &io->fsb;

    /*
     * 3152 is an arbitrary port number, stolen from symmon
     * bind this port and the network address to the open file
     */

    /* bind port number */
    io->iob.IoctlCmd = (IOCTL_CMD)(__psint_t)NIOCBIND;
    io->iob.IoctlArg = (IOCTL_ARG)htons((u_short)3152);
    if ((err = _ifioctl(&(io->iob))) != ESUCCESS)
    {
	printf("Error in binding (err = %d)\n", err);
	goto ExitPt;
    }

    /* bind network address */
    io->iob.FunctionCode = FC_IOCTL;
    io->iob.IoctlCmd = (IOCTL_CMD)(__psint_t)NIOCSIFADDR;
    io->iob.IoctlArg = (IOCTL_ARG)&myiaddr;
    if ((err = _ifioctl(&(io->iob))) != ESUCCESS) {
	printf("Unable to set interface address\n");
	goto ExitPt;
    }

    /*
     * Set the destination 
     */
    to->sin_family = AF_INET;
    to->sin_port = IPPORT_ECHO;
    bcopy((const void *)&piaddr.s_addr,(caddr_t)&to->sin_addr,
	    sizeof(struct in_addr));

    /* stolen from libsk/lib/arcsio.c BindSource() - don't want to
     * build whole library in debug mode to get this
     */

    cei(io)->ei_dstaddr.sin_addr.s_addr = to->sin_addr.s_addr;
    cei(io)->ei_dstaddr.sin_port = to->sin_port;
    cei(io)->ei_dstaddr.sin_family = to->sin_family;

    bcopy((const void *)&cei(io)->ei_dstaddr,
	    (caddr_t)&cei(io)->ei_gateaddr, sizeof(struct sockaddr_in));

    /* print the output info */
    printf("PING (%s): %d data bytes\n",
	    inet_ntoa(piaddr), DATALEN);

    /*
     * loop through the pings
     */
    while ((ping_count) || (floodmode))
    {
	/* zero data in buffers, just in case */
	bzero(obuf, DATALEN+8);
	bzero(ibuf, DATALEN+8);

	/* build packet - stolen from irix/cmd/bsd/ping.c
	 * this is a phony icmp header - the real icmp header is wrapped
	 * around this by the low level code - but the info is useful for
	 * validation purposes.
	 */ 
	{
	    TIMEINFO *tod;

	    icp = (struct icmp *)obuf;

	    icp->icmp_type = ICMP_ECHO;
	    icp->icmp_code = 0;
	    icp->icmp_cksum = 0;
	    icp->icmp_seq = htons((u_short)(iter));
	    icp->icmp_id = cpuid() & 0xFFFF;	/* ID - using cpu as process */

	    cc = DATALEN+8;         /* skips ICMP portion */

	    /* Copy in TOD info */
	    tod = GetTime();
	    bcopy(tod,&obuf[8],sizeof(TIMEINFO));

	    /* Compute ICMP checksum here */
	    icp->icmp_cksum = in_cksum( (u_short *)icp, cc );
	}


	/* send packet */

        /*
         * set-up device independent iob parameters
         */
        fsb->IO->Address = obuf;
        fsb->IO->Count = DATALEN+8;
	DEVWRITE(fsb);
	
	/*
	 * attempt to receive reply
	 */
	fsb->IO->Count = PACKLEN;
	fsb->IO->Address = ibuf;
	do {
	    _scandevs();    /* allow for console interrupts */
#if SN0
	    /* Do this again, since _scandevs calling epoll()
	     * could have messed with Function code.
	     */
	    fsb->IO->Count = PACKLEN;
	    fsb->IO->Address = ibuf;
#endif
	} while ((cc = DEVREAD(fsb)) <= 0);

	/* print result */
	{
	    TIMEINFO *tod, *got;
	    unsigned short csum;
	    unsigned delta;

	    icp = (struct icmp *) ibuf;

	    cc = DATALEN+8;         /* skips ICMP portion */

	    /*
	     * Now the ICMP part
	     * just returned data and not real ICMP header, but we still
	     * can use it to validate data bounce, calculate elapsed time,
	     * and such
	     */
	    if (icp->icmp_type == ICMP_ECHO)
	    {
		if( icp->icmp_id != (cpuid() & 0xFFFF))
		{
		    printf("ECHO reply to unknown process\n");
		    goto END_RESULT;
		}

		if (icp->icmp_seq != iter)
		{
		    printf("wrong sequence number - got %d, expected %d\n",
			    icp->icmp_seq, iter);
		}

		/* save recieved cksum and recompute */
		csum = icp->icmp_cksum;
		icp->icmp_cksum = 0;
		if ((icp->icmp_cksum = in_cksum((u_short *)icp, cc)) != csum)
		{
		    printf("wrong checksum - got %d, expected %d\n",
			    csum, icp->icmp_cksum);
		}

		/*
		 * figure out trip time 
		 * Despite Field Name, "Milliseconds" is really 16 bit
		 * uS free-running counter. Hmm.
		 */
		tod = GetTime();
		got = (TIMEINFO *)&ibuf[8];
		if (tod->Milliseconds >= got->Milliseconds)
		    delta = tod->Milliseconds - got->Milliseconds;
		else
		    delta = (tod ->Milliseconds + 0xffff) - got->Milliseconds;
		delta /= 1000;
		
		/*
		 * print out all the nice info - since we don't have access
		 * to the ip header information without a major hack, we
		 * just assume that the echo source is the system we sent
		 * the request to
		 */
		printf("%d bytes from %s: icmp_seq=%u", cc,
			inet_ntoa(piaddr), ntohs((u_short)icp->icmp_seq));
		printf(" time=%d ms", delta);

	    }
	    else
	    {
                /* We've got something other than ECHO */
                printf("%d bytes from %s", cc, inet_ntoa(piaddr));
	    }

END_RESULT:	
	/* guarantee newline */
	printf("\n");
	}

	/* wait before pinging again */
	us_delay(ping_interval);

	iter++;
	if(!floodmode)
	    ping_count--;

	err= 0;
    }


ExitPt:
    Close(fd);
    return((int)err);
}

/*
 *                      I N _ C K S U M
 *
 * Checksum routine for Internet Protocol family headers (C Version)
 *
 */
int
in_cksum(u_short *addr, int len)
{
    register int nleft = len;
    register u_short *w = addr;
    register int sum = 0;
    u_short answer = 0;

    /*
     *  Our algorithm is simple, using a 32 bit accumulator (sum),
     *  we add sequential 16 bit words to it, and at the end, fold
     *  back all the carry bits from the top 16 bits into the lower
     *  16 bits.
     */
    while( nleft > 1 )  {
	sum += *w++;
	nleft -= 2;
    }

    /* mop up an odd byte, if necessary */
    if( nleft == 1 ) {
	*(u_char *)(&answer) = *(u_char *)w ;
	sum += answer;
    }

    /*
     * add back carry outs from top 16 bits to low 16 bits
     */
    sum = (sum >> 16) + (sum & 0xffff);     /* add hi 16 to low 16 */
    sum += (sum >> 16);                     /* add carry */
    answer = ~sum;                          /* truncate to 16 bits */
    return (answer);
}
