/*
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE".  Use of the mrouted program represents acceptance of
 * the terms and conditions listed in that file.
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 *
 * route.c,v 3.8.4.24 1997/06/06 20:42:19 fenner Exp
 */


#include "defs.h"


/*
 * This define statement saves a lot of space later
 */
#define RT_ADDR	(struct rtentry *)&routing_table

/*
 * Exported variables.
 */
int routes_changed;			/* 1=>some routes have changed */
int delay_change_reports;		/* 1=>postpone change reports  */


/*
 * The routing table is shared with prune.c , so must not be static.
 */
struct rtentry *routing_table;		/* pointer to list of route entries */

/*
 * Private variables.
 */
static struct rtentry *rtp;		/* pointer to a route entry         */
static struct rtentry *rt_end;		/* pointer to last route entry      */
unsigned int nroutes;			/* current number of route entries  */

/*
 * Private functions.
 */
static int init_children_and_leaves	__P((struct rtentry *r,
						vifi_t parent));
static int find_route		__P((u_int32 origin, u_int32 mask));
static void create_route	__P((u_int32 origin, u_int32 mask));
static void discard_route	__P((struct rtentry *prev_r));
static int compare_rts		__P((const void *rt1, const void *rt2));
static int report_chunk		__P((int, struct rtentry *start_rt, vifi_t vifi,
						u_int32 dst));

#ifdef SNMP
#include <sys/types.h>
#include "snmp.h"

/*
 * Return pointer to a specific route entry.  This must be a separate
 * function from find_route() which modifies rtp.
 */
struct rtentry *
snmp_find_route(src, mask)
    register u_int32 src, mask;
{
    register struct rtentry *rt;

   for (rt = routing_table; rt; rt = rt->rt_next) {
      if (src == rt->rt_origin && mask == rt->rt_originmask)
         return rt;
   }
   return NULL;
}

/*
 * Find next route entry > specification 
 */
int
next_route(rtpp, src, mask)
   struct rtentry **rtpp;
   u_int32 src;
   u_int32 mask;
{
   struct rtentry *rt, *rbest = NULL;

   /* Among all entries > spec, find "lowest" one in order */
   for (rt = routing_table; rt; rt=rt->rt_next) {
      if ((ntohl(rt->rt_origin) > ntohl(src) 
          || (ntohl(rt->rt_origin) == ntohl(src) 
             && ntohl(rt->rt_originmask) > ntohl(mask)))
       && (!rbest || (ntohl(rt->rt_origin) < ntohl(rbest->rt_origin))
          || (ntohl(rt->rt_origin) == ntohl(rbest->rt_origin)
             && ntohl(rt->rt_originmask) < ntohl(rbest->rt_originmask))))
               rbest = rt;
   }
   (*rtpp) = rbest;
   return (*rtpp)!=0;
}

/*
 * Given a routing table entry, and a vifi, find the next vifi/entry
 */
int
next_route_child(rtpp, src, mask, vifi)
   struct rtentry **rtpp;
   u_int32   src;
   u_int32   mask;
   vifi_t   *vifi;     /* vif at which to start looking */
{
   /* Get (S,M) entry */
   if (!((*rtpp) = snmp_find_route(src,mask)))
      if (!next_route(rtpp, src, mask))
         return 0;

   /* Continue until we get one with a valid next vif */
   do {
      for (; (*rtpp)->rt_children && *vifi<numvifs; (*vifi)++)
         if (VIFM_ISSET(*vifi, (*rtpp)->rt_children))
            return 1;
      *vifi = 0;
   } while( next_route(rtpp, (*rtpp)->rt_origin, (*rtpp)->rt_originmask) );

   return 0;
}
#endif

/*
 * Initialize the routing table and associated variables.
 */
void
init_routes()
{
    routing_table        = NULL;
    rt_end		 = RT_ADDR;
    nroutes		 = 0;
    routes_changed       = FALSE;
    delay_change_reports = FALSE;
}


/*
 * Initialize the children bits for route 'r', along with the
 * associated dominant and subordinate data structures.
 * Return TRUE if this changes the value of either the children or
 * leaf bitmaps for 'r'. (XXX!!!)
 */
static int
init_children_and_leaves(r, parent)
    register struct rtentry *r;
    register vifi_t parent;
{
    register vifi_t vifi;
    register struct uvif *v;
    vifbitmap_t old_children;

    VIFM_COPY(r->rt_children, old_children);

    VIFM_CLRALL(r->rt_children);

    for (vifi = 0, v = uvifs; vifi < numvifs; ++vifi, ++v) {
	r->rt_dominants   [vifi] = 0;
	if (vifi == parent || uvifs[vifi].uv_flags & VIFF_NOFLOOD)
	    NBRM_CLRMASK(r->rt_subordinates, uvifs[vifi].uv_nbrmap);
	else
	    NBRM_SETMASK(r->rt_subordinates, uvifs[vifi].uv_nbrmap);

	if (vifi != parent && !(v->uv_flags & (VIFF_DOWN|VIFF_DISABLED))) {
	    VIFM_SET(vifi, r->rt_children);
	}
    }

    return (!VIFM_SAME(r->rt_children, old_children));
}


/*
 * A new vif has come up -- update the children bitmaps in all route
 * entries to take that into account.
 */
