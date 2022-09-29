#ident	"lib/libsk/net/udpip.c:  $Revision: 1.23 $"

#include <sys/param.h>
#include <net/in_systm.h>
#include <net/in.h>
#include <net/ip.h>
#include <net/ip_var.h>
#include <net/udp.h>

#include <net/udp_var.h>
#include <saio.h>
#include <net/socket.h>
#include <net/arp.h>
#include <net/ei.h>
#include <net/mbuf.h>

#include <libsc.h>
#include <libsk.h>

#define cei(x)	((struct ether_info *)(x->DevPtr))

static int in_cksum(struct mbuf *, int);


#define veprintf(a)		(Debug?printf a:0)

u_char	_ipcksum = 1;		/* require ip checksum good */

/*
 * Ip input routine.  Checksum and byte swap header.  If fragmented
 * try to reassemble.  If complete and fragment queue exists, discard.
 * Process options.  Pass to next level.
 */
void _ip_input(struct ifnet *ifp, struct mbuf *m)
{
	register struct ip *ip;
	int hlen;
	struct sockaddr_in *sin;

	if (m->m_len < sizeof (struct ip))
		goto bad1;

	ip = mtod(m, struct ip *);
	if ((hlen = ip->ip_hl << 2) > m->m_len) {
bad1:
		veprintf(("ip hdr len"));
		goto bad;
	}

	if (_ipcksum)
		if (ip->ip_sum = in_cksum(m, hlen)) {
			veprintf(("ip cksum"));
			goto bad;
		}

	/*
	 * Convert fields to host representation.
	 */
	ip->ip_len = ntohs((u_short)ip->ip_len);
	if (ip->ip_len < hlen) {
		veprintf(("ip len"));
		goto bad;
	}

	ip->ip_id = ntohs(ip->ip_id);
	ip->ip_off = ntohs((u_short)ip->ip_off);

	/*
	 * Check that the amount of data in the buffers
	 * is as at least much as the IP header would have us expect.
	 * Drop packet if shorter than we expect.
	 */
	if (m->m_len < ip->ip_len) {
		veprintf(("ip pkt len"));
		goto bad;
	}
	
	/*
	 * Trim excess data in mbuf past the ip length.
	 */
	m->m_len = ip->ip_len;

	if (hlen > sizeof (struct ip)) {
		veprintf(("ip options"));
		goto bad;
	}

	/*
	 * Only accept packets addressed directly to us,
	 * in particular, discard broadcast packets, unless
	 * the interface is configured for promiscuous reception.
	 */
	sin = (struct sockaddr_in *)&ifp->if_addr;
	if (sin->sin_addr.s_addr != ip->ip_dst.s_addr) {
		/*
		 * If promiscuous flag is set, receive the packet anyway.
		 * This mode is used by BOOTP in the case that we don't
		 * yet know our own IP address and we are awaiting the
		 * response packet from the BOOTP server containing
		 * our address.  The destination address on that packet
		 * will be our IP address, not the BCAST address.
		 */
		if ((ifp->if_flags & IFF_PROMISC) == 0) {
#ifdef	DEBUG
			veprintf(("no PROMISC"));
#endif
			goto bad;
	}
	}

	/*
	 * Adjust ip_len to not reflect header,
	 * abort if packet is fragmented.
	 */
	ip->ip_len -= hlen;
	if (ip->ip_off & (IP_MF|IP_OFFMASK)) {
		veprintf(("ip fragments"));
		goto bad;
	}

	if (ip->ip_p != IPPROTO_UDP) {
#ifdef	DEBUG
	    extern int Debug;

	    if (Debug)
		printf("not UDP error: %x\n", ip->ip_p);
#endif
bad:
		_m_freem(m);
		return;
	}

	_udp_input(ifp, m);
}

static u_short ip_id;

int
_ip_output(struct ifnet *ifp, struct mbuf *m, struct sockaddr_in *dstp)
{
	register struct ip *ip = mtod(m, struct ip *);
	int hlen = sizeof (struct ip);

	/*
	 * Fill in IP header.
	 */
	ip->ip_hl = hlen >> 2;
	ip->ip_v = IPVERSION;
	ip->ip_off &= IP_DF;
	ip->ip_id = htons(ip_id++);

	/*
	 * If source address not specified yet, use address
	 * of outgoing interface.
	 */
	if (inet_lnaof(ip->ip_src) == INADDR_ANY)
		ip->ip_src.s_addr =
		    ((struct sockaddr_in *)&ifp->if_addr)->sin_addr.s_addr;

	/*
	 * standalone ip doesn't allow messages to be fragmented
	 */
	if (ip->ip_len > ifp->if_mtu) {
		printf("ip packet too large to xmit\n");
		goto bad;
	}

	ip->ip_len = htons((u_short)ip->ip_len);
	ip->ip_off = htons((u_short)ip->ip_off);
	ip->ip_sum = 0;
	ip->ip_sum = in_cksum(m, hlen);
	return((*ifp->if_output)(ifp, m, (struct sockaddr *)dstp));

bad:
	_m_freem(m);
	return(-1);
}

/*
 * UDP protocol implementation.
 * Per RFC 768, August, 1980.
 */

/*
 * _udpcksum must be an int, it's referenced from _init_saio and
 * prom environment routines
 */
extern int	_udpcksum;	/* 0 => 4.2BSD compatibility, 1 => 4.3BSD */

static struct	sockaddr_in udp_in;

/*
 * _udp_input -- unwrap udp packet and hang on appropriate socket buffer
 */
