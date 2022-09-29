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
static char *rcsid = "$Header: /proj/irix6.5.7m/isms/eoe/cmd/bsd/gated/RCS/egp.c,v 1.2 1989/11/27 13:54:47 jleong Exp $";
#endif	not lint

#include        "include.h"

/*
 * The major EGP state changes and initializations occur when the
 * gateway first comes up, init_egp() (in init.c), when a neighbor goes into
 * state UP, egpstngh(), and when a neighbor goes into state ACQUISITION,
 * egpstunacq().
 */

/*
 * egpjob(): periodic EGP functions and packet sending.
 *
 * egpjob() is called periodically by timeout() after each timer interrupt. It
 * checks each neighbor's state table in turn and decides what action to take
 * based on the state and time elapsed. It initiates the sending of 
 * Acquisition Request, Hello and Poll commands and the retransmission of 
 * Acquisition Request, Poll and Cease commands. Appropriate functions are 
 * called to format each of these  messages.  In the UP state it 
 * determines the the current reachability of the peer. After 480 seconds have
 * elapsed it calls egpchkcmd() to check whether peer commands are being 
 * received at an excess rate.
 *
 * At most maxacq neighbors will be acquired. The number of new acq. requests
 * sent this interrupt does not exceed the number of neighbors yet to be
 * acquired. Any number of retransmitted acq. requests may be sent to 
 * non-responding neighbors.
 */

egpjob()
{
  register egpng *ngp;
  register int   shiftreg;
  static long    chkcmdtime = 0;
  int   n_new_acqsnt = 0,         /* # new acq. requests sent this interrupt */
                       i,
                       nresponses;

  /* commence periodic processing for each egp neighbor */

  for (ngp = egpngh; ngp != NULL; ngp = ngp->ng_next) {
    switch(ngp->ng_state) {
      case NGS_ACQUISITION:
        if (terminate)
          break;
        if (n_new_acqsnt >= (maxacq - n_acquired))
          break;
        if (gatedtime < ngp->ng_htime)
          break;
        ngp->ng_state = NGS_DOWN;
        ngp->ng_htime = gatedtime + ngp->ng_hint;
        egpsacq(ngp->ng_addr, ngp->ng_myaddr,NAREQ, ACTIVE, ngp->ng_sid, ngp);
        n_new_acqsnt++;
        break;
      case NGS_DOWN:
        if (gatedtime < ngp->ng_htime)
          break;
        if (++ngp->ng_retry >= NACQ_RETRY) {
          ngp->ng_hint = LONGACQINT;
          egpstime();
          ngp->ng_retry = 0;
        }
        ngp->ng_htime = gatedtime + ngp->ng_hint;
        egpsacq(ngp->ng_addr, ngp->ng_myaddr, NAREQ, ACTIVE, ngp->ng_sid, ngp);
        break;
      case NGS_UP:
        /* determine reachability */
        if (gatedtime >= ngp->ng_htime) {
          shiftreg = ngp->ng_responses;
          nresponses = 0;
          for (i = 0; i < NCOMMANDS; i++) {     /* count responses */
             if (shiftreg & 1)
               nresponses++;
             shiftreg >>= 1;
          }
          if (ngp->ng_reach & NG_DOWN) {
            if (nresponses > NRESPONSES) {      /* down -> up */
	     syslog(LOG_WARNING, "egpjob: EGP neighbor %s state change DOWN -> UP, %d/%d responses received",
		    inet_ntoa(ngp->ng_addr), nresponses, NCOMMANDS);
	     TRACE_EXT("egpjob: EGP neighbor %s state change DOWN -> UP, %d/%d responses received at %s",
		    inet_ntoa(ngp->ng_addr), nresponses, NCOMMANDS, strtime);
              ngp->ng_reach &= ~NG_DOWN;
              ngp->ng_retry = 0;
            } else {
              /* down -> down */
              if (++ngp->ng_retry >= NUNREACH) {
                egpcease(ngp, UNSPEC);
                break;
              }
            }
          } else {
            if (nresponses < NRESPONSES) {  /* up -> down */
	     syslog(LOG_WARNING, "egpjob: EGP neighbor %s state change UP -> DOWN, %d/%d responses received",
		    inet_ntoa(ngp->ng_addr), nresponses, NCOMMANDS);
	     TRACE_EXT("egpjob: EGP neighbor %s state change UP -> DOWN, %d/%d responses received at %s",
		    inet_ntoa(ngp->ng_addr), nresponses, NCOMMANDS, strtime);
              ngp->ng_reach |= NG_DOWN;
#ifndef	NSS
              if (maxacq < nneigh ) {
                egpcease(ngp, UNSPEC);
                break;
              } else {
                ngp->ng_retry = 1;
              }
#else	NSS
              egpcease(ngp, UNSPEC);
#endif	NSS
            }
          }  
        }
        /*
         * Send NR poll
         */
        if (gatedtime >= ngp->ng_stime) {       /* time for poll */
          if (ngp->ng_reach == BOTH_UP) {
            if (ngp->ng_snpoll < NPOLL) {
              if (ngp->ng_snpoll++ == 0) {       /* not repoll */
                ngp->ng_sid++;
                ngp->ng_runsol = 0;
                ngp->ng_noupdate++;
              }
              /* repoll interval is hello int */
              ngp->ng_stime = gatedtime + ngp->ng_hint;
              ngp->ng_responses <<= 1;
              if (ngp->ng_noupdate <= MAXNOUPDATE)
                egpspoll(ngp);
              else {
                TRACE_EXT("egpjob: no valid NR update");
                TRACE_EXT(" for %d successive new poll", MAXNOUPDATE);
                TRACE_EXT(" id's\n");
                egpcease(ngp, UNSPEC);
              }
              break;
            }
            else {          /* no more repolls */
              ngp->ng_stime += ngp->ng_spint - ngp->ng_hint;
              ngp->ng_snpoll = 0;
            }
          }
          else
            ngp->ng_snpoll = 0; /* reach. maybe changed when poll outstanding */
        }
        /*
         * Send hello
         */
        if (gatedtime >= ngp->ng_htime) {
          ngp->ng_htime = gatedtime + ngp->ng_hint;
          ngp->ng_responses <<= 1;
          egpshello(ngp, HELLO, ngp->ng_sid);
        }
        break;
      case NGS_CEASE:
        if (gatedtime < ngp->ng_htime)
          break;
        if (++ngp->ng_retry > NCEASE_RETRY) {
          egpstunacq(ngp);
          break;
        }
        ngp->ng_htime = gatedtime + ngp->ng_hint;
        egpsacq(ngp->ng_addr,ngp->ng_myaddr,NACEASE,ngp->ng_status,ngp->ng_sid, ngp);
        break;
      default:
        break;
    }
  }
  if (gatedtime >= chkcmdtime) {
    if (chkcmdtime)                 /* ignore initialization */
      egpchkcmd((u_long)gatedtime - chkcmdtime);
    chkcmdtime = gatedtime + CHKCMDTIME;
  }
  return;
}