void
add_vif_to_routes(vifi)
    register vifi_t vifi;
{
    register struct rtentry *r;
    register struct uvif *v;

    v = &uvifs[vifi];
    for (r = routing_table; r != NULL; r = r->rt_next) {
	if (r->rt_metric != UNREACHABLE &&
	    !VIFM_ISSET(vifi, r->rt_children)) {
	    VIFM_SET(vifi, r->rt_children);
	    r->rt_dominants   [vifi] = 0;
	    /*XXX isn't uv_nbrmap going to be empty?*/
	    NBRM_CLRMASK(r->rt_subordinates, v->uv_nbrmap);
	    update_table_entry(r, r->rt_gateway);
	}
    }
}


/*
 * A vif has gone down -- expire all routes that have that vif as parent,
 * and update the children bitmaps in all other route entries to take into
 * account the failed vif.
 */
void
delete_vif_from_routes(vifi)
    register vifi_t vifi;
{
    register struct rtentry *r;

    for (r = routing_table; r != NULL; r = r->rt_next) {
	if (r->rt_metric != UNREACHABLE) {
	    if (vifi == r->rt_parent) {
		del_table_entry(r, 0, DEL_ALL_ROUTES);
		r->rt_timer    = ROUTE_EXPIRE_TIME;
		r->rt_metric   = UNREACHABLE;
		r->rt_flags   |= RTF_CHANGED;
		routes_changed = TRUE;
	    }
	    else if (VIFM_ISSET(vifi, r->rt_children)) {
		VIFM_CLR(vifi, r->rt_children);
		NBRM_CLRMASK(r->rt_subordinates, uvifs[vifi].uv_nbrmap);
		update_table_entry(r, r->rt_gateway);
	    }
	    else {
		r->rt_dominants[vifi] = 0;
	    }
	}
    }
}


/*
 * A new neighbor has come up.  If we're flooding on the neighbor's
 * vif, mark that neighbor as subordinate for all routes whose parent
 * is not this vif.
 */
void
add_neighbor_to_routes(vifi, index)
    register vifi_t vifi;
    register int index;
{
    register struct rtentry *r;
    register struct uvif *v;

    v = &uvifs[vifi];
    if (v->uv_flags & VIFF_NOFLOOD)
	return;
    for (r = routing_table; r != NULL; r = r->rt_next) {
	if (r->rt_metric != UNREACHABLE && r->rt_parent != vifi) {
	    NBRM_SET(index, r->rt_subordinates);
	    update_table_entry(r, r->rt_gateway);
	}
    }
}


/*
 * A neighbor has failed or become unreachable.  If that neighbor was
 * considered a dominant or subordinate router in any route entries,
 * take appropriate action.  Expire all routes this neighbor advertised
 * to us.
 */
void
delete_neighbor_from_routes(addr, vifi, index)
    register u_int32 addr;
    register vifi_t vifi;
    int index;
{
    register struct rtentry *r;
    register struct uvif *v;

    v = &uvifs[vifi];
    for (r = routing_table; r != NULL; r = r->rt_next) {
	if (r->rt_metric != UNREACHABLE) {
	    if (r->rt_parent == vifi && r->rt_gateway == addr) {
		del_table_entry(r, 0, DEL_ALL_ROUTES);
		r->rt_timer    = ROUTE_EXPIRE_TIME;
		r->rt_metric   = UNREACHABLE;
		r->rt_flags   |= RTF_CHANGED;
		routes_changed = TRUE;
	    } else if (r->rt_dominants[vifi] == addr) {
		VIFM_SET(vifi, r->rt_children);
		r->rt_dominants[vifi] = 0;
		if (uvifs[vifi].uv_flags & VIFF_NOFLOOD)
		    NBRM_CLRMASK(r->rt_subordinates, uvifs[vifi].uv_nbrmap);
		else
		    NBRM_SETMASK(r->rt_subordinates, uvifs[vifi].uv_nbrmap);
		update_table_entry(r, r->rt_gateway);
	    } else if (NBRM_ISSET(index, r->rt_subordinates)) {
		NBRM_CLR(index, r->rt_subordinates);
		update_table_entry(r, r->rt_gateway);
	    }
	}
    }
}


/*
 * Prepare for a sequence of ordered route updates by initializing a pointer
 * to the start of the routing table.  The pointer is used to remember our
 * position in the routing table in order to avoid searching from the
 * beginning for each update; this relies on having the route reports in
 * a single message be in the same order as the route entries in the routing
 * table.
 */
void
start_route_updates()
{
    rtp = RT_ADDR;
}


/*
 * Starting at the route entry following the one to which 'rtp' points,
 * look for a route entry matching the specified origin and mask.  If a
 * match is found, return TRUE and leave 'rtp' pointing at the found entry.
 * If no match is found, return FALSE and leave 'rtp' pointing to the route
 * entry preceding the point at which the new origin should be inserted.
 * This code is optimized for the normal case in which the first entry to
 * be examined is the matching entry.
 */
static int
find_route(origin, mask)
    register u_int32 origin, mask;
{
    register struct rtentry *r;

    r = rtp->rt_next;
    while (r != NULL) {
	if (origin == r->rt_origin && mask == r->rt_originmask) {
	    rtp = r;
	    return (TRUE);
	}
	if (ntohl(mask) < ntohl(r->rt_originmask) ||
	    (mask == r->rt_originmask &&
	     ntohl(origin) < ntohl(r->rt_origin))) {
	    rtp = r;
	    r = r->rt_next;
	}
	else break;
    }
    return (FALSE);
}

