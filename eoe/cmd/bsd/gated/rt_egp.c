/*
 *   CENTER FOR THEORY AND SIMULATION IN SCIENCE AND ENGINEERING
 *			CORNELL UNIVERSITY
 *
 *      Portions of this software may fall under the following
 *      copyrights: 
 *
 *	Copyright (c) 1983 Regents of the University of California.
 *	All rights reserved.  The Berkeley software License Agreement
 *	specifies the terms and conditions for redistribution.
 *
 *  GATED - based on Kirton's EGP, UC Berkeley's routing daemon (routed),
 *	    and DCN's HELLO routing Protocol.
 */

#ifndef	lint
static char *rcsid = "$Header: /proj/irix6.5.7m/isms/eoe/cmd/bsd/gated/RCS/rt_egp.c,v 1.1 1989/09/18 19:02:15 jleong Exp $";
#endif	not lint

/* 
 * rt_egp.c
 *
 * EGP route update and processing and preparation functions.
 *
 * Functions: rt_NRnets, rt_NRupdate
 */

#include "include.h"

/*
 * rt_NRnets() prepares the network part of the EGP Network Reachability 
 * update message with respect to the shared network of the EGP peer.
 * This only includes the networks in the interior routing table (direct 
 * networks, and remote networks of non-routing gateways of this autonomous
 * system) other than the net shared by the EGP peer. If the user has
 * specified that only certain networks are allowed to be advised all others 
 * are excluded from outgoing NR update messages.
 * If the interior routing table includes other interior gateways on the
 * network shared with the EGP peer (i.e. indirect neighbors), they are
 * included in updates as the appropriate first hop to their attached
 * networks.
 * This function checks the status of routes and if down sets the
 * distance as unreachable.
 *
 * Returns the length of the EGP NR packet in octets or ERROR if an error
 * has occurred.
 */

rt_NRnets(nrpkt, ngp)
        struct  egpnr  *nrpkt;          /* start of NR message */
	struct  egpngh *ngp;		/* Pointer to entry in neighbor table */
{
  struct  rt_entry  *rt;
  struct  rthash *rh;
  struct  net_order {       /* temporary linked list for ordering nets */
         struct net_order *next;
         struct in_addr  net;            /* net # */
         struct in_addr  gateway;
         int     distance;
  } *start_net, *free_net;

  register  struct  net_order *net_pt, *this_net;  /* current search point */
  int       n_bytes;
  struct in_addr current_gw;
  register  u_char  *nrp;                   /* next octet of NR message */
  u_char    *n_distance, *distance, *n_nets;
  int       this_metric;
  u_long current_net;
#ifndef	NSS
  struct interface *tifp;
  struct sockaddr_in tsock;
  
  bzero((char *)&tsock, sizeof(tsock));
  tsock.sin_family = AF_INET;
  tsock.sin_addr.s_addr = ngp->ng_myaddr.s_addr;
  if ((tifp = if_ifwithaddr((struct sockaddr *)&tsock)) <= (struct interface *)0) {
    TRACE_INT("No interface for egp, %s wanted\n", inet_ntoa(ngp->ng_myaddr));
    syslog(LOG_ERR, "No interface for egp, %s wanted", inet_ntoa(ngp->ng_myaddr));
    return(ERROR);
  }
#else	NSS
  struct rt_entry *rt_igp;
  struct rt_entry *rt_egp;
#endif	NSS

  /*
   * Reorder the interior routes as required for the NR message with respect to
   * the given shared net. Uses a temporary linked list terminated by NULL
   * pointer. The first element of the list is a dummy so insertions can be done
   * before the first true entry. The route status is checked and if down the
   * distance is set as unreachable.  The required order groups nets by gateway
   * and in order of increasing metric. This gateway is listed first (with all
   * nets not reached by gateways on the shared net) and then neighbor gateways
   * on the shared net, in any order. As there are few nets to be reported by a
   * stub gateway, each route is copied from the interior routing table and
   * inserted in the temporary reordered list using a linear search.
   *
   * Use the total number of networks to be sure we allocate a large enough
   * buffer.
   */
  start_net = (struct net_order *)malloc((unsigned)((n_routes +
                                    n_interfaces) * sizeof(struct net_order)));
  if (start_net == NULL) {
    syslog(LOG_ERR, "rt_NRnets: malloc: out of memory");
    TRACE_TRC("rt_NRnets: malloc: out of memory\n");
    return(ERROR);
  }
  start_net->next = NULL;
  /*
   * ensures first gateway listed is self
   */
  start_net->gateway = ngp->ng_myaddr;
  free_net = start_net + 1;   /* first element dummy to ease insertion code */