/*
 * egpchkcmd() check command reception rate for neighbors and if too high
 * cease and go to state BAD so no further peering.
 */

egpchkcmd( extratime)
        u_long  extratime;
{
  reg egpng *ngp;
  u_long  cmdtime;                /* time since last check # commands recvd */
  u_long  nmaxcmd;                /* max # commands allowed */

  cmdtime = extratime + CHKCMDTIME;      /* compute max. allowed cmds */
  nmaxcmd = ((NMAXCMD * cmdtime) / CHKCMDTIME) + 1;

  /*
   * For each neighbor check if too many cmds recvd
   */
  for (ngp = egpngh; ngp != NULL; ngp = ngp->ng_next) {
    if (ngp->ng_rcmd > nmaxcmd) {
      ngp->ng_flags |= NG_BAD;
      TRACE_EXT("egpjob: recv too many commands (%d) in ", ngp->ng_rcmd);
      TRACE_EXT("%d sec from %s\n", cmdtime, inet_ntoa(ngp->ng_addr));
      egpcease(ngp, PARAMPROB);
    }
    ngp->ng_rcmd = 0;
  }
}

/*
 * egpin() handles received EGP packets. 
 *
 * It first checks for fragmented packets, valid EGP checksum, version, and
 * length. If any of these are in error the packet is discarded. If the source
 * and destination addresses match those expected in the state tables the
 * appropriate function is called according to the EGP message type. Otherwise
 * the appropriate Refuse, Cease-ack, Error or Cease message is returned.
 * Received Error messages are merely logged with no other action taken.
 */

egpin(pkt)
        char *pkt;
{
  register     struct  ip      *ip = (struct ip *)pkt;
  register     egpng           *ngp;
  register     egpkt           *egp;
  int          iphlen, egplen;
#ifndef	NSS
  struct sockaddr_in gateway;
  struct interface *ifp;
#endif	NSS

  if (ip->ip_off & ~IP_DF) {
    TRACE_EXT("egpin: recv fragmanted pkt from %s\n", inet_ntoa(ip->ip_src));
    egp_stats.inerrors++;
    return;
  }
  iphlen = ip->ip_hl;
  iphlen <<= 2;
  egp = (egpkt *) (pkt + iphlen);
  egplen = ip->ip_len;

#ifndef	NSS
  bzero((char *)&gateway, sizeof (gateway));
  gateway.sin_family = AF_INET;
  gateway.sin_addr = ip->ip_dst;
  /*
   * Locate interface that this packet was received from
   */
  ifp = if_withnet(&gateway);
  if (ifp == NULL) {
    TRACE_EXT("egpin: gw %s no interface?\n", inet_ntoa(ip->ip_src));
    syslog(LOG_ERR, "egpin: gw %s no interface?", inet_ntoa(ip->ip_src));
    egp_stats.inerrors++;
    return;
  }
  /*
   * Update interface timer for interface this packet came in on
   */
  rt_ifupdate(ifp);
#endif	NSS

  if (egp_cksum((char *)egp, egplen) != 0) {
    TRACE_EXT("egpin: bad EGP checksum from %s at %s", inet_ntoa(ip->ip_src), strtime);
    syslog(LOG_WARNING, "egpin: bad EGP checksum from %s", inet_ntoa(ip->ip_src));
    egp_stats.inerrors++;
    return;
  }
  if (egplen < EGPLEN) {
    syslog(LOG_WARNING, "egpin: bad pkt length %d from %s", egplen, inet_ntoa(ip->ip_src));
    TRACE_EXT("egpin: bad pkt length %d from %s at %s", egplen, inet_ntoa( ip->ip_src), strtime);
    egp_stats.inerrors++;
    return;
  }
  egprid_h = ntohs(egp->egp_id);  /* save sequence number in host byte order */

    egp_stats.inmsgs++;
  /*
   * check if in legitimate neighbor list
   */
  for (ngp=egpngh; ngp != NULL; ngp = ngp->ng_next) {    /* all legit neigh */
    if (ngp->ng_state == NGS_IDLE)
      continue;
    if (ngp->ng_addr.s_addr == ip->ip_src.s_addr) {      /* legit neigh */
      if (ngp->ng_myaddr.s_addr != ip->ip_dst.s_addr && egp->egp_type != EGPERR) {
        egpserr(ip->ip_src, ip->ip_dst, egp, egplen, EUNSPEC, (egpng *)0, "invalid destination address");
        egp_stats.inerrors++;
        egp_stats.inmsgs--;
        return;
      }
      if (egp->egp_ver != EGPVER) {
        int reason = -1;

        if (egp->egp_type == EGPERR) {
          reason = ntohs(((struct egperr *) egp)->ee_rsn);
        }
        if (reason != EUVERSION) {
          egpserr(ip->ip_src, ip->ip_dst, egp, egplen, EUVERSION, ngp, "unsupported Version");
          egp_stats.inerrors++;
          egp_stats.inmsgs--;
        }
        ngp->ng_flags |= NG_BAD;
        egpstunacq(ngp);
        return;
      }
      switch (egp->egp_type) {
        case EGPACQ:
          egpacq(ngp, egp, egplen);
          return;
        case EGPHELLO:
          egphello(ngp, egp);
          return;
        case EGPNR:
          egpnr(ngp, egp, egplen);
          return;
        case EGPPOLL:
          egppoll(ngp, egp);
          return;
        case EGPERR:
          if (ngp->ng_state == NGS_UP) {
	    syslog(LOG_WARNING, "egpin: error packet (reason %s) from EGP neighbor %s",
	                        egp_reasons[((struct egperr *) egp)->ee_rsn], inet_ntoa(ngp->ng_addr));
	    TRACE_EXT("egping: error packet (reason %) from EGP neighbor %s at %s",
	                        egp_reasons[((struct egperr *) egp)->ee_rsn], inet_ntoa(ngp->ng_addr), strtime);
            if (egp->egp_status == DOWN) {
		syslog(LOG_WARNING, "egpin: EGP neighbor %s thinks we are down", inet_ntoa(ngp->ng_addr));
		TRACE_EXT("egpin: EGP neighbor %s thinks we are down at %s", inet_ntoa(ngp->ng_addr), strtime);
              ngp->ng_reach |= ME_DOWN;
            } else {
              ngp->ng_reach &= ~ME_DOWN;
	    }
            ngp->ng_responses |= 1;
          }
          break;
        default:
          egpserr(ip->ip_src, ip->ip_dst, egp, egplen, EBADHEAD, (egpng *)0, "invalid Type field");
          return;
      }                               /* end switch type */
      break;
    }                                 /* end if legit */
  }                                   /* end for all legit neigh */
  if (egp->egp_ver != EGPVER) {
    int reason = -1;

    if (egp->egp_type == EGPERR) {
      reason = ntohs(((struct egperr *) egp)->ee_rsn);
    }
    if (reason != EUVERSION) {
      egpserr(ip->ip_src, ip->ip_dst, egp, egplen, EUVERSION, (egpng *)0, "unsupported Version");
      egp_stats.inerrors++;
      egp_stats.inmsgs--;
    }
    return;
  }
  switch (egp->egp_type) {            /* May be legit. neighbor */
    case EGPERR:
      if (tracing & TR_EXT) {
        if (tracing & TR_EGP) {
          printf("egpin: recv above error packet\n");
        } else {
          traceegp("egpin: recv error pkt: ", ip->ip_src,ip->ip_dst,egp,egplen);
        }
      }
      break;
    case EGPACQ:        /* Not legit. neighbor */
      TRACE_EXT("egpin: ACQUIRE packet from illegit neighbor %s\n", inet_ntoa(ip->ip_src));
      syslog(LOG_ERR, "egpin: ACQUIRE packet from illegit neighbor %s at %s", inet_ntoa(ip->ip_src), strtime);
      egp_stats.inerrors++;
      egp_stats.inmsgs--;
      switch (egp->egp_code) {
        case NAREQ:
          egpsacq(ip->ip_src, ip->ip_dst, NAREFUS, ADMINPROHIB, egprid_h, ngp);
          break;
        case NACEASE:
          egpsacq(ip->ip_src, ip->ip_dst, NACACK, egp->egp_status, egprid_h, ngp);
          break;
        case NACACK:
          break;
        case NACONF:
        default:
          egpsacq(ip->ip_src, ip->ip_dst, NACEASE, PROTOVIOL, egprid_h, ngp);
          break;
      }
      break;
    case EGPNR:
    case EGPHELLO:
    case EGPPOLL:
    default:
      egpsacq(ip->ip_src, ip->ip_dst, NACEASE, PROTOVIOL, egprid_h, ngp);
      egp_stats.inerrors++;
      egp_stats.inmsgs--;
      break;
  }                                  /* end switch type */
  return;
}

