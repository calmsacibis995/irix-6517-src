/*
 * $Id: ping.c,v 1.2 1999/05/26 22:09:11 eddiem Exp $
 * ping - does blocking and non blocking pings
 */
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>

extern int ntransmitted;			/* sequence number */
static char *__progname;


#define MIN_IPICMPLEN sizeof(struct ip) + ICMP_MINLEN
#define BUFSIZE 2048  

#ifdef PRE_KUDZU
#include <cap_net.h>
#else
#define cap_socket socket
#endif

static void warn(const char *, ...);
static u_short in_cksum(u_short *, u_int);

static void warn(const char *pat, ...)
{
        int serrno;
        va_list args;

        serrno = errno;
        (void)fprintf(stderr, "%s: ", __progname);
        if (pat) {
                va_start(args, pat);
                (void)vfprintf(stderr, pat, args);
                (void)fprintf(stderr, ": ");
                va_end(args);
        }
        (void)fprintf(stderr, "%s\n", strerror(serrno));
}

int init_ping()
{
    int sd = 0;
    int ret = 0;
    int arg = 1;
    int off = 0;
    
    
    sd = cap_socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sd < 0)
	return(sd);
    
    ret = ioctl(sd, FIONBIO, &arg);
    if (ret < 0)
	return(ret);
    if (setsockopt(sd, SOL_SOCKET, SO_DONTROUTE, (char *)&off, sizeof(off)) == -1)
        warn ("Unable to disable SO_DONTROUTE on ICMP socket");
    return(sd);
}

int
send_ping(int sd, u_long ipaddr, u_short seqnum, u_short ident)
{
    static struct sockaddr_in to;
    static long force_word_align [  64 / sizeof (long) + 1];
    static u_char *opack = (u_char *) force_word_align;
    register struct icmp *icp;
    int rc;


    to.sin_addr.s_addr = htonl(ipaddr);
    to.sin_family = AF_INET;
	    
    icp = (struct icmp *) opack;
    icp->icmp_type = ICMP_ECHO;
    icp->icmp_code = 0;
    icp->icmp_cksum = 0;
    icp->icmp_seq = htons(seqnum);
    icp->icmp_id = ident;

    /* Compute ICMP checksum here */
    icp->icmp_cksum = htons(in_cksum((unsigned short *)icp, sizeof(struct icmp)));

    rc = sendto(sd, (const void *)opack, sizeof(struct icmp),  0,
			    (struct sockaddr *)&to, sizeof(struct sockaddr));
    return rc;
}


/* Compute the IP checksum
 *      This assumes the packet is less than 32K long.
 */
static u_short
in_cksum(u_short *p,
         u_int len)
{
    u_int sum = 0;
    int nwords = len >> 1;
    
    while (nwords-- != 0)
	sum += *p++;
    
    if (len & 1) {
	union {
	    u_short w;
	    u_char c[2];
	} u;
	u.c[0] = *(u_char *)p;
	u.c[1] = 0;
	sum += u.w;
    }
    
    /* end-around-carry */
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return (~sum);
}

struct icmp *
recv_icmp_msg( int sd, u_long *ipaddr, 
	       u_char *inbuf, int inbuflen)
{
    int	ret;
    struct sockaddr_in	from;
    struct icmp *icp;
    struct ip	*ip;
    int	len;
    int hlen;

    len = sizeof(from);

    if (inbuf == NULL )
	return((struct icmp *)NULL);
    
    ret = recvfrom( sd, (void *)inbuf, inbuflen, 0, (void *)&from, &len );
    if (ret < ICMP_MINLEN) {
	return ((struct icmp *) NULL);
    }
    if (ret < MIN_IPICMPLEN)
	return((struct icmp *)NULL);

    *ipaddr = ntohl(from.sin_addr.s_addr);
    ip = (struct ip *) inbuf;   /* Get whole packet */
    hlen = ip->ip_hl << 2;      /* Convert longwords to bytes */

    icp = (struct icmp*)(inbuf+hlen);
    return icp;
}


int
recv_echo_reply(int *seqnum,int *ident, struct icmp* icp)
{

    if (!icp) return(-1);
    
    if ( icp->icmp_type != ICMP_ECHOREPLY ) {
	return(-1);
    }
    
    *seqnum = ntohs(icp->icmp_seq);
    *ident = ntohs(icp->icmp_id);

    return(0);
}


int
send_blocking_ping(int sd, u_long addr, int timeoutSecs, int timeoutMsecs,
		   int tries)
{

    int ident = htons(getpid()) & 0xFFFF;
    int seqnum = ++ntransmitted;
    int seqstart = seqnum;
    int res_seq;
    int res_ident;
    u_long res_node;
    struct timeval timeout;
    u_char respack[2048];
    fd_set readfds;
    struct icmp *icmp_pack;
    int ret;

    while ( recv_icmp_msg(sd, &res_node, respack, sizeof(respack)) );
    
    while ( tries-- > 0 ) {
        ret = send_ping( sd, addr, seqnum++, ident);
        if ( (ret < 0) && (errno != EWOULDBLOCK) ) {
            return(-1);
        }
	
    reselect:
        FD_ZERO(&readfds);
        FD_SET(sd, &readfds);
        timeout.tv_sec = timeoutSecs;
        timeout.tv_usec = timeoutMsecs;
        ret = select( sd+1, &readfds, NULL, NULL, &timeout);
        if ( ret == 0 ) {       /* Timeout occurred */
	    continue;
        }
        if ( ret < 0 ) {
            return(-1);
        }
	
        icmp_pack = recv_icmp_msg( sd, &res_node, respack, 
				   sizeof(respack));
        if (icmp_pack == (struct icmp *)NULL) {
	    return(-1);
        }
	
        ret = recv_echo_reply( &res_seq, &res_ident, icmp_pack );
        if (ret == 0) {
            if ( (res_ident != ident) || (res_seq < seqstart) ||
                 (res_seq >= seqnum) || (res_node != addr) ) {
		goto reselect;
            }
            return 1;
	} else
            goto reselect;
    } 
    
    return(-2);
}

