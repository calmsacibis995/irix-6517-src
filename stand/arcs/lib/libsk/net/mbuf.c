#ident	"lib/libsk/net/mbuf.c:  $Revision: 1.22 $"

/*
 * mbuf.c -- prom message buffer routines
 *
 * (Knocked together so kernel network protocol code could be used
 * with minor mods.)
 *
 * Enhanced 10/31/90 to use the new standalone malloc to get the mbuf
 * space.  With this change, the routines in the module are just wrappers
 * that guarantee alignment and some counting sanity.  Also provided is
 * a small pool of mbufs so as to avoid excessive malloc calls.  The NMBUFS
 * define is only used for sanity checking and can be set at any reasonable
 * value.
 */

#include <sys/types.h>
#include <net/in.h>
#include <net/socket.h>
#include <net/arp.h>
#include <net/mbuf.h>
#include <libsc.h>
#include <libsk.h>

#define	NMBUFS	256

extern int Debug;
extern int Verbose;

static int mbufcnt;
static struct mbuf *freepool; /* short list of mbufs for fast turnaround */
static int freecnt; /* number of mbufs on freepool list */
struct mbuf *old_mbufs;		/* list of memory to free */

/*
 * _init_mbufs -- put all mbufs on free list
 */
void
_init_mbufs(void)
{
	mbufcnt = 0;
	freepool = 0;
	freecnt = 0;
}

/*
 * _m_get -- get a free mbuf
 */
/*ARGSUSED*/
struct mbuf *
_m_get(int i, int j)
{
	register struct mbuf *m;
	register struct mbuf *t;

	if ( m = freepool ) {
		freepool = m->m_act;
		freecnt--;
	} else for (;;) {
		m = (struct mbuf *)dmabuf_malloc(sizeof *m);
		t = m;
		if (m == 0)
			panic("cannot allocate mbuf\n");

		/*
		 * check for page crossing and throw out mbuf if
		 * does. 
		 */
		if ((__psunsigned_t)t >> 12 == ((__psunsigned_t)t + sizeof *m) >> 12)
			break;

		/* thread unused memory onto a list to be freed later
		 */
		m->m_act = old_mbufs;
		old_mbufs = m;
	}

	m->m_len = 0;
	m->m_off = 0;
	m->m_inuse = 1;
	m->m_act = (struct mbuf *)0;
	mbufcnt++;

	if (mbufcnt >= NMBUFS && (Debug || Verbose))
		printf("WARNING: internal mbuf maximum exceeded\n");

	return (m);
}

#define	FREEPOOL_SIZE	(NSO_TABLE * MAXMBUFS)
/*
 * _m_freem -- put an mbuf on free list
 */
void
_m_freem(struct mbuf *m)
{
	if (!m->m_inuse) {
#ifdef DEBUG
		/*
		 * This happens sometimes after bootp errors.
		 * Somehow a packet comes in after the bootp connection
		 * is prematurely closed and it gets freed twice.
		 */
		backtrace(3);
		panic ("freeing mbuf not inuse\n");
#endif
		return;
	}
	m->m_inuse = 0;

	if (freecnt < FREEPOOL_SIZE) {
		freecnt++;
		m->m_act = freepool;
		freepool = m;
	} else {
		dmabuf_free(m);

		/* free up old memory now so it can be coalesced
		 */
		while (old_mbufs) {
			struct mbuf *x = old_mbufs->m_act;
			free (old_mbufs);
			old_mbufs = x;
		}
	}
	mbufcnt--;
	if (mbufcnt < 0) {
#ifdef DEBUG
		backtrace(3);
#endif
		panic("mbufcnt < 0\n");
	}
}
