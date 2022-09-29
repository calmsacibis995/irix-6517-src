/* this is the code implementing the
 * Numbering-Coherent Minimal Routing algorithm;
 * PROM edition 
 */

#include <sys/cpu.h>
#include <sys/mips_addrspace.h>
#include "libasm.h"
#include "ip27prom.h"
#include "discover.h"
#include "nasid.h" /* includes promcfg.h */
#include "libc.h"
#include <sys/SN/SN0/hub.h>

/*#ifdef DISCTEST*/
#define db_printf printf
/*#endif*/

/* 128p = 64 nodes + 40 routers = 104 pcfg elements (120 ok)
   256p = 128 nodes + 128 routers = 256 pcfg elements
   512p = 256 nodes + 256 routers = 512 pcfg elements
 */
#define NEW_MAXVRT 512
#define NEW_MAXQUEUE (NEW_MAXVRT+10)

/* in the macros these will be compared to the result of IS_META */
#define NEW_8PROUTER 0
#define NEW_METAROUTER 1

#define NEW_INIT 1
#define NEW_NOINIT 0

/* used in bit-wise comparisons */
#define NEW_VERY_BIG 0xfff

/* used to announce that there was a problem creating the vector */
#define NEW_INVALID_ROUTE_VEC ((net_vec_t)0xf0)

/* some router checking macros 
 * IS_ROUTER <=> is entry idx a router?
 * IS_PORTROUTER <=> is the thing connected to port prt of idx a router?
 * IS_ROUTERTYPE <=> is entry idx a router, of type rtp?
 * IS_PORTROUTERTYPE <=> is the thing connected to port prt a router of
 *	type rtp?
 */
#define IS_ROUTER(p,idx) ((p)->array[(idx)].any.type==PCFG_TYPE_ROUTER)
#define IS_PORTROUTER(p,idx,prt) \
 ((p)->array[(idx)].router.port[(prt)].index!=PCFG_INDEX_INVALID&& \
  (p)->array[(idx)].router.port[(prt)].port!=7&& \
  IS_ROUTER(p,(p)->array[(idx)].router.port[(prt)].index))
#define IS_ROUTERTYPE(p,idx,rtp) \
 (IS_ROUTER(p,idx)&& \
  (IS_META(&((p)->array[(idx)].router)) \
   ==(rtp)))
#define IS_PORTROUTERTYPE(p,idx,prt,rtp) \
 ((p)->array[(idx)].router.port[(prt)].index!=PCFG_INDEX_INVALID&& \
  (p)->array[(idx)].router.port[(prt)].port!=7&& \
  IS_ROUTERTYPE(p,(p)->array[(idx)].router.port[(prt)].index,rtp))

/* r1 & r2 MUST be 8Prouters if you use this macro */
#define ARE_SAMECUBE(p,r1,r2) \
 (NASID_GET_META((p)->array[(r1)].router.metaid)== \
  NASID_GET_META((p)->array[(r2)].router.metaid))

/* used for the routing table and distance table */
typedef int new_table[NEW_MAXVRT][NEW_MAXVRT];

static void err(char *errmsg) {
  printf("\nroute: error: %s!\n",errmsg);
}

static void err_i(char *errmsg,int number) {
  char buf[200];

  sprintf(buf,errmsg,number);
  printf("\nroute: error: %s!\n",buf);
}

static __psunsigned_t route_allocated = 0;

static __psunsigned_t
route_alloc(int size)
{
  if (route_allocated + size > ROUTE_SIZE)
    printf("*** route_alloc: route size exceeded!\n");

  route_allocated += size;

  return (__psunsigned_t) ROUTE_BASE + route_allocated - size;
}

/* this is the routing part:
 *
 * find_cubes finds the different cube types
 *	(metacube numbers, for each router);
 * find_dist finds the absolute distances between routers of
 * 	the same type (8P vs. META); 
 * find_routing does the job at the 'cube' level; 
 * find_metarouting connects the cubes using the meta layer.
 */