/*
 * egpacq() handles received Neighbor Acquisition messages: Request, Confirm,
 * Refuse, Cease and Cease-ack. 
 *
 * It calls egpsetint() to validate and set Hello and Poll intervals, 
 * egpstngh() to initialize state tables when the state changes to ACQUIRED,
 * and egpstunacq() when the state changes to ACQUISITION. The last two
 * functions call egpstime() to recompute the interrupt timer interval.
 */

egpacq(ngp, egp, egplen)
	register     egpng   *ngp;
	register     egpkt   *egp;
        int     egplen;
{
  u_short error = 0;

  switch (egp->egp_code) {
    case NAREQ:                     /* Neighbor acquisition request */
      ngp->ng_rcmd++;
      if (egplen < sizeof(struct egpacq)) {
        error = 1;
        break;
      }
      if ( (ngp->ng_flags & NG_ASIN) && (ngp->ng_asin != htons(egp->egp_system)) ) {
        TRACE_EXT("egpacq: neighbor %s specified AS %d, we expected %d at %s",
          inet_ntoa(ngp->ng_addr), htons(egp->egp_system), ngp->ng_asin, strtime);
        syslog(LOG_ERR, "egpacq: neighbor %s specified AS %d, we expected %d",
          inet_ntoa(ngp->ng_addr), htons(egp->egp_system), ngp->ng_asin);
        egpsacq(ngp->ng_addr, ngp->ng_myaddr, NAREFUS, ADMINPROHIB, egprid_h, ngp);
      } else if (ngp->ng_state == NGS_CEASE) {
          egpsacq(ngp->ng_addr, ngp->ng_myaddr, NACEASE, ngp->ng_status,egprid_h, ngp);
      } else if (terminate) {
          egpsacq(ngp->ng_addr, ngp->ng_myaddr, NAREFUS, GODOWN, egprid_h, ngp);
      } else if (ngp->ng_state != NGS_UP && n_acquired == maxacq) {
          egpsacq(ngp->ng_addr, ngp->ng_myaddr, NAREFUS, NORESOURCE, egprid_h, ngp);
      } else if (error = egpsetint(ngp, egp)) {       /* intervals too big */
          egpsacq(ngp->ng_addr, ngp->ng_myaddr, NAREFUS, PARAMPROB, egprid_h, ngp);
      } else {
        egpstngh(ngp, egp);
        ngp->ng_rid = egprid_h;
        egpsacq(ngp->ng_addr, ngp->ng_myaddr, NACONF, ACTIVE, egprid_h, ngp);
      }
      break;
    case NACONF:                    /* Neighbor acq. confirm */
      if (ngp->ng_state == NGS_DOWN && egprid_h == ngp->ng_sid && egplen >= sizeof(struct egpacq)) {
        if ( (ngp->ng_flags & NG_ASIN) && (ngp->ng_asin != htons(egp->egp_system)) ) {
          TRACE_EXT("egpacq: neighbor %s specified AS %d, we expected %d at %s",
            inet_ntoa(ngp->ng_addr), htons(egp->egp_system), ngp->ng_asin, strtime);
          syslog(LOG_ERR, "egpacq: neighbor %s specified AS %d, we expected %d",
            inet_ntoa(ngp->ng_addr), htons(egp->egp_system), ngp->ng_asin);
          ngp->ng_flags |= NG_BAD;              	/* delay acq retry */
          egpcease(ngp, ADMINPROHIB);
        } else if (error = egpsetint(ngp, egp)) {	/* intervals too big */
          ngp->ng_flags |= NG_BAD;              	/* delay acq retry */
          egpcease(ngp, PARAMPROB);
        } else {
          egpstngh(ngp, egp);
          ngp->ng_rid = egprid_h;
        }
      } else {
        if (ngp->ng_state == NGS_ACQUISITION) {
          egpsacq(ngp->ng_addr, ngp->ng_myaddr, NACEASE, PROTOVIOL, egprid_h, ngp);
        }
        error = 1;
      }
      break;
    case NAREFUS:                   /* Neighbor acq. refuse */
      if (ngp->ng_state == NGS_DOWN && egprid_h == ngp->ng_sid) {
        syslog(LOG_WARNING, "egpacq: Neighbor acquistion refuse from EGP neighbor %s", inet_ntoa(ngp->ng_addr));
        TRACE_EXT("egpacq: Neighbor acquistion refuse from EGP neighbor %s at %s", inet_ntoa(ngp->ng_addr), strtime);
        egpstunacq(ngp);
      } else {
        error = 1;
      }
      break;
    case NACEASE:                   /* Neighbor acq. cease */
      ngp->ng_rcmd++;
      if (ngp->ng_state == NGS_UP) {
        syslog(LOG_WARNING, "egpacq cease from EGP neighbor %s, status %s", inet_ntoa(ngp->ng_addr), egp_acq_status[egp->egp_status]);
        TRACE_EXT("egpacq: cease from EGP neighbor %s, status %s at %s", inet_ntoa(ngp->ng_addr), egp_acq_status[egp->egp_status], strtime);
      }
      egpstunacq(ngp);
      egpsacq(ngp->ng_addr, ngp->ng_myaddr, NACACK, egp->egp_status, egprid_h, ngp);
      return;
    case NACACK:                    /* Neighbor acq. cease ack */
      if (ngp->ng_state == NGS_CEASE && egprid_h == ngp->ng_sid) {
        syslog(LOG_WARNING, "egpacq cease-ack from EGP neighbor %s", inet_ntoa(ngp->ng_addr));
        TRACE_EXT("egpacq: cease-ack from EGP neighbor %s at %s", inet_ntoa(ngp->ng_addr), strtime);
        egpstunacq(ngp);
      } else {
        /*
         * in unacq. state cease may be sent as response to invalid
         * packets so may get valid ceaseack
         */
        if (ngp->ng_state != NGS_ACQUISITION) {
          error = 1;
        }
      }
      break;
    default:
      error = 1;
      break;
  }
  if (error && tracing & TR_EXT) {
    if (!(tracing & TR_EGP))
      traceegp("egpacq: recv bad pkt: ",
                        ngp->ng_addr, ngp->ng_myaddr, egp, egplen);
    TRACE_EXT("egpacq: last recvd pkt bad, recvd in state %d\n", ngp->ng_state);
  }
  return;
}