/*ARGSUSED*/
void
_udp_input(struct ifnet *ifp, struct mbuf *m0)
{
	register struct udpiphdr *ui;
	struct so_table *st;
	int len;

	if (m0->m_len < sizeof (struct udpiphdr)) {
		veprintf(("udp hdr len"));
		goto bad;
	}
	ui = mtod(m0, struct udpiphdr *);
#ifdef notdef	/* redundant check ... ip_input takes care of this */
	if (((struct ip *)ui)->ip_hl > (sizeof (struct ip) >> 2)) {
		veprintf(("ip options"));
		goto bad;
	}
#endif

	/*
	 * If not enough data to reflect UDP length, drop.
	 * Note that _ip_input has already subtracted the
	 * the size of the ip header from the original
	 * value of ip_len (in host order).
	 */
	len = ntohs((u_short)ui->ui_ulen);
	if (len > ((struct ip *)ui)->ip_len) {
		veprintf(("udp pkt len"));
		goto bad;
	}

	/*
	 * Checksum extended UDP header and data.
	 */
	if (_udpcksum && ui->ui_sum) {
		ui->ui_next = ui->ui_prev = 0;
		ui->ui_x1 = 0;
		ui->ui_len = ui->ui_ulen;
		if (ui->ui_sum = in_cksum(m0, len + sizeof (struct ip))) {
			veprintf(("udp cksum"));
			goto bad;
		}
	}

	/*
	 * Make mbuf data length reflect UDP length.
	 * Set data pointer after UDP and IP headers.
	 */
	m0->m_len = len - sizeof (struct udphdr);
	m0->m_off += sizeof (struct udpiphdr);

	/*
	 * find appropriate socket buffer
	 */
	st = _find_socket(ui->ui_dport);
	if (st == 0) {
#ifdef	DEBUG
		extern int Debug;
		if (Debug)
		    printf("udp port error (%d)", ui->ui_dport);
#else
		veprintf(("udp port"));
#endif
		goto bad;
	}

	/*
	 * Construct sockaddr format source address.
	 * Stuff source address and datagram in user buffer.
	 */
	udp_in.sin_family = AF_INET;
	udp_in.sin_port = ui->ui_sport;
	udp_in.sin_addr = ui->ui_src;
	
	/*
	 * HANG ON SO_TABLE IF SPACE, TOSS OTHERWISE
	 */
	if (_so_append(st, (struct sockaddr *)&udp_in, m0) == 0) {
		veprintf(("udp sktbuf"));
		return;		/* so_append has m_free'd the mbuf */
	}
	return;

bad:
	_m_freem(m0);
}

int
_udp_output(iob, ifp, m0)
IOBLOCK *iob;
struct ifnet *ifp;
struct mbuf *m0;
{
	register struct udpiphdr *ui;
	int len;
	struct sockaddr_in dst;

	/*
	 * Calculate data length and get a mbuf
	 * for UDP and IP headers.
	 */
	len = m0->m_len;

	/*
	 * Fill in mbuf with extended UDP header
	 * and addresses and length put into network format.
	 */
	m0->m_off -= sizeof (struct udpiphdr);
	if (m0->m_off < MMINOFF) {
		printf("udp_output failure\n");
		return -1;
	}

	m0->m_len += sizeof (struct udpiphdr);
	ui = mtod(m0, struct udpiphdr *);
	ui->ui_next = ui->ui_prev = 0;
	ui->ui_x1 = 0;
	ui->ui_pr = IPPROTO_UDP;
	ui->ui_len = htons((u_short)len + sizeof (struct udphdr));
	/*
	 * CHECK THESE
	 */
	ui->ui_src = ((struct sockaddr_in *)&ifp->if_addr)->sin_addr;
	ui->ui_dst = cei(iob)->ei_dstaddr.sin_addr;
	ui->ui_sport = cei(iob)->ei_udpport;
	ui->ui_dport = cei(iob)->ei_dstaddr.sin_port;
	ui->ui_ulen = ui->ui_len;

	/*
	 * Gateway support
	 *
	 * The destination address in the ip header is the final
	 * destination.  The actual immediate destination on the
	 * local wire is passed to _ip_output in the third parameter.
	 * The IP layer of the gateway will forward the packet using
	 * normal IP routing.
	 */
	bzero((caddr_t)&dst, sizeof dst);
	dst.sin_family = AF_INET;
	dst.sin_addr = cei(iob)->ei_gateaddr.sin_addr;

	/*
	 * Stuff checksum and output datagram.
	 */
	ui->ui_sum = 0;
	if (_udpcksum) {
		if ((ui->ui_sum=in_cksum(m0, sizeof(struct udpiphdr)+len)) == 0)
			ui->ui_sum = (u_short)-1;
	}
	((struct ip *)ui)->ip_len = sizeof (struct udpiphdr) + len;
	((struct ip *)ui)->ip_ttl = MAXTTL;
	return(_ip_output(ifp, m0, &dst));
}

#ifdef NOTDEF
int _nocksum;
#endif

/*
 * Checksum routine for Internet Protocol family headers (mips version).
 *
 * This routine is very heavily used in the network
 * code and should be modified for each CPU to be as fast as possible.
 */
static int
in_cksum(struct mbuf *m, int len)
{
	int ck;
	extern unsigned	_cksum1(void *, unsigned, unsigned);
	int mlen;
	char *addr;

#ifdef NOTDEF
	if (_nocksum)
		return(0);
#endif

	mlen = (m->m_len > len) ? len : m->m_len;
	addr = mtod(m, char *);

	if ((__psint_t)addr & 1)
		ck = nuxi_s(_cksum1(addr, mlen, 0));
	else
		ck = _cksum1(addr, mlen, 0);

	len -= mlen;
	if (len)
		printf("in_cksum, ran out of data, %d bytes left\n", len);

	return(~ck & 0xFFFF);
}

