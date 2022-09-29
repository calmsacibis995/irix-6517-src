/*******************************************************\
 * nasid.c						*
 *							*
 *   given a network connectivity graph, 		*
 *   assign node IDs to hubs, meta IDs to routers.	*
 *							*
 \*******************************************************/

#ident "$Revision: 1.34 $"

#include <sys/cpu.h>
#include <sys/mips_addrspace.h>
#include <libkl.h>
#include <sys/SN/SN0/ip27log.h>
#include "libasm.h"
#include "ip27prom.h"
#include "discover.h"
#include "nasid.h" /* includes promcfg.h */
#include "libc.h"
#include "router.h"
#include "hub.h"

#define LDEBUG		0	/* Set to 1 for debug compilation */
static int debug_verbose = 1;

#ifdef DISCTEST
#define db_printf printf
#endif

extern int router_led_error(pcfg_t *p,
			    int index, char port_mask);		/* main.c */

/*
 * NodeId/Nasid assignment algorithm:
 *
 *   perform a BFS of the network graph, propagating dimension and
 *   nasid constraints from the root across the network graph.
 *
 *   Handles SN0/Sn00 topologies with express links, loopback links
 *   as well as multiple redundant links between routers.
 *
 *   No vector operations are launched by this code.
 *   All assignments are based on network topology information
 *   contained in the promcfg structure that is passed in. All results
 *   are entered into the same structure, and must be written out to
 *   hub/router registers to take effect.
 */

#define PCFG_TYPE_MASK		0x0fU
#define PCFG_TYPE_SHFT		0

#define LINK_TYPE_XPRESS	0x1U
#define LINK_TYPE_META		0x2U
#define LINK_TYPE_MASK		0x30U
#define LINK_TYPE_SHFT		4

#define LINK_STATE_UNSET	0x40U
#define LINK_STATE_UNUSED	0x80U
#define LINK_STATE_MASK		0xc0U

#define PEER_PORT	6
#define PEER_DIMN	1

/* ouch - when searching through metarouters this bites the bullet
#define QUEUE_SZ	(MAX_ROUTER_PORTS*MAX_ROUTER_PORTS)
*/
#define QUEUE_SZ        4 * MAX_ROUTERS
#define MAX_METAID	(MAX_ROUTERS/16) /* leave extra room for
					  * coarse mode testing
					  */
#define EMPTY_ENTRY	-1

typedef struct queue_s {
  short head;
  short tail;
  int array[QUEUE_SZ];
} queue_t;

/* toggle the dth bit of n, indicating a hop along the dth dimension.
 */
#define MOVE_DIMN(_n, _d) ((_n) ^ (1 << (_d)))

/* set/get used_metaid entries
 */
#define SET_METAID_USED(_m, _um) (_um[_m/64] |= 1ULL<<(_m%64))
#define GET_METAID_USED(_m, _um) ((_um[_m/64] & (1ULL<<_m%64)) >> (_m%64))

static void QUEUE_INIT(queue_t *q)
{
  int i;

  q->head = q->tail = 0;
  for (i=0; i<QUEUE_SZ; i++)
    q->array[i] = EMPTY_ENTRY;
}

static int QUEUE_EMPTY(queue_t *q)
{
  return(q->head == q->tail);
}

static void QUEUE_PUSH(queue_t *q, int entry)
{
  q->array[q->head++] = entry;
  q->head %= QUEUE_SZ;
}

static int QUEUE_POP(queue_t *q)
{
  int entry;

  entry = q->array[q->tail++];
  q->tail %= QUEUE_SZ;

  return(entry);
}

static void bfs_init(pcfg_t *p, queue_t *q, int root)
{
  int i, j;

  for (i=0; i<p->count; i++)
    if (p->array[i].any.type == PCFG_TYPE_ROUTER) {
      SET_FIELD(p->array[i].router.flags, PCFG_ROUTER_STATE, PCFG_ROUTER_STATE_NEW);
      SET_FIELD(p->array[i].router.flags, PCFG_ROUTER_DEPTH, 0xf); /* max depth */
      for (j=1; j<=MAX_ROUTER_PORTS; j++)
	SET_FIELD(p->array[i].router.port[j].flags, PCFG_PORT_ESCP, 0);
    }

  SET_FIELD(p->array[root].router.flags, PCFG_ROUTER_STATE, PCFG_ROUTER_STATE_SEEN);
  SET_FIELD(p->array[root].router.flags, PCFG_ROUTER_DEPTH, 0);

  QUEUE_INIT(q);
  QUEUE_PUSH(q, root);
}

/*
 * bfs_run()
 */
int bfs_run(pcfg_t *p, int root)
{
  pcfg_ent_t	*pa;
  pcfg_router_t *cr, *nr;
  pcfg_port_t	*cp;
  queue_t	 q;
  int		 ci, cd, ns, i;
  uint		 seen;		/* bit vector for up to 32 cubes (1024p)
				 */

  /* prepare for the BFS
   */
  bfs_init(p, &q, root);
  pa = p->array;

  /* mark the root cube as seen to avoid looping
   * through another cube and back.
   */
  seen = 1<<NASID_GET_META(pa[root].router.metaid);

  while (!QUEUE_EMPTY(&q)) {

    ci = QUEUE_POP(&q);
    cr = &pa[ci].router;
    cd = GET_FIELD(cr->flags, PCFG_ROUTER_DEPTH);

    /*
     * sanity checks:
     *  current entry must be a router,
     *  must have its meta_id assigned.
     *  search state must be PCFG_ROUTER_STATE_SEEN.
     */
    if (cr->type != PCFG_TYPE_ROUTER ||
	GET_FIELD(cr->flags, PCFG_ROUTER_STATE) != PCFG_ROUTER_STATE_SEEN) {
      printf("bfs_run at %d: Internal Error\n", ci);
      return(-1);
    }

    for (i=MAX_ROUTER_PORTS;i>0;i--) {
      cp = &cr->port[i];
      if (cp->index==PCFG_INDEX_INVALID ||
	  pa[cp->index].any.type!=PCFG_TYPE_ROUTER)
	continue;

      nr = &pa[cp->index].router;
      ns = GET_FIELD(nr->flags, PCFG_ROUTER_STATE);

      if (ns == PCFG_ROUTER_STATE_NEW) {
	if (SAME(cr, nr) ||
	    (IS_META(nr) && LOCAL(cr, &pa[root].router))) {
	  SET_FIELD(nr->flags, PCFG_ROUTER_STATE, PCFG_ROUTER_STATE_SEEN);
	  SET_FIELD(nr->flags, PCFG_ROUTER_DEPTH, cd+1);
	  QUEUE_PUSH(&q, cp->index);
	}
	else if (!((seen>>NASID_GET_META(nr->metaid))&1)) {
	  SET_FIELD(nr->flags, PCFG_ROUTER_STATE, PCFG_ROUTER_STATE_SEEN);
	  SET_FIELD(nr->flags, PCFG_ROUTER_DEPTH, cd+1);
	  SET_FIELD(cr->port[i].flags, PCFG_PORT_ESCP, 1);
	  QUEUE_PUSH(&q, cp->index);
	  if(debug_verbose) {
	  db_printf("at %d: marking meta %d (%d) seen\n",
		    ci, NASID_GET_META(nr->metaid), nr->metaid);
	  }
	  seen |= 1<<NASID_GET_META(nr->metaid);
	}
      }
    }
    SET_FIELD(cr->flags, PCFG_ROUTER_STATE, PCFG_ROUTER_STATE_DONE);
  }

  return(0);
}