/*
 * Create a new routing table entry for the specified origin and link it into
 * the routing table.  The shared variable 'rtp' is assumed to point to the
 * routing entry after which the new one should be inserted.  It is left
 * pointing to the new entry.
 *
 * Only the origin, originmask, originwidth and flags fields are initialized
 * in the new route entry; the caller is responsible for filling in the the
 * rest.
 */
static void
create_route(origin, mask)
    u_int32 origin, mask;
{
    register struct rtentry *r;

    if ((r = (struct rtentry *) malloc(sizeof(struct rtentry) +
				       (numvifs * sizeof(u_int32)) +
				       (numvifs * sizeof(u_int)))) == NULL) {
	log(LOG_ERR, 0, "ran out of memory");	/* fatal */
    }
    r->rt_origin     = origin;
    r->rt_originmask = mask;
    if      (((char *)&mask)[3] != 0) r->rt_originwidth = 4;
    else if (((char *)&mask)[2] != 0) r->rt_originwidth = 3;
    else if (((char *)&mask)[1] != 0) r->rt_originwidth = 2;
    else                              r->rt_originwidth = 1;
    r->rt_flags        = 0;
    r->rt_dominants    = (u_int32 *)(r + 1);
    r->rt_groups       = NULL;
    VIFM_CLRALL(r->rt_children);
    NBRM_CLRALL(r->rt_subordinates);
    NBRM_CLRALL(r->rt_subordadv);

    r->rt_next = rtp->rt_next;
    rtp->rt_next = r;
    r->rt_prev = rtp;
    if (r->rt_next != NULL)
      (r->rt_next)->rt_prev = r;
    else 
      rt_end = r;
    rtp = r;
    ++nroutes;
}


/*
 * Discard the routing table entry following the one to which 'prev_r' points.
 */
static void
discard_route(prev_r)
    register struct rtentry *prev_r;
{
    register struct rtentry *r;

    r = prev_r->rt_next;
    prev_r->rt_next = r->rt_next;
    if (prev_r->rt_next != NULL)
      (prev_r->rt_next)->rt_prev = prev_r;
    else
      rt_end = prev_r;
    free((char *)r);
    --nroutes;
}


/*
 * Process a route report for a single origin, creating or updating the
 * corresponding routing table entry if necessary.  'src' is either the
 * address of a neighboring router from which the report arrived, or zero
 * to indicate a change of status of one of our own interfaces.
 */
void
update_route(origin, mask, metric, src, vifi, n)
    u_int32 origin, mask;
    u_int metric;
    u_int32 src;
    vifi_t vifi;
    struct listaddr *n;
{
    register struct rtentry *r;
    u_int adj_metric;
    extern int did_final_init;		/* main.c */

    /*
     * Compute an adjusted metric, taking into account the cost of the
     * subnet or tunnel over which the report arrived, and normalizing
     * all unreachable/poisoned metrics into a single value.
     */
    if (src != 0 && (metric < 1 || metric >= 2*UNREACHABLE)) {
	log(LOG_WARNING, 0,
	    "%s reports out-of-range metric %u for origin %s",
	    inet_fmt(src, s1), metric, inet_fmts(origin, mask, s2));
	return;
    }
    adj_metric = metric + uvifs[vifi].uv_metric;
    if (adj_metric > UNREACHABLE) adj_metric = UNREACHABLE;

    /*
     * Look up the reported origin in the routing table.
     */
    if (!find_route(origin, mask)) {
	/*
	 * Not found.
	 * Don't create a new entry if the report says it's unreachable,
	 * or if the reported origin and mask are invalid.
	 */
	if (adj_metric == UNREACHABLE) {
	    return;
	}
	if (src != 0 && !inet_valid_subnet(origin, mask)) {
	    log(LOG_WARNING, 0,
		"%s reports an invalid origin (%s) and/or mask (%08x)",
		inet_fmt(src, s1), inet_fmt(origin, s2), ntohl(mask));
	    return;
	}

	IF_DEBUG(DEBUG_RTDETAIL)
	log(LOG_DEBUG, 0, "%s advertises new route %s",
		inet_fmt(src, s1), inet_fmts(origin, mask, s2));

	/*
	 * OK, create the new routing entry.  'rtp' will be left pointing
	 * to the new entry.
	 */
	create_route(origin, mask);

	rtp->rt_metric = UNREACHABLE;	/* temporary; updated below */
    }

