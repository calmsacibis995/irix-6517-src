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
static char *rcsid = "$Header: /proj/irix6.5.7m/isms/eoe/cmd/bsd/gated/RCS/rip.c,v 1.3 1990/01/09 15:36:05 jleong Exp $";
#endif	not lint

#include "include.h"

/*
 * 	Check out a newly received RIP packet.
 */

ripin(from, size, pkt)
	struct sockaddr *from;
	int size;
	char *pkt;
{
  register struct rt_entry *rt, *exrt;
  register struct netinfo *n;
  register struct afswitch *afp;
  register struct interface *ifp, *ifpc;
  register int OK = 0;
  struct rip *inripmsg = (struct rip *)pkt;
  int change = FALSE, check_zero = FALSE;
  int newsize, rte_table, fromproto;
  struct sockaddr_in *sin_from = (struct sockaddr_in *)from;
  u_short src_port = sin_from->sin_port;
  char *reject_msg = (char *)0;
  char type[MAXHOSTNAMELENGTH];
  int answer = FALSE;
  int split_horizon = TRUE;

  if (from->sa_family != AF_INET) {
    reject_msg = "protocol not INET";
    goto Reject;
  }

  switch (inripmsg->rip_vers) {
    case 0:
       reject_msg = "ignoring version 0 packets";
       goto Reject;
    case 1:
       check_zero++;
       break;
  }

  /* ignore if not from a trusted RIPer */
  if (trustedripperlist != (struct advlist *)NULL) {
    register struct advlist *ad;   /* list of trusted RIPpers */

    for (ad = trustedripperlist; ad; ad = ad->next)
      if (sin_from->sin_addr.s_addr == ad->destnet.s_addr) {
        OK++;
	break;
      }
  } else {
    OK++;
  }

  afp = &afswitch[from->sa_family];

  TRACE_RIPINPUT(ifp, (struct sockaddr_in *)from, size, (char *)inripmsg);

  switch (inripmsg->rip_cmd) {
#ifdef	RIPCMD_POLL
    case RIPCMD_POLL:
       answer = TRUE;
       split_horizon = FALSE;
#endif	RIPCMD_POLL
    case RIPCMD_REQUEST:
      (*afp->af_canon)(from);

      if ((src_port != sp->s_port) || answer) {
        sin_from->sin_port = src_port;
        if ((ifp = if_withnet((struct sockaddr_in *)from)) <= (struct interface *)0) {
          struct sockaddr_in dst;

          dst = *sin_from;
          dst.sin_addr.s_addr = htonl(gd_inet_netof(dst.sin_addr));
          if ((rt = rt_lookup((int) (INTERIOR+EXTERIOR), &dst)) == (struct rt_entry *)0) {
            if ((rt = rt_lookup((int) (INTERIOR+EXTERIOR), &hello_dfltnet)) == (struct rt_entry *)0) {
              reject_msg = "can not find interface for route";
              goto Reject;
            }
          }
          ifp = rt->rt_ifp;
        }
      } else {
        if (ifpc = if_ifwithaddr(from)) {
          return;
        }
        if (!OK) {
          reject_msg = "not on trustedripgateways list";
          goto Reject;
        } 
        if ((ifp = if_withnet((struct sockaddr_in *)from)) <= (struct interface *)0) {
          reject_msg = "not on same net";
          goto Reject;
        } 
        if ( ifp->int_flags & (IFF_NORIPIN|IFF_NORIPOUT) ) {
          reject_msg = "interface marked for no RIP in/out";
          goto Reject;
        }
        if (rip_supplier <= 0) {
          reject_msg = "not supplying RIP";
          goto Reject;
        } 
        (void) if_updateactivegw(ifp, sin_from->sin_addr.s_addr, RTPROTO_RIP);
      }

      newsize = 0;
      size -= 4 * sizeof(char);
      n = inripmsg->rip_nets;
      while (size > 0) {
        if (size < sizeof(struct netinfo)) {
          break;
        }
        size -= sizeof(struct netinfo);
        n->rip_dst.sa_family = ntohs(n->rip_dst.sa_family);
        n->rip_metric = ntohl((u_long)n->rip_metric);
        if (n->rip_dst.sa_family == AF_UNSPEC &&
            n->rip_metric == RIPHOPCNT_INFINITY &&
            size == 0) {
          supply(from, 0, ifp, split_horizon);
          return;
        }
        rt = rt_lookup((int)INTERIOR, (struct sockaddr_in *)&n->rip_dst);
        n->rip_metric = (rt == 0) ? RIPHOPCNT_INFINITY :
                        min(mapmetric(HELLO_TO_RIP, (u_short)rt->rt_metric) + 1 +
                            ifp->int_metric, (u_short)RIPHOPCNT_INFINITY);
        n++;
        newsize += sizeof(struct netinfo);
      }
      if (newsize > 0) {
        inripmsg->rip_cmd = RIPCMD_RESPONSE;
        newsize += sizeof(int);
        bcopy((char *) inripmsg, (char *)ripmsg, newsize);
        TRACE_RIPOUTPUT(ifp, (struct sockaddr_in *)from, newsize);
        (*afp->af_output)(rip_socket, 0, from, newsize);
      }
      return;
    case RIPCMD_TRACEON:
    case RIPCMD_TRACEOFF:
      if (!OK) {
        reject_msg = "not on trustedripgateways list";
        goto Reject;
      }
      if ((*afp->af_portcheck)(from) == 0) {
        reject_msg = "not from a trusted port";
        goto Reject;
      }
      if ((ifp = if_withnet((struct sockaddr_in *)from)) <= (struct interface *)0) {
        reject_msg = "not on same net";
        goto Reject;
      }
      if (ifp->int_flags & IFF_NORIPIN) {
        reject_msg = "not listening to RIP on this interface";
        goto Reject;
      }
      *(pkt + size) = '\0';
      reject_msg = "TRACE packets not supported";
      goto Reject;
#ifdef	RIPCMD_POLLENTRY
    case RIPCMD_POLLENTRY:
      n = inripmsg->rip_nets;
      newsize = sizeof (struct entryinfo);
      n->rip_dst.sa_family = ntohs(n->rip_dst.sa_family);
      if (n->rip_dst.sa_family == AF_INET && afswitch[n->rip_dst.sa_family].af_hash) {
        rt = rt_lookup((int)INTERIOR, (struct sockaddr_in *)&n->rip_dst);
      } else {
        rt = 0;
      }
      if (rt) {       /* don't bother to check rip_vers */
        struct entryinfo *e = (struct entryinfo *) n;
        e->rtu_dst = rt->rt_dst;
        e->rtu_dst.sa_family = ntohs(e->rtu_dst.sa_family);
        e->rtu_router = rt->rt_router;
        e->rtu_router.sa_family = ntohs(e->rtu_router.sa_family);
        e->rtu_flags = ntohs((unsigned short) rt->rt_flags);
        e->rtu_state = ntohs((unsigned short) rt->rt_state);
        e->rtu_timer = ntohl((unsigned long) rt->rt_timer);
        e->rtu_metric = ntohl((unsigned long) mapmetric(HELLO_TO_RIP, (unsigned short) rt->rt_metric));
        if (ifp = rt->rt_ifp) {
          e->int_flags = ntohl((unsigned long) ifp->int_flags);
          (void) strncpy(e->int_name, rt->rt_ifp->int_name, sizeof(e->int_name));
        } else {
          e->int_flags = 0;
          (void) strcpy(e->int_name, "(none)");
        }
      }	else {
        bzero((char *)n, newsize);
      }
      bcopy((char *) inripmsg, (char *)ripmsg, newsize);
      TRACE_RIPOUTPUT(ifp, (struct sockaddr_in *)from, newsize);
      (*afp->af_output)(rip_socket, 0, from, newsize);
      return;
#endif	RIPCMD_POLLENTRY
    case RIPCMD_RESPONSE:
      /*
       *  Are we talking to ourselves???
       *
       *  if_ifwithaddr() handles PTP's also.  If from a
       *  dst of a PTP link, let it through for further processing.
       *  you shouldn't receive your own RIPs on a PTP.
       */

      if (ifpc = if_ifwithaddr(from)) {
#ifdef sgi
      /*
       * In the case of a PTP link, the above check may result in ifpc = 0
       * for a SLIP connection. There is a small window where the dst_addr
       * is incorrect when one SLIP connection ends and another connection
       * to a different host immediately starts up. The next timeout() will
       * make a call to if_check() to fix this. But no problem, a SLIP
       * connection should fall through for further processing anyways.
       */
#endif
        rt_ifupdate(ifpc);
        if ((ifpc->int_flags & IFF_POINTOPOINT) == 0) {
#ifdef sgi
	  return;	/* must be me so don't bother with this packet */
#else
          return;
#endif
        }
      }

      if (!OK) {
#ifdef	notdef
        reject_msg = "not on trustedripgateways list";
        goto Reject;
#else	notdef
	return;
#endif	notdef
      }
      if ((*afp->af_portmatch)(from) == 0) {
        reject_msg = "not from a trusted port";
        goto Reject;
      }
      if ((ifp = if_withnet((struct sockaddr_in *)from)) <= (struct interface *)0) {
        reject_msg = "not on same net";
        goto Reject;
      }
      if (ifp->int_flags & IFF_NORIPIN) {
        reject_msg = "interface marked for no RIP in";
        goto Reject;
      }

      (*afp->af_canon)(from);

      /*
       * update interface timer on interface that packet came in on.
       */
      rt_ifupdate(ifp);

      fromproto = if_updateactivegw(ifp,sin_from->sin_addr.s_addr,RTPROTO_RIP);
      size -= 4 * sizeof (char);
      n = inripmsg->rip_nets;
      for (; size > 0; size -= sizeof (struct netinfo), n++) {
        if (size < sizeof (struct netinfo))
          break;
#ifdef sgi
	/* see if the other guy is telling us to send
	 *	our packets to him.  This can happen with
	 *	SLIP links.
	 */
	if ( if_ifwithint_addr(&n->rip_dst) )
	    continue;
#endif
        n->rip_dst.sa_family = ntohs(n->rip_dst.sa_family);
        /*
         *  Convert metric to host byte order.  If metric is zero, set to one to avoid interface routes
         */
        if ( (n->rip_metric = ntohl((u_long)n->rip_metric)) == 0) {
          n->rip_metric = 1;
        }
        /*
         * Now map rip metric to Time delay in millisec's.
         */
        n->rip_metric = mapmetric(RIP_TO_HELLO,(u_short)(n->rip_metric+ifp->int_metric));

        if (n->rip_dst.sa_family != AF_INET)
          continue;
        afp = &afswitch[n->rip_dst.sa_family];
        if (((*afp->af_checkhost)(&n->rip_dst)) == 0)
          continue;
        if ((*afp->af_ishost)(&n->rip_dst)) {
          rte_table = HOSTTABLE;
        } else {
          rte_table = INTERIOR;
        }
        rt = rt_lookup(rte_table, (struct sockaddr_in *)&n->rip_dst);
        if (rt == NULL) {  /* new route */
          struct sockaddr_in *tmp = (struct sockaddr_in *)&n->rip_dst;
          struct rt_entry rttmp;

          bzero((char *)&rttmp, sizeof(rttmp));
          rttmp.rt_listenlist = control_lookup(RT_NOLISTEN, (struct sockaddr_in *)&n->rip_dst);
          rttmp.rt_srclisten = control_lookup(RT_SRCLISTEN, (struct sockaddr_in *)&n->rip_dst);
          if (is_valid_in(&rttmp, RTPROTO_RIP, ifp, (struct sockaddr_in *)from) == 0)
            continue;
          /*
           *	Check for and ignore martain nets
           */
          if (is_martian(tmp->sin_addr)) {
            char badgate[16];
            (void) strcpy(badgate, inet_ntoa(sin_from->sin_addr));
            TRACE_EXT("ripin: ignoring invalid net %s from %s at %s",
              inet_ntoa(tmp->sin_addr), badgate, strtime);
            continue;
          }
          if (n->rip_metric >= DELAY_INFINITY) {
            continue;
          }
          if ((rte_table == INTERIOR) &&
              (exrt = rt_lookup((int)EXTERIOR, (struct sockaddr_in *)&n->rip_dst))) {
            do {
              rt_delete(exrt, KERNEL_INTR);
            } while (exrt = rt_lookup((int)EXTERIOR, (struct sockaddr_in *)&n->rip_dst));
          }
          (void) rt_add(rte_table, &n->rip_dst, from, n->rip_metric, 0, RTPROTO_RIP, fromproto, 0, 0);
          change = TRUE;
        } else {
          if ((rt->rt_flags & RTF_GATEWAY) == 0) {
            continue;
          }
          if (rt->rt_state & (RTS_INTERFACE | RTS_STATIC) ) {
            continue;
          }
          if (is_valid_in(rt, RTPROTO_RIP, ifp, (struct sockaddr_in *)from) == 0) {
            continue;
          }
          if ((rte_table == INTERIOR) &&
              (exrt = rt_lookup((int)EXTERIOR, (struct sockaddr_in *)&n->rip_dst))) {
            do {
              rt_delete(exrt, KERNEL_INTR);
            } while (exrt = rt_lookup((int)EXTERIOR, (struct sockaddr_in *)&n->rip_dst));
          }
          if (equal(&rt->rt_router, from)) {
            if (n->rip_metric >= DELAY_INFINITY) {
              if (rt->rt_metric < DELAY_INFINITY) {
                (void) rt_unreach(rt);
                change = TRUE;
              }
              continue;
            }
            if (n->rip_metric != rt->rt_metric)
              if (rt_change(rt, from, n->rip_metric, RTPROTO_RIP, fromproto, 0, 0))
                change = TRUE;
            rt->rt_timer = 0;
          } else {
            /* if a metric is INFINITY at this point
             * we don't care about the new router.
             * The only way it would accept this
             * route anyway would be if the metric
             * was already 16 and the route was old.
             * we will stick with old gateway and let
             * it time out in 2 minutes if it wants to.
             *
             * also, if the current metric is INFINITY,
             * we will only listen to our current
             * gateway.  Yes, a terrible hold down!
             * if our current gateway says nothing then
             * this route will expire in 120 seconds.
             */
            if ((n->rip_metric >= DELAY_INFINITY) ||
                (rt->rt_metric >= DELAY_INFINITY))
              continue;
            if ((n->rip_metric < rt->rt_metric) ||
                ((rt->rt_timer > (EXPIRE_TIME/2)) &&
                (rt->rt_metric == n->rip_metric))) {
              if ((rt->rt_proto & RTPROTO_HELLO) &&
                  (n->rip_metric >= rt->rt_hwindow.h_min))
                continue;
              if (rt_change(rt, from, n->rip_metric, RTPROTO_RIP, fromproto, 0, 0))
                change = TRUE;
              rt->rt_timer = 0;
            }
          }
        }
      }  /*  for each net */
      break;
    default:
      reject_msg = "invalid or not implemented command";
      goto Reject;
  }
  if ( change && (tracing & TR_RT) ) {
    printf("rip_update: above routes supplied from %s updates %s\n",
                inet_ntoa(sin_from->sin_addr), strtime);
  }
  return;

Reject:
  if (inripmsg->rip_cmd < RIPCMD_MAX) {
    (void) strcpy(type, ripcmds[inripmsg->rip_cmd]);
  } else {
    (void) sprintf(type, "#%d", inripmsg->rip_cmd);
  }
  TRACE_RIP("ripin: ignoring RIP %s packet from %s - %s\n",
    type, inet_ntoa(sin_from->sin_addr), reject_msg);
#ifdef	notdef
  syslog(LOG_INFO, "ripin: ignoring RIP %s packet from %s - %s\n",
    type, inet_ntoa(sin_from->sin_addr), reject_msg);
#endif	notdef
  return;
}