/*
 * egpsetint() sets EGP hello and poll intervals and times.
 * Returns 1 if either poll or hello intervals too big, 0 otherwise.
 */

egpsetint(ngp, egppkt)
        register     egpng   *ngp;
        register     struct  egppkt  *egppkt;
{
  register     struct  egpacq  *egpa = (struct egpacq *)egppkt;
  u_short helloint, pollint, ratio;

  /*
   * check parameters within bounds
   */
  helloint = ntohs(egpa->ea_hint);
  pollint  = ntohs(egpa->ea_pint);
  if (helloint > MAXHELLOINT || pollint > MAXPOLLINT) {
    TRACE_EXT("egpsetint: Hello interval = %d or poll interval = %d too big from %s, code %d at %s",
                        helloint, pollint, inet_ntoa(ngp->ng_addr), egpa->ea_pkt.egp_code, strtime);
    syslog(LOG_WARNING, "egpsetint: Hello interval = %d or poll interval = %d too big from %s, code %d",
                        helloint, pollint, inet_ntoa(ngp->ng_addr), egpa->ea_pkt.egp_code);
    return(1);
  }
  if ((helloint != MINHELLOINT) || (pollint != MINPOLLINT))
    syslog(LOG_WARNING, "egpsetint: EGP neighbor %s specified hello/poll intervals %d/%d, we specified %d/%d",
           inet_ntoa(ngp->ng_addr), helloint, pollint, MINHELLOINT, MINPOLLINT);
    TRACE_EXT("egpsetint: EGP neighbor %s specified hello/poll intervals %d/%d, we specified %d/%d at %s",
           inet_ntoa(ngp->ng_addr), helloint, pollint, MINHELLOINT, MINPOLLINT, strtime);
  if (helloint < MINHELLOINT)
      helloint = MINHELLOINT;
  if (pollint < MINPOLLINT)
      pollint = MINPOLLINT;
  ratio = (pollint - 1) / helloint + 1;   /* keep ratio pollint:helloint */
  helloint += HELLOMARGIN;
  pollint = ratio * helloint;
  syslog(LOG_WARNING, "egpsetint: Using actual intervals %d/%d with EGP neighbor %s",
	 helloint, pollint, inet_ntoa(ngp->ng_addr));
  TRACE_EXT("egpsetint: Using actual intervals %d/%d with EGP neighbor %s at %s\n",
	 helloint, pollint, inet_ntoa(ngp->ng_addr), strtime);
  ngp->ng_hint = helloint;
  ngp->ng_htime = gatedtime + helloint;
  ngp->ng_spint = helloint * ratio;
  ngp->ng_stime = gatedtime + helloint;
  if (helloint < egpsleep)
      egpsleep = helloint;
#ifdef debug
  printf("egpsetint: intervals hello %d, poll %d, egpsleep %d\n",
	 ngp->ng_hint, ngp->ng_spint, egpsleep);
#endif  
  return(0);
}

/*
 * egpstngh() go into neighbor state, initialize most variables.
 */

/* ARGSUSED */
egpstngh(ngp, egp)
	register     egpng   *ngp;
	register     egpkt   *egp;
{
#ifdef lint
  egp = egp;    /* make lint happy */
#endif lint

  if (ngp->ng_state != NGS_UP) {
    n_acquired++;
    TRACE_EXT("egpstngh: acquired EGP neighbor %s at %s", inet_ntoa(ngp->ng_addr), strtime);
    syslog(LOG_WARNING,"egpstngh: acquired EGP neighbor %s", inet_ntoa(ngp->ng_addr));
  }
  ngp->ng_state = NGS_UP;
  ngp->ng_reach = ME_DOWN;        /* so don't send initial poll until
                                   * after peer reports me up, but I will
                                   * respond to peer polls immediately 
                                   * as a poll implies he thinks I am up
                                   */
  ngp->ng_responses = ~0;         /* assume neighbor up when acquired */
  ngp->ng_flags &= ~NG_BAD;
  ngp->ng_status = 0;
  ngp->ng_retry = 0;
  ngp->ng_snpoll = 0;
  ngp->ng_runsol = 0;
  ngp->ng_noupdate = 0;
  ngp->ng_rpint = MINPOLLINT;
  ngp->ng_rnpoll = 0;
  ngp->ng_rtime = gatedtime;

  if (!(ngp->ng_flags & NG_ASIN)) {
    ngp->ng_asin = htons(egp->egp_system);
  }
  if (!(ngp->ng_flags & NG_AS)) {
    ngp->ng_as = htons(egp->egp_system);
  }
  if (sendas) {
    struct as_list *tmp_list;

    for (tmp_list = sendas; tmp_list; tmp_list = tmp_list->next) {
      if (tmp_list->as == ngp->ng_asin) {
        ngp->ng_aslist = tmp_list;
        break;
      }
    }
  }
#ifdef	NSS
  rt_area_adr_add(htons(egp->egp_system));		/* Add to Router Link State PDU */
  rt_area_add(htons(egp->egp_system), ngp->ng_myaddr);	/* Add to directly conn AS */
  cir2lsrec();						/* Generate new Router Links Record */
  /* install_default(ngp->ng_myaddr, ngp->ng_addr);	/* Install as a default */
#endif	NSS

  if (n_acquired == maxacq) 
    egp_maxacq();

  egpstime();
}

/*
 * egp_maxacq() ceases any neighbors with outstanding acquisition requests
 * once the maximum number of neighbors have been acquired. It also resets
 * all acquisition intervals to the short value.
 */

egp_maxacq()
{
  struct egpngh *ngp;

  for (ngp = egpngh; ngp != NULL; ngp = ngp->ng_next) {
    if (ngp->ng_state == NGS_DOWN)
      egpcease(ngp, NORESOURCE);
    if (ngp->ng_state == NGS_ACQUISITION)
      ngp->ng_hint = MINHELLOINT + HELLOMARGIN;
  }
  return;
}