    /*
     * We now have a routing entry for the reported origin.  Update it?
     */
    r = rtp;
    if (r->rt_metric == UNREACHABLE) {
	/*
	 * The routing entry is for a formerly-unreachable or new origin.
	 * If the report claims reachability, update the entry to use
	 * the reported route.
	 */
	if (adj_metric == UNREACHABLE)
	    return;

	IF_DEBUG(DEBUG_RTDETAIL)
	log(LOG_DEBUG, 0, "%s advertises %s with metric %d (ours was %d)",
		inet_fmt(src, s1), inet_fmts(origin, mask, s2),
		adj_metric, r->rt_metric);

	/*
	 * Now "steal away" any sources that belong under this route
	 * by deleting any cache entries they might have created
	 * and allowing the kernel to re-request them.
	 *
	 * If we haven't performed final initialization yet and are
	 * just collecting the routing table, we can't have any
	 * sources so we don't perform this step.
	 */
	if (did_final_init)
	    steal_sources(rtp);

	r->rt_parent   = vifi;
	init_children_and_leaves(r, vifi);

	r->rt_gateway  = src;
	r->rt_timer    = 0;
	r->rt_metric   = adj_metric;
	r->rt_flags   |= RTF_CHANGED;
	routes_changed = TRUE;
	update_table_entry(r, r->rt_gateway);
    }
    else if (src == r->rt_gateway) {
	/*
	 * The report has come either from the interface directly-connected
	 * to the origin subnet (src and r->rt_gateway both equal zero) or
	 * from the gateway we have chosen as the best first-hop gateway back
	 * towards the origin (src and r->rt_gateway not equal zero).  Reset
	 * the route timer and, if the reported metric has changed, update
	 * our entry accordingly.
	 */
	r->rt_timer = 0;

	IF_DEBUG(DEBUG_RTDETAIL)
	log(LOG_DEBUG, 0, "%s (current parent) advertises %s with metric %d (ours was %d)",
		inet_fmt(src, s1), inet_fmts(origin, mask, s2),
		adj_metric, r->rt_metric);

	if (adj_metric == r->rt_metric)
	    return;

	if (adj_metric == UNREACHABLE) {
	    del_table_entry(r, 0, DEL_ALL_ROUTES);
	    r->rt_timer = ROUTE_EXPIRE_TIME;
	}
	else if (adj_metric < r->rt_metric) {
	    /*XXX This update is changing nothing other than the metric
	     * of the route; why do we need to init_...? */
	    if (init_children_and_leaves(r, vifi)) {
		update_table_entry(r, r->rt_gateway);
	    }
	}
	r->rt_metric   = adj_metric;
	r->rt_flags   |= RTF_CHANGED;
	routes_changed = TRUE;
    }
    else if (src == 0 ||
	     (r->rt_gateway != 0 &&
	      (adj_metric < r->rt_metric ||
	       (adj_metric == r->rt_metric &&
		(ntohl(src) < ntohl(r->rt_gateway) ||
		 r->rt_timer >= ROUTE_SWITCH_TIME))))) {
	/*
	 * The report is for an origin we consider reachable; the report
	 * comes either from one of our own interfaces or from a gateway
	 * other than the one we have chosen as the best first-hop gateway
	 * back towards the origin.  If the source of the update is one of
	 * our own interfaces, or if the origin is not a directly-connected
	 * subnet and the reported metric for that origin is better than
	 * what our routing entry says, update the entry to use the new
	 * gateway and metric.  We also switch gateways if the reported
	 * metric is the same as the one in the route entry and the gateway
	 * associated with the route entry has not been heard from recently,
	 * or if the metric is the same but the reporting gateway has a lower
	 * IP address than the gateway associated with the route entry.
	 * Did you get all that?
	 */
	u_int32 old_gateway;
	old_gateway = r->rt_gateway;
	r->rt_gateway  = src;

	IF_DEBUG(DEBUG_RTDETAIL)
	log(LOG_DEBUG, 0, "%s advertises %s with metric %d (old parent was %s, metric %d)",
		inet_fmt(src, s1), inet_fmts(origin, mask, s2),
		adj_metric, inet_fmt(old_gateway, s3), r->rt_metric);

	if (old_gateway != src || adj_metric < r->rt_metric) {
	    /*
	     * XXX Why do we do this if we are just changing the metric?
	     */
	    r->rt_parent = vifi;
	    if (init_children_and_leaves(r, vifi)) {
		update_table_entry(r, old_gateway);
	    }
	}
	r->rt_timer    = 0;
	r->rt_metric   = adj_metric;
	r->rt_flags   |= RTF_CHANGED;
	routes_changed = TRUE;
    }
    else if (vifi != r->rt_parent) {
	/*
	 * The report came from a vif other than the route's parent vif.
	 * Update the children info, if necessary.
	 */
	if (VIFM_ISSET(vifi, r->rt_children)) {
	    /*
	     * Vif is a child vif for this route.
	     */
	    if (metric  < r->rt_metric ||
		(metric == r->rt_metric &&
		 ntohl(src) < ntohl(uvifs[vifi].uv_lcl_addr))) {
		/*
		 * Neighbor has lower metric to origin (or has same metric
		 * and lower IP address) -- it becomes the dominant router,
		 * and vif is no longer a child for me.
		 */
		VIFM_CLR(vifi, r->rt_children);
		r->rt_dominants   [vifi] = src;
		NBRM_CLRMASK(r->rt_subordinates, uvifs[vifi].uv_nbrmap);/*XXX*/
		update_table_entry(r, r->rt_gateway);
	    }
	    else if (metric > UNREACHABLE) {	/* "poisoned reverse" */
		/*
		 * Neighbor considers this vif to be on path to route's
		 * origin; record this neighbor as subordinate
		 */
		if (!NBRM_ISSET(n->al_index, r->rt_subordinates)) {
		    NBRM_SET(n->al_index, r->rt_subordinates);
		    update_table_entry(r, r->rt_gateway);
		}
		NBRM_SET(n->al_index, r->rt_subordadv);
	    }
	    else if (NBRM_ISSET(n->al_index, r->rt_subordinates)) {
		/*
		 * Current subordinate no longer considers this vif to be on
		 * path to route's origin; it is no longer a subordinate
		 * router.
		 */
		NBRM_CLR(n->al_index, r->rt_subordinates);
		update_table_entry(r, r->rt_gateway);
	    }

	}
	else if (src == r->rt_dominants[vifi] &&
		 (metric  > r->rt_metric ||
		  (metric == r->rt_metric &&
		   ntohl(src) > ntohl(uvifs[vifi].uv_lcl_addr)))) {
	    /*
	     * Current dominant no longer has a lower metric to origin
	     * (or same metric and lower IP address); we adopt the vif
	     * as our own child.
	     */
	    VIFM_SET(vifi, r->rt_children);
	    r->rt_dominants[vifi] = 0;
	    if (uvifs[vifi].uv_flags & VIFF_NOFLOOD)
		NBRM_CLRMASK(r->rt_subordinates, uvifs[vifi].uv_nbrmap);
	    else
		NBRM_SETMASK(r->rt_subordinates, uvifs[vifi].uv_nbrmap);
	    if (metric > UNREACHABLE) {
		NBRM_SET(n->al_index, r->rt_subordinates);
		NBRM_SET(n->al_index, r->rt_subordadv);
	    }
	    update_table_entry(r, r->rt_gateway);
	}
    }
}