/*
 * consistent()
 *
 *   return the dimension between two meta ids if they are consistent,
 *   (consistent metaids differ in only one bit position) -1 otherwise.
 *   routers connected by express links will not be consistent.
 */
static int consistent(pcfg_router_t *r1, pcfg_router_t *r2)
{
  nasid_t m1, m2;
  int i, count;
  int dimn=-1;

  m1 = r1->metaid;
  m2 = r2->metaid;

  if (IS_META(r1) || IS_META(r2)) {
    /*
     * connected meta routers, connected meta/8p routers
     * must have equal local ids.
     */
    if (NASID_GET_LOCAL(m1) != NASID_GET_LOCAL(m2))
      return(-1);
    /*
     * meta=>8p connection, use metaid as dimension XXX.
     *   use placeholder dimension.
     */
    if (!IS_META(r2))
      return(0);
      /* return(NASID_GET_META(m2)); */
    /*
     * 8p=>meta connection, dimension is fixed at 4. 
     */
    if (!IS_META(r1)) {
    if(debug_verbose) {
      db_printf("\n ***consistent() returning 4!\n\n");
      }
      return(4);
    }
  }

  /* handle 8p=>8p, meta=>meta connections. 
   */
  m1 ^= m2;
  count=0;

  for (i=0; i<NASID_BITS; i++) {
    if (m1%2) {
      count++;
      dimn = i;
    }
    m1 = m1>>1;
  }

  if (count==1) {
#if LDEBUG
    db_printf("consistent(%d, %d) = %d\n", r1->metaid, r2->metaid, dimn);
#endif
    return(dimn);
  }
  else
    return(-1);
}

/*
 * unique()
 *
 * returns 1 if the nasid is not in the used nasid array,
 * 0 otherwise.
 */
static int unique(nasid_t metaid, unsigned long long *used_metaid)
{
  int rc;

  rc = metaid != PCFG_METAID_INVALID &&
    (GET_METAID_USED(metaid, used_metaid) == 0);
#if LDEBUG
  db_printf("unique(%d) returning %d\n", metaid, rc);
#endif
  return(rc);
}

/*
 * get_used_dimn()
 *
 * returns a bit vector of used dimensions
 * surrounding the router at from_index.
 *
 * a dimension is considered used if it:
 *  - connects the router to a hub (i.e. a bristle dimension).
 *  - bridges the current router and a router whose metaid is
 *    consistent, whether or not the remote router's metaid has been
 *    fixed.
 */
static int get_used_dimn(pcfg_ent_t *pa, int from_index, int bristle_bits)
{
  pcfg_router_t *cr, *nr;
  pcfg_port_t	*cp;
#ifndef DISCTEST
  char		buf[64];
  nasid_t	rsrvd_dimn;
#endif
  nasid_t	dimn_used=0;	/* bit vector */
  int		dimn, i;

  cr = &pa[from_index].router;

  /* mark the bristle dimensions used.
   * the first dimension is reserved for internal connections
   */
  if (!IS_META(cr)) {
    for (i=0; i<bristle_bits; i++)
      dimn_used |= 1 << i;
    if (cr->port[PEER_PORT].index==PCFG_INDEX_INVALID)
      dimn_used |= 1 << PEER_DIMN;
  }

  /* mark the dimensions used connecting to other routers.
   */
  for (i=MAX_ROUTER_PORTS;i>0;i--) {
    cp = &cr->port[i];
    if ((cp->index!=PCFG_INDEX_INVALID) &&
	(pa[cp->index].any.type==PCFG_TYPE_ROUTER)) {
      /*
       * we are connected to a router -
       * if the metaids are consistent,
       * set the corresponding dimn_used bit.
       */
      nr = &pa[cp->index].router;
      if ((dimn=consistent(cr, nr)) >= 0)
	dimn_used |= 1 << dimn;
    }
  }

#ifndef DISCTEST
#ifdef COARSE_TESTING
  /* artificially mark a few dimensions used
   * to force high nasids and put the system in coarse mode.
   * to move from 0->7 to 64->71 , set mask to 0x38.
   */
  ip27log_getenv(get_nasid(), "force_coarse", buf, "0", 0);
  rsrvd_dimn = strtoull(buf, 0, 0);
  if (rsrvd_dimn) {
      printf("Note: ip27env rsrvd_dimn = 0x%x\n", rsrvd_dimn);
      dimn_used |= rsrvd_dimn;
  }
#endif
#endif

#if LDEBUG
  db_printf("get_used_dimn at %d returns %d\n", from_index, dimn_used);
#endif
  return(dimn_used);
}

/*
 * get_free_dimn()
 *
 * return bit position of first 0 in used_dimn.
 */
static int get_free_dimn(int used_dimn)
{
  int dimn=0;
  int dimn_vec, i;

  dimn_vec = used_dimn ^ (used_dimn+1);

  for (i=0; i<NASID_BITS; i++) {
    if (dimn_vec==1)
      break;
    dimn++;
    dimn_vec = dimn_vec>>1;
  }

#if LDEBUG
  db_printf("get_free_dimn(%d) returning %d\n", used_dimn, dimn);
#endif
  return(dimn);
}

/*
 * get_metaid()
 *
 * returns (but does not set) a metaid for the router connected
 * to from_port on from_index.
 *
 *   - if the metaids are equal, check for loopback
 *   - if the next hop metaid is consistent with our own and unique
 *    in the system, return it.
 *   - otherwise, find an unused dimension which results in a
 *    unique next hop metaid and return it.
 *
 *  actions are different for meta routers.
 */