  /*
   * check all interior routes of route table
   */
  for (rh = nethash; rh < &nethash[ROUTEHASHSIZ]; rh++)
    for (rt = rh->rt_forw; rt != (struct rt_entry *)rh; rt = rt->rt_forw) {
      /*
       *  extra sanity check cause we don't want to EGP out
       *  subnets.
       */
      if (rt->rt_state & RTS_SUBNET) {
      	continue;
      }
      TRACE_EGPUPD("EGP: net %-15s AS %5d - ",inet_ntoa(sock_inaddr(&rt->rt_dst)), rt->rt_as);
      /*
       *  Don't allow the DEFAULT net through, unless we are allowed to
       *  send DEFAULT and this is the internally generated default.
       */
      if ((sock_inaddr(&rt->rt_dst).s_addr == DEFAULTNET) && (rt->rt_proto != RTPROTO_DEFAULT) ) {
        TRACE_EGPUPD("not propogating default.\n");
        continue;
      }
      /*
       * ignore nets that are not Class A, B or C
       */
      current_net = sock_inaddr(&rt->rt_dst).s_addr;
      if ( gd_inet_class((u_char *)&current_net) == 0 ) {
        TRACE_EGPUPD("not Class A, B or C\n");
        syslog(LOG_ERR, "rt_NRnets: net not class A, B or C: %s",
          inet_ntoa(sock_inaddr(&rt->rt_dst)));
        continue;
      }
      /*
       * ignore nets not to be advised
       */
      if (adlist && !(rt->rt_proto & RTPROTO_DEFAULT) && !(rt->rt_proto & RTPROTO_EGP)) {
        register struct advlist *ad;
        register int OK = 0;

        for (ad = adlist; ad; ad = ad->next) {
          if (sock_inaddr(&rt->rt_dst).s_addr == ad->destnet.s_addr) {
            OK++;
          }
        }
        if (!OK) {
          TRACE_EGPUPD("not on egpnetsreachable list\n");
          continue;
        }
      }
      switch (rt->rt_proto) {
        case RTPROTO_DIRECT:
          /*
           *	Direct routes use the lowest metric of interfaces to this net
           */
#ifdef	EGP_SKIP_SHARED
          /*
           * ignore shared net
           */
          if (gd_inet_wholenetof(sock_inaddr(&rt->rt_dst)) == gd_inet_wholenetof(ngp->ng_myaddr)) {
            TRACE_EGPUPD("shared net.\n");
            continue;
          }
#endif	EGP_SKIP_SHARED
          if (sendAS(ngp->ng_aslist, mysystem) == 0) {
            TRACE_EGPUPD("direct routes not valid for this AS.\n");
            goto Continue;
          }
	  if (adlist || is_valid(rt, RTPROTO_EGP, tifp)) {
	    if (rt->rt_flags & RTF_UP) {
	      if (rt->rt_announcelist) {
		this_metric = rt->rt_announcelist->regpmetric;
	      } else {
#ifdef	notdef
                this_metric = mapmetric(HELLO_TO_EGP, (u_short)rt->rt_metric);
#else	notdef
		this_metric = conv_factor;	/* Default metric specified in 'defaultegpmetric' clauase */
#endif	notdef
	      }
            } else {
              this_metric = HOPCNT_INFINITY;
	    }
	  } else {
	    TRACE_EGPUPD("restrictions prohibit announcement\n");
	    goto Continue;
	  }
          break;
#ifndef	NSS
        case RTPROTO_RIP:
        case RTPROTO_HELLO:
          if ((rt->rt_state & RTS_INTERIOR) == 0) {
            TRACE_EGPUPD("exterior routes not announced\n");
            continue;
          }
          if (sendAS(ngp->ng_aslist, mysystem) == 0) {
            TRACE_EGPUPD("interior routes not valid for this AS.\n");
            goto Continue;
          }
	  if (adlist || is_valid(rt, RTPROTO_EGP, tifp)) {
	    if (rt->rt_metric < DELAY_INFINITY) {
	      if (rt->rt_announcelist) {
		this_metric = rt->rt_announcelist->regpmetric;
	      } else {
		this_metric = conv_factor;
	      }
	    } else {
	      this_metric = HOPCNT_INFINITY;
	    }
	  } else {
	    TRACE_EGPUPD("restrictions prohibit announcement\n");
	    goto Continue;
	  }
          break;
#endif	NSS
        case RTPROTO_DEFAULT:
          if ( !(ngp->ng_flags & NG_DEFAULTOUT) ) {
            TRACE_EGPUPD("not originating default\n");
            goto Continue;
          }
          break;
#ifdef	NSS
        case RTPROTO_IGP:
          if (as_reach(rt->rt_as) != TRUE) {
            TRACE_EGPUPD("Autonomous System %d is unreachable\n", rt->rt_as);
            goto Continue;
          }
          if (rt->rt_as == ngp->ng_asin) {
            TRACE_EGPUPD("split horizon\n");
            goto Continue;
          }
          switch (sendAS(ngp->ng_aslist, rt->rt_as)) {
            case 0:	/* not valid for this AS */
              TRACE_EGPUPD("not valid for this AS\n");
              goto Continue;
            case 2:	/* valid to send - announce clauses apply */
	      if (is_valid(rt, RTPROTO_EGP, tifp)) {
		if (rt->rt_metric < DELAY_INFINITY) {
		  if (rt->rt_announcelist) {
		    this_metric = rt->rt_announcelist->regpmetric;
		  } else {
#ifdef	notdef
		    this_metric = mapmetric(HELLO_TO_EGP, (u_short)rt->rt_metric);  /* What metric should I use? */
#else	notdef
                    this_metric = conv_factor;	/* No metric specified, use 'defaultegpmetric' */
#endif	notdef
		  }
		} else {
		  this_metric = HOPCNT_INFINITY;
		}
	      } else {
		TRACE_EGPUPD("restrictions prohibit announcement\n");
		goto Continue;
	      }
              break;
            case 1:	/* valid to send - announce clauses do not apply */
            case -1:	/* no AS restrictions */
#ifdef	notdef
              this_metric = mapmetric(HELLO_TO_EGP, (u_short)rt->rt_metric);	/* What metric should I use? */
#else	notdef
              this_metric = conv_factor;	/* No metric specified, use 'defaultegpmetric' */
#endif	notdef
              break;
          }
	  /*
	   * If we have both IGP and EGP learned route, do
	   * not bother to advertize it twice.
	   */
	  if ((rt_egp = rt_locate((int) EXTERIOR, (struct sockaddr_in *) &rt->rt_dst,RTPROTO_EGP)) != NULL) {
	    if ((rt_egp->rt_metric <= rt->rt_metric) && (rt_egp->rt_as != ngp->ng_asin) &&
                  (as_reach(rt_egp->rt_as) == TRUE) && (sendAS(ngp->ng_aslist, rt_egp->rt_as) != 0)) {
              TRACE_EGPUPD("Have both IGP and EGP learned route... ignoring IGP\n");
	      goto Continue;
            }
	  }
          break;
#endif	NSS
        case RTPROTO_EGP:
          if (rt->rt_as == ngp->ng_asin) {
            TRACE_EGPUPD("split horizon\n");
            goto Continue;
          }
          switch (sendAS(ngp->ng_aslist, rt->rt_as)) {
            case 0:	/* not valid to send */
              TRACE_EGPUPD("not valid for this AS\n");
              goto Continue;
            case 2:	/* valid to send - announce clauses apply */
	      if (is_valid(rt, RTPROTO_EGP, tifp)) {
		if (rt->rt_metric < DELAY_INFINITY) {
		  if (rt->rt_announcelist) {
		    this_metric = rt->rt_announcelist->regpmetric;
		  } else {
#ifdef	notdef
		    this_metric = mapmetric(HELLO_TO_EGP, (u_short)rt->rt_metric);
#else	notdef
                    this_metric = rt->rt_metric_exterior;	/* Propogate EGP metric? */
#endif	notdef
		  }
		} else {
		  this_metric = HOPCNT_INFINITY;
		}
	      } else {
		TRACE_EGPUPD("restrictions prohibit announcement\n");
		goto Continue;
	      }
              break;
            case 1:	/* valid to send - announce clauses do not apply */
#ifdef	notdef
              this_metric = mapmetric(HELLO_TO_EGP, (u_short)rt->rt_metric);
#else	notdef
              this_metric = rt->rt_metric_exterior;	/* Propogate EGP metric? */
#endif	notdef
              break;
            case -1:	/* no AS restrictions */
              TRACE_EGPUPD("no AS restrictions\n");
              goto Continue;
          }
#ifdef	NSS
	  /*
	   * If we have both IGP and EGP learned route, do
	   * not bother to advertize it twice.
	   */
	  if ((rt_egp = rt_locate((int) EXTERIOR, (struct sockaddr_in *) &rt->rt_dst, RTPROTO_EGP)) != NULL) {
	    if ((rt_egp->rt_metric <= rt->rt_metric) && (rt_egp->rt_as != ngp->ng_asin) &&
                (as_reach(rt_egp->rt_as) == TRUE) && (sendAS(ngp->ng_aslist, rt_egp->rt_as) != 0)) {
              TRACE_EGPUPD("Have both IGP and EGP learned route... ignoring IGP\n");
	      goto Continue;
	    }
	  }
#endif	NSS
          break;
        case RTPROTO_KERNEL:
          TRACE_EGPUPD("not sending kernel routes\n");
          goto Continue;
        case RTPROTO_REDIRECT:
          TRACE_EGPUPD("not sending redirected routes\n");
          goto Continue;
        default:
          TRACE_EGPUPD("unknown protocol %d\n", rt->rt_proto);
          goto Continue;
      } /* switch (rt->rt_proto) */
      /*
       * committed to advertising net
       */
      this_net = free_net++;
      this_net->net.s_addr = sock_inaddr(&rt->rt_dst).s_addr;
      this_net->distance = this_metric;
      /*
       * assign gw on shared net
       */
      if (gd_inet_wholenetof(sock_inaddr(&rt->rt_router)) != gd_inet_wholenetof(ngp->ng_myaddr)) {
        this_net->gateway = ngp->ng_myaddr;                /* gw is self */
      } else {
        this_net->gateway = sock_inaddr(&rt->rt_router);   /* gw is neighbor */
      }
      /*
       * If this is the DEFAULT net, set the specified metric, we are
       * the gateway.
       */
      if ( this_net->net.s_addr == DEFAULTNET ) {
        this_net->gateway = ngp->ng_myaddr;
        this_net->distance = ngp->ng_defaultmetric;
        TRACE_EGPUPD(" DEFAULT - ");
      }
      TRACE_EGPUPD("metric %3d ..", this_net->distance);
      /*
       * insert net in ordered list
       */
      for (net_pt = start_net; net_pt->next; net_pt = net_pt->next) {
        if (this_net->gateway.s_addr == net_pt->next->gateway.s_addr) {
          if (this_net->distance <= net_pt->next->distance)
            break;
        } else {
          if (this_net->gateway.s_addr == net_pt->gateway.s_addr) {
            break;
          }
        }
      }  /* for (all nets to be announced) */
      /*
       * insert this net after search net
       */
      this_net->next = net_pt->next;
      net_pt->next = this_net;
      TRACE_EGPUPD("added to update distance %3d gateway %s\n",
        this_net->distance, inet_ntoa(this_net->gateway));
Continue: ;
  }                               /* end for all interior routes */