static void find_dist(pcfg_t *p,int routertype,int init,
		      new_table d,int cube[NEW_MAXVRT]) {
  int i,j,k;

  if (init)
    for (i=0;i<p->count;i++) for (j=0;j<p->count;j++) d[i][j]=((i!=j)?-1:0);

  /* init the d[s][d] for linked routers */
  for (i=0;i<p->count;i++) if (IS_ROUTERTYPE(p,i,routertype))
    for (j=1;j<=MAX_ROUTER_PORTS;j++)
      if (IS_PORTROUTERTYPE(p,i,j,routertype)) {
	/* the following two we need in order to be able to
	 * deal with two special cases:
	 * 1. a 4D cube made of 8p-routers;
	 * 2. 8 meta-routers, <4 cubes, each meta-router is connected to
	 * 	all cubes and to another meta-router.
	 */
	if (routertype==NEW_8PROUTER&&
	    cube[i]!=cube[p->array[i].router.port[j].index]) continue;
	if (routertype==NEW_METAROUTER&&
	    (p->array[i].router.force_local||
	     p->array[p->array[i].router.port[j].index].router.force_local))
	    continue;
	d[i][p->array[i].router.port[j].index]=1;
      }

  /* Floyd-Warshall algorithm */
  for (k=0;k<p->count;k++)
    for (i=0;i<p->count;i++)
      for (j=0;j<p->count;j++) if (d[i][k]>0&&d[k][j]>0&&
				   (d[i][j]==-1||d[i][j]>d[i][k]+d[k][j]))
	d[i][j]=d[i][k]+d[k][j];
}

static void find_routing(pcfg_t *p,int routertype,int init,
			 new_table d,new_table rt) {
  int i,j,k,i1,i2,phase,x;
  
  if (init)
    for (i=0;i<p->count;i++) for (j=0;j<p->count;j++) {
      if (i==j)
	rt[i][j]=i;
      else
	rt[i][j]=-1;
    }

  for (i=0;i<p->count;i++) for (j=0;j<p->count;j++) 
    if (IS_ROUTERTYPE(p,i,routertype)&&IS_ROUTERTYPE(p,j,routertype)) {
      if (d[i][j]==1) rt[i][j]=j;
    }

  /* generate for dist==2 */
  for (i=0;i<p->count;i++) for (j=0;j<p->count;j++) 
    if (d[i][j]==2&&
	IS_ROUTERTYPE(p,i,routertype)&&
	IS_ROUTERTYPE(p,j,routertype)) {
      int mdiff,mvert,cdiff;

      mdiff=NEW_VERY_BIG;
      for (k=0;k<p->count;k++) if (d[i][k]==1&&d[k][j]==1) {
	/* no need to check if 'k' is of same routertype if d[i][k]&&d[k][j] */

	/* hm... try the following with k^j too... this could show that the
	 * numbering in our graph is actually insignificant...
	 * if we could only prove all that :);
	 * this means "is it actually enought just to be coherent..."
	 * probably not, but -- oh well.
	 */
	cdiff=(p->array[k].router.metaid)^(p->array[j].router.metaid);    
	if (cdiff<mdiff) {
	  mdiff=cdiff;
	  mvert=k;
	}
      }

      rt[i][j]=mvert;
    }

  /* now generate the longer routes */
  for (phase=3;phase<p->count;phase++) 
    for (i=0;i<p->count;i++) for (j=0;j<p->count;j++) 
      if (d[i][j]==phase&&
	  IS_ROUTERTYPE(p,i,routertype)&&
	  IS_ROUTERTYPE(p,j,routertype)) {
	int flag,path[NEW_MAXVRT+10];
	
	flag=0;
	for (k=0;k<p->count;k++) if (d[i][k]==1&&d[k][j]==phase-1) {
	  for (x=k,i1=0;x!=j;x=rt[x][j],i1++) path[i1]=x;
	  path[i1]=-100; /* could be anything <-1 */
	  for (x=rt[i][path[i1-1]],i2=0;
	       x==path[i2];x=rt[x][path[i1-1]],i2++);
	  if (i1==i2) {
	    if (flag) 
	     if(verbose) {
	      db_printf("route: find_routing: not well defined between %d %d.\n",i,j);
	     }
	    if (!flag||
		(p->array[rt[i][j]].router.metaid^p->array[j].router.metaid)>
		(p->array[k].router.metaid^p->array[j].router.metaid))
	      rt[i][j]=k;
	    flag=1;
	  }
	}
      }
}

static void inc(int *a) {
  (*a)++;
  if (*a>=NEW_MAXQUEUE) *a=0;
}

static void find_cubes(pcfg_t *p,int cube[NEW_MAXVRT]) {
  int i;

  for (i=0;i<p->count;i++) if (IS_ROUTERTYPE(p,i,NEW_8PROUTER))
    cube[i]=NASID_GET_META(p->array[i].router.metaid);
  else
    cube[i]=-1;
}