static nasid_t get_metaid(pcfg_ent_t *pa,
			  int from_index,
			  int from_port,
			  int bristle_bits,
			  unsigned long long *used_metaid)
{
  pcfg_router_t	*cr, *nr;
  int dimn, used_dimn;
  nasid_t next_metaid = PCFG_METAID_INVALID;

  /* Sanity check.
   */
  if (pa[from_index].any.type != PCFG_TYPE_ROUTER ||
      pa[from_index].router.metaid == PCFG_METAID_INVALID ||
      pa[from_index].router.port[from_port].index == PCFG_INDEX_INVALID) {
    printf("get_metaid at %d: Internal Error\n", from_index);
    return(-1);
  }

  cr = &pa[from_index].router;
  nr = &pa[cr->port[from_port].index].router;

  /*
   * 1. loopback - this link will not be used, metaid is the same.
   * 2. transition from local to meta level is also a special case.
   */
  if (cr->nic == nr->nic ||
      (!IS_META(cr) && IS_META(nr))) {
    next_metaid = cr->metaid;
    goto done;
  }

  /*
   * loop until the next hop metaid is unique
   * and consistent with the current metaid.
   *
   * bit(s) in the field reserved for bristles
   * are always considered used, so they will
   * never be returned as the dimension.
   */
  if ((dimn=consistent(cr, nr))>0)
      next_metaid = nr->metaid;

  /* returns a different result if from_index is a meta_router
   */
  used_dimn = get_used_dimn(pa, from_index, bristle_bits);

  while(!unique(next_metaid, used_metaid)) {
    /*
     * mark the current dimn as used to
     * allow us to procceed to the next one.
     */
    if (dimn > 0) used_dimn |= 1<<dimn;
    dimn = get_free_dimn(used_dimn);
    if (IS_META(cr) && !IS_META(nr))
      next_metaid = NASID_MAKE(dimn, NASID_GET_LOCAL(cr->metaid));
    else
      next_metaid = MOVE_DIMN(cr->metaid, dimn);
  }

 done:
  if(debug_verbose) {
  db_printf("***get_metaid returning %d\n", next_metaid);
  }

  return(next_metaid);
}

/*
 * cross_face()
 *
 * detects cross-face express links by finding the number of neighbors
 * at equal depth to the start node. returns number of such neighbors.
 * (Any such neighbors indicate an express link is present.)
 */
static int cross_face(pcfg_ent_t *pa, int index)
{
  pcfg_router_t *cr;
  int cd, i;
  char errmask = 0;

  cr = &pa[index].router;
  cd = GET_FIELD(cr->flags, PCFG_ROUTER_DEPTH);

  for (i=MAX_ROUTER_PORTS;i>0;i--)
    if (cr->port[i].index != PCFG_INDEX_INVALID &&
	pa[cr->port[i].index].any.type == PCFG_TYPE_ROUTER &&
	cr->nic != pa[cr->port[i].index].router.nic &&
	!(GET_FIELD(cr->port[i].flags, PCFG_PORT_XPRESS)) &&
	cd == GET_FIELD(pa[cr->port[i].index].router.flags, PCFG_ROUTER_DEPTH))
      errmask |= 1<<(i-1);

  return(errmask);
}

static int config_error(pcfg_t* p, int ci, char errmask, char *errmsg)
{
  /* avoid printing message if module = 0 ?
   */
  printf("\n*** Configuration Error at module %d, slot r%d, port(s) 0x%x:"
	 "\n\t%s.\n",
	 p->array[ci].router.module,
	 p->array[ci].router.slot, errmask, errmsg);
  printf("\tNote: misconfigured ports will have their amber LEDs set.\n");

  return(router_led_error(p, ci, errmask));
}

static void decode_link_type(uchar_t type, char *buf)
{

  switch(type&LINK_STATE_MASK) {
  case LINK_STATE_UNSET: strcpy(buf, "UNSET,"); break;
  case LINK_STATE_UNUSED: strcpy(buf, "UNUSED,"); break;
  default: 
    sprintf(buf, " SET,  %d", type);
    return;
  }

  switch(GET_FIELD(type, LINK_TYPE)) {
  case LINK_TYPE_XPRESS: strcat(buf, "EXPRESS,"); break;
  case LINK_TYPE_META: strcat(buf, "META,"); break;
  default: break;
  }

  switch(GET_FIELD(type, PCFG_TYPE)) {
  case PCFG_TYPE_HUB: strcat(buf, "HUB"); break;
  case PCFG_TYPE_ROUTER: strcat(buf, "RTR"); break;
  default: strcat(buf, "*"); break;
  }

  return;
}

/*
 * nasid_loop()
 *
 *   Perform a BFS of the network, distributing metaids/nasids
 *   by assigning dimensions to links and propagating the root
 *   nasid along those dimensions. Metaids as used here can be
 *   though of as SN0net addresses for routers. They occupy
 *   the full nasid width, and can be used to easily derive the
 *   nasids of directly connected (bristled) hubs.
 *
 *   Once the search through all of a router's links has completed,
 *   its metaID is fixed and can be used to set the bristled hubs'
 *   nasids. Bristle nasid assignment is based on absolute port
 *   position. Use slot # information instead?
 *
 *   Assumes root is a node.
 *   Assumes that express links have been marked as such, and avoids
 *   using them to propagate nasids.
 */