  /*
   * copy nets into NR message
   */
  nrpkt->en_igw = 0;              /* init # interior gateways */
  nrp = (u_char *)(nrpkt + 1);    /* start nets part NR msg */
  current_gw.s_addr = 0;          /* ensure first gateway addr copied */
  for (net_pt = start_net->next; net_pt != NULL; net_pt = net_pt->next) {
    if ( (net_pt->distance < HOPCNT_INFINITY) && (ngp->ng_flags & NG_METRICOUT) ) {
      net_pt->distance = ngp->ng_metricout;
    }
    if (net_pt->gateway.s_addr != current_gw.s_addr) {
      /* new gateway */
      current_gw.s_addr = net_pt->gateway.s_addr;
      current_net = current_gw.s_addr;
      n_bytes = 4 - gd_inet_class((u_char *)&current_net);
      bcopy((char *)&current_net + 4 - n_bytes, (char *)nrp, n_bytes);
      nrp += n_bytes;
      nrpkt->en_igw++;
      n_distance = nrp++;
      *n_distance = 1;
      distance = nrp++;
      *distance = net_pt->distance;
      n_nets = nrp++;
      *n_nets = 1;
    } else if ( (net_pt->distance != *distance) || (*n_nets == 255) ) {
      /* New distance or this distance if ull */
      (*n_distance)++;
      distance = nrp++;
      *distance = net_pt->distance;
      n_nets = nrp++;
      *n_nets = 1;
    } else {
      (*n_nets)++;
    }

    current_net = net_pt->net.s_addr;
    n_bytes = gd_inet_class((u_char *)&current_net);
    bcopy((char *)&current_net, (char *)nrp, n_bytes);
    nrp += n_bytes;
  }                                      /* end for each net */
  free((char *)start_net);
  return(nrp - (u_char *)nrpkt);         /* length of NR message */
}

