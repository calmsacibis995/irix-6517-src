/* this is a poor substitute for 4.2 configuration stuff.
 *
 * $Revision: 3.16 $
 */

#define INET				/* generate internet code, of course */
#define NDECNET 1			/* let lboot decide DECnet */

#define NETHER 1			/* surely have an ethernet board */
#define NHY 0				/* no hyper-channel */
#define NIMP 0				/* no IMP interface */
#define NUNIXDOMAN 0			/* no unix domain sockets */

#define IPPRINTFS			/* generate debugging code */
#define ICMPPRINTFS
#define TCPPRINTFS
#define IP6PRINTFS			/* generate debugging code */

#define CLBYTES NBPC			/* XXX clean this up someday */

/* XXX this is the wrong place to put this stuff, but... */
#define	imin(a,b)		MIN((a),(b))

#define insque(q,p)	_insque((caddr_t)(q), (caddr_t)(p))
#define remque(q)	_remque((caddr_t)(q))

/*
 * Any structure that is linked via _insque/_remque should have the
 * forward and backward link pointers as the first two elements.
 */
void _insque(void *ep, void *pp);
void _remque(void *ep);

struct mbuf;
extern unsigned int in_cksum(struct mbuf *m, int len);