static int nasid_loop(pcfg_t *p, int from_index,
		      uchar_t ptype[], uchar_t mptype[], nasid_t rsrvd_mdimn)
{

  pcfg_ent_t		*pa;
  pcfg_router_t		*cr, *nr;
  pcfg_port_t		*cp;
  unsigned long long	 used_metaid[MAX_METAID];

  int		root, hub, cd, ns;
  int		ci, i;
  char		dimn, mdimn, errmask;
  queue_t	q, mq;

  pa = p->array;
  root = pa[from_index].hub.port.index;

  /* prepare for the BFS
   */
  bfs_init(p, &q, root);
  QUEUE_INIT(&mq);

  /* Initialize the used_metaid array,
   */
  for (i=0; i<MAX_METAID; i++)
    used_metaid[i] = 0;
  if (pa[root].router.metaid == PCFG_METAID_INVALID)
    pa[root].router.metaid = 0;

  /* force coarse in single meta systems
   */
  mdimn = 0;

  while (!QUEUE_EMPTY(&q) || !QUEUE_EMPTY(&mq)) {

    ci = (QUEUE_EMPTY(&q)) ? QUEUE_POP(&mq) : QUEUE_POP(&q);
    cr = &pa[ci].router;
    cd = GET_FIELD(cr->flags, PCFG_ROUTER_DEPTH);

    /*
     * sanity checks:
     *  current entry must be a router,
     *  must have its meta_id assigned.
     *  search state must be PCFG_ROUTER_STATE_SEEN.
     */
    if (cr->type != PCFG_TYPE_ROUTER ||
	cr->metaid == PCFG_METAID_INVALID ||
	GET_FIELD(cr->flags, PCFG_ROUTER_STATE) != PCFG_ROUTER_STATE_SEEN) {
      printf("nasid_loop at %d: Internal Error\n", ci);
      return(-1);
    }

    if (IS_META(cr)) {
      /*
       * In single meta configs, assign metaids to ports if they
       * haven't been assigned yet. 
       * 
       * First assign dimensions to meta->8p connections that are
       * already consistent. Cube 0 in a single partition system and
       * any running cubes in a partitioned system will satisfy this
       * property. 
       *
       * Next, assign dimensions to the remaining meta->8p connection
       * ports, avoiding already used dimensions as indicated by
       * rsrvd_mdimn. 
       *
       * Meta cube configs (>128p) have a single meta->8p connection
       * whose dimension has already been set.
       */
      for (i=MAX_ROUTER_PORTS; i>0; i--) {

	int d;

	if (mptype[i]&LINK_STATE_UNSET &&
	    cr->port[i].index != PCFG_INDEX_INVALID) {
	  if ((d=consistent(cr, &pa[cr->port[i].index].router)) >= 0) {
	    mptype[i] = d; /* 0; */
	    rsrvd_mdimn |= 1 << d;
	    if(debug_verbose) {
	    db_printf("\nport %d metaid = %d.\n",
		      i, pa[cr->port[i].index].router.metaid);
	    db_printf("assigning meta port %d dimn = %d.\n", i, mptype[i]);
	    db_printf("rsrvd_mdimn = %x.\n", i, rsrvd_mdimn);
	    }
	  }
	}
      }

      /*
       * fill in unused meta dimns...
       */
      for (i=MAX_ROUTER_PORTS; i>0; i--) {
	if (mptype[i]&LINK_STATE_UNSET &&
	    cr->port[i].index != PCFG_INDEX_INVALID) {
	  while((rsrvd_mdimn>>mdimn)%2)
	    mdimn++;
	  mptype[i] = mdimn++;
	  if(debug_verbose) {
	  db_printf("\nport %d metaid = %d.\n",
		    i, pa[cr->port[i].index].router.metaid);
	  db_printf("assigning meta port %d dimn = %d.\n", i, mptype[i]);
	  }
	}
      }
    }

    if(debug_verbose) {
    if (IS_META(cr))
      db_printf("***nasid_loop at meta router %d, metaid fixed to %d\n",
		ci, cr->metaid);
    else
      db_printf("***nasid_loop at %d, metaid fixed to %d\n", ci, cr->metaid);
    }

    if (!IS_META(cr)) {
      if (!unique(cr->metaid, used_metaid)) {
	printf("*** Configuration Error: please check CrayLink cabling.");
	return(-1);
      }
      SET_METAID_USED((unsigned long long) cr->metaid, used_metaid);
    }

    /*
     * loop through working, regular links (port numbers are absolute),
     * and then through express links.
     *
     * if a link has not been marked as part of the BFS,
     * pick a dimension for the link and set the remote nasid.
     */
    for (i=MAX_ROUTER_PORTS; i>0; i--) {

      cp = &cr->port[i];

      if (cp->index==PCFG_INDEX_INVALID ||
	  pa[cp->index].any.type!=PCFG_TYPE_ROUTER ||
	  GET_FIELD(cp->flags, PCFG_PORT_XPRESS))
	continue;

      if(debug_verbose) {
      db_printf("search through port %d\n", i);
      }

      nr = &pa[cp->index].router;
      ns = GET_FIELD(nr->flags, PCFG_ROUTER_STATE);

      /*
       * next hop meta_id is unfixed.
       * set it based on the current metaid etc.
       * if it has not been seen by the BFS yet,
       * add it to the queue.
       *
       * 8p->8p, meta->meta:	move along port dimn.
       * 8p->meta:		metarouter id = 8p router id.
       * meta->8p 128p:		use port dimn as 8p router metaid.
       * meta->8p 256p:		8p router id = metarouter id.
       *
       */
      if (ns != PCFG_ROUTER_STATE_DONE && consistent(cr, nr) < 0) {
	if (IS_META(cr)) {
	  dimn = mptype[i];
	  nr->metaid = (IS_META(nr)) ?
	    MOVE_DIMN(cr->metaid, dimn) :
	      NASID_MAKE(NASID_GET_META(cr->metaid)+dimn,
			 NASID_GET_LOCAL(cr->metaid));
	}
	else {
	  dimn = ptype[i];
	  nr->metaid = (IS_META(nr)) ?
	    cr->metaid : MOVE_DIMN(cr->metaid, dimn);
	}
	if(debug_verbose) {
	  db_printf("next metaid set to %d\n", nr->metaid);
	}
      }
      else {
	if(debug_verbose) {
	  db_printf("next metaid consistent at %d\n", nr->metaid);
	}
      }

      if (ns == PCFG_ROUTER_STATE_NEW) {
	SET_FIELD(nr->flags, PCFG_ROUTER_STATE, PCFG_ROUTER_STATE_SEEN);
	SET_FIELD(nr->flags, PCFG_ROUTER_DEPTH, cd+1);
	if (IS_META(nr))
	  QUEUE_PUSH(&mq, cp->index);
	else
	  QUEUE_PUSH(&q, cp->index);
      }
    }

    /* check for illegal express links
     */
    if (errmask = cross_face(pa, ci)) {
      config_error(p, ci, errmask, "unsupported express link");
      printf("Note: express links must connect routers in different slots.\n");
      return(-1);
    }

    /*
     * continue the BFS,
     * recursing through neighbors in PCFG_ROUTER_STATE_SEEN.
     */
    SET_FIELD(cr->flags, PCFG_ROUTER_STATE, PCFG_ROUTER_STATE_DONE);

    if (IS_META(cr))
      continue;

    /*
     * BFS is complete from this node: assign nasids to directly
     * connected hubs.
     *
     * The number of bits dedicated to bristles is based on the
     * largest number of bristles found on any node in the system.
     * The max number of bristles will vary from 2 for an SN0 rack,
     * to 4 for an SN0 star-router (deskside) system, to 6 for an SN00
     * router-only box system.
     *
     * XXX check to see whether the node already has a nasid,
     * bomb out if there is a mismatch.
     */
    hub=0;

    for (i=MAX_ROUTER_PORTS;i>0;i--)
      if ((cr->port[i].index!=PCFG_INDEX_INVALID) &&
	  (pa[cr->port[i].index].any.type==PCFG_TYPE_HUB)) {
	pa[cr->port[i].index].hub.nasid = cr->metaid + hub;
	hub++;
#if LDEBUG
	db_printf("assigned hub %d nasid %d\n",
		  cr->port[i].index, pa[cr->port[i].index].hub.nasid);
#endif
    }
  }

  return(0);
}

/*
 * get_peer_router()
 *
 * returns the index of the other router in the same
 * 8P12 module. ci is assumed to point to an 8p router.
 *
 * if there is no router connected on the PEER_PORT,
 * look for another router in the same module
 * by walking through promcfg and comparing mod #s.
 *
 * Note: 0 is not a valid mod#, and indicates that
 *       mod #s have not been set up yet.
 */