static void find_metarouting(pcfg_t *p,int cube[NEW_MAXVRT],new_table rt) {
  int iix,i,j,k,w,qf,ql,cagent;
#ifdef USE_ROUTE_ALLOC_MEMORY
  int *queue;
  int *usedv;
  int *passed;
  int *agent;

  queue = (int *) route_alloc(sizeof(int) * NEW_MAXQUEUE); /* 2k + 40 */
  usedv = (int *) route_alloc(sizeof(int) * NEW_MAXVRT);  /* 2k */
  passed = (int *) route_alloc(sizeof(int) * NEW_MAXVRT); /* 2k */
  agent = (int *) route_alloc(sizeof(int) * NEW_MAXVRT);  /* 2k */
#else
  int queue[NEW_MAXQUEUE];
  int usedv[NEW_MAXVRT];
  int passed[NEW_MAXVRT];
  int agent[NEW_MAXVRT];
#endif

  /* we will consider every cube as a destination and find the
   * routes to its members; first we route the metalayer;
   * then the 8p-cubes; for a certain destination cube every
   * meta-router gets its own 'agent' which is a meta-router connected
   * directly to the cube and each 8p-router gets and 8p-router 'agent'
   * which is directly connected to the meta-layer; these 'agents' are
   * used for all traffic to that specific destination 8p-cube.
   */
  for (iix=0;iix<p->count;iix++) passed[iix]=0;
  for (iix=0;iix<p->count;iix++) if (cube[iix]!=-1&&!passed[iix]) {
    i=cube[iix];

    /* the roots in the BFS are the routers from the 8p-cube we
     * have picked; thus all the meta-routers connected to the cube
     * will send traffic directly to the cube; also every meta-router
     * will use an agent that's closest to it
     */

    for (j=0;j<p->count;j++) { usedv[j]=0; agent[j]=-1; }
    qf=0; ql=0;
    for (j=0;j<p->count;j++) if (cube[j]==i) {
      queue[ql]=j;
      usedv[j]=1;
      agent[j]=-1;
      ql++;
    }

    while (qf!=ql) {
      w=queue[qf];
      cagent=agent[w];
      inc(&qf);
      for (j=1;j<=MAX_ROUTER_PORTS;j++) 
	if (IS_PORTROUTERTYPE(p,w,j,NEW_METAROUTER)&&
	    !usedv[p->array[w].router.port[j].index]) {
	  if (IS_ROUTERTYPE(p,w,NEW_METAROUTER)&&
	      (p->array[w].router.force_local||
	      p->array[p->array[w].router.port[j].index].router.force_local))
	    continue;
	  usedv[p->array[w].router.port[j].index]=1;
	  queue[ql]=p->array[w].router.port[j].index;
	  inc(&ql);
	  for (k=0;k<p->count;k++) 
	    if (cube[k]==i)
	      if (cagent==-1) {
	        rt[p->array[w].router.port[j].index][k]=w;
		agent[p->array[w].router.port[j].index]=
		    p->array[w].router.port[j].index;
	      } else {
		rt[p->array[w].router.port[j].index][k]=
		    rt[p->array[w].router.port[j].index][cagent];
		agent[p->array[w].router.port[j].index]=cagent;
	      }
	}
    }

    /* now route the 8p-cubes in pretty much the same way */

    for (j=0;j<p->count;j++) agent[j]=-1;
    qf=0; ql=0;
    for (j=0;j<p->count;j++) if (usedv[j]) {
      queue[ql]=j;
      ql++;
    }

    while (qf!=ql) {
      w=queue[qf];
      cagent=agent[w];
      inc(&qf);
      for (j=1;j<=MAX_ROUTER_PORTS;j++)
	if (IS_PORTROUTERTYPE(p,w,j,NEW_8PROUTER)&&
	    !usedv[p->array[w].router.port[j].index]) {
	  usedv[p->array[w].router.port[j].index]=1;
	  queue[ql]=p->array[w].router.port[j].index;
	  inc(&ql);
	  for (k=0;k<p->count;k++)
	    if (cube[k]==i)
	      if (cagent==-1) {
	        rt[p->array[w].router.port[j].index][k]=w;
		agent[p->array[w].router.port[j].index]=
		    p->array[w].router.port[j].index;
	      } else {
		rt[p->array[w].router.port[j].index][k]=
		    rt[p->array[w].router.port[j].index][cagent];
		agent[p->array[w].router.port[j].index]=cagent;
	      }
	}
    }

    /* make sure we don't use pick this cube as a destination again */
    for (j=0;j<p->count;j++) if (cube[j]==i) passed[j]=1;
  }
}