/*
 * Apply the function "f" to all non-passive
 * interfaces.  If the interface supports the
 * use of broadcasting use it, otherwise address
 * the output to the known router.
 */

toall(f)
	int (*f)();
{
  register struct interface *ifp;
  register struct advlist *ad;
  register struct sockaddr *dst;
  register int flags;
  extern struct interface *ifnet;

  if (!(rip_pointopoint)) {
    for (ifp = ifnet; ifp; ifp = ifp->int_next) {
      TRACE_JOB("toall: Checking interface %s\n", ifp->int_name);
      if (ifp->int_flags & IFF_PASSIVE) {
        TRACE_JOB("toall: No RIP - %s is passive\n", ifp->int_name);
        continue;
      }
#ifdef sgi
      if ( !_IFF_UP_RUNNING(ifp->int_flags) ) {
#else
      if ( !(ifp->int_flags & IFF_UP) ) {
#endif
        TRACE_JOB("toall: No RIP - %s is down\n", ifp->int_name);
        continue;
      }
      if (ifp->int_flags & IFF_NORIPOUT) {
        TRACE_JOB("toall: RIP not allowed out %s\n", ifp->int_name);
        continue;
      }
      dst = (ifp->int_flags & IFF_BROADCAST) ? &ifp->int_broadaddr :
               (ifp->int_flags & IFF_POINTOPOINT) ? &ifp->int_dstaddr :
                   &ifp->int_addr;
      dst->sa_family = AF_INET;   /*  what else???? */
      flags = ((ifp->int_flags & IFF_INTERFACE) &&
               !(ifp->int_flags & IFF_POINTOPOINT)) ? MSG_DONTROUTE : 0;
      TRACE_JOB("toall: Sending RIP packet to %s, flags %d, interface %s\n",
                inet_ntoa(sock_inaddr(dst)), flags, ifp->int_name);
      (*f)(dst, flags, ifp, TRUE);
    }
  }
  for (ad = srcriplist; ad; ad = ad->next) {
    struct sockaddr_in tmpdst;

    bzero((char *)&tmpdst, sizeof(tmpdst));
    tmpdst.sin_family = AF_INET;
    tmpdst.sin_addr = ad->destnet;
    if ((ifp = if_withnet(&tmpdst)) <= (struct interface *)0) {
      syslog(LOG_ERR, "toall: Source RIP gateway %s not on same net",
                    inet_ntoa(tmpdst.sin_addr));
      TRACE_TRC("toall: Source RIP gateway %s not on same net\n",
                    inet_ntoa(tmpdst.sin_addr));
      continue;
    }
#ifdef sgi
    if (!_IFF_UP_RUNNING(ifp->int_flags) || (ifp->int_flags & IFF_NORIPOUT)) {
#else
    if (((ifp->int_flags & IFF_UP) == 0) || (ifp->int_flags & IFF_NORIPOUT)) {
#endif
      continue;
    }
    flags = ((ifp->int_flags & IFF_INTERFACE) &&
             !(ifp->int_flags & IFF_POINTOPOINT)) ? MSG_DONTROUTE : 0;
    TRACE_JOB("toall: Sending RIP packet to %s, flags %d, interface %s\n",
              inet_ntoa(sock_inaddr(&tmpdst)), flags, ifp->int_name);
    (*f)(&tmpdst, flags, ifp, TRUE);
  }
}

/*
 * Output a preformed RIP packet.
 */

/*ARGSUSED*/
sendripmsg(dst, flags, ifp, do_split_horizon)
	struct sockaddr *dst;
	int flags;
	struct interface *ifp;
	int do_split_horizon;
{
  register u_long tmp = ntohl((u_long)ripmsg->rip_nets[0].rip_metric);
  struct rt_entry *rt;
  struct netinfo *n = ripmsg->rip_nets;

  if (dst->sa_family != AF_INET) {
    return;
  }
  /*
   * Check to see if we are sending the initial RIP request to other
   * gateways.  That request has no restrictions other than whether RIP
   * is allowed on that interface or not.  This restriction is handled
   * in toall().
   */
  if (!((ntohs(n->rip_dst.sa_family) == AF_UNSPEC) &&
       (ntohl((u_long)n->rip_metric) == RIPHOPCNT_INFINITY))) {
    n->rip_dst.sa_family = ntohs(n->rip_dst.sa_family);
    rt = rt_lookup((int)INTERIOR, (struct sockaddr_in *)&n->rip_dst);
    if (rt == NULL) {
      rt = rt_lookup((int)HOSTTABLE, (struct sockaddr_in *)&n->rip_dst);
      if (rt == NULL) {
        syslog(LOG_ERR, "sendripmsg: bad route %s",
               inet_ntoa(sock_inaddr(&n->rip_dst)));
        TRACE_TRC("sendripmsg: bad route %s\n",
               inet_ntoa(sock_inaddr(&n->rip_dst)));
        return;
      }
    }
    n->rip_dst.sa_family = htons(n->rip_dst.sa_family);
    /*
     * make sure this route can be announced via this interface/proto.
     */
    if (!is_valid(rt, RTPROTO_RIP, ifp)) {
      return;
    }
    /*
     * since we are only sending out this one packet, we can add the
     * interface metric here.  Don't forget Split Horizon.
     */
    if ((rt->rt_ifp == ifp) &&
        do_split_horizon &&
        (rt->rt_fromproto & (RTPROTO_RIP|RTPROTO_DIRECT|RTPROTO_KERNEL)) &&
        ((rt->rt_state & RTS_STATIC) == 0)) {
      tmp = ntohl((u_long)n->rip_metric);
      n->rip_metric = htonl((u_long)RIPHOPCNT_INFINITY);
    } else if ((tmp = ntohl((u_long) n->rip_metric)) != RIPHOPCNT_INFINITY) {
      if ((tmp + ifp->int_metric) >= RIPHOPCNT_INFINITY) {
        n->rip_metric = htonl((u_long)RIPHOPCNT_INFINITY);
      } else {
        n->rip_metric = htonl((u_long)(tmp + ifp->int_metric));
      }
    }
  }
  (*afswitch[dst->sa_family].af_output)(rip_socket, flags,
                                           dst, sizeof (struct rip));
  TRACE_RIPOUTPUT(ifp, (struct sockaddr_in *)dst, sizeof (struct rip));
  n->rip_metric = htonl(tmp);
}

/*
 * Supply dst with the contents of the routing tables.
 * If this won't fit in one packet, chop it up into several.
 */
supply(dst, flags, ifp, do_split_horizion)
	struct sockaddr *dst;
	int flags;
	struct interface *ifp;
	int do_split_horizion;
{
  register struct rt_entry *rt;
  struct netinfo *n;
  register struct rthash *rh;
  struct rthash *base;
  int doinghost, size, iff_subnets, same_net;
  int (*output)() = afswitch[AF_INET].af_output;
  u_short metric, split_horizon;

  ripmsg->rip_cmd = RIPCMD_RESPONSE;
  ripmsg->rip_vers = RIPVERSION;
  n = ripmsg->rip_nets;
  
  iff_subnets = (ifp->int_flags & IFF_SUBNET) == IFF_SUBNET;

  for (base = hosthash, doinghost = 1; doinghost >= 0; base = nethash, doinghost--) {
    for (rh = base; rh < &base[ROUTEHASHSIZ]; rh++) {
      for (rt = rh->rt_forw; rt != (struct rt_entry *)rh; rt = rt->rt_forw) {
        /*
         * don't broadcast subnets where they shouldn't be announced.
         */
	if (!doinghost) {
	  same_net = (gd_inet_wholenetof(sock_inaddr(&rt->rt_dst)) == gd_inet_wholenetof(sock_inaddr(dst)));
	  if (rt->rt_state & RTS_SUBNET) {
	    if ( !iff_subnets ) {
	      continue;		/* Subnets not allowed out this interface */
	    }
	    if (!same_net) {
	      continue;		/* Not a subnet of this interface */
	    }
	  } else {
	    if (iff_subnets && same_net) {
	      continue;		/* Don't sent whole net via a subneted interface */
	    }
	  }
        }
        if ((ifp == rt->rt_ifp) &&
	    do_split_horizion &&
            (rt->rt_fromproto & (RTPROTO_RIP|RTPROTO_DIRECT|RTPROTO_KERNEL)) &&
            !(rt->rt_state & RTS_STATIC) ) {
#ifdef sgi
	  /* Split horizon means don't resend the information on the
	   * network from which it was received
	   */
	  continue;
#else
          split_horizon = RIPHOPCNT_INFINITY;
#endif
        } else {
          split_horizon = 0;
        }
        switch (rt->rt_proto) {
          case RTPROTO_RIP:
          case RTPROTO_HELLO:
          case RTPROTO_DIRECT:
          case RTPROTO_KERNEL:
            if (!(doinghost) && !(rt->rt_state & RTS_INTERIOR)) {
              continue;
            }
            if ( !is_valid(rt, RTPROTO_RIP, ifp) ) {
              continue;
            }
            metric = mapmetric(HELLO_TO_RIP, (u_short)rt->rt_metric) + ifp->int_metric + 1;
            break;
          case RTPROTO_REDIRECT:
            continue;
          case RTPROTO_DEFAULT:
            if (!rip_gateway) {
              continue;
            }
            if ( !is_valid(rt, RTPROTO_RIP, ifp) ) {
              continue;
            }
            metric = rip_default + ifp->int_metric;
            split_horizon = 0;
            break;
          case RTPROTO_EGP:
            if (rt->rt_as == mysystem) {
              continue;
            }
            switch (sendAS(my_aslist, rt->rt_as)) {
              case 0:	/* not valid to send */
                continue;
              case 2:	/* valid to send - announce clauses apply */
                if ( !is_valid(rt, RTPROTO_RIP, ifp) ) {
                  continue;
                }
                break;
              case 1:	/* valid to send - announce clauses do not apply */
                break;
              case -1:	/* no AS restrictions */
                continue;
            }
            metric = mapmetric(HELLO_TO_RIP, (u_short)rt->rt_metric) + ifp->int_metric;
            split_horizon = 0;
            break;
          default:
            syslog(LOG_ERR, "supply: Unknown protocol %d for net %s",
                   rt->rt_proto, inet_ntoa(sock_inaddr(&rt->rt_dst)));
            TRACE_TRC("supply: Unknown protocol %d for net %s\n",
                   rt->rt_proto, inet_ntoa(sock_inaddr(&rt->rt_dst)));
        }
        if (split_horizon) {
          metric = split_horizon;
        }
#ifdef sgi
        if (!_IFF_UP_RUNNING(rt->rt_ifp->int_flags)) {
#else
        if (!(rt->rt_ifp->int_flags & IFF_UP)) {
#endif
          metric = RIPHOPCNT_INFINITY;
        }
        if ((ifp->int_flags & IFF_RIPFIXEDMETRIC) &&
            (rt->rt_proto != RTPROTO_DEFAULT) &&
            (metric < RIPHOPCNT_INFINITY)) {
          metric = ifp->int_ripfixedmetric;
        }
        size = (char *)n - rip_packet;
        if (size > (RIPPACKETSIZE - sizeof (struct netinfo))) {
          (*output)(rip_socket, flags, dst, size);
          TRACE_RIPOUTPUT(ifp, (struct sockaddr_in *)dst, size);
          n = ripmsg->rip_nets;
        }
        n->rip_dst = rt->rt_dst;
        n->rip_dst.sa_family = htons(n->rip_dst.sa_family);
        if (metric > RIPHOPCNT_INFINITY) {
          metric = RIPHOPCNT_INFINITY;
        }
        n->rip_metric = htonl((u_long)metric);
        n++;
      }
    }
  }
  if (n != ripmsg->rip_nets) {
    size = (char *)n - rip_packet;
    (*output)(rip_socket, flags, dst, size);
    TRACE_RIPOUTPUT(ifp, (struct sockaddr_in *)dst, size);
  }
}
