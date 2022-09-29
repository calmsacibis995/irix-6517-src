#ident	"lib/libsk/net/arp.c:  $Revision: 1.20 $"

/*
 * Ethernet address resolution protocol.
 * Swiped from 4.2BSD kernel code.
 */

#include <sys/types.h>
#include <net/in.h>
#include <setjmp.h>
#include <net/socket.h>
#include <net/arp.h>		/* should just use standard files */
#include <net/mbuf.h>
#include <arcs/types.h>
#include <arcs/signal.h>
#include <libsc.h>
#include <libsk.h>

#ifndef NULL
#define	NULL	0
#endif

#define dprintf(x)	(Debug?printf x : 0)

/*
 * Internet to ethernet address resolution table.
 */
struct	arptab {
	struct	in_addr at_iaddr;	/* internet address */
	u_char	at_enaddr[6];		/* ethernet address */
	u_char	at_flags;		/* flags */
};
/* at_flags field values */
#define	ATF_INUSE	1		/* entry in use */
#define ATF_COM		2		/* completed entry (enaddr valid) */

#define	ARPTAB_BSIZ	5		/* bucket size */
#define	ARPTAB_NB	19		/* number of buckets */
#define	ARPTAB_SIZE	(ARPTAB_BSIZ * ARPTAB_NB)

static struct arptab arptab[ARPTAB_SIZE];
static struct arptab *arptnew(struct in_addr *);
static struct arptab *lastat;
static void arptfree(struct arptab *);
static void arpclose(void);

#define	ARPTAB_HASH(a) \
	((short)((((a) >> 16) ^ (a)) & 0x7fff) % ARPTAB_NB)

#define	ARPTAB_LOOK(at,addr) { \
	register int n; \
	at = &arptab[ARPTAB_HASH(addr) * ARPTAB_BSIZ]; \
	for (n = 0 ; n < ARPTAB_BSIZ ; n++,at++) \
		if (at->at_iaddr.s_addr == addr) \
			break; \
	if (n >= ARPTAB_BSIZ) \
		at = 0; }