static int assign_routes(pcfg_t *p,new_table rt) {
  int i,j;
  new_table *d;
#ifdef USE_ROUTE_ALLOC_MEMORY
  int *cube;

  cube = (int *) route_alloc(sizeof(int) * NEW_MAXVRT); /* 2k */
#else
  int cube[NEW_MAXVRT];
#endif

  d = (new_table *) route_alloc(sizeof(new_table));	/* 1 MB */

  /* route the 8p- and meta- layers separately,
   * then take care of the connections between them
   */
  find_cubes(p,cube);
  /* the first calls to find_dist and find_routing are with INIT;
   * the subsequent ones are with NOINIT, to preserve the info
   * that is already in the tables.
   */
  find_dist(p,NEW_8PROUTER,NEW_INIT,*d,cube);
  find_routing(p,NEW_8PROUTER,NEW_INIT,*d,rt);
  find_dist(p,NEW_METAROUTER,NEW_NOINIT,*d,cube);
  find_routing(p,NEW_METAROUTER,NEW_NOINIT,*d,rt);
  
  if(verbose) {
  db_printf("\n");
  db_printf("route: assign_routes: local routing table (in cubes & in metalayer) :\n");
  for (i=0;i<p->count;i++) if (IS_ROUTER(p,i)) {
    for (j=0;j<p->count;j++) if (IS_ROUTER(p,j)) db_printf("%5d",rt[i][j]);
    db_printf("\n");
  }
  db_printf("\n");
  }

  find_metarouting(p,cube,rt);
  
  if(verbose) {
  db_printf("\n");
  db_printf("route: assign_routes: routing table:\n");
  for (i=0;i<p->count;i++) if (IS_ROUTER(p,i)) {
    for (j=0;j<p->count;j++) if (IS_ROUTER(p,j)) db_printf("%5d",rt[i][j]);
    db_printf("\n");
  }
  db_printf("\n");
  }

  return 0;
}

/* this part will distribute the routes from the routing
 * table among the local routing tables of the nodes;
 *
 * get_route creates a single vector between a pait of hubs;
 * distribute_route_info goest through the pairs of hubs and
 *	calls get_route for each.
 */

static net_vec_t get_route(pcfg_t *p,int sr,int dr,int dh,new_table rt) {
  net_vec_t rte;
  net_vec_t rte1;
  int localcube,localpartition,partition;
  int cr,nr,i;
  int links,clink;
  
  rte=0; cr=sr;
  localcube=ARE_SAMECUBE(p,sr,dr);
  partition=p->array[sr].router.partition;
  localpartition=(partition==p->array[dr].router.partition);

  if(verbose) {
  db_printf("route: get_route: tracking:");
  }
  
  while (cr!=dr) {
  if(verbose) {
    db_printf(" %d",cr);
  }

    if (cr==-1) {
      err("get_route: bad routing table -- probably bad system structure");
      return NEW_INVALID_ROUTE_VEC;
    }
    if (localcube&&IS_ROUTERTYPE(p,cr,NEW_METAROUTER)) {
      err_i("local cube route uses meta-router %d",cr);
      return NEW_INVALID_ROUTE_VEC;
    }
    if (localpartition&&!IS_ROUTERTYPE(p,cr,NEW_METAROUTER)&&
	p->array[cr].router.partition!=partition) {
      err_i("local partition route exits partition to router %d",cr);
      return NEW_INVALID_ROUTE_VEC;
    }
    nr=rt[cr][dr];
    
    /* now we'll find how many links we have and spread the
     * traffic across them.
     */
    links=0;
    for (i=1;i<=MAX_ROUTER_PORTS;i++)
      if (p->array[cr].router.port[i].index==nr) links++;
    clink=0;
    for (i=1;i<=MAX_ROUTER_PORTS;i++)
      if (p->array[cr].router.port[i].index==nr)
        if (clink==(dh%links))
	  break;
	else
	  clink++;

    rte<<=4;
    rte|=i;
    cr=nr;
  }

  if(verbose) {
  db_printf("\n");
  }
  
  /* last hop: dst_router -> dst_hub */
  for (i=1;i<=MAX_ROUTER_PORTS;i++)
    if (p->array[dr].router.port[i].index==dh) break;
  rte<<=4;
  rte|=i;
  
  rte1=0;
  while (rte) {
    rte1<<=4;
    rte1|=(rte&(net_vec_t)0xf);
    rte>>=4;
  }

  return rte1;
}

