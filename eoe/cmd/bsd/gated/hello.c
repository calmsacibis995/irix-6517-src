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
static char *rcsid = "$Header: /proj/irix6.5.7m/isms/eoe/cmd/bsd/gated/RCS/hello.c,v 1.2 1990/01/09 15:35:17 jleong Exp $";
#endif	not lint

/*
 *	Hello output routines were taken from Mike Petry (petry@trantor.umd.edu)
 *	Also, hello input routines were written by Bill Nesheim, Cornell
 *	CS Dept,  Currently at nesheim@think.com
 */

#include "include.h"

helloin(hellobuf)
char *hellobuf;
{
  register int dsize, index, i;
  struct hellohdr *hi;
  struct m_hdr *mh;
  struct type1pair *t1;
  struct ip *ip;
  struct sockaddr_in gateway;
  struct rt_entry *rt, *exrt;
  struct interface *ifp, *ifpc;
  int change = FALSE;
  int fromproto;
  int latched;			/* Fuzzball has this route in hold down */
  u_short hello_cksum(), cksum;
    
  ip = (struct ip *)hellobuf;
    
  /* drop the ip header	*/
  dsize = ip->ip_len  /* - (ip->ip_hl<<2)  data length here, NOT total */;
  index = ip->ip_hl << 2;
  hi = (struct hellohdr *) &hellobuf[index];
  index +=  /* sizeof (struct hellohdr) */ 10;

  TRACE_HELLOPKT(HELLO RECV, ip->ip_src, ip->ip_dst, (char *)hi, dsize, -1);

  /* ignore if not from a trusted HELLOer */
  if (trustedhelloerlist != (struct advlist *)NULL) {
    register struct advlist *ad;      /* list of trusted HELLOers */
    register int OK = 0;

    for(ad = trustedhelloerlist; ad; ad = ad->next)
      if (ip->ip_src.s_addr == ad->destnet.s_addr) {
        OK++;
        break;
      }
    if (OK == 0)
      return;
  }

  /* Do we share a net with the sender? */
  bzero((char *)&gateway, sizeof (gateway));
  gateway.sin_family = AF_INET;
  gateway.sin_addr = ip->ip_src;
  if ((ifp = if_withnet(&gateway)) == NULL) {
    TRACE_EXT("helloin: gw %s no shared net?\n", inet_ntoa(ip->ip_src));
    return;
  }
  if (ifp->int_flags & IFF_NOHELLOIN)
	return;

  /*
   * Are we talking to ourselves?
   *
   * if_ifwithaddr() handles PTP links also.  If packet came
   * from other end of a PTP, let it fall through for further
   * processing.  We shouldn't ever hear our own RIPs on a PTP link.
   */
  if (ifpc = if_ifwithaddr((struct sockaddr *)&gateway)) {
    rt_ifupdate(ifpc);
    if ((ifpc->int_flags & IFF_POINTOPOINT) == 0) {
      return;
    }
  }
  /*
   * update the interface timer on interface the packet came in on.
   */
  rt_ifupdate(ifp);

  fromproto = if_updateactivegw(ifp, gateway.sin_addr.s_addr, RTPROTO_HELLO);
  /* check the hello checksum */

  cksum = hello_cksum((u_short *) hi, dsize);
  if (cksum == hi->h_cksum) {
    
    /* message is made up of one or more sub messages */
    dsize -= /* sizeof (struct hellohdr) */ 10; 
    while (dsize > 0) {
      mh = (struct m_hdr *) &hellobuf[index];
      index += sizeof (struct m_hdr);
      dsize -= sizeof (struct m_hdr);
      switch (mh->m_type) {
        case 0:
          index += mh->m_count * sizeof (struct type0pair);
          dsize -= mh->m_count * sizeof (struct type0pair);
          /* not interested in type 0 messages */
          break;
	case 1:
          t1 = (struct type1pair *)&hellobuf[index];
          for (i = 0; i < mh->m_count; i++, t1++) {
            int delay = ntohs(t1->d1_delay);
            struct sockaddr_in dst;

            bzero((char *)&dst, sizeof (dst));
            dst.sin_family = AF_INET;
            /* dogone alignment problems! */
            bcopy((char *)&t1->d1_dst, (char *)&dst.sin_addr,
                    sizeof (struct in_addr));

            index += sizeof (struct type1pair);
            dsize -= sizeof (struct type1pair);

	    /*
	     * Fuzzballs set the top bit of the delay field when the
	     * delay jumps too high too fast, and latches the route.
	     * They unlatch the route when the metric by that route
	     * goes down some, or when the hold-down-latch period
	     * expires (in which case they start saying infinity)
	     * 
	     * The current (Dec 1987) idea is to insulate the RIP
	     * world from a Fuzzball hold down and to avoid making
	     * any routing changes during a hold down.  
	     * 
	     * If the route in hold down is not being used, the
	     * new route is ignored, even if it is more attractive.
	     * 
	     * If we are using the route, we will not update the
	     * routing table with the new metric, but continue to
	     * use the last non-hold down metric.  We will also not
	     * update the hello window.  We will however, reset the
	     * timer so that the route is not expired during hold down.
	     */
	    if ((u_short)delay & 0x8000) {	/* latched */
		delay = (int)((u_short)delay & 0x7fff);
		latched = TRUE;
	    } else {
		latched = FALSE;
	    }
	/*
	 *	Add the interface metric converted to a HELLO delay.
	 */
            delay += mapmetric(RIP_TO_HELLO, (u_short)ifp->int_metric);

            if (tracing & TR_UPDATE) {
              TRACE_HEL("HELLO: %s time delay %d %s\n",
				inet_ntoa(dst.sin_addr), delay,
				latched ? "latched" : "");
            }

            /* check for internal route */
            rt = rt_lookup((int)INTERIOR, &dst);
            if (rt == NULL) {	/* new route */
              struct rt_entry rttmp;

              if (delay >= DELAY_INFINITY)
                continue;
              /*
               * If the route is in Fuzzball hold down, ignore
               * it.  We don't want to use it until it stabilizes.
               */
              if (latched) {
                continue;
              }
              /*  is this new net acceptable? */
              bzero((char *)&rttmp, sizeof(rttmp));
              rttmp.rt_listenlist = control_lookup(RT_NOLISTEN, &dst);
              rttmp.rt_srclisten = control_lookup(RT_SRCLISTEN, &dst);
              if (is_valid_in(&rttmp, RTPROTO_HELLO, ifp, &gateway) == 0) {
                continue;
              }
              /*
               *	Check for and ignore martain nets
               */
              if (is_martian(dst.sin_addr)) {
                char badgate[16];
                (void) strcpy(badgate, inet_ntoa(gateway.sin_addr));
                TRACE_EXT("helloin: ignoring invalid net %s from %s at %s", inet_ntoa(dst.sin_addr), badgate, strtime);
                continue;
              }
              /* 
               * delete old external route(s), if any,
               * and add to internal table.
               */
              while (exrt = rt_lookup((int)EXTERIOR, &dst)) {
                rt_delete(exrt, KERNEL_INTR);
              }
              (void) rt_add((int)INTERIOR,(struct sockaddr *)&dst,(struct sockaddr *)&gateway,delay,0,RTPROTO_HELLO, fromproto, 0, 0);
              change = TRUE;
            } else {	/* update existing route */
              if ((rt->rt_flags & RTF_GATEWAY) == 0) {
                continue;	/* don't mess with non-gw's */
              }
              if (rt->rt_state & (RTS_INTERFACE | RTS_STATIC)) {
                continue;
              }
              if (is_valid_in(rt, RTPROTO_HELLO, ifp, &gateway) == 0) {
                continue;
              }
              if (exrt = rt_lookup((int)EXTERIOR, &dst)) {
                do {
                  rt_delete(exrt, KERNEL_INTR);
                } while (exrt = rt_lookup((int)EXTERIOR, &dst));
              }
              if (delay >= DELAY_INFINITY) {
                /* destination now unreachable */
                if (equal (&rt->rt_router, &gateway)) {
		  if ( latched ) {
                    TRACE_ACTION(LATCHED, rt);
                    change = TRUE;
                    rt->rt_timer = 0;
                  } else {
                    if (rt->rt_metric < DELAY_INFINITY) {
                      (void) rt_unreach(rt);
                      change = TRUE;
                    }
                    add_win(&rt->rt_hwindow, DELAY_INFINITY);
                  }
                }
                continue;
              }
              if (equal (&rt->rt_router, &gateway)) {
		if (latched) {
                  TRACE_ACTION(LATCHED, rt);
                  change = TRUE;
		} else {
                  if (METRIC_DIFF(rt->rt_metric,delay) >= HELLO_HYST(rt->rt_metric)) {
                    if (rt_change(rt, (struct sockaddr *)&gateway, delay, RTPROTO_HELLO, fromproto, 0, 0)) {
                      change = TRUE;
                    }
		  }
                  add_win(&rt->rt_hwindow, delay);
		}
                rt->rt_timer = 0;
              } else {
                /*
                 * if the current metric is INFINITY, then we
                 * will only listen to our current gateway.
                 * yes, I know this is a hold down. Yes, I
                 * know it's yucky.  If nothing is heard from our
                 * gateway within 120 seconds, the route is
                 * deleted.
                 */
                if (rt->rt_metric >= DELAY_INFINITY)
                  continue;
		/*
		 * If the route is in Fuzzball hold down, ignore
		 * it.  We don't want to use it until it stabilizes.
		 */
		if (latched)
		  continue;
                /*
                 * use the new gateway.
                 */
                if (((delay < rt->rt_metric) &&
                    (METRIC_DIFF(delay,rt->rt_metric) >= HELLO_HYST(rt->rt_metric))) ||
                    (rt->rt_timer > MINPOLLINT &&
                    !(rt->rt_state & RTS_CHANGED))) {
                   if (rt_change(rt, (struct sockaddr *)&gateway, delay, RTPROTO_HELLO, fromproto, 0, 0))
                     change = TRUE;
                   add_win(&rt->rt_hwindow, delay);
                   rt->rt_timer = 0;
                }
              }
            }
          } /* for each advertized net */

          break;
        default:
          syslog(LOG_ERR, "helloin: invalid type %d\n", mh->m_type);
      } /* switch (mh->m_type) */
    } /* while dsize */
  }	/* checksum */	
    
  if ( (change && (tracing & TR_RT)) || (tracing & TR_HELLO) ) {
    printf("hello_update: above %d routes from %s updated %s\n",
                   mh->m_count,inet_ntoa(ip->ip_src), strtime);
  }
} 