static int get_peer_router(pcfg_t *p, int ci)
{
  int pi, i;
  uchar_t module;

  pi = p->array[ci].router.port[PEER_PORT].index;

  if (pi != PCFG_INDEX_INVALID &&
      p->array[pi].any.type == PCFG_TYPE_ROUTER)
    return(pi);

  module = p->array[ci].router.module;

  /*
   * scan through all routers looking for a module match.
   * (module# == 0) implies it has not been assigned yet,
   * therefore we cannot match up routers with disconnected
   * PEER_PORTs in the presence of unassigned mod#s.
   */
  if (p->count_type[PCFG_TYPE_ROUTER]>2)
    for (i=0; i<p->count; i++)
      if (p->array[i].any.type == PCFG_TYPE_ROUTER &&
	  p->array[i].router.module == 0)
	return(PCFG_INDEX_INVALID);

  for (i=0; i<p->count; i++)
    if (p->array[i].any.type == PCFG_TYPE_ROUTER &&
	p->array[i].router.module == module && i != ci) {
      pi = i;
      if(debug_verbose) {
      db_printf("found peer for router %d at %d\n", ci, pi);
      }
      return(pi);
    }

  return(PCFG_INDEX_INVALID);
}

static
void create_connection(pcfg_ent_t *pa, int from_index, int to_index, int port)
{

  pa[from_index].router.port[port].index = to_index;
  pa[from_index].router.port[port].port = port;
  SET_FIELD(pa[from_index].router.port[port].flags, PCFG_PORT_FAKE, 1);

  pa[to_index].router.port[port].index = from_index;
  pa[to_index].router.port[port].port = port;
  SET_FIELD(pa[to_index].router.port[port].flags, PCFG_PORT_FAKE, 1);
}

/*
 * check_module()
 *
 *  - check all of a module's connections to other modules.
 *  - create correponding connections among peer routers where
 *   only one exists.
 *  - make sure that a module has a non-express connection to
 *   other modules if it has an express connection.
 *
 * check_module is not affected by partitions.
 */
static int check_module(pcfg_t *p, int ci)
{
  pcfg_ent_t	*pa;
  pcfg_router_t	*cr, *cpr;

  int nbrs, xnbrs=0;
  int ni, cpi, npi, ncpi;
  int i, j, skip;

  pa = p->array;
  cr = &pa[ci].router;

  if(debug_verbose) {
  db_printf("check_module at %d\n", ci);
  }

  /*
   * cycle through all pairs involving this router where:
   *  i is peer port (forming the "module"),
   *  j is the connection to another module that is being checked.
   *
   * only 8p<->8p or meta<->meta connections are checked.
   */
  for (i=MAX_ROUTER_PORTS; i>0; i--) {
    cpi = cr->port[i].index;
    if (cpi == PCFG_INDEX_INVALID ||
	pa[cpi].any.type != PCFG_TYPE_ROUTER ||
	IS_META(cpr=&pa[cpi].router) != IS_META(cr))
      continue;

    nbrs = xnbrs = 0;

#if LDEBUG
    db_printf("\npeer port is %d (%d<=>%d)\n", i, ci, cpi);
#endif

    for (j=MAX_ROUTER_PORTS; j>0; j--) {
      if (i==j)
	continue;
      /*
       * look for a connection to another module.
       * only 8p<->8p or meta<->meta connections are checked.
       */
      ni   = pa[ci].router.port[j].index;
      ncpi = pa[cpi].router.port[j].index;
      skip = 0;

      if (ni == cpi ||
	  ni == PCFG_INDEX_INVALID ||
	  pa[ni].any.type != PCFG_TYPE_ROUTER ||
	  IS_META(&pa[ni].router) != IS_META(cr))
	skip = 1;
      else if (GET_FIELD(pa[ci].router.port[j].flags, PCFG_PORT_XPRESS)) {
	xnbrs++;
	skip = 1;
      }
      else
	nbrs++;

      if (ncpi != ci &&
	  ncpi != PCFG_INDEX_INVALID &&
	  pa[ncpi].any.type == PCFG_TYPE_ROUTER &&
	  IS_META(&pa[ncpi].router) == IS_META(cpr)) {
	if (GET_FIELD(pa[cpi].router.port[j].flags, PCFG_PORT_XPRESS))
	  xnbrs++;
	else
	  nbrs++;
      }

      /* avoid checking npi if ni was not a valid router index.
       */
      if (skip)
	continue;

      npi = pa[ni].router.port[i].index;

      if (npi == ci ||
	  npi == PCFG_INDEX_INVALID ||
	  pa[npi].any.type != PCFG_TYPE_ROUTER ||
	  IS_META(&pa[npi].router) != IS_META(&pa[cpi].router))
	continue;
      else if (GET_FIELD(pa[cpi].router.port[j].flags, PCFG_PORT_XPRESS)) {
	xnbrs++;
	skip = 1;
      }
      else
	nbrs++;

      /* avoid checking the cpi<=>npi link if npi was not a valid
       * router index.
       */
      if (skip)
	continue;

#if LDEBUG
      db_printf("next port is %d (%d<=>%d)\n", j, ni, npi);
#endif

      /*
       * if this port is used on either of the peer routers,
       * it must be to connect them to each other.
       *
       * if the corresponding ports on corresp. routers are disconnected,
       * create a FAKE connection between them.
       */
      if (pa[cpi].router.port[j].index == PCFG_INDEX_INVALID &&
	  pa[npi].router.port[j].index == PCFG_INDEX_INVALID) {
	printf("*** Configuration Note at module %d, slot r%d, port %d:"
	       "\n\tmissing cable or link down.\n",
	       pa[cpi].router.module,
	       pa[cpi].router.slot, j);
	printf("\tNote: down links will have their amber LEDs set.\n");
	router_led_error(p, cpi, 1<<(j-1));
	router_led_error(p, npi, 1<<(j-1));
	create_connection(pa, cpi, npi, j);
      }
      else if (pa[cpi].router.port[j].index != npi) {
	config_error(p, cpi, 1<<(j-1),
		     "corresponding ports must connect to the same module");
	return(-1);
      }
    }

    /*
     * check for a module which has only express links to the rest of the
     * system. should be able to create non-express links if necessary...
     *
     * in case of an error, light all three cable LEDs to indicate which
     * router/module has the problem.
     */
    if (xnbrs && !nbrs) {
      config_error(p, ci, 0x7, "missing non-express links");
      return(-1);
    }
  }

  return(0);
}

/*
 * check_router()
 *
 *  prepare for nasid assignment by enforcing the following
 *  constraints:
 *
 * 1. Over-bristled routers (>2 nodes) are only supported in
 *   star router or 12P4IO configs (to maintain bisection bandwidth).
 *
 * 2. Multiple connections to the meta layer are never allowed.
 *
 *  2a. Either meta routers are present in the system, or routers in separate
 *     cubes are directly attached, not both.  XXX needs to be implemented.
 *
 * 3. Nodes cannot be connected directly to meta-routers.
 *
 * 4. Express links are only supported in <=32p/8r configs.
 *
 * 5. Any router can only have one express link.
 *
 * 6. Corresponding links must connect corresponding components.
 *
 * 7. Corresponding ports must be used when connecting components
 *   of the same type.
 *
 *  Also assign type to ports.
 *
 *  In a partitioned system,
 *   assign dimensions to ports that bridge assigned nasids.
 */