/*
 * egpstime() sets egpsleep, the time between periodic sending of EGP packets;
 * maxpollint - the maximum poll interval for all acquired neighbors;
 * and rt_maxage - the maximum life time of routes. egpsleep is set to the
 * minimum interval for all potential peers or 60 s, whichever is less.
 * rt_maxage is set to the maximum of RT_MINAGE and RT_NPOLLAGE times the
 * maximum poll interval.
 * Called by egpstunacq(), egpstngh() and egpjob()
 */

egpstime()
{
  register     egpng   *ngp;

  maxpollint = MINPOLLINT;

  for (ngp = egpngh; ngp != NULL; ngp = ngp->ng_next) {
    switch (ngp->ng_state) {
      case NGS_UP:
        if (ngp->ng_spint > maxpollint)
          maxpollint = ngp->ng_spint;
      case NGS_ACQUISITION:
      case NGS_DOWN:
      case NGS_CEASE:
        if (ngp->ng_hint < egpsleep)
          egpsleep = ngp->ng_hint;
        break;
      default:
        break;
    }
  }
  if (egpsleep < MINHELLOINT) {
    TRACE_EXT("egpstime: bad sleep time %d\n", egpsleep);
    egpsleep = MINHELLOINT + HELLOMARGIN;
  }
  if (n_acquired > 0) /* only change rt_maxage if have an acquired neighbor */
    rt_maxage = (maxpollint * RT_NPOLLAGE > RT_MINAGE) ? 
                           (maxpollint * RT_NPOLLAGE) : RT_MINAGE;
  return;
}

/*
 * egpstunacq() sets state as ACQUISITION.
 *
 * External variables:
 * terminate
 */

egpstunacq(ngp)
reg     egpng   *ngp;
{
  int     allceased;

  if (ngp->ng_state == NGS_UP) {
    egpneighlost(ngp);
  }
  ngp->ng_state = NGS_ACQUISITION;
  ngp->ng_status = 0;
  ngp->ng_retry = 0;
  /*
   * if max # acq., set acq. interval to short
   * value for faster retry if current neigh goes down
   */
  ngp->ng_hint = (n_acquired < maxacq) ? LONGACQINT : MINHELLOINT;
  if (ngp->ng_flags & NG_BAD) {
    ngp->ng_htime = gatedtime + ACQDELAY;
  } else {
    ngp->ng_htime = gatedtime + ngp->ng_hint;
  }
  egpstime();

  /*
   * If terminate signal received and all neighbors ceased then exit
   */
  if (terminate) {
    allceased = TRUE;
    for (ngp = egpngh; ngp != NULL; ngp = ngp->ng_next)
      if (ngp->ng_state == NGS_DOWN || ngp->ng_state == NGS_UP ||
          ngp->ng_state == NGS_CEASE)
         allceased = FALSE;
    if (allceased)
      quit();
  }
}

/*
 * egphello() processes received hello packet
 */

egphello(ngp, egp)
	register     egpng   *ngp;
	register     egpkt   *egp;
{
  int     error = 0;

  if (ngp->ng_state != NGS_UP) {
    if (ngp->ng_state == NGS_ACQUISITION)
      egpsacq(ngp->ng_addr, ngp->ng_myaddr, NACEASE, PROTOVIOL, egprid_h, ngp);
    ngp->ng_rcmd++;
    error = 1;
  }
  else {
    if (egp->egp_status == DOWN) {
      syslog(LOG_WARNING, "egphello: EGP neighbor %s thinks we are down", inet_ntoa(ngp->ng_addr));
      TRACE_EXT("egphello: EGP neighbor %s thinks we are down at %s", inet_ntoa(ngp->ng_addr), strtime);
      ngp->ng_reach |= ME_DOWN;
    } else
      ngp->ng_reach &= ~ME_DOWN;

    switch (egp->egp_code) {
      case HELLO:
        ngp->ng_rcmd++;
        ngp->ng_rid = egprid_h;
        egpshello(ngp, HEARDU, egprid_h);
        return;
      case HEARDU:
        if (egprid_h == ngp->ng_sid)
          ngp->ng_responses |= 1;
        break;
      default:
        error = 1;
        break;
    }
  }
  if (error && tracing & TR_EXT) {
    if (!(tracing & TR_EGP))
      traceegp("egphello: recv bad pkt: ", ngp->ng_addr,
                        ngp->ng_myaddr, egp, sizeof(struct egppkt));
    printf("egphello: last recvd pkt bad, recvd in state %d\n", ngp->ng_state);
  }
  return;
}

/*
 * egppoll() processes received egp NR poll packet.
 */

egppoll(ngp, egp)
	register     egpng   *ngp;
	register     struct  egppkt  *egp;
{
  short   error = NOERROR;
  struct sockaddr_in source_net;
  char *msg;

  bzero((char *)&source_net, sizeof(source_net));
  source_net.sin_family = AF_INET;
  source_net.sin_addr = ((struct egppoll *)egp)->ep_net; /* note shared net */
  ngp->ng_rcmd++;
  ngp->ng_reach &= ~ME_DOWN;              /* poll => neighbor considers me up */
  ngp->ng_responses |= 1;                 /* mark him as reachable */

  if (ngp->ng_state != NGS_UP) {
    if (ngp->ng_state == NGS_ACQUISITION) {
      egpsacq( ngp->ng_addr, ngp->ng_myaddr, NACEASE, PROTOVIOL, egprid_h, ngp);
    }
    error = 100;
  } else if (egp->egp_code != 0) {
      error = EBADHEAD;
      msg = "invalid Code field in NR Poll";
  } else if ((ngp->ng_flags & NG_GATEWAY) && gd_inet_wholenetof(source_net.sin_addr) != gd_inet_wholenetof(ngp->ng_myaddr)) {
      /* source net was not what we expected */
      error = ENOREACH;
      msg = "IP Net Address field of NR Poll not expected";
#if	!SAMENET
  } else if (!(ngp->ng_flags & NG_GATEWAY) && gd_inet_wholenetof(source_net.sin_addr) != gd_inet_wholenetof(ngp->ng_addr)) {
      /* source net was not what we expected */
      error = ENOREACH;
      msg = "IP Net Address field of NR Poll not expected";
#endif	!SAMENET
#ifndef	NSS
  } else if (!(ngp->ng_flags & NG_GATEWAY) && rt_locate((int)INTERIOR, &source_net, RTPROTO_DIRECT) == NULL ) {
    /* No interface on shared net */
    error = ENOREACH;
    msg = "IP Net Address field of NR Poll not a directly connected network";
#endif	NSS
  } else if (ngp->ng_reach & NG_DOWN) {
      error = EUNSPEC;
      msg = "NR Poll received while neighbor substate is down";
  } else {
    if (gatedtime < ngp->ng_rtime) {        /* my NR may be lost*/
      if (egprid_h != ngp->ng_rid || ++ngp->ng_rnpoll > NPOLL)
        error = EXSPOLL;
        msg = "too many NR Polls received";
    } else {
      ngp->ng_rnpoll = 1;     /* new poll */
    }
  }
  ngp->ng_paddr = source_net.sin_addr;
  if (error == NOERROR) {
    ngp->ng_rid = egprid_h;
    egpsnr(ngp, 0, egprid_h);
  } else {
    if (error < 100) {
      egpserr(ngp->ng_addr, ngp->ng_myaddr, egp, sizeof(struct egppoll), error, ngp, msg);
    } else {
      if (tracing & TR_EXT) {
        if (!(tracing & TR_EGP)) {
          traceegp("egppoll: recv bad pkt: ", ngp->ng_addr, ngp->ng_myaddr, egp, sizeof(struct egppoll));
        }
        printf("egppoll: last recvd pkt bad, recvd in state %d\n", ngp->ng_state);
      }
    }
  }
  return;
}

