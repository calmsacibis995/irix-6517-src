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
static char *rcsid = "$Header: /proj/irix6.5.7m/isms/eoe/cmd/bsd/gated/RCS/rt_if.c,v 1.2 1990/01/09 15:36:11 jleong Exp $";
#endif	not lint

/*
 *  Routines for handling routes to interfaces
 */
 
#include "include.h"

#ifndef	NSS
/*
 *  rt_ifdown updates the routing table when an interface has transitioned
 *  down.  It is also used at init when an interface is found to be down
 *  by having init_flag be TRUE.
 *
 *  At init time, routes to direct interfaces are assumed to be
 *  RTPROTO_KERNEL as read from the kernel's routing table.  When an
 *  interface transitions down routes are assumed to be RTPROTO_DIRECT.
 *
 *  If the interface is point-to-point, the hostroute to the other side
 *  of the link is deleted.  If not, the route to the attached interface
 *  is deleted.  If the interface is subnetted, the internal route to
 *  the whole net is deleted if this interface is the gateway.
 */
 
rt_ifdown(ifp, init_flag)
	struct interface *ifp;
int init_flag;
{
  struct sockaddr_in dst;
  struct rt_entry *rt;
  int proto;
  
  if (init_flag) {
    proto = RTPROTO_KERNEL;
  } else {
    proto = RTPROTO_DIRECT;
  }
  
  if (ifp->int_flags & IFF_POINTOPOINT) {
    /****************************************************************/
    /*  Delete or declare unreachable route to host at other end of */
    /*  point-to-point link                                         */
    /****************************************************************/
    if (rt = rt_locate((int)HOSTTABLE, (struct sockaddr_in *)&ifp->int_dstaddr, RTPROTO_DIRECT) ) {
      if (init_flag) {
      	rt_delete(rt, KERNEL_INTR);
      } else {
      	(void) rt_unreach(rt);
      }
    }
  } else {
    /***************************************************************/
    /*	Delete or declare unreachable route to subnet and full net */
    /***************************************************************/
    bzero((char *)&dst, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_addr = gd_inet_makeaddr(ifp->int_subnet, 0, TRUE);
    if (rt = rt_locate((int)INTERIOR, &dst, proto) ) {
      if (init_flag) {
      	rt_delete(rt, KERNEL_INTR);
      } else {
      	(void) rt_unreach(rt);
      }
    }
    if (ifp->int_net != ifp->int_subnet) {
      dst.sin_addr = gd_inet_makeaddr(ifp->int_net, 0, FALSE);
      if (rt = rt_locate((int)INTERIOR, &dst, proto) ) {
      	if (rt->rt_ifp == ifp) {
      	  if (init_flag) {
      	    rt_delete(rt, KERNEL_INTR);
      	  } else {
      	    (void) rt_unreach(rt);
      	  }
      	}
      }
    }
  }
} /* rt_ifdown() */


/*
 *  rt_ifip processes an interface transition to up or an interface that
 *  is up at init time.
 *
 *  If the interface is point-to-point, any host route to that interface
 *  is deleted and a host route to the interface with a protocol of
 *  RTPROTO_DIRECT is added to the routing table.
 *
 *  For non-point-to-point interfaces, the route to the attached network
 *  is deleted from the routing table and a RTPROTO_DIRECT route is
 *  added. 
 *
 *  If non-point-to-point subnetted interfaces, the routing table is
 *  searched for a direct route to this interface
 */
  
rt_ifup(ifp)
	struct interface *ifp;
{
  struct interface *net_ifp, *tifp;
  struct sockaddr_in dst;
  struct rt_entry *rt;
  int iflags = RTS_INTERFACE;
  int save_install = install;

  install = TRUE;
  if (ifp->int_flags & IFF_NOAGE) {
    iflags |= RTS_PASSIVE;
  }
  
  if (ifp->int_flags & IFF_POINTOPOINT) {
    /*******************************************************/
    /*  Delete all routes to the host at other end of      */
    /*  point-to-point  link then add a host route to it.  */
    /*******************************************************/
    iflags |= RTS_POINTOPOINT;
    if (rt = rt_lookup((int)HOSTTABLE, (struct sockaddr_in *)&ifp->int_dstaddr) ) {
      rt_delete(rt, KERNEL_INTR);
    }
    (void) rt_add((int)HOSTTABLE, (struct sockaddr *)&ifp->int_dstaddr,
                  &ifp->int_addr,
                  mapmetric(RIP_TO_HELLO, (u_short)ifp->int_metric),
                  iflags, RTPROTO_DIRECT, RTPROTO_DIRECT, 0, 0);
  } else {
    /********************************************************************/
    /*  Delete any routes to this subnet and add an interface route to  */
    /*  it.                                                             */
    /********************************************************************/
    bzero((char *)&dst, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_addr = gd_inet_makeaddr(ifp->int_subnet, 0, TRUE);
    if (rt = rt_lookup((int)INTERIOR|(int)EXTERIOR, (struct sockaddr_in *)&dst) ) {
      rt_delete(rt, KERNEL_INTR);
    }
    (void) rt_add((int)INTERIOR, (struct sockaddr *)&dst,
                  &ifp->int_addr,
                  mapmetric(RIP_TO_HELLO, (u_short)ifp->int_metric),
                  iflags, RTPROTO_DIRECT, RTPROTO_DIRECT, 0, 0);
    if (ifp->int_net != ifp->int_subnet) {
      /******************************************************************/
      /*  Interface is to a subnet.  Must update route to the main net  */
      /*  if this is the most attractive interface to it.               */
      /******************************************************************/
      dst.sin_addr = gd_inet_makeaddr(ifp->int_net, 0, FALSE);
      if (rt = rt_lookup((int)INTERIOR|(int)EXTERIOR, &dst) ) {
      	/*****************************/
      	/* Route to whole net exists */
      	/*****************************/
      	if (rt->rt_proto == RTPROTO_DIRECT) {
      	  /***********************************************************/
      	  /*  Route is via a direct interface, find most attractive  */
          /*  interface to this net.                                 */
      	  /***********************************************************/
          net_ifp = ifp;
      	  for (tifp = ifnet; tifp; tifp = tifp->int_next) {
#ifdef sgi
      	    if ( _IFF_UP_RUNNING(tifp->int_flags) 
#else
      	    if ( (tifp->int_flags & IFF_UP) 
#endif
		&& (tifp->int_metric < net_ifp->int_metric) ) {
      	      net_ifp = tifp;
      	    }
      	  }
          install = FALSE;
      	  if ( net_ifp == ifp ) {
      	    /*********************************************************/
      	    /*  This interface is the most attractive route, update  */
	    /*  existing route                                       */
      	    /*********************************************************/
            (void) rt_change(rt, &ifp->int_addr,
                             mapmetric(RIP_TO_HELLO, (u_short)ifp->int_metric),
                             RTPROTO_DIRECT, RTPROTO_DIRECT, 0, 0);
      	  } else if (rt->rt_ifp == ifp) {
              /*******************************************************/
              /*  This interface is not the most attractive route,   */
	      /*  but is existing route, delete                      */
       	      /*******************************************************/
              rt_delete(rt, KERNEL_INTR);
          }
        } else {
          /***********************************************************/
          /*  This is not an interface route, delete it and add an   */
	  /*  interface route                                        */
          /***********************************************************/
          rt_delete(rt, KERNEL_INTR);
          install = FALSE;
          (void) rt_add((int)INTERIOR, (struct sockaddr *)&dst,
                        &ifp->int_addr,
                        mapmetric(RIP_TO_HELLO, (u_short)ifp->int_metric),
                        iflags, RTPROTO_DIRECT, RTPROTO_DIRECT, 0, 0);
        }
      } else {
      	/************************************************************/
      	/*  No route to the net exists, add an interface route to   */
	/*  our tables only                                         */
      	/************************************************************/
      	install = FALSE;
        (void) rt_add((int)INTERIOR, (struct sockaddr *)&dst,
                      &ifp->int_addr,
                      mapmetric(RIP_TO_HELLO, (u_short)ifp->int_metric),
                      iflags, RTPROTO_DIRECT, RTPROTO_DIRECT, 0, 0);
      }
    }
  }
  install = save_install;
} /* rt_ifup() */


/*
 *  rt_ifupdate() is used when a routing packet is received from an
 *  interface to make sure that a route to this interface exists.
 *
 *  If the route to this interface exists, it's metric is set to the
 *  interface metric of this interface and it's timer is reset.  If this
 *  is a subnet route, the internal route to the main net is also updated.
 *
 *  If the route to this interface does not exist, rt_ifup is called to
 *  add it to the routing table.
 */
 
rt_ifupdate(ifp)
	struct interface *ifp;
{
  struct rt_entry *rt;
  struct sockaddr_in dst;
  
  bzero((char *)&dst, sizeof(dst));
  dst.sin_family = AF_INET;
  dst.sin_addr = gd_inet_makeaddr(ifp->int_subnet, 0, TRUE);
  if ( (rt = rt_locate((int)INTERIOR, (struct sockaddr_in *)&dst, RTPROTO_DIRECT)) && (rt->rt_ifp == ifp) ) {
    rt->rt_metric = mapmetric(RIP_TO_HELLO, (u_short)ifp->int_metric);
    rt->rt_timer = 0;
    if (ifp->int_net != ifp->int_subnet) {
      dst.sin_addr = gd_inet_makeaddr(ifp->int_net, 0, FALSE);
      if ( rt = rt_locate((int)INTERIOR, (struct sockaddr_in *)&dst, RTPROTO_DIRECT) ) {
      	if (rt->rt_ifp == ifp) {
          rt->rt_metric = mapmetric(RIP_TO_HELLO, (u_short)ifp->int_metric);
          rt->rt_timer = 0;
        }
      }
    }
  } else {
    rt_ifup(ifp);
  }
} /* rt_ifupdate() */


/*
 * rt_ifinit() initializes the interior routing table with direct nets as
 * per the interface table. Such routes read from the kernel routing tables
 * are deleted from the exterior routing table.
 */

rt_ifinit()
{
  register  struct 	interface *ifp;

  TRACE_RT("\nrt_ifinit: interior routes for direct interfaces:\n");

  for (ifp = ifnet; ifp; ifp = ifp->int_next) {
    if (((ifp->int_flags & (IFF_NORIPIN|IFF_NOHELLOIN)) == (IFF_NORIPIN|IFF_NOHELLOIN)) ||
        (!rip_supplier && !hello_supplier) ||
        (!doing_rip && !doing_hello) ) {
      ifp->int_flags |= IFF_NOAGE;
      TRACE_INT("rt_ifinit: interface %s: %s marked passive\n",
	ifp->int_name, inet_ntoa(sock_inaddr(&ifp->int_addr)));
    }
#ifdef sgi
    if ( _IFF_UP_RUNNING(ifp->int_flags) ) {
#else
    if (ifp->int_flags & IFF_UP) {
#endif
      rt_ifup(ifp);
    } else {
      rt_ifdown(ifp, TRUE);
    }
  }
} /* rt_ifinit() */


/*
 * rt_ifoptinit() reads the initialization file EGPINITFILE to
 * initialize: 
 * 	options that pertain to interfaces.
 */

rt_ifoptinit(fp)
	FILE *fp;
{
  char keyword[MAXHOSTNAMELENGTH+1];
  char gname[MAXHOSTNAMELENGTH+1];
  char proto_type[MAXHOSTNAMELENGTH+1];
  char deftype[MAXHOSTNAMELENGTH+1];
  char buf[BUFSIZ];
  struct 	sockaddr_in	netaddr, gateway, defaultdst;
  int	metric, error = FALSE, line = 0;
  struct interface *ifptr;

  bzero((char *)&netaddr, sizeof(netaddr));
  bzero((char *)&gateway, sizeof(gateway));
  bzero((char *)&defaultdst, sizeof(defaultdst));

  TRACE_RT("\nrt_ifoptinit: interface options (if any):\n");

  rewind(fp);

  while (fgets(buf, sizeof(buf), fp) != NULL) {
    line++;
    if ((buf[0] == '#') || (buf[0] == '\n')) {
      continue;
    }
    if (sscanf(buf, "%s", keyword) != 1) {
      continue;
    }
    if (strcasecmp(keyword, "interfacemetric") == 0) {
      if (sscanf(buf, "%*s %s %s", gname, proto_type) != 2) {
        syslog(LOG_WARNING, "rt_ifoptinit: syntax error, line %d\n", line);
        error = TRUE;
      }
      else if (!getnetorhostname("host", gname, &gateway)) {
        syslog(LOG_WARNING, "rt_ifoptinit: invalid interface address %s\n",
                  gname);
        error = TRUE;
      } else if ((ifptr = if_ifwithaddr((struct sockaddr *)&gateway)) <= (struct interface *)0) {
        syslog(LOG_WARNING, "rt_ifoptinit: invalid interface address %s\n",
                  inet_ntoa(gateway.sin_addr));
        error = TRUE;
      } else {
        ifptr->int_metric = atoi(proto_type);
      }
      if (!error) {
        TRACE_INT("rt_ifoptinit: interfacemetric %s %d\n", inet_ntoa(sock_inaddr(&ifptr->int_addr)), ifptr->int_metric);
      }
    } /* end interfacemetric */
    else if (strcasecmp(keyword, "fixedmetric") == 0) {
      if (sscanf(buf, "%*s %s proto %s %s", gname, proto_type, deftype) != 3) {
        syslog(LOG_WARNING, "rt_ifoptinit: syntax error, line %d\n", line);
        error = TRUE;
      } else if (!getnetorhostname("host", gname, &gateway)) {
        syslog(LOG_WARNING, "rt_ifoptinit: invalid interface address %s\n",
                  gname);
        error = TRUE;
      } else if ((ifptr = if_ifwithaddr((struct sockaddr *)&gateway)) <= (struct interface *)0) {
        syslog(LOG_WARNING, "rt_ifoptinit: invalid interface address %s\n",
                  inet_ntoa(gateway.sin_addr));
        error = TRUE;
      } else if (strcasecmp("rip", proto_type) == 0) {
        metric = atoi(deftype);
        if ((metric < 0) || (metric > RIPHOPCNT_INFINITY)) {
          syslog(LOG_WARNING, "rt_ifoptinit: metric %d invalid in line %d\n",
                   metric, line);
          error = TRUE;
        } else {
          ifptr->int_ripfixedmetric = metric;
          ifptr->int_flags |= IFF_RIPFIXEDMETRIC;
        }
      } else if (strcasecmp("hello", proto_type) == 0) {
        metric = atoi(deftype);
        if ((metric < 0) || (metric > DELAY_INFINITY)) {
          syslog(LOG_WARNING, "rt_ifoptinit: metric %d invalid in line %d\n",
                   metric, line);
          error = TRUE;
        } else {
          ifptr->int_hellofixedmetric = metric;
          ifptr->int_flags |= IFF_HELLOFIXEDMETRIC;
        }
      } else {
        syslog(LOG_WARNING, "rt_ifoptinit: unsupported protocol %s in line %d\n",
                 proto_type, line);
        error = TRUE;
      }
      if (!error) {
        syslog(LOG_WARNING, "fixed metric interface %s protocol %s metric %d",
                 inet_ntoa(gateway.sin_addr), proto_type, metric);
        TRACE_INT("fixedmetric %s proto %s %d\n", inet_ntoa(gateway.sin_addr), proto_type, metric);
      }
    } /* end fixedmetric */
  } /* end while */
  if (error) {
    syslog(LOG_EMERG, "rt_ifoptinit: %s: initialization error\n", EGPINITFILE);
    quit();
  }
} /* rt_ifoptinit() */

#endif	NSS