/*
 * On every timer interrupt, advance the timer in each routing entry.
 */
void
age_routes()
{
    register struct rtentry *r;
    register struct rtentry *prev_r;
    extern u_long virtual_time;		/* from main.c */

    for (prev_r = RT_ADDR, r = routing_table;
	 r != NULL;
	 prev_r = r, r = r->rt_next) {

	if ((r->rt_timer += TIMER_INTERVAL) >= ROUTE_DISCARD_TIME) {
	    /*
	     * Time to garbage-collect the route entry.
	     */
	    del_table_entry(r, 0, DEL_ALL_ROUTES);
	    discard_route(prev_r);
	    r = prev_r;
	}
	else if (r->rt_timer >= ROUTE_EXPIRE_TIME &&
		 r->rt_metric != UNREACHABLE) {
	    /*
	     * Time to expire the route entry.  If the gateway is zero,
	     * i.e., it is a route to a directly-connected subnet, just
	     * set the timer back to zero; such routes expire only when
	     * the interface to the subnet goes down.
	     */
	    if (r->rt_gateway == 0) {
		r->rt_timer = 0;
	    }
	    else {
		del_table_entry(r, 0, DEL_ALL_ROUTES);
		r->rt_metric   = UNREACHABLE;
		r->rt_flags   |= RTF_CHANGED;
		routes_changed = TRUE;
	    }
	}
	else if (virtual_time % (ROUTE_REPORT_INTERVAL * 2) == 0) {
	    /*
	     * Time out subordinateness that hasn't been reported in
	     * the last 2 intervals.
	     */
	    if (!NBRM_SAME(r->rt_subordinates, r->rt_subordadv)) {
		IF_DEBUG(DEBUG_ROUTE)
		log(LOG_DEBUG, 0, "rt %s sub 0x%08x%08x subadv 0x%08x%08x metric %d",
			RT_FMT(r, s1),
			r->rt_subordinates.hi, r->rt_subordinates.lo,
			r->rt_subordadv.hi, r->rt_subordadv.lo, r->rt_metric);
		NBRM_MASK(r->rt_subordinates, r->rt_subordadv);
#if 0
/*
 * This happens when non-timeout happens, e.g. nbr1 says
 * I'm subordinate but then explicitly says I'm not subordinate;
 * subordadv doesn't get cleared.
 */
		if (!NBRM_SAME(r->rt_subordinates, r->rt_subordadv)) {
		    log(LOG_WARNING, 0,
			"!!!rt %s sub 0x%08x%08x subadv 0x%08x%08x",
			RT_FMT(r, s1),
			r->rt_subordinates.hi, r->rt_subordinates.lo,
			r->rt_subordadv.hi, r->rt_subordadv.lo);
		}
#endif
	    }
	    NBRM_CLRALL(r->rt_subordadv);
	}
    }
}


/*
 * Mark all routes as unreachable.  This function is called only from
 * hup() in preparation for informing all neighbors that we are going
 * off the air.  For consistency, we ought also to delete all reachable
 * route entries from the kernel, but since we are about to exit we rely
 * on the kernel to do its own cleanup -- no point in making all those
 * expensive kernel calls now.
 */
void
expire_all_routes()
{
    register struct rtentry *r;

    for (r = routing_table; r != NULL; r = r->rt_next) {
	r->rt_metric   = UNREACHABLE;
	r->rt_flags   |= RTF_CHANGED;
	routes_changed = TRUE;
    }
}


/*
 * Delete all the routes in the routing table.
 */
void
free_all_routes()
{
    register struct rtentry *r;

    r = RT_ADDR;

    while (r->rt_next)
	discard_route(r);
}


/*
 * Process an incoming neighbor probe message.
 */
void
accept_probe(src, dst, p, datalen, level)
    u_int32 src;
    u_int32 dst;
    char *p;
    int datalen;
    u_int32 level;
{
    vifi_t vifi;
    static struct listaddr *unknowns = NULL;

    if ((vifi = find_vif(src, dst)) == NO_VIF) {
	struct listaddr *p, **prev;
	struct listaddr *match = NULL;
	time_t now = time(0);

	for (prev = &unknowns, p = *prev; p; p = *prev) {
	    if (p->al_addr == src)
		match = p;
	    if (p->al_ctime + 2 * p->al_timer < now) {
		/* We haven't heard from it in a long time */
		*prev = p->al_next;
		free(p);
	    } else {
		prev = &p->al_next;
	    }
	}
	if (match == NULL) {
	    match = *prev = (struct listaddr *)malloc(sizeof(struct listaddr));
	    match->al_next = NULL;
	    match->al_addr = src;
	    match->al_timer = OLD_NEIGHBOR_EXPIRE_TIME;
	    match->al_ctime = now - match->al_timer;
	}

	if (match->al_ctime + match->al_timer <= now) {
	    log(LOG_WARNING, 0,
		"ignoring probe from non-neighbor %s, check for misconfigured tunnel or routing on %s",
		inet_fmt(src, s1), s1);
	    match->al_timer *= 2;
	} else
	    IF_DEBUG(DEBUG_PEER)
	    log(LOG_DEBUG, 0,
		"ignoring probe from non-neighbor %s (%d seconds until next warning)", inet_fmt(src, s1), match->al_ctime + match->al_timer - now);
	return;
    }

    update_neighbor(vifi, src, DVMRP_PROBE, p, datalen, level);
}