int check_router(pcfg_t *p, int ci, uchar_t ptype[], int *singleMeta)
{
  int	num_routers, found_peer, dimn, i, ni, np;
  int	node_bristle, rtr_bristle, meta_bristle, exp_bristle;
  char	node_mask, meta_mask, exp_mask;
  char	errmsg[80];
  uchar_t type;

  pcfg_ent_t	*pa;
  pcfg_router_t	*cr, *nr;

  pa = p->array;
  cr = &pa[ci].router;
  node_bristle = rtr_bristle = exp_bristle = meta_bristle = 0;
  node_mask = exp_mask = meta_mask = found_peer = 0;
  num_routers = p->count_type[PCFG_TYPE_ROUTER];

  if(debug_verbose) {
  db_printf("check_router at %d\n", ci);
  }

  for (i=MAX_ROUTER_PORTS;i>0;i--) {
    ni = cr->port[i].index;

    if (ni == PCFG_INDEX_INVALID)
      continue;

    /* 
     * set state to indicate that the link is being used, 
     * but the dimn has not been determined yet.
     * set type based on type of connected element.
     */
    type = LINK_STATE_UNSET;
    SET_FIELD(type, PCFG_TYPE, pa[ni].any.type);

    if (GET_FIELD(type, PCFG_TYPE) == PCFG_TYPE_HUB) {
      node_bristle++;
      node_mask |= 1<<(i-1);
    }

    if (GET_FIELD(type, PCFG_TYPE) == PCFG_TYPE_ROUTER) {
      nr = &pa[ni].router;

      /* 
       * check for assigned metaids, indicating a live partition
       * consistent returns the dimension that bridges cr and nr.
       */
      if (cr->metaid != PCFG_METAID_INVALID &&
	  nr->metaid != PCFG_METAID_INVALID &&
	  (dimn=consistent(cr, nr)) >= 0) {
	if (ptype[i] & LINK_STATE_MASK) { 
	  /* Link dimn is unset or unused, but the routers
	   * bridging it imply a dimn -> set it! 
	   */
	  if(debug_verbose) {
	  db_printf("assigned dimn %d to port %d\n", dimn, i);
	  }
	  ptype[i] = dimn;
	}
	/* Link dimn is set - is it consistent?
	 */
	if (ptype[i] != dimn) {
	  sprintf(errmsg, 
		  "inconsistent port dimensions %d and %d during rediscovery",
		  ptype[i], dimn);
	  config_error(p, ci, 1<<(i-1), errmsg);
	}
      }

      /*
       * check for meta connections, express links.
       */
      if (IS_META(nr)) {
	meta_bristle++;
	meta_mask |= 1<<(i-1);
	SET_FIELD(type, LINK_TYPE, LINK_TYPE_META);
      }
      else {
	rtr_bristle++;
	if (!IS_META(cr)) {
	  np = cr->port[i].port;
	  if (np != i) {
	    /* issue a warning, disconnect the link, and continue?
	     */
	    sprintf(errmsg, "corresponding ports must be connected");
	    config_error(p, ci, 1<<(i-1), errmsg);
	    return(1);
	  }
	  if (cr->slot != nr->slot) {
	    if (!found_peer && ni == get_peer_router(p, ci))
	      found_peer = 1;
	    else {
	      exp_bristle++;
	      exp_mask |= 1<<(i-1);
	      SET_FIELD(type, LINK_TYPE, LINK_TYPE_XPRESS);
	      SET_FIELD(cr->port[i].flags, PCFG_PORT_XPRESS, 1);
	      SET_FIELD(nr->port[np].flags, PCFG_PORT_XPRESS, 1);
	    }
	  }
	}
      }
    }

    /*
     * if this port has an assigned type, 
     * make sure it matches the type we generated here.
     *
     * XXX 
     * what about type mismatches between the live and
     * reset portions of a pttn'd machine???
     */
    if (ptype[i]&LINK_STATE_UNSET &&
	ptype[i] != type) {
      if(debug_verbose) {
      db_printf("type = 0x%x, expected type: 0x%x", type, ptype[i]);
      }
      sprintf(errmsg, "connection of inconsistent type");
      config_error(p, ci, 1<<(i-1), errmsg);
      return(-1);
    }

    /*
     * if this is the first router to use this port,
     * set the global port type here, and update the port
     * state to UNSET (from UNUSED).
     */
    if (ptype[i]&LINK_STATE_UNUSED &&
	GET_FIELD(ptype[i], PCFG_TYPE) == PCFG_TYPE_NONE) {
      if(debug_verbose) {
      if (IS_META(cr))
	db_printf("set mptype[%d] = 0x%x\n", i, type);
      else
	db_printf("set ptype[%d] = 0x%x\n", i, type);
      }
      ptype[i] = type;
    }
  }

  if (rtr_bristle>1 && IS_META(&pa[ci].router))
    *singleMeta = 1;

  if (meta_bristle>1 && !IS_META(&pa[ci].router)) {
    sprintf(errmsg, "multiple meta connections");
    config_error(p, ci, meta_mask, errmsg);
    return(1);
  }

  if (node_bristle && IS_META(&pa[ci].router)) {
    sprintf(errmsg, "nodes attached to meta router");
    config_error(p, ci, node_mask, errmsg);
    return(1);
  }

  if (node_bristle>2 && num_routers>2) {
    sprintf(errmsg, "%d nodes attached (max 2 allowed)", node_bristle);
    config_error(p, ci, node_mask, errmsg);
    return(1);
  }

  if (exp_bristle && num_routers>8) {
    sprintf(errmsg, "express links are not supported in >32p systems");
    config_error(p, ci, exp_mask, errmsg);
    return(1);
  }

  if (exp_bristle>1 && !DIP_ROUTER_OVEN()) {
    sprintf(errmsg, "multiple express links (max 1 allowed)");
    config_error(p, ci, exp_mask, errmsg);
    return(1);
  }

  return(0);
}

/*
 * nasid_assign()
 *
 * Assume we were called with index of root hub:
 *
 *   if the network link is down,
 *       assign nasid to 0 and return.
 *
 *   if the network link is up,
 *     and we are connected directly to another hub,
 *       assign nasids 0 and 1 and return.
 *
 *   if we are connected to a router, give it meta_id 0,
 *       call nasid_loop with the router as the root.
 *
 * Unless we are in a partitioned system where some
 * paritions are upNrunning:
 *
 *   discovery must fill in nasids + metaids for running elements.
 *   Note: nasid_loop root may or may not be a hub with a valid nasid.
 *
 */