/*
 * egpnr() processes received NR message packet.
 * It calls rt_NRupdate() to update the routing table.
 */

egpnr( ngp, egp, egplen)
        register     egpng   *ngp;
        register     egpkt   *egp;
        int     egplen;
{
  short   error = NOERROR;
  char *msg;

  if (ngp->ng_state != NGS_UP) {
    if (ngp->ng_state == NGS_ACQUISITION) {
      egpsacq(ngp->ng_addr, ngp->ng_myaddr, NACEASE, PROTOVIOL, egprid_h, ngp);
    }
    error = 100;                            
  } else if (egp->egp_code != 0) {
      error = EBADHEAD;
      msg = "invalid Code field in NR Response";
  } else if (gd_inet_wholenetof(((struct egpnr *)egp)->en_net) !=  gd_inet_wholenetof(ngp->ng_saddr)) {
      error = EBADHEAD;
      msg = "NR Response IP Net Address field does not match command";
  } else if (egp->egp_status & UNSOLICITED) {
      if (egprid_h != ngp->ng_sid) {   /* wrong seq. # */
        error = 100;
      } else if (++ngp->ng_runsol > 1) {  /* too many unsol. NR */
          error = EUNSPEC;
          msg = "too many unsolicited NR responses";
      }
    } else if (egprid_h != ngp->ng_sid || ngp->ng_snpoll == 0)  {  /* NR already recvd */
      error = 100;                    /* discard */
    } else {
    /*
     * reduce next poll time slightly to
     * take account of net delays on response
     * in order to preserve sending ratio to
     * hellos. Note HELLOMARGIN*ratio already
     * added to advised min. poll interval so
     * latter still met
     */
    ngp->ng_stime +=  ngp->ng_spint - ngp->ng_hint;
    ngp->ng_snpoll = 0;             /* no more repolls required */
    ngp->ng_reach &= ~ME_DOWN;      /* update => neighbor considers me up */
    ngp->ng_responses |= 1;
    if ((error = rt_NRupdate(ngp, egp, egplen)) > NOERROR) {
      switch (error) {
      	case EBADDATA:
      	  msg = "invalid NR Response message format";
      	  break;
      	default:
      	  msg = "internal error parsing NR Response";
      }
    } else {
      ngp->ng_noupdate = 0;
    }
  }
  /*
   * poll cmds will be delayed if unsolicited updates are
   * received.  Also, unsolicited updates count for reachability.
   */
  if (error == NOERROR) {
    ngp->ng_reach &= ~ME_DOWN;      /* update => neighbor considers me up */
    ngp->ng_responses |= 1;
    if (egp->egp_status & UNSOLICITED) {
      /* reduce next poll time slightly to take account
       * of net delays on response in order to preserve sending ratio to
       * hellos. Note HELLOMARGIN*ratio already added
       * to advised min. poll interval so latter still met
       *
       * if next cmd is poll, replace with hello
       */
      if (ngp->ng_htime >= ngp->ng_stime) {
        ngp->ng_htime = ngp->ng_stime;
      }
      ngp->ng_stime += ngp->ng_hint;
      ngp->ng_snpoll = 0;         /* no more repolls required */
    }
    ngp->ng_snpoll = 0;         /* no more repolls required */
  }
  if (error != NOERROR) {
    if (error < 100) {
      egpserr( ngp->ng_addr, ngp->ng_myaddr, egp, egplen, error, ngp, msg);
    } else {
      if (tracing & TR_EXT) {
        if (!(tracing & TR_EGP)) {
          traceegp("egpnr: recv bad pkt: ", ngp->ng_addr, ngp->ng_myaddr, egp, egplen);
        }
        printf("egpnr: last recvd pkt bad, recvd in state %d\n", ngp->ng_state);
      }
    }
  }
  return;
}

/*
 * egpsacq() sends an acquisition or cease packet.
 */

egpsacq( dst, src, code, status, id, ngp)
        struct in_addr  dst, src;    /* destination and source inet address */
        u_char  code, status;
        u_short id;
	struct egpngh *ngp;
{
  struct egpacq   acqpkt;
  int length;

  acqpkt.ea_pkt.egp_ver = EGPVER;
  acqpkt.ea_pkt.egp_type = EGPACQ;
  acqpkt.ea_pkt.egp_code = code;
  acqpkt.ea_pkt.egp_status = status;
  acqpkt.ea_pkt.egp_system = MY_AS(ngp);
  acqpkt.ea_pkt.egp_id = htons(id);

  acqpkt.ea_hint = ntohs(MINHELLOINT);
  acqpkt.ea_pint = ntohs(MINPOLLINT);
  if (code == NAREQ || code == NACONF)
    length = sizeof(acqpkt);
  else                                    /* omit hello & poll int */
    length = sizeof(acqpkt.ea_pkt);

  acqpkt.ea_pkt.egp_chksum = 0;
  acqpkt.ea_pkt.egp_chksum = egp_cksum((char *)&acqpkt, length);

  egp_send(dst, src, (struct egppkt *)&acqpkt, length);
  return;
}

/*
 * egphello() sends a hello or I-H-U packet.
 */

egpshello( ngp, code, id)
        egpng   *ngp;
        u_char  code;
        u_short id;
{
  struct egppkt hellopkt;

  hellopkt.egp_ver = EGPVER;
  hellopkt.egp_type = EGPHELLO;
  hellopkt.egp_code = code;
  hellopkt.egp_status = (ngp->ng_reach & NG_DOWN) ? DOWN : UP;
  hellopkt.egp_system = MY_AS(ngp);
  hellopkt.egp_id = htons(id);
  hellopkt.egp_chksum = 0;
  hellopkt.egp_chksum = egp_cksum((char *)&hellopkt, sizeof(hellopkt));

  egp_send(ngp->ng_addr, ngp->ng_myaddr, (struct egppkt *)&hellopkt,
                 sizeof(hellopkt));
  return;
}

/*
 * egpspoll() sends an NR poll packet.
 */