struct newrt {
	u_int32 mask;
	u_int32 origin;
	int metric;
	int pad;
}; 

static int
compare_rts(rt1, rt2)
    const void *rt1;
    const void *rt2;
{
    register struct newrt *r1 = (struct newrt *)rt1;
    register struct newrt *r2 = (struct newrt *)rt2;
    register u_int32 m1 = ntohl(r1->mask);
    register u_int32 m2 = ntohl(r2->mask);
    register u_int32 o1, o2;

    if (m1 > m2)
	return (-1);
    if (m1 < m2)
	return (1);

    /* masks are equal */
    o1 = ntohl(r1->origin);
    o2 = ntohl(r2->origin);
    if (o1 > o2)
	return (-1);
    if (o1 < o2)
	return (1);
    return (0);
}

/*
 * Process an incoming route report message.
 */
void
accept_report(src, dst, p, datalen, level)
    u_int32 src, dst, level;
    register char *p;
    register int datalen;
{
    vifi_t vifi;
    register int width, i, nrt = 0;
    int metric;
    u_int32 mask;
    u_int32 origin;
    struct newrt rt[4096];
    struct listaddr *nbr;

    if ((vifi = find_vif(src, dst)) == NO_VIF) {
	log(LOG_INFO, 0,
    	    "ignoring route report from non-neighbor %s", inet_fmt(src, s1));
	return;
    }

    if (!(nbr = update_neighbor(vifi, src, DVMRP_REPORT, NULL, 0, level)))
	return;

    if (datalen > 2*4096) {
	log(LOG_INFO, 0,
    	    "ignoring oversize (%d bytes) route report from %s",
	    datalen, inet_fmt(src, s1));
	return;
    }

    while (datalen > 0) {	/* Loop through per-mask lists. */

	if (datalen < 3) {
	    log(LOG_WARNING, 0,
		"received truncated route report from %s", 
		inet_fmt(src, s1));
	    return;
	}
	((u_char *)&mask)[0] = 0xff;            width = 1;
	if ((((u_char *)&mask)[1] = *p++) != 0) width = 2;
	if ((((u_char *)&mask)[2] = *p++) != 0) width = 3;
	if ((((u_char *)&mask)[3] = *p++) != 0) width = 4;
	if (!inet_valid_mask(ntohl(mask))) {
	    log(LOG_WARNING, 0,
		"%s reports bogus netmask 0x%08x (%s)",
		inet_fmt(src, s1), ntohl(mask), inet_fmt(mask, s2));
	    return;
	}
	datalen -= 3;

	do {			/* Loop through (origin, metric) pairs */
	    if (datalen < width + 1) {
		log(LOG_WARNING, 0,
		    "received truncated route report from %s", 
		    inet_fmt(src, s1));
		return;
	    }
	    origin = 0;
	    for (i = 0; i < width; ++i)
		((char *)&origin)[i] = *p++;
	    metric = *p++;
	    datalen -= width + 1;
	    rt[nrt].mask   = mask;
	    rt[nrt].origin = origin;
	    rt[nrt].metric = (metric & 0x7f);
	    ++nrt;
	} while (!(metric & 0x80));
    }

    qsort((char*)rt, nrt, sizeof(rt[0]), compare_rts);
    start_route_updates();
    /*
     * If the last entry is default, change mask from 0xff000000 to 0
     */
    if (rt[nrt-1].origin == 0)
	rt[nrt-1].mask = 0;

    IF_DEBUG(DEBUG_ROUTE)
    log(LOG_DEBUG, 0, "Updating %d routes from %s to %s", nrt,
		inet_fmt(src, s1), inet_fmt(dst, s2));
    for (i = 0; i < nrt; ++i) {
	if (i != 0 && rt[i].origin == rt[i-1].origin &&
		      rt[i].mask == rt[i-1].mask) {
	    log(LOG_WARNING, 0, "%s reports duplicate route for %s",
		inet_fmt(src, s1), inet_fmts(rt[i].origin, rt[i].mask, s2));
	    continue;
	}
	/* Only filter non-poisoned updates. */
	if (uvifs[vifi].uv_filter && rt[i].metric < UNREACHABLE) {
	    struct vf_element *p;
	    int match = 0;

	    for (p = uvifs[vifi].uv_filter->vf_filter; p; p = p->vfe_next) {
		if (p->vfe_flags & VFEF_EXACT) {
		    if ((p->vfe_addr == rt[i].origin) &&
			(p->vfe_mask == rt[i].mask)) {
			    match = 1;
			    break;
		    }
		} else {
		    if ((rt[i].origin & p->vfe_mask) == p->vfe_addr) {
			    match = 1;
			    break;
		    }
		}
	    }
	    if ((uvifs[vifi].uv_filter->vf_type == VFT_ACCEPT && match == 0) ||
		(uvifs[vifi].uv_filter->vf_type == VFT_DENY && match == 1)) {
		    IF_DEBUG(DEBUG_ROUTE)
		    log(LOG_DEBUG, 0, "%s skipped on vif %d because it %s %s",
			inet_fmts(rt[i].origin, rt[i].mask, s1),
			vifi,
			match ? "matches" : "doesn't match",
			match ? inet_fmts(p->vfe_addr, p->vfe_mask, s2) :
				"the filter");
#if 0
		    rt[i].metric += p->vfe_addmetric;
		    if (rt[i].metric > UNREACHABLE)
#endif
			rt[i].metric = UNREACHABLE;
	    }
	}
	update_route(rt[i].origin, rt[i].mask, rt[i].metric, 
		     src, vifi, nbr);
    }

    if (routes_changed && !delay_change_reports)
	report_to_all_neighbors(CHANGED_ROUTES);
}


