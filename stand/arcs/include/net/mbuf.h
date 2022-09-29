#ident	"include/net/mbuf.h: $Revision: 1.10 $"

/*
 * mbuf.h -- definitions for message buffers
 */

#define	MMINOFF		0
#define	MMAXOFF		100+128+32
#define	MLEN		(ETHERMTU+MMAXOFF)

/*
 * The IP12 Ethernet driver allocates more mbufs for each socket
 * to prevent overflow backing up on the SEEQ chip.
 */
#define	MAXMBUFS	8		/* queue up to 8 mbufs per socket */

struct mbuf {
	short m_len;
	short m_inuse;			/* 1 when allocated, 0 when freed */
	struct sockaddr m_srcaddr;
	int m_off;
	char m_dat[MLEN];
	struct	mbuf *m_act;		/* link in higher-level mbuf list */
};

/*
 * These aren't used in prom version, they're just here
 * so the kernel code works unmodified.
 */
#define	M_DONTWAIT	0
#define	MT_DATA		0

/* mbuf head, to typed data */
#define	mtod(x,t)	((t)((__psint_t)((x)->m_dat) + (x)->m_off))

extern struct mbuf *	_m_get(int,int);
extern void		_m_freem(struct mbuf *);
extern struct mbuf *	_so_remove(struct so_table *);