egpspoll(ngp)
	register     egpng   *ngp;
{
  struct egppoll  pollpkt;

  pollpkt.ep_pkt.egp_ver = EGPVER;
  pollpkt.ep_pkt.egp_type = EGPPOLL;
  pollpkt.ep_pkt.egp_code = 0;
  pollpkt.ep_pkt.egp_status = (ngp->ng_reach & NG_DOWN) ? DOWN : UP;
  pollpkt.ep_pkt.egp_system = MY_AS(ngp);
  pollpkt.ep_pkt.egp_id = htons(ngp->ng_sid);
  pollpkt.ep_unused = 0;
  pollpkt.ep_net = ngp->ng_saddr;

  pollpkt.ep_pkt.egp_chksum = 0;
  pollpkt.ep_pkt.egp_chksum = egp_cksum((char *)&pollpkt,sizeof(pollpkt));

  egp_send(ngp->ng_addr, ngp->ng_myaddr, (struct egppkt *)&pollpkt, sizeof(pollpkt));
  return;
}

/*
 * egpserr() sends an error packet.
 */

egpserr(dst, src, egp, length, error, ngp, msg)
        struct in_addr  dst, src;    /* destination and source inet address */
        egpkt   *egp;   /* erroneous egp packet */
        int     length; /* length erroneous packet */
        short   error;  
        egpng   *ngp;   /* ponter to legit. neighbor table, else zero */
	char	*msg;
{
  struct egperr errpkt;

  errpkt.ee_pkt.egp_ver = EGPVER;
  errpkt.ee_pkt.egp_type = EGPERR;
  errpkt.ee_pkt.egp_code = (error == EUVERSION) ? EGPVMASK : 0;
  if (ngp && ngp->ng_state == NGS_UP) {
    errpkt.ee_pkt.egp_status = (ngp->ng_reach & NG_DOWN) ? DOWN : UP;
  } else {
    errpkt.ee_pkt.egp_status = 0;
  }
  errpkt.ee_pkt.egp_system = MY_AS(ngp);
  errpkt.ee_pkt.egp_id = htons(egprid_h);         /* recvd seq.# */
  errpkt.ee_rsn = htons((u_short)error);
  /*
   * copy header of erroneous egp packet
   */
  bzero((char *)errpkt.ee_egphd, sizeof(errpkt.ee_egphd));
  if (length > sizeof(errpkt.ee_egphd))
    length = sizeof(errpkt.ee_egphd);
  bcopy((char *)egp, (char *)errpkt.ee_egphd, length);

  errpkt.ee_pkt.egp_chksum = 0;
  errpkt.ee_pkt.egp_chksum = egp_cksum((char *)&errpkt, sizeof(errpkt));

  if (ngp) {
    TRACE_EXT("egpserr: error packet to neighbor %s: %s at %s", inet_ntoa(ngp->ng_addr), msg, strtime);
    syslog(LOG_WARNING, "egpserr: error packet to neighbor %s: %s", inet_ntoa(ngp->ng_addr), msg);
  } else {
    TRACE_EXT("egpserr: error packet to non-neighbor %s: %s at %s", inet_ntoa(dst), msg, strtime);
    syslog(LOG_WARNING, "egpserr: error packet to non-neighbor %s: %s", inet_ntoa(dst), msg);
  }
  if (tracing & TR_EXT) {
    if (tracing & TR_EGP) {
      printf("egpserr: send error pkt:\n");
    } else {
      traceegp("egpserr: send error pkt ", src, dst, (struct egppkt *)&errpkt, sizeof(errpkt));
    }
  }
  egp_send( dst, src, (struct egppkt *)&errpkt, sizeof(errpkt));
  return;
}

/*
 * egpsnr() sends an NR message packet.
 *
 * It fills in the header information, calls rt_ifcheck() to update the
 * interface status information and rt_NRnets() to fill in the reachable
 * networks.
 */

extern  u_short mysystem;
extern  int     n_interfaces;
extern  int     n_remote_nets;

egpsnr(ngp, unsol, id)
        egpng   *ngp;
        int  unsol;                     /* TRUE => set unsolicited bit */
        u_short id;
{
  int     maxsize, length;
  register  struct  egpnr   *nrp;
  struct  in_addr   egpsnraddr;

  /*
   * allocate message buffer
   */
  maxsize = sizeof(struct egpnr) + NRMAXNETUNIT * (n_interfaces + n_routes);
  nrp = (struct egpnr *)malloc((unsigned)maxsize);
  if (nrp == NULL) {
    syslog(LOG_WARNING, "egpsnr: malloc: out of memory\n");
    TRACE_TRC("egpsnr: malloc: out of memory at %s", strtime);
    return;
  }
  nrp->en_pkt.egp_ver = EGPVER; /* prepare static part of NR message header */
  nrp->en_pkt.egp_type = EGPNR;
  nrp->en_pkt.egp_code = 0;
  nrp->en_pkt.egp_status = (ngp->ng_reach & NG_DOWN) ? DOWN : UP;
  if (unsol) {
    nrp->en_pkt.egp_status |= UNSOLICITED;
  }
  nrp->en_pkt.egp_system = MY_AS(ngp);
  nrp->en_pkt.egp_id = htons(id);
  nrp->en_egw = 0;                /* no exterior gateways */
  /*
   * copy shared net address
   */
  egpsnraddr = gd_inet_makeaddr(gd_inet_wholenetof(ngp->ng_myaddr), 0, FALSE);
  nrp->en_net.s_addr = egpsnraddr.s_addr;
  length = rt_NRnets(nrp, ngp); 

  if (length != ERROR) {
    nrp->en_pkt.egp_chksum = 0;
    nrp->en_pkt.egp_chksum = egp_cksum((char *)nrp, length);
    if (length > EGPMAXPACKETSIZE) {
      syslog(LOG_WARNING, "egpsnr: NR message size (%d) larger than EGPMAXPACKETSIZE (%d)",
             length, EGPMAXPACKETSIZE);
      TRACE_TRC("egpsnr: NR message size (%d) larger than EGPMAXPACKETSIZE (%d) at %s",
                length, EGPMAXPACKETSIZE, strtime);
    }
    egp_send( ngp->ng_addr, ngp->ng_myaddr, (struct egppkt *)nrp, length);
  } else {
    syslog(LOG_WARNING, "egpsnr: NR message not sent");
    TRACE_TRC("egpsnr: NR message not sent at %s", strtime);
  }
  free((char *)nrp);
  return;
}

/*
 * egp_cksum() computes the egp checksum.
 * returns checksum, note that C-GW routine
 * just returned ones complement sum
 */