int nasid_assign(pcfg_t *p, int from_index)
{
  pcfg_ent_t	*pa;

  int  ni, i, j, error, rc;
  int  port, dimn, mdimn;
  int  nodeBristle, singleMeta;
  uchar_t ptype[MAX_ROUTER_PORTS+1];
  uchar_t mptype[MAX_ROUTER_PORTS+1];

  char buf[64];
  nasid_t rsrvd_dimn, rsrvd_mdimn;

  if(debug_verbose) {
  db_printf("Calculating NASIDs\n");
  db_printf("num_routers is %d\n", p->count_type[PCFG_TYPE_ROUTER]);
  }

  pa = p->array;

  /*
   * Sanity check: make sure root is a hub.
   */
  if (pa[from_index].any.type != PCFG_TYPE_HUB) {
    printf("Error: expected hub at pcfg entry %d\n", from_index);
    return(-1);
  }

  ni = pa[from_index].hub.port.index;

  if (ni == PCFG_INDEX_INVALID) {
    /* network link is down
     */
    pa[from_index].hub.nasid = 0;
    return(0);
  }

  if (pa[ni].any.type == PCFG_TYPE_HUB) {
    /* back-to-back hubs
     *  is the system is partitioned? (legal for SN00)
     */
    if (pa[from_index].hub.nasid != PCFG_METAID_INVALID)
      pa[ni].hub.nasid = !pa[from_index].hub.nasid;
    else if (pa[ni].hub.nasid != PCFG_METAID_INVALID)
      pa[from_index].hub.nasid = !pa[ni].hub.nasid;
    else {
      pa[from_index].hub.nasid = 0;
      pa[ni].hub.nasid = 1;
    }

    return(0);
  }

  if (pa[ni].any.type != PCFG_TYPE_ROUTER) {
    printf("Error: bad pcfg entry %d: type = %d\n",
	   ni, pa[ni].any.type);
  }

  /*
   * ignore port 6 links for special METAROUTER_DIAG config,
   * only when metarouters are actually present.
   */
  if (DIP_METAROUTER_DIAG()) {
    int meta_present = 0;

    for (i=0; i<p->count; i++)
      if (pa[i].any.type == PCFG_TYPE_ROUTER && IS_META(&pa[i].router)) {
	meta_present = 1;
	break;
      }
    if (meta_present)
      for (i=0; i<p->count; i++)
	if (pa[i].any.type == PCFG_TYPE_ROUTER && !IS_META(&pa[i].router)) {
	  db_printf("Note: ignoring port %d connection on pcfg router %d\n",
		    PEER_PORT, i);
	  /*
	   * save connected router index for later restore.
	   * use available space in unused port[0] entry. 
	   */
	  pa[i].router.port[0].index = pa[i].router.port[PEER_PORT].index;
	  SET_FIELD(pa[i].router.port[PEER_PORT].flags, PCFG_PORT_FAKE, 1);

	  /* disconnect port 6
	   */
	  pa[i].router.port[PEER_PORT].index = PCFG_INDEX_INVALID;
	}
  }

  /*
   * there is a router:
   *  prepare for nasid assignment by checking
   *  that the configuration is valid.
   *
   * all port types are set to unused. ptype[] array is used to assign
   * type to 8p->node, 8p->8p and 8p->meta connections, mptype[] is
   * used for meta->8p and meta->meta. 
   */
  for (i=1; i<=MAX_ROUTER_PORTS; i++) {
    ptype[i] = LINK_STATE_UNUSED;
    SET_FIELD(ptype[i], PCFG_TYPE, PCFG_TYPE_NONE);
    mptype[i] = LINK_STATE_UNUSED;
    SET_FIELD(mptype[i], PCFG_TYPE, PCFG_TYPE_NONE);
  }
  singleMeta = error = 0;

  for (i=0; i<p->count; i++)
    if (pa[i].any.type == PCFG_TYPE_ROUTER) {
      rc = (IS_META(&pa[i].router)) ?
	check_router(p, i, mptype, &singleMeta) :
	  check_router(p, i, ptype, &singleMeta);
      if (rc<0)
	return(-1);
      if (rc)
	error++;
    }

  /*
   * find node dimensions
   */
  mdimn = 0; 
  rsrvd_mdimn = rsrvd_dimn = 0;
  nodeBristle = 0;

  for (port=1; port<=MAX_ROUTER_PORTS; port++)
    if (ptype[port]&LINK_STATE_UNSET &&
	GET_FIELD(ptype[port], PCFG_TYPE) == PCFG_TYPE_HUB)
      nodeBristle++;

  for(dimn=1; nodeBristle>2; nodeBristle>>=1)
    dimn++;

  if(debug_verbose) {
  db_printf("router links begin at dimn %d\n", dimn);
  }

  /* use the state field to avoid redundant checking?
   * GET_FIELD(pa[i].router.flags, STATE) != PCFG_ROUTER_STATE_DONE)
   */
  for (i=0; i<p->count; i++) {
    if (pa[i].any.type == PCFG_TYPE_ROUTER) {
      if (check_module(p, i))
	error++;
    }
    else {
      /* if NASID is valid,
       * make sure it is consistent with router metaid.
       */
      if (pa[i].hub.nasid != PCFG_METAID_INVALID) {
	from_index = i; /* select a root with nasid assigned */
	if (pa[i].hub.nasid>>dimn !=
	    pa[pa[i].hub.port.index].router.metaid>>dimn) {
	  sprintf(buf,
		  "nasid %d and metaid %d mismatch during rediscovery",
		  pa[i].hub.nasid,
		  pa[pa[i].hub.port.index].router.metaid);
	  config_error(p, pa[i].hub.port.index, 1<<pa[i].hub.port.port, buf);
	  return -1;
	}
      }
    }
  }

  if (error)
    return(-1);

  /*
   * assign dimensions to ports:
   * some port dimensions may have been assigned already 
   * (to reflect routers in a live partition), so they need to be
   * marked reserved for the purposes of further dimension selection. 
   */

  if(debug_verbose) {
  db_printf("\nPort types, pre dimension assignment:\n");
  }

  for (i=0; i<MAX_ROUTER_PORTS; i++) {
    port = ((i+5) % MAX_ROUTER_PORTS) + 1;
    if (!(ptype[port] & LINK_STATE_MASK))
      rsrvd_dimn |= 1<<GET_FIELD(ptype[port], PCFG_TYPE);
    if (!(mptype[port] & LINK_STATE_MASK))
      rsrvd_mdimn |= 1<<GET_FIELD(mptype[port], PCFG_TYPE);
    if(debug_verbose) {
      decode_link_type(ptype[port], buf);
      db_printf("\tptype[%d] = 0x%x(%s)", port, ptype[port], buf);
      decode_link_type(mptype[port], buf);
      db_printf("\tmptype[%d] = 0x%x(%s)\n", port, mptype[port], buf);
    }
  }

  if(debug_verbose) {
  db_printf("\n");
  }

#ifndef DISCTEST
#ifdef COARSE_TESTING
  /*
   * skip a few dimensions to force high nasids
   * and put the system in coarse mode.
   * to move from 0->7 to 64->71 , set force_coarse=0x38.
   */
  if (!singleMeta) {
    ip27log_getenv(get_nasid(), "force_coarse", buf, "0", 0);
    if (rsrvd_dimn) {
      printf("Note: ip27env rsrvd_dimn = 0x%x\n", rsrvd_dimn);
      rsrvd_dimn |= strtoull(buf, 0, 0);
    }
  }
#endif
#endif

  if(debug_verbose) {
  if (rsrvd_dimn)
    db_printf("Note: rsrvd_dimn = 0x%x\n", rsrvd_dimn);

  if (rsrvd_mdimn)
    db_printf("Note: rsrvd_mdimn = 0x%x\n", rsrvd_mdimn);

  db_printf("\nPort types, post dimension assignment:\n");
  }

  for (i=0; i<MAX_ROUTER_PORTS; i++) {
    port = ((i+5) % MAX_ROUTER_PORTS) + 1;

    if (ptype[port]&LINK_STATE_UNSET) {

      /* Clear UNSET bit and assign a dimension to this port
       */
      ptype[port] &= ~LINK_STATE_UNSET;

      switch (ptype[port]) {
      case PCFG_TYPE_ROUTER:
	while ((rsrvd_dimn>>dimn)%2)
	  dimn++;
	ptype[port] = dimn++;
	break;
      case PCFG_TYPE_ROUTER|(LINK_TYPE_META<<LINK_TYPE_SHFT):
	ptype[port] = 4;
	break;
      case PCFG_TYPE_ROUTER|(LINK_TYPE_XPRESS<<LINK_TYPE_SHFT):
	ptype[port] = -1; /* XXX */
	break;
      case PCFG_TYPE_HUB:
	ptype[port] = 0;
	break;
      default:
	ptype[port] |= LINK_STATE_UNUSED;
	break;
      }
    }

    if (mptype[port] & LINK_STATE_UNSET) {

      /* Clear UNSET bit and assign a dimension to this port
       */
      mptype[port] &= ~LINK_STATE_UNSET;

      switch (mptype[port]) {
      case PCFG_TYPE_ROUTER:
	/* in the singleMeta config,
	 * defer selecting a dimension until nasid_loop is run,
	 * since we don't know which cube the gmaster resides in.
	 */ 
	mptype[port] = (singleMeta) ? LINK_STATE_UNSET : 0;
	break;
      case PCFG_TYPE_ROUTER|(LINK_TYPE_META<<LINK_TYPE_SHFT):
	/* port==3 check is a HACK for inconsistently cabled 256p systems
	mptype[port] = (port==3) ? mptype[6] : NASID_LOCAL_BITS + mdimn++;
	 */
	mptype[port] = NASID_LOCAL_BITS + mdimn++;
	break;
      default:
	mptype[port] |= LINK_STATE_UNUSED;
	break;
      }
    }

    if(debug_verbose) {
      decode_link_type(ptype[port], buf);
      db_printf("\tptype[%d] = 0x%x(%s)", port, ptype[port], buf);
      decode_link_type(mptype[port], buf);
      db_printf("\tmptype[%d] = 0x%x(%s)\n", port, mptype[port], buf);
    }
  }

  /*
   * assign NASIDs
   */
  if (nasid_loop(p, from_index, ptype, mptype, rsrvd_mdimn) < 0)
    return(-1);

#ifndef DISCTEST
  /*
   * avoid using express links if "DisableExpress" is set.
   */
  ip27log_getenv(get_nasid(), "DisableExpress", buf, "0", 0);
  if (strtoull(buf, 0, 0)) {
    printf("*** Note: DisableExpress!=0, express links will not be used.\n");
    for (i=0; i<p->count; i++)
      if (pa[i].any.type == PCFG_TYPE_ROUTER)
	for (j=MAX_ROUTER_PORTS; j>0; j--)
	  if (GET_FIELD(pa[i].router.port[j].flags, PCFG_PORT_XPRESS))
	    pa[i].router.port[j].index = PCFG_INDEX_INVALID;
  }
#endif

  /*
   * remove FAKE links created by check_module before we return.
   * links marked FAKE for METAROUTER_DIAG support are restored
   * by route_assign().
   */
  for (i=0; i<p->count; i++)
    if (pa[i].any.type == PCFG_TYPE_ROUTER)
      for (j=MAX_ROUTER_PORTS; j>0; j--)
	if (GET_FIELD(pa[i].router.port[j].flags, PCFG_PORT_FAKE) &&
	    (!DIP_METAROUTER_DIAG() || 
	     pa[i].router.port[j].index != PCFG_INDEX_INVALID)) {
	  pa[i].router.port[j].index = PCFG_INDEX_INVALID;
	  SET_FIELD(pa[i].router.port[j].flags, PCFG_PORT_FAKE, 0);
	}

/*
  for (i=0; i<p->count; i++)
    if (pa[i].any.type == PCFG_TYPE_ROUTER)
      for (j=MAX_ROUTER_PORTS; j>0; j--)
	if (GET_FIELD(pa[i].router.port[j].flags, PCFG_PORT_FAKE)) {
	  if (DIP_METAROUTER_DIAG() &&
	      pa[i].router.port[j].index == PCFG_INDEX_INVALID)
	    pa[i].router.port[j].index = pa[i].router.port[0].index;
	  else
	    pa[i].router.port[j].index = PCFG_INDEX_INVALID;
	}
*/

  return(0);
}

/*
 * touch_promcfg_nasids: updates nasids in all promcfg entries for
 * global master. Must be called by global master only.
 *
 * returns:  0 - if fully successful
 *          -1 - if any failures
 */

#ifndef DISCTEST

int touch_pcfg_nasids(pcfg_t *p)
{
   int i, r = 0;

   p->array[0].hub.nasid = get_nasid();

   for (i = 1; i < p->count; i++) {
      net_vec_t route;
      __uint64_t ni_status_id;

      if (p->array[i].any.type != PCFG_TYPE_HUB)
	 continue;

      route = discover_route(p, 0, i, 0);
      if (route == NET_VEC_BAD) {
	 r = -1;
	 continue;
      }

      if (vector_read(route, 0, NI_STATUS_REV_ID, &ni_status_id) < 0) {
	 r = -1;
	 continue;
      }

      p->array[i].hub.nasid =
		 (nasid_t) GET_FIELD(ni_status_id, NSRI_NODEID);
   }

   return(r);
}

#endif