static int distribute_route_info(pcfg_t *p,new_table rt) {
  int i,j,irt,jrt;
  net_vec_t route;

  /* now loop through all sources and destinations and fill in tables */
  for (i=0;i<p->count;i++) if (p->array[i].any.type==PCFG_TYPE_HUB) {
    irt=p->array[i].hub.port.index;
    if (p->array[i].hub.port.index==PCFG_INDEX_INVALID||
	!IS_ROUTER(p,irt)) {
      err_i("distribute_route_info: expected router connection at hub %d\n",i);
      return NEW_FAIL;
    }
    for (j=0;j<p->count;j++) if (i!=j&&p->array[j].any.type==PCFG_TYPE_HUB) {
      jrt=p->array[j].hub.port.index;
      if (p->array[j].hub.port.index==PCFG_INDEX_INVALID||
	  !IS_ROUTER(p,jrt)) {
	err_i("distribute_route_info: expected router connection at hub %d\n",j);
	return NEW_FAIL;
      }
      route=get_route(p,irt,jrt,j,rt);
  if(verbose) {
      db_printf("route: distribute_route_info: from (h%d at r%d) to (h%d r%d) : vector 0x%llx\n",
		i,irt,j,jrt,route);
  }
      if (route==NEW_INVALID_ROUTE_VEC) return NEW_FAIL;
      discover_program(p,i,p->array[j].hub.nasid,route,NULL,NULL);
    }
  }
  
  return 0;
}

/* set the force_local flag for the appropriate routers */

int set_force_local(pcfg_t *p) {
  int i,j;

  /* if we are a router connected to a 8P-router with different
   * metaid, our force_local must be set
   */
  for (i=0;i<p->count;i++) if (IS_ROUTER(p,i)) {
    for (j=1;j<=MAX_ROUTER_PORTS;j++) 
      if (IS_PORTROUTERTYPE(p,i,j,NEW_8PROUTER)) {
	if (NASID_GET_META(p->array[i].router.metaid)!=
	    NASID_GET_META(p->array[p->array[i].router.port[j].index].
			   router.metaid)) {
	  if(verbose) {
	  db_printf("route: set_force_local: enable force local at router %d.\n",i);
	  }
	  p->array[i].router.force_local=1;
	  break;
	}
      }
  }

  return 0;
}

/* this function calls the rest */

int route_assign(pcfg_t *p) {
  /*
   * routing table; if rt[i][j]==k, that means: "to go from i to j, first
   *	go to k, which is a neighbor of i".
   */
  new_table *rt;

  /* hmm, maybe every time through here we should set route_allocated
     to zero (as this is called multiple times, main.c, exec.c */
  route_allocated = 0;

  rt = (new_table *) route_alloc(sizeof(new_table));	/* 1 MB */

  if(verbose) {
  db_printf("Calculating routes\n");
  }

  if (p->count <= 2)	/* No programming necessary */
    return 0;

  /* memory is cheap, but finite... we need O(n^n) of it... sorry */
  if (p->count>NEW_MAXVRT) {
    err("route_assign: promcfg entries more than allowed");
    return NEW_FAIL;
  }

  if (set_force_local(p)==NEW_FAIL) return NEW_FAIL;
  if (assign_routes(p,*rt)==NEW_FAIL) return NEW_FAIL;
  if (distribute_route_info(p,*rt)==NEW_FAIL) return NEW_FAIL;
  if (deadlock_check(p)==NEW_FAIL) return NEW_FAIL;

  /*
   * restore real links that were ignored for METAROUTER_DIAG mode.
   */
  if (DIP_METAROUTER_DIAG()) {
    pcfg_ent_t	*pa = p->array;
    int i, j;

    for (i=0; i<p->count; i++)
      if (pa[i].any.type == PCFG_TYPE_ROUTER)
	for (j=MAX_ROUTER_PORTS; j>0; j--)
	  if (GET_FIELD(pa[i].router.port[j].flags, PCFG_PORT_FAKE) &&
	      pa[i].router.port[j].index == PCFG_INDEX_INVALID)
	    pa[i].router.port[j].index = pa[i].router.port[0].index;
  }

  return 0;
}