static u_char etherbroadcastaddr[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

/*
 * _init_arp -- cleanup routing table
 */
void _init_arp(void)
{
	bzero(arptab, sizeof(arptab));
	lastat = 0;
}


/*
 * arpclose -- free arp table entries at net device close
 */
static void
arpclose(void)
{
	register struct arptab *at;
	register int i;

	at = &arptab[0];
	for (i = 0; i < ARPTAB_SIZE; i++, at++) {
		if (at->at_flags == 0)
			continue;
		arptfree(at);
	}
	lastat = 0;
}

static jmp_buf arp_buf;
static void
arp_handler (void)
{
	longjmp (arp_buf, 1);
}

/*
 * Broadcast an ARP packet, asking who has addr on interface ac.
 */
void
arpwhohas( struct arpcom *ac, struct in_addr *addr)
{
	register struct mbuf *m;
	register struct ether_header *eh;
	register struct ether_arp *ea;
	register struct arptab *at;
	volatile int tries;
	struct sockaddr sa;
	int old_alarm;
	SIGNALHANDLER old_handler;
	struct in_addr myaddr;

	myaddr = ((struct sockaddr_in *)&ac->ac_if.if_addr)->sin_addr;
	/*
	 * must save currently running timer since bfs has one running
	 * when arp is called
	 */
	old_handler = Signal (SIGALRM, (SIGNALHANDLER)arp_handler);
	old_alarm = alarm(0);

	/*
	 * set timer and try 5 times
	 */
	tries = 0;
	if (setjmp(arp_buf)) {
		if (++tries >= ARP_TRIES) {
			arpclose();
			printf("ARP couldn't resolve network address.\n");
			return;
		}
	}

	if ((m = _m_get(M_DONTWAIT, MT_DATA)) == NULL) {
		printf("arpwhohas: out of mbufs.\n");
		return;
	}

	m->m_len = sizeof *ea + sizeof *eh;
	m->m_off = MMAXOFF - m->m_len;
	ea = mtod(m, struct ether_arp *);
	eh = (struct ether_header *)sa.sa_data;
	bzero((caddr_t)ea, sizeof (*ea));
	bcopy((caddr_t)etherbroadcastaddr, (caddr_t)eh->ether_dhost,
	   sizeof (etherbroadcastaddr));
	eh->ether_type = ETHERTYPE_ARP;		/* if_output will swap */
	ea->arp_hrd = htons(ARPHRD_ETHER);
	ea->arp_pro = htons(ETHERTYPE_IP);
	ea->arp_hln = sizeof ea->arp_sha;	/* hardware address length */
	ea->arp_pln = sizeof ea->arp_spa;	/* protocol address length */
	ea->arp_op = htons(ARPOP_REQUEST);
	bcopy((caddr_t)ac->ac_enaddr, (caddr_t)ea->arp_sha,
	   sizeof (ea->arp_sha));
	bcopy((caddr_t)&((struct sockaddr_in *)&ac->ac_if.if_addr)->sin_addr,
	   (caddr_t)ea->arp_spa, sizeof (ea->arp_spa));
	bcopy((caddr_t)addr, (caddr_t)ea->arp_tpa, sizeof (ea->arp_tpa));
	sa.sa_family = AF_UNSPEC;
	(void) (*ac->ac_if.if_output)(&ac->ac_if, m, &sa);

	if (addr->s_addr != myaddr.s_addr) {
		alarm(ARP_TIME);
		for (;;) {
			_scandevs();
			ARPTAB_LOOK(at, addr->s_addr);
			if (at && (at->at_flags & ATF_COM))
				break;
		}
		alarm(0);
		Signal (SIGALRM, old_handler);
		alarm(old_alarm);
	}
}

/*
 * Resolve an IP address into an ethernet address.  If success, 
 * desten is filled in and 1 is returned.  If there is no entry
 * in arptab, set one up and broadcast a request 
 * for the IP address;  return 0.
 */
void
_arpresolve(struct arpcom *ac, struct in_addr *destip, u_char *desten)
{
	register struct arptab *at;
	register struct ifnet *ifp;
	struct in_addr ipbcast;
	register int lna;

	/*
	 * Single entry fast cache
	 */
	if ( lastat && destip->s_addr == lastat->at_iaddr.s_addr ) {
		desten[0] = lastat->at_enaddr[0];
		desten[1] = lastat->at_enaddr[1];
		desten[2] = lastat->at_enaddr[2];
		desten[3] = lastat->at_enaddr[3];
		desten[4] = lastat->at_enaddr[4];
		desten[5] = lastat->at_enaddr[5];
		return;
	}

	dprintf(("arpresolve ip %s -> ", inet_ntoa(*destip)));

	lna = inet_lnaof(*destip);
	/*
	 * Use new 4.3 style IP broadcast addresses
	 */
	ipbcast.s_addr = INADDR_BROADCAST;
	if (destip->s_addr == ipbcast.s_addr ||
	    lna == inet_lnaof(ipbcast)) {
		bcopy((caddr_t)etherbroadcastaddr, (caddr_t)desten,
		   sizeof (etherbroadcastaddr));
		dprintf(("broadcast\n"));
		return;
	}

	ifp = &ac->ac_if;
	/* if for us, then error */
	if (destip->s_addr ==
	    ((struct sockaddr_in *)&ifp->if_addr)-> sin_addr.s_addr) {
		dprintf(("net destination is local host"));
		return;
	}

	ARPTAB_LOOK(at, destip->s_addr);
	if (at == 0)
		at = arptnew(destip);
	if ((at->at_flags & ATF_COM) == 0) 
		arpwhohas(ac, destip);
	bcopy((caddr_t)at->at_enaddr, (caddr_t)desten, 6);
	lastat = at;
	dprintf(("ether %s\n", ether_sprintf(desten)));
}

/*
 * Called from network device recv interrupt routines when ether
 * packet type ETHERTYPE_ARP is received.  Algorithm is exactly that
 * given in RFC 826.  In addition, a sanity check is performed on the
 * sender protocol address, to catch impersonators.
 */
void
_arpinput(struct arpcom *ac, struct mbuf *m)
{
	register struct ether_arp *ea;
	struct ether_header *eh;
	struct arptab *at = 0;  /* same as "merge" flag */
	struct sockaddr sa;
	struct in_addr isaddr,itaddr,myaddr;

#ifdef DEBUG
	dprintf(("arpinput\n"));
#endif
	if (m->m_len < sizeof *ea)
		goto out;
	myaddr = ((struct sockaddr_in *)&ac->ac_if.if_addr)->sin_addr;
	ea = mtod(m, struct ether_arp *);
	if (ntohs(ea->arp_pro) != ETHERTYPE_IP) {
#ifdef	DEBUG
	    dprintf((
		    "_arpinput(): discarding, not ETHERTYPE_IP (%x)\n",
		    ntohs(ea->arp_pro)));
#endif
		goto out;
	}
	bcopy(ea->arp_spa, &isaddr.s_addr, sizeof(isaddr.s_addr));
	bcopy(ea->arp_tpa, &itaddr.s_addr, sizeof(itaddr.s_addr));
	if (!bcmp((caddr_t)ea->arp_sha, (caddr_t)ac->ac_enaddr,
	  sizeof (ac->ac_enaddr))) {
#ifdef	DEBUG
	    char *htoe();
	    dprintf((
		    "_arpinput(): discarding, self directed(%s)\n",
		    htoe(ea->arp_sha)));
#endif
		goto out;	/* it's from me, ignore it. */
	}
	if (isaddr.s_addr == myaddr.s_addr) {
		printf("duplicate IP address!! sent from ethernet address: ");
		printf("%x %x %x %x %x %x\n", ea->arp_sha[0], ea->arp_sha[1],
		    ea->arp_sha[2], ea->arp_sha[3],
		    ea->arp_sha[4], ea->arp_sha[5]);
		if (ntohs(ea->arp_op) == ARPOP_REQUEST)
			goto reply;
		goto out;
	}
	ARPTAB_LOOK(at, isaddr.s_addr);
	if (at) {
		bcopy((caddr_t)ea->arp_sha, (caddr_t)at->at_enaddr,
		   sizeof (ea->arp_sha));
		at->at_flags |= ATF_COM;
	}
	if (itaddr.s_addr != myaddr.s_addr) {
#ifdef	DEBUG
	    dprintf((
		    "_arpinput(): discarding, not target(%s)\n",
		    inet_ntoa(itaddr)));
#endif
		goto out;	/* if I am not the target */
	}
	if (at == 0) {		/* ensure we have a table entry */
		at = arptnew(&isaddr);
		bcopy((caddr_t)ea->arp_sha, (caddr_t)at->at_enaddr,
		   sizeof (ea->arp_sha));
		at->at_flags |= ATF_COM;
	}
	if (ntohs(ea->arp_op) != ARPOP_REQUEST) {
#ifdef	DEBUG
	    dprintf((
		    "_arpinput(): discarding, not ARPOP_REQUEST(%x)\n",
		    ntohs(ea->arp_op)));
#endif
		goto out;
	}
reply:
#ifdef DEBUG
	printf("arpinput replying\n");
#endif
	bcopy((caddr_t)ea->arp_sha, (caddr_t)ea->arp_tha,
	   sizeof (ea->arp_sha));
	bcopy((caddr_t)ea->arp_spa, (caddr_t)ea->arp_tpa,
	   sizeof (ea->arp_spa));
	bcopy((caddr_t)ac->ac_enaddr, (caddr_t)ea->arp_sha,
	   sizeof (ea->arp_sha));
	bcopy((caddr_t)&myaddr, (caddr_t)ea->arp_spa,
	   sizeof (ea->arp_spa));
	ea->arp_op = htons(ARPOP_REPLY);
	eh = (struct ether_header *)sa.sa_data;
	bcopy((caddr_t)ea->arp_tha, (caddr_t)eh->ether_dhost,
	   sizeof (eh->ether_dhost));
	eh->ether_type = ETHERTYPE_ARP;
	sa.sa_family = AF_UNSPEC;
	(*ac->ac_if.if_output)(&ac->ac_if, m, &sa);
	return;
out:
	_m_freem(m);
	return;
}

/*
 * Free an arptab entry.
 */
static void
arptfree(struct arptab *at)
{
	at->at_iaddr.s_addr = 0;
}

/*
 * Enter a new address in arptab, pushing out the oldest entry 
 * from the bucket if there is no room.
 */
static int lastused_at;

static struct arptab *
arptnew(struct in_addr *addr)
{
	register int n;
	register struct arptab *at, *ato;

	ato = at = &arptab[ARPTAB_HASH(addr->s_addr) * ARPTAB_BSIZ];
	for (n = 0 ; n < ARPTAB_BSIZ ; n++,at++) {
		if (at->at_flags == 0)
			goto out;	 /* found an empty entry */
	}
	if (n >= ARPTAB_BSIZ) {
		at = &ato[lastused_at];
		lastused_at = (lastused_at + 1) % ARPTAB_BSIZ;
		arptfree(at);
	}
out:
	at->at_iaddr = *addr;
	at->at_flags = ATF_INUSE;
	return (at);
}