/*
 * rt_NRupdate() updates the exterior routing tables on receipt of an NR
 * update message from an EGP neighbor. It first checks for valid NR counts 
 * before updating the routing tables.
 *
 * EGP Updates are used to update the exterior routing table if one of the
 * following is satisfied:
 *   - No routing table entry exists for the destination network and the
 *     metric indicates the route is reachable (< 255).
 *   - The advised gateway is the same as the current route.
 *   - The advised distance metric is less than the current metric.
 *   - The current route is older (plus a margin) than the maximum poll
 *     interval for all acquired EGP neighbors. That is, the route was
 *     omitted from the last Update.
 *
 * Returns 1 if there is an error in NR message data, 0 otherwise.
 */

rt_NRupdate(ngp, pkt, egplen)
        struct egpngh  *ngp;            /* pointer to neighbor state table */
        struct egppkt  *pkt;
        int             egplen;         /* length EGP NR packet */
{
  register  u_char  *nrb;
  struct egpnr *nrp = (struct egpnr *)pkt;
  struct sockaddr_in      destination, gateway;
  u_char  gw[4];                          /* gateway internet address */
  int     gw_class,
          net_class,
          ng,
          nd,
          nn,
          n_gw,
          n_dist,
          n_net,
          metric,
          state,
          checkingNR = TRUE,
          change = FALSE,
          NR_nets = 0;
  u_short distance;
  struct rt_entry *rt;
  u_short pkt_system;
#ifndef	NSS
  struct interface *ifp;
#else	NSS
  struct rt_entry *rt_igp;
  int flood = FALSE;
#endif	NSS

  bzero((char *)&destination, sizeof(destination));
  bzero((char *)&gateway, sizeof(gateway));
  gateway.sin_family = AF_INET;
  destination.sin_family = AF_INET;
  
#ifndef	NSS
  gateway.sin_addr.s_addr = ngp->ng_myaddr.s_addr;
  if ((ifp = if_ifwithaddr((struct sockaddr *)&gateway)) <= (struct interface *)0) {
    TRACE_INT("No interface for egp, %s wanted\n", inet_ntoa(ngp->ng_myaddr));
    syslog(LOG_ERR, "No interface for egp, %s wanted", inet_ntoa(ngp->ng_myaddr));
    return(EUNSPEC);
  }
#endif	NSS

  /*
   * check class of shared net
   */
  *(u_long *)gw = nrp->en_net.s_addr;     /* set net part of gateways */
  if ( (gw_class = gd_inet_class((u_char *)&gw[0])) == 0 ) {
    return(EBADDATA);			/* NR message error */
  }

  pkt_system = htons(pkt->egp_system);

  n_gw = nrp->en_igw + nrp->en_egw;

  /*
   * First check NR message for valid counts, then repeat and update routing
   * tables
   */
repeat:
  nrb = (u_char *)nrp + sizeof(struct egpnr);  /* start first gw */

  for (ng = 0; ng < n_gw; ng++) {         /* all gateways */
    switch (gw_class) {         /* fill gateway local address */
      case CLAA:      gw[1] = *nrb++;
      case CLAB:      gw[2] = *nrb++;
      case CLAC:      gw[3] = *nrb++;
    }
    gateway.sin_addr.s_addr = (ngp->ng_flags & NG_GATEWAY) ? ngp->ng_gateway.s_addr : *(u_long *)gw;
    n_dist = *nrb++;

    for (nd = 0; nd < n_dist; nd++) {   /* all distances this gateway */
      distance = (u_short)(*nrb++);
#ifdef	NSS
      metric = mapmetric(EGP_TO_HELLO, distance);	/* Is there a value to use as default metric? */
      if ( ngp->ng_flags & NG_METRICIN ) {
        metric = ngp->ng_metricin;
      }
#else	NSS
      metric = mapmetric(RIP_TO_HELLO, (unsigned short) ifp->int_metric);
      if ( (metric < TDHOPCNT_INFINITY) && (ngp->ng_flags & NG_METRICIN) ) {
        metric = ngp->ng_metricin;
      }
#endif	NSS
      n_net = *nrb++;
      if ( !checkingNR ) {
        NR_nets += n_net;
      }
      for (nn = 0; nn < n_net; nn++) {  /* all nets this distance */
        if ( (net_class = gd_inet_class(nrb)) == 0 ) {
          net_class = 3;
        }
        destination.sin_addr.s_addr = 0;    /* zero unused bytes*/
        bcopy((char *)nrb, (char *)&destination.sin_addr.s_addr, net_class);
        nrb += net_class;
	if ( !gd_inet_class((u_char *)&destination.sin_addr.s_addr) ) {
          char badgate[16], badvia[16];
          (void) strcpy(badgate, inet_ntoa(ngp->ng_addr));
          (void) strcpy(badvia, inet_ntoa(gateway.sin_addr));
	  if ( checkingNR ) {
            TRACE_EXT("rt_NRupdate: net %-15s not class A, B or C from %-15s via %-15s at %s",
              inet_ntoa(destination.sin_addr), badgate, badvia, strtime);
            change = TRUE;
            syslog(LOG_ERR,
              "rt_NRupdate: net %s not class A, B or C from %s via %s\n",
              inet_ntoa(destination.sin_addr), badgate, badvia);
          }
#ifdef	notdef
          return(EBADDATA);	/* Ignore complete NR packet */
#else	notdef
          continue;	/* Ignore only this route */
#endif	notdef
	}
	if ( is_martian(destination.sin_addr) ) {
          char badgate[16], badvia[16];
          (void) strcpy(badgate, inet_ntoa(ngp->ng_addr));
          (void) strcpy(badvia, inet_ntoa(gateway.sin_addr));
          if ( checkingNR ) {
            TRACE_EXT("rt_NRupdate: ignoring invalid net %-15s from %-15s via %-15s at %s",
              inet_ntoa(destination.sin_addr), badgate, badvia, strtime);
            change = TRUE;
            syslog(LOG_WARNING, "rt_NRupdate: ignoring invalid net %s from %s via %s",
              inet_ntoa(destination.sin_addr), badgate, badvia);
          }
          continue;
	}
        if ( (destination.sin_addr.s_addr == DEFAULTNET) && !(ngp->ng_flags & NG_DEFAULTIN) ) {
          char badgate[16], badvia[16];
          (void) strcpy(badgate, inet_ntoa(ngp->ng_addr));
          (void) strcpy(badvia, inet_ntoa(gateway.sin_addr));
	  if ( checkingNR ) {
            TRACE_EXT("rt_NRupdate: ignoring net %-15s from %-15s via %-15s at %s",
              inet_ntoa(destination.sin_addr), badgate, badvia, strtime);
            change = TRUE;
            syslog(LOG_WARNING, "rt_NRupdate: ignoring net %s from %s via %s",
              inet_ntoa(destination.sin_addr), badgate, badvia);
          }
	  continue;
	}
        if (checkingNR) {           /* first check counts only */
          if (nrb > (u_char *)nrp + egplen + 1)
            return(EBADDATA);          /* erroneous counts in NR */
        } else {                      /* update routing table */
          if (gateway.sin_addr.s_addr == ngp->ng_myaddr.s_addr)
            continue;
          /*
           * If the validas list exists, check if this net/AS combination is on it.
           */
          if (ngp->ng_flags & NG_VALIDATE) {
            int valid = 0;

            if (validas) {
              struct as_valid *tmp_valid;
              for (tmp_valid = validas; tmp_valid; tmp_valid = tmp_valid->next) {
                if ( (tmp_valid->as == pkt_system) &&
                     (tmp_valid->dst.s_addr == destination.sin_addr.s_addr) ) {
                  valid++;
                  if ( metric < TDHOPCNT_INFINITY ) {
                    metric = tmp_valid->metric;
                  }
                  break;
                }
              }
            }
            if (!valid) {
              if ( tracing & TR_EGP ) {
                printf("rt_NRupdate: net %-15s not valid from AS %5d at %s",
                  inet_ntoa(destination.sin_addr), pkt_system, strtime);
                change = TRUE;
              }
#ifdef	notdef
              syslog(LOG_WARNING, "rt_NRupdate: net %s not valid from AS %d",
                inet_ntoa(destination.sin_addr), pkt_system);
#endif	notdef
              continue;
            }
          }
          /*
           * check for internal route
           */
          state = 0;
          if ((rt = rt_lookup((int)INTERIOR, &destination)) != NULL) {
            /*
             * assume INTERIOR route is better ,
             * if it's there at all.
             * 
             *  Extra sanity check to keep duplicate routes
             *  out of the kernel.   Makes things neater.
             */
            while (rt = rt_lookup((int)EXTERIOR, &destination)) {
              rt_delete(rt, KERNEL_INTR);
            }
            continue;
          }
#ifdef	NSS
          rt = rt_locate((int)EXTERIOR, &destination, RTPROTO_EGP);
#else	NSS
          rt = rt_lookup((int)EXTERIOR, &destination);
#endif	NSS
          if (rt == NULL) {       /* new route */
            if (metric >= TDHOPCNT_INFINITY) {
              continue;
            }
            if (rt = rt_add((int)EXTERIOR, (struct sockaddr *)&destination, (struct sockaddr *)&gateway, metric, state, 
                   RTPROTO_EGP, RTPROTO_EGP, ngp->ng_as, distance) ) {
#ifdef	NSS
              psp_egp_rtadd(&destination, &gateway, ngp->ng_myaddr);
              rt_igp = rt_locate((int) EXTERIOR, &destination, RTPROTO_IGP);
              if (rt_igp != NULL) {
                if (rt_igp->rt_metric >= metric) {
                  es_rtdel(&destination);
                  es_rtadd(&destination, ngp->ng_as);
                }
              } else {
                es_rtadd(&destination, pkt_ngp->ng_as);
              }
              flood = TRUE;
#endif	NSS
              change = TRUE;
            }
          } else {                  /* existing route */
            if (equal(&rt->rt_router, &gateway) && (rt->rt_as == ngp->ng_as)) {     /* same gw */
              if (metric < TDHOPCNT_INFINITY) {
                if (metric < rt->rt_metric) {
                  if (rt_change(rt, (struct sockaddr *)&gateway, metric, RTPROTO_EGP, RTPROTO_EGP, ngp->ng_as, distance)) {
#ifdef	NSS
                    flood = TRUE;
#endif	NSS
                    change = TRUE;
                  }
                }
                rt->rt_timer = 0;
              } else {
                change = rt_unreach(rt);
              }
            } else {              /* different gateway */
              if (rt->rt_as == ngp->ng_as) {
                /* Same Autonotmous system */
                if (distance < rt->rt_metric_exterior) {
#ifdef	NSS
                  struct sockaddr	old_gateway = rt->rt_router;
		  /* Advertized metric is better */

#endif	NSS
                  if (rt_change(rt, (struct sockaddr *)&gateway, metric, RTPROTO_EGP, RTPROTO_EGP, ngp->ng_as, distance)) {
#ifdef	NSS
                    psp_egp_rtdel(&rt->rt_dst, &old_gateway);
                    psp_egp_rtadd(&destination, &gateway, ngp->ng_myaddr);
#endif	NSS
                    rt->rt_timer = 0;
                    change = TRUE;
                  }
                }
              } else
              if ((metric < rt->rt_metric) || 
                         ( (metric >= rt->rt_metric) && (rt->rt_timer > maxpollint) && !(rt->rt_state & RTS_CHANGED)) ) {
#ifdef	NSS
                int egp_metric = rt->rt_metric;
                psp_egp_rtdel(&rt->rt_dst, &rt->rt_router);
                psp_egp_rtadd(&destination, &gateway, ngp->ng_myaddr);
#endif	NSS
                if (rt_change(rt, (struct sockaddr *)&gateway, metric, RTPROTO_EGP, RTPROTO_EGP, ngp->ng_as, distance)) {
#ifdef	NSS
            	  rt_igp = rt_locate((int) EXTERIOR, &destination, RTPROTO_IGP);
		  /*
		   * Check whether we have to replace IGP route (if exists)
		   * with EGP learned route.
		   * We'll do it if either there is not IGP route or
		   * IGP route is more expensive.
		   */
            	  if (((rt_igp != NULL) && (metric <= rt_igp->rt_metric)) || (rt_igp == NULL)) {
                    TRACE_INT("rt_NRupdate: Replacing IGP route with EGP route\n");
                    es_rtdel(&destination);
                    es_rtadd(&destination, ngp->ng_as);
                  }
		  /*
		   * Check whether we have to replace EGP route with IGP
		   * route (if exists).
		   * We'll do it if IGP metric is less than new EGP metric
		   * and greater than the old EGP metric
		   */
		  if ((rt_igp != NULL) && (metric > rt_igp->rt_metric) && (rt_igp->rt_metric > egp_metric)) {
                    TRACE_INT("rt_NRupdate: Replacing EGP route with IGP route\n");
                    es_rtdel(&destination);
                    es_rtadd(&destination, rt_igp->rt_as);
                  }
                  flood = TRUE;
#endif	NSS
                  change = TRUE;
                  rt->rt_timer = 0;
                }
              }
            }
          }       /* end else existing route */
        }           /* end else update routing table */
      }               /* end for all nets */
    }                   /* end for all distances */
  }                       /* end for all gateways */
  if (checkingNR) {
    if (nrb > (u_char *)nrp + egplen) {
      return(EBADDATA);                      /* erroneous counts */
    } else {
      checkingNR = FALSE;
    }
    goto repeat;
  }
  /*
   * Generate default if not prohibited and the NR packet
   * contains more than one route
   */
  if (!(ngp->ng_flags & NG_NOGENDEFAULT) && (NR_nets > 1)) {
    change += rt_default("ADD");
  }
  if (change) {
    TRACE_RT("rt_NRupdate: above %d routes from %s updated %s\n",
                NR_nets, inet_ntoa(ngp->ng_addr), strtime);
  }
#ifdef	NSS
  if (flood) {
    egp2esrec();
  }
#endif	NSS
  return(NOERROR);
}