u_short
hello_cksum(cksumaddr, len)
u_short *cksumaddr;
int len;
{
  register int nleft = len - 2 ;
  register u_short *w = cksumaddr + 1 ;
  register int sum = 0;

  while (nleft > 0) {
    sum += *w++ ;
    if (sum & 0x10000) {
      sum &= 0xffff ;
      sum++ ;
    }
    nleft -= 2;
  }

  if (nleft == -1)
  sum += *(u_char *)w;

  return (~sum);
}

hellojob()
{
  struct interface *ifp;
  struct sockaddr_in *sin,*din;
  register struct advlist *ad;

  if (!(hello_pointopoint)) {
    for (ifp = ifnet; ifp != 0; ifp = ifp->int_next) {
      TRACE_JOB("hellojob: Checking interface %s\n", ifp->int_name);
      if (ifp->int_flags & IFF_PASSIVE) {
        TRACE_JOB("hellojob: No HELLO - %s is passive\n", ifp->int_name);
        continue;
      }
#ifdef sgi
      if ( !_IFF_UP_RUNNING(ifp->int_flags) ) {
#else
      if ( !(ifp->int_flags & IFF_UP) ) {
#endif
        TRACE_JOB("hellojob: No HELLO - %s is down\n", ifp->int_name);
        continue;
      }
      if (ifp->int_flags & IFF_NOHELLOOUT) {
        TRACE_JOB("hellojob: HELLO not allowed out %s\n", ifp->int_name);
        continue;
      }
      din = (ifp->int_flags & IFF_BROADCAST) ?
                (struct sockaddr_in *)&ifp->int_broadaddr :
                (ifp->int_flags & IFF_POINTOPOINT) ?
                    (struct sockaddr_in *)&ifp->int_dstaddr :
                    (struct sockaddr_in *)&ifp->int_addr;
      sin = (struct sockaddr_in *)&ifp->int_addr;
      TRACE_JOB("hellojob: Sending HELLO packet to %s, interface %s\n",
                inet_ntoa(din->sin_addr), ifp->int_name);
      hellotsend(din->sin_addr, sin->sin_addr, ifp);
    }
  }
  for (ad = srchellolist; ad; ad = ad->next) {
    struct sockaddr_in tmpdst;

    bzero((char *)&tmpdst, sizeof(tmpdst));
    tmpdst.sin_family = AF_INET;
    tmpdst.sin_addr = ad->destnet;
    if ((ifp = if_withnet(&tmpdst)) <= (struct interface *)0) {
      TRACE_INT("Source HELLO gateway %s not on same net\n",
                     inet_ntoa(tmpdst.sin_addr));
      continue;
    }
#ifdef sgi
    if ((!_IFF_UP_RUNNING(ifp->int_flags)) ||
	(ifp->int_flags & IFF_NOHELLOOUT))
#else
    if (((ifp->int_flags & IFF_UP) == 0) ||
        (ifp->int_flags & IFF_NOHELLOOUT))
#endif
      continue;
    sin = (struct sockaddr_in *)&ifp->int_addr;
    TRACE_JOB("hellojob: Sending HELLO packet to %s, interface %s\n",
              inet_ntoa(sock_inaddr(&tmpdst)), ifp->int_name);
    hellotsend(tmpdst.sin_addr, sin->sin_addr, ifp);
  }
}

hellotsend( dst, src, ifp)
	struct in_addr dst, src;  /* dest. and source internet address */
	struct interface *ifp;
{
  struct rt_entry *rt;
  struct rthash *rh;
  struct type1pair *t1;
  struct sockaddr_in sdst;
  struct m_hdr *mh;
  u_short hello_cksum();
  u_short delay, split_horizon;
  char hpkt[HELLOMAXPACKETSIZE];
  int index, save_index;
  
  /*
   * Advertise only nets that are reachable by an interface other then
   * the one we are going to use (ifp).  if that is the case, announce
   * with a metric of INFINITY.
   */

  index = /* sizeof (struct hellohdr) */ 10;

  mh = (struct m_hdr *) &hpkt[index];
  save_index = index += sizeof (struct m_hdr);
  mh->m_type = 1;
  mh->m_count = 0;

  for (rh = nethash; rh < &nethash[ROUTEHASHSIZ]; rh++) {
    for (rt = rh->rt_forw; rt != (struct rt_entry *)rh; rt = rt->rt_forw ) {
      if (rt->rt_proto & RTPROTO_REDIRECT) {
        continue;
      }
      if (rt->rt_state & RTS_SUBNET) {
        continue;
      }
      if ((ifp == rt->rt_ifp) &&
          (rt->rt_fromproto & (RTPROTO_HELLO|RTPROTO_DIRECT|RTPROTO_KERNEL)) &&
          !(rt->rt_state & RTS_STATIC) ) {
        split_horizon = DELAY_INFINITY;
      } else {
        split_horizon = 0;
      }
      switch (rt->rt_proto) {
        case RTPROTO_RIP:
        case RTPROTO_HELLO:
        case RTPROTO_DIRECT:
        case RTPROTO_KERNEL:
          if (!is_valid(rt, RTPROTO_HELLO, ifp)) {
            continue;
          }
          if (!(rt->rt_state & RTS_INTERIOR)) {
            continue;
          }
          delay = rt->rt_metric + mapmetric(RIP_TO_HELLO, (u_short)ifp->int_metric) + 100;
          break;
        case RTPROTO_REDIRECT:
          continue;
        case RTPROTO_DEFAULT:
          if (!is_valid(rt, RTPROTO_HELLO, ifp)) {
            continue;
          }
          if (!hello_gateway) {
            continue;
          }
          delay = hello_default + mapmetric(RIP_TO_HELLO, (u_short)ifp->int_metric);
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
              if (!is_valid(rt, RTPROTO_HELLO, ifp)) {
                continue;
              }
              break;
            case 1:	/* valid to send */
              break;
            case -1:	/* no AS restrictions */
              continue;
          }
          delay = rt->rt_metric + mapmetric(RIP_TO_HELLO, (u_short)ifp->int_metric);
          split_horizon = 0;
          break;
        default:
          syslog(LOG_ERR, "supply: Unknown protocol %d for net %s",
                 rt->rt_proto, inet_ntoa(sock_inaddr(&rt->rt_dst)));
          TRACE_TRC("supply: Unknown protocol %d for net %s\n",
                 rt->rt_proto, inet_ntoa(sock_inaddr(&rt->rt_dst)));
      }
      if (split_horizon) {
        delay = split_horizon;
      }
#ifdef sgi
      if (!_IFF_UP_RUNNING(rt->rt_ifp->int_flags)) {
#else
      if (!(rt->rt_ifp->int_flags & IFF_UP)) {
#endif
        delay = DELAY_INFINITY;
      }
      if ((ifp->int_flags & IFF_HELLOFIXEDMETRIC) &&
          (rt->rt_proto != RTPROTO_DEFAULT) &&
          (delay < DELAY_INFINITY)) {
        delay = ifp->int_hellofixedmetric;
      }
      if ( index + sizeof(struct type1pair) >= HELLOMAXPACKETSIZE ) {
#ifdef sgi
        hello_send(dst, src, (u_char *)hpkt, index, (int) mh->m_count, ifp);
#else
        hello_send(dst, src, (u_char *)hpkt, index, (int) mh->m_count);
#endif
        index = save_index;	/* Reset to start of network info */
	mh->m_count = 0;
      }
      t1 = (struct type1pair *)&hpkt[index];
      t1->d1_delay = htons(delay);
      if (tracing & TR_UPDATE) {
        TRACE_HEL("HELLO_SND: net %s time delay %d\n",
                inet_ntoa( sock_inaddr(&rt->rt_dst)), ntohs(t1->d1_delay));
      }
      t1->d1_offset = htons(0);		/* should be signed clock offset */
      sdst.sin_addr.s_addr =
                         htonl((u_long)gd_inet_netof(sock_inaddr(&rt->rt_dst)));
      bcopy((char *)&sdst.sin_addr,(char *)&t1->d1_dst,sizeof(struct in_addr));
      index += sizeof (struct type1pair);
      mh->m_count++;
    }
  }
#ifdef sgi
  hello_send(dst, src, (u_char *)hpkt, index, (int) mh->m_count, ifp);
#else
  hello_send(dst, src, (u_char *)hpkt, index, (int) mh->m_count);
#endif
}

/*
 * hello_send():
 * 	Fill in the hello header and checksum, then send the packet.
 */

#ifdef sgi
hello_send(dst, src, hello, length, n_nets, ifp)
#else
hello_send(dst, src, hello, length, n_nets)
#endif
     struct in_addr dst, src;  /* destination and source internet address */
     u_char	*hello;		/* pointer to start of hello packet */
     int	length;		/* length in octets of hello packet */
     int	n_nets;		/* number of nets in this update for debugging */
     struct interface *ifp;	/* interface to send hello */
{
  struct sockaddr_in hellosaddr;
  struct hellohdr *hi;
#ifndef sgi
  struct interface   *ifp;
#endif
  struct tm *gmt;
  struct timeval tp;
  int	error = FALSE;
  int	sendhello = TRUE;		/*  for debugging */

  hi = (struct hellohdr *) hello;
    
  (void) gettimeofday(&tp, (struct timezone *)0);
  gmt = (struct tm *) gmtime(&tp.tv_sec);
    
  /*
   * set the date field in the HELLO header.  Be very careful here as
   * the last two bits (14&15) should be set so the Fuzzware doesn't use
   * this packet to synchronize its Master Clock.  Using bitwise OR's
   * instead of addition just to be safe when dealing with h_date which
   * is an unsigned short.
   */
  bzero((char *)&hi->h_date, sizeof(hi->h_date));
#ifdef notdef
  hi->h_date = htons((u_short)(((gmt->tm_year - 72) & 037) |
                     ((gmt->tm_mday & 037) << 5) |
                     (((gmt->tm_mon + 1) & 037) << 10) |
                     ((1 & 037) << 15)));
#else notdef
  hi->h_date = htons((u_short)0xC000);
#endif notdef
  /*
   * milliseconds since midnight UT of current day
   */
  hi->h_time = htonl((u_long)((gmt->tm_sec + gmt->tm_min * 60 + gmt->tm_hour
                        * 3600) * 1000 + tp.tv_usec / 1000));
  /*
   * 16 bit field used in rt calculation,  0 for ethernets
   */
  hi->h_tstp = 0;
  hi->h_cksum = 0;
  hi->h_cksum = hello_cksum((u_short *)hello, length);

  bzero((char *)&hellosaddr, sizeof (hellosaddr));
  hellosaddr.sin_family = AF_INET;

#ifndef sgi
  /* find interface to send packet on */
  hellosaddr.sin_addr = src;
  ifp = if_withnet(&hellosaddr);
#endif
  if (ifp == NULL) {
    syslog(LOG_ERR, "hello_send: no interface for source address %s\n",
              inet_ntoa( src));
    error = TRUE;
  } else {
    extern int hello_socket;

    hellosaddr.sin_addr = dst;
    if (sendhello) {
      register int flags;

      flags = ((ifp->int_flags & IFF_INTERFACE) &&
               !(ifp->int_flags & IFF_POINTOPOINT)) ? MSG_DONTROUTE : 0;
      if (sendto(hello_socket, (char *)hello, length, flags,
                     (struct sockaddr *)&hellosaddr, sizeof(hellosaddr)) < 0) {
        (void) sprintf(err_message, "hello_send: sendto() error sending %d bytes to %s",
          length, inet_ntoa(dst));
        p_error(err_message);
        error = TRUE;
      }
    }
  }
  if (!error) {
    TRACE_HELLOPKT(HELLO SENT, src, dst, (char *)hello, length, n_nets);
  } else {
    TRACE_HELLOPKT(HELLO *NOT* SENT, src, dst, (char *)hello, length, n_nets);
  }
  return;
}
