#include <sys/types.h>

#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/SN/router.h>

#ifdef DISCTEST
#include <stdio.h>
#include <stdlib.h>
#define db_printf printf
#else
#include "libc.h"
#endif

#include "ip27prom.h"
#include "nasid.h"

/* move to promcfg.h?
 */
#define CHANNEL_SHFT	(2)
#define CHANNEL_MASK	(0xfcU)

#define LDEBUG 0

/* replace 512 w/ max pcfg size define? */
char dgraph[512][MAX_ROUTER_PORTS+1];
char dinfo[512][MAX_ROUTER_PORTS+1][MAX_ROUTER_PORTS+1];

pcfg_port_t *root;

/*
 * scan the routing tables 
 * looking for channel dependencies
 */
static void dgraph_add_edge(int router_num, pcfg_router_t *cr, int pt)
{
  int i, turns, npt;

  /* scan local tables
   */
  for (i=0;i<RR_LOCAL_ENTRIES;i++) {
    /* get the nextDir nibble corres. to our port (nibbles 0->5)
     */
    turns = vector_get(cr->rtlocal[i], pt-1);
#if LDEBUG
    db_printf("entry 0x%x, turns 0x%x\n", cr->rtlocal[i], turns);
#endif
    if (turns>0 && turns<MAX_ROUTER_PORTS) {
      /* 1 <= npt <= 6 
       */
      npt = ((cr->port[pt].port)-1+turns)%MAX_ROUTER_PORTS + 1;
      dgraph[router_num][pt] |= (char) 1<<(npt-1+CHANNEL_SHFT);
      dinfo[router_num][pt][npt]=i;
    if(verbose) {
      db_printf("entry 0x%x: pt %d + turns %d = npt %d, channels = 0x%x\n", 
	     cr->rtlocal[i], pt, turns, npt, 
	     dgraph[router_num][pt]>>CHANNEL_SHFT);
    }
    }
  }

  if (cr->flags & RPCONF_FORCELOCAL)
    return;

  /* scan meta table
   */
  for (i=0;i<RR_META_ENTRIES;i++) {
    /* get the nextDir nibble corres. to our port (nibbles 0->5)
     */
    turns = vector_get(cr->rtmeta[i], pt-1);
    if (turns>0 && turns<MAX_ROUTER_PORTS) {
      npt = ((cr->port[pt].port)-1+turns)%MAX_ROUTER_PORTS + 1;
      dgraph[router_num][pt] |= (char) 1<<(npt-1+CHANNEL_SHFT);
      dinfo[router_num][pt][npt]=i+100;
    }
  }
}

static void dgraph_init(pcfg_t *p)
{
    int i, j, v;

    v = 0;
    root = NULL;

    if(verbose) {
    db_printf(" *** Initializing dgraph.\n");
    }

    for (i=0;i<p->count;i++) {
	if (p->array[i].any.type == PCFG_TYPE_ROUTER) {
  	if(verbose) {
	    db_printf("router %d:\n", i);
  	   }
	    for (j=1;j<=MAX_ROUTER_PORTS;j++) {
		dgraph[i][j]=0;
		if (p->array[i].router.port[j].index!=PCFG_INDEX_INVALID &&
		    p->array[i].router.port[j].port>0 &&
		    p->array[i].router.port[j].port<=MAX_ROUTER_PORTS) {
		    dgraph_add_edge(i, &p->array[i].router, j);
 	            if(verbose) {
		    db_printf("\tport %d dependency vector = 0x%x\n", 
			   j, 
			   dgraph[i][j]>>CHANNEL_SHFT);
	     		}
		    v++;
		}
	    }
	}
    }
    if(verbose) {
    db_printf("Initialized %d channels.\n", v);
    }
    return;
}

/* look for cycles and deadlock potential
 *
 * does the graph have to be connected?
 * can we do a DFS from the root and see if we loop back?
 */
static int dgraph_check(pcfg_t *p, int ci, int cp, int dp)
{
  uint channels;
  int  ni, np;
  pcfg_port_t *pp;

  if(verbose) {
  db_printf("%*sdgraph_check at %d (metaid:%d) port %d, depth %d\n", 
	 dp*3, "", ci, p->array[ci].router.metaid, cp, dp);
  }

  pp = &p->array[ci].router.port[cp];
  ni = p->array[ci].router.port[cp].index;

  if (root == NULL)
    root = pp;
  else
    if (root == pp) {
      printf("Error: routing cycle detected!\n");
      return -1;
    }

  if (ni == PCFG_INDEX_INVALID)
    return 0;

  channels = GET_FIELD(dgraph[ci][cp], CHANNEL);

  if(verbose) {
  db_printf("\tchannels are 0x%x\n", channels);
  }
  np = 1;

  while(channels) {
    if (channels%2) {
      /* clear the bit & recurse
       */
      dgraph[ci][cp] &= ~(1 << (np-1+CHANNEL_SHFT));
      if(verbose) {
      db_printf("%*sdgraph_check goes because of %d\n",
	     dp*3, "", dinfo[ci][cp][np]);
      db_printf("npt %d, channels are 0x%x\n",
		np, dgraph[ci][cp]>>CHANNEL_SHFT);
      }
      if (dgraph_check(p, ni, np, dp+1))
	return -1;
    }
    channels >>= 1;
    np++;
  }

  return 0;
}

int deadlock_check(pcfg_t *p)
{
  int i, j, rc;

  /* set up vertices in the channel dependency graph (dgraph)
   */
  for (i=0; i<p->count; i++)
    if (p->array[i].any.type == PCFG_TYPE_ROUTER)
      for (j=1; j<=MAX_ROUTER_PORTS; j++)
	if (p->array[i].router.port[j].index!=PCFG_INDEX_INVALID) {
	  dgraph_init(p);
	  if (rc=dgraph_check(p, i, j, 1)) {
	    db_printf("\ndgraph_check failed!!! (%d)\n\n", rc);
	    return -1;
	  }
	  break;
	}

  return 0;
}