/*
 * Send a route report message to destination 'dst', via virtual interface
 * 'vifi'.  'which_routes' specifies ALL_ROUTES or CHANGED_ROUTES.
 */
void
report(which_routes, vifi, dst)
    int which_routes;
    vifi_t vifi;
    u_int32 dst;
{
    register struct rtentry *r;
    register int i;

    r = rt_end;
    while (r != RT_ADDR) {
	i = report_chunk(which_routes, r, vifi, dst);
	while (i-- > 0)
	    r = r->rt_prev;
    }
}


/*
 * Send a route report message to all neighboring routers.
 * 'which_routes' specifies ALL_ROUTES or CHANGED_ROUTES.
 */
void
report_to_all_neighbors(which_routes)
    int which_routes;
{
    register vifi_t vifi;
    register struct uvif *v;
    register struct rtentry *r;
    int routes_changed_before;

    /*
     * Remember the state of the global routes_changed flag before
     * generating the reports, and clear the flag.
     */
    routes_changed_before = routes_changed;
    routes_changed = FALSE;


    for (vifi = 0, v = uvifs; vifi < numvifs; ++vifi, ++v) {
	if (!NBRM_ISEMPTY(v->uv_nbrmap)) {
	    report(which_routes, vifi, v->uv_dst_addr);
	}
    }

    /*
     * If there were changed routes before we sent the reports AND
     * if no new changes occurred while sending the reports, clear
     * the change flags in the individual route entries.  If changes
     * did occur while sending the reports, new reports will be
     * generated at the next timer interrupt.
     */
    if (routes_changed_before && !routes_changed) {
	for (r = routing_table; r != NULL; r = r->rt_next) {
	    r->rt_flags &= ~RTF_CHANGED;
	}
    }

    /*
     * Set a flag to inhibit further reports of changed routes until the
     * next timer interrupt.  This is to alleviate update storms.
     */
    delay_change_reports = TRUE;
}

/*
 * Send a route report message to destination 'dst', via virtual interface
 * 'vifi'.  'which_routes' specifies ALL_ROUTES or CHANGED_ROUTES.
 */
static int
report_chunk(which_routes, start_rt, vifi, dst)
    int which_routes;
    register struct rtentry *start_rt;
    vifi_t vifi;
    u_int32 dst;
{
    register struct rtentry *r;
    register char *p;
    register int i;
    register int nrt = 0;
    int datalen = 0;
    int width = 0;
    u_int32 mask = 0;
    u_int32 src;
    u_int32 nflags;
    int admetric = uvifs[vifi].uv_admetric;
    int metric;

    src = uvifs[vifi].uv_lcl_addr;
    p = send_buf + MIN_IP_HEADER_LEN + IGMP_MINLEN;

    nflags = (uvifs[vifi].uv_flags & VIFF_LEAF) ? 0 : LEAF_FLAGS;

    for (r = start_rt; r != RT_ADDR; r = r->rt_prev) {
	if (which_routes == CHANGED_ROUTES && !(r->rt_flags & RTF_CHANGED)) {
	    nrt++;
	    continue;
	}

	/*
	 * Do not poison-reverse a route for a directly-connected
	 * subnetwork on that subnetwork.  This can cause loops when
	 * some router on the subnetwork is misconfigured.
	 */
	if (r->rt_gateway == 0 && r->rt_parent == vifi) {
	    nrt++;
	    continue;
	}

	if (uvifs[vifi].uv_filter && uvifs[vifi].uv_filter->vf_flags & VFF_BIDIR) {
	    struct vf_element *p;
	    int match = 0;

	    for (p = uvifs[vifi].uv_filter->vf_filter; p; p = p->vfe_next) {
		if (p->vfe_flags & VFEF_EXACT) {
		    if ((p->vfe_addr == r->rt_origin) &&
			(p->vfe_mask == r->rt_originmask)) {
			    match = 1;
			    break;
		    }
		} else {
		    if ((r->rt_origin & p->vfe_mask) == p->vfe_addr) {
			    match = 1;
			    break;
		    }
		}
	    }
	    if ((uvifs[vifi].uv_filter->vf_type == VFT_ACCEPT && match == 0) ||
		(uvifs[vifi].uv_filter->vf_type == VFT_DENY && match == 1)) {
		    IF_DEBUG(DEBUG_ROUTE)
		    log(LOG_DEBUG, 0, "%s not reported on vif %d because it %s %s",
			RT_FMT(r, s1), vifi,
			match ? "matches" : "doesn't match",
			match ? inet_fmts(p->vfe_addr, p->vfe_mask, s2) :
				"the filter");
		    nrt++;
		    continue;
	    }
	}

	/*
	 * If there is no room for this route in the current message,
	 * send it & return how many routes we sent.
	 */
	if (datalen + ((r->rt_originmask == mask) ?
		       (width + 1) :
		       (r->rt_originwidth + 4)) > MAX_DVMRP_DATA_LEN) {
	    *(p-1) |= 0x80;
	    send_igmp(src, dst, IGMP_DVMRP, DVMRP_REPORT,
		      htonl(MROUTED_LEVEL | nflags), datalen);
	    return (nrt);
	}

	if (r->rt_originmask != mask || datalen == 0) {
	    mask  = r->rt_originmask;
	    width = r->rt_originwidth;
	    if (datalen != 0) *(p-1) |= 0x80;
	    *p++ = ((char *)&mask)[1];
	    *p++ = ((char *)&mask)[2];
	    *p++ = ((char *)&mask)[3];
	    datalen += 3;
	}
	for (i = 0; i < width; ++i)
	    *p++ = ((char *)&(r->rt_origin))[i];

	metric = r->rt_metric + admetric;
	if (metric > UNREACHABLE)
	    metric = UNREACHABLE;
	*p++ = (r->rt_parent == vifi && metric != UNREACHABLE) ?
	    (char)(metric + UNREACHABLE) :  /* "poisoned reverse" */
		(char)(metric);
	++nrt;
	datalen += width + 1;
    }
    if (datalen != 0) {
	*(p-1) |= 0x80;
	send_igmp(src, dst, IGMP_DVMRP, DVMRP_REPORT,
		  htonl(MROUTED_LEVEL | nflags), datalen);
    }
    return (nrt);
}