egp_cksum(egp, len)
        char    *egp;           /* pointer to start egp packet */
        int     len;            /* length of egp packet */
{
  register    u_short * wd, *end_wd;
  u_long l;

  if (len & 01) {             /* pad if odd number octets */
    *(egp + len) = 0;
    len++;
  }
  wd = (u_short *)egp;       /* start word */
  end_wd = wd + (len >> 1);   /* end word */
  l = 0;
  while (wd < end_wd)
    l += *wd++;
  /*
   * convert to 16-bit ones complement sum - add 16-bit carry
   */
  l = (l & 0xffff) + ((l & 0xffff0000) >> 16);
  l = (l & 0xffff) + ((l & 0xffff0000) >> 16);
  return (((~l) & 0xffff));
}

/*
 * egp_send() sends an egp packet.
 */

egp_send(dst, src, egp, length)
        struct in_addr  dst, src;   /* destination and source inet address */
        struct egppkt   *egp;   /* pointer to start of egp packet */
        int     length;         /* length in octets of egp packet */

{
  struct sockaddr_in egpsaddr;
  struct interface   *ifp;
#ifdef	notdef
  struct rt_entry *rt;
#endif	notdef
  int     dontroute;
  int     error = FALSE;

  bzero((char *)&egpsaddr, sizeof (egpsaddr));
  egpsaddr.sin_family = AF_INET;
  /*
   * find interface to send packet on
   */
  egpsaddr.sin_addr = src;
  ifp = if_withnet(&egpsaddr);
  if (ifp == NULL) {
    syslog(LOG_ERR, "egp_send: no interface for source address %s", inet_ntoa(src));
    TRACE_INT("egp_send: no interface for source address %s at %s", inet_ntoa(src), strtime);
    error = TRUE;
  } else {
    extern int egp_socket;

    egpsaddr.sin_addr = dst;
#ifdef	NSS
    if (egp_sendto((char *)egp, length, src, dst) < 0)
#else	NSS
    dontroute = (gd_inet_netof(src) == gd_inet_netof(dst)) ? MSG_DONTROUTE : 0;
#ifdef	notdef
    if (!dontroute && (!(rt = rt_find(&egpsaddr)) || rt->rt_ifp != ifp)) {
      char *string = "egp_send: improper route sending %d bytes to %s\n";

      TRACE_TRC(string, length, inet_ntoa(dst));
      syslog(LOG_ERR, string, length, inet_ntoa(dst));
      error = TRUE;
    } else
#endif	notdef
    if (sendto(egp_socket, (char *)egp, length, dontroute, (struct sockaddr *)&egpsaddr, sizeof(egpsaddr)) < 0)
#endif	NSS
    {
      char *string = "egp_send: sendto error sending %d bytes to %s: %s\n";

      TRACE_TRC(string, length, inet_ntoa(dst), gd_error(errno));
      syslog(LOG_ERR, string, length, inet_ntoa(dst), gd_error(errno));
      error = TRUE;
    }
  }
  egp_stats.outmsgs++;
  if (!error) {
    TRACE_EGPPKT(EGP SENT, src, dst, egp, length);
  } else {
    TRACE_EGPPKT(EGP *NOT* SENT, src, dst, egp, length);
    egp_stats.outerrors++;
  }

  return;
}

/*
 * egpcease() initiates the sending of an egp neighbor cease.
 */

egpcease( ngp, rsn)
	register     egpng   *ngp;
        u_char  rsn;
{
  egpsacq( ngp->ng_addr, ngp->ng_myaddr, NACEASE, rsn, ngp->ng_sid, ngp);
  if (ngp->ng_state == NGS_UP) {
    egpneighlost(ngp);
  }
  ngp->ng_state = NGS_CEASE;
  ngp->ng_retry = 0;
  if (ngp->ng_state != NGS_UP)
    ngp->ng_hint = MINHELLOINT;
  ngp->ng_htime = gatedtime + ngp->ng_hint;
  ngp->ng_status = rsn;           /* save reason for retransmission */
  return;
}

/*
 * egpallcease() sends Ceases to all neighbors when going down (when SIGTERM
 * received).
 *
 * Global variables:
 * terminate - set here, tested by egpstunacq()
 */

SIGTYPE egpallcease()
{
  register egpng *ngp;
  int     ceasesent = FALSE;

#if sgi && !defined(_BSD_SIGNALS)
  HOLD_MY_SIG;
#endif
  getod();                        /* time of current interrupt */
  syslog(LOG_WARNING, "egpallcease: Terminate signal received");
  TRACE_TRC("egpallcease: Terminate signal received at %s\n", strtime);
  /*
   * scan neighbor state tables
   */
  for (ngp = egpngh; ngp != NULL; ngp = ngp->ng_next) {
    if ((ngp->ng_state == NGS_UP) || (ngp->ng_state == NGS_DOWN)) {
      egpcease(ngp, GODOWN);
      ceasesent = TRUE;
    }
  }
  if (ceasesent) {
    terminate = TRUE;
    syslog(LOG_WARNING, "egpallcease: Shutting down EGP");
    TRACE_TRC("egpallcease: Shutting down EGP at %s\n", strtime);
  } else {
    quit();
  }
#if sgi && !defined(_BSD_SIGNALS)
  RELSE_MY_SIG;
#endif
}

/*
 *  egpcheckas() is called when we loose reachability to
 *  a neighbor.  It scans the neighbor list to determine if there
 *  are any other active neighbors to this AS.  If there are not
 *  it currently only performs NSS functions, but it should probably
 *  delete any EGP learned routes from the AS of the neighbor.
 */

egpcheckas(down_ngp)
struct egpngh *down_ngp;
{
  struct egpngh *ngp;

  for (ngp = egpngh; ngp; ngp = ngp->ng_next) {
    if ((ngp != down_ngp) &&
        (ngp->ng_asin == down_ngp->ng_asin) &&
        (ngp->ng_state == NGS_UP) &&
        !(ngp->ng_reach & NG_DOWN)) {
      return(0);
    }
  }
  TRACE_EXT("egpcheckas: lost all neighbors to AS %d\n", down_ngp->ng_asin);
#ifdef	NSS
  if (!rt_area_adr_del(down_ngp->ng_as)) {
    if (!rt_area_del(down_ngp->ng_as)) {
      cir2lsrec();
    }
  }
  return(1);
#else	NSS
  return(0);
#endif	NSS
}

/*
 * egpneighlost() handles the loss of a neighbor.  It deletes any
 * routes in the routing table pointing at this gateway, calls egpcheckas()
 * to determine if we lost all neighbors for this AS and calls
 * rt_check_default() to be sure we should still be generating default.
 */
egpneighlost(ngp)
struct egpngh *ngp;
{
  int changes = 0;

  n_acquired--;
  changes += egpcheckas(ngp);		/* check for other direct neighbors in this AS */
  changes += rt_gwunreach(ngp->ng_addr);/* delete routes for down gateway */
#ifndef	NSS
  (void) rt_check_default();		/* verify that we should still generate default */
#endif	NSS
  if (changes) {
#ifdef	NSS
    egp2esrec();
#endif	NSS
  TRACE_RT("rt_NRupdate: above changes due to loss of neighbor %s at %s",
                      inet_ntoa(ngp->ng_addr), strtime);
  }
}