/*
 * send the next chunk of our routing table to all neighbors.
 * return the length of the smallest chunk we sent out.
 */
int
report_next_chunk()
{
    register vifi_t vifi;
    register struct uvif *v;
    register struct rtentry *sr;
    register int i, n = 0, min = 20000;
    static int start_rt;

    if (nroutes <= 0)
	return (0);

    /*
     * find this round's starting route.
     */
    for (sr = rt_end, i = start_rt; --i >= 0; ) {
	sr = sr->rt_prev;
	if (sr == RT_ADDR)
	    sr = rt_end;
    }

    /*
     * send one chunk of routes starting at this round's start to
     * all our neighbors.
     */
    for (vifi = 0, v = uvifs; vifi < numvifs; ++vifi, ++v) {
	if (!NBRM_ISEMPTY(v->uv_nbrmap)) {
	    n = report_chunk(ALL_ROUTES, sr, vifi, v->uv_dst_addr);
	    if (n < min)
		min = n;
	}
    }
    if (min == 20000)
	min = 0;	/* Neighborless router didn't send any routes */

    n = min;
    IF_DEBUG(DEBUG_ROUTE)
    log(LOG_INFO, 0, "update %d starting at %d of %d",
	n, (nroutes - start_rt), nroutes);

    start_rt = (start_rt + n) % nroutes;
    return (n);
}


/*
 * Print the contents of the routing table on file 'fp'.
 */
void
dump_routes(fp)
    FILE *fp;
{
    register struct rtentry *r;
    register vifi_t i;


    fprintf(fp,
	    "Multicast Routing Table (%u %s)\n%s\n",
	    nroutes, (nroutes == 1) ? "entry" : "entries",
	    " Origin-Subnet      From-Gateway    Metric Tmr In-Vif  Out-Vifs");

    for (r = routing_table; r != NULL; r = r->rt_next) {

	fprintf(fp, " %-18s %-15s ",
		inet_fmts(r->rt_origin, r->rt_originmask, s1),
		(r->rt_gateway == 0) ? "" : inet_fmt(r->rt_gateway, s2));

	fprintf(fp, (r->rt_metric == UNREACHABLE) ? "  NR " : "%4u ",
		r->rt_metric);

	fprintf(fp, "  %3u %3u   ", r->rt_timer, r->rt_parent);

	for (i = 0; i < numvifs; ++i) {
	    struct listaddr *n;
	    char l = '[';

	    if (VIFM_ISSET(i, r->rt_children)) {
		if ((uvifs[i].uv_flags & VIFF_TUNNEL) &&
		    !NBRM_ISSETMASK(uvifs[i].uv_nbrmap, r->rt_subordinates))
			/* Don't print out parenthood of a leaf tunnel. */
			continue;
		fprintf(fp, " %u", i);
		if (!NBRM_ISSETMASK(uvifs[i].uv_nbrmap, r->rt_subordinates))
		    fprintf(fp, "*");
		for (n = uvifs[i].uv_neighbors; n; n = n->al_next) {
		    if (NBRM_ISSET(n->al_index, r->rt_subordinates)) {
			fprintf(fp, "%c%d", l, n->al_index);
			l = ',';
		    }
		}
		if (l == ',')
		    fprintf(fp, "]");
	    }
	}
	fprintf(fp, "\n");
    }
    fprintf(fp, "\n");
}

struct rtentry *
determine_route(src)
    u_int32 src;
{
    struct rtentry *rt;

    for (rt = routing_table; rt != NULL; rt = rt->rt_next) {
	if (rt->rt_origin == (src & rt->rt_originmask) &&
	    rt->rt_metric != UNREACHABLE) 
	    break;
    }
    return rt;
}
