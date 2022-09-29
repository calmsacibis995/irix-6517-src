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
static char *rcsid = "$Header: /proj/irix6.5.7m/isms/eoe/cmd/bsd/gated/RCS/if.c,v 1.2 1990/01/09 15:35:29 jleong Exp $";
#endif	not lint

/*
 * if.c
 *
 * Functions: if_withnet, if_check, if_print
 */

#include "include.h"

#ifdef sgi
#define same(a1, a2) \
	(bcmp((caddr_t)((a1)->sa_data), (caddr_t)((a2)->sa_data), 14) == 0)
#endif

/*
 * Find the interface on the network of the specified address.
 */
#ifdef sgi
struct interface *
if_withnet(addr)
	register struct sockaddr_in *addr;
{
	register struct interface *ifp;
	register u_long net1, tmp, net2;

	if (addr->sin_family != AF_INET)
	    return (0);

	/* get network part of withnetaddr */
	tmp = ntohl(addr->sin_addr.s_addr);
	net1 = gd_inet_wholenetof(addr->sin_addr);
	for (ifp = ifnet; ifp; ifp = ifp->int_next) {
	    if ((ifp->int_netmask & net1) == ifp->int_net) {
		net1 = tmp & ifp->int_subnetmask;
		break;
	    }
	}

	/*
	 * Search for ifp. For Point-to-Point links, don't just compare
	 * the network number; the full destination address must be
	 * compared to find the correct interface.
	 *
	 * TBD: Should a check be make for whether or not the interface
	 * is up and running? 
	 */
	for (ifp = ifnet; ifp; ifp = ifp->int_next) {
	    if (ifp->int_flags & IFF_POINTOPOINT) {
		if (same((struct sockaddr*)addr, &ifp->int_dstaddr))
		    break;
	    } else {
		tmp = ntohl(in_addr_ofs(&ifp->int_addr).s_addr);
		net2 = gd_inet_wholenetof(in_addr_ofs(&ifp->int_addr));
		if ((ifp->int_netmask & net2) == ifp->int_net)
		    net2 = tmp & ifp->int_subnetmask;
		if (net1 == net2)
		    break;
	    }
	}
	return(ifp);
}
#else /* sgi */
struct interface *
if_withnet(withnetaddr)
	register struct sockaddr_in *withnetaddr;
{
  register struct interface *ifp;
  register u_long net1, tmp, net2;

  if (withnetaddr->sin_family != AF_INET)
    return (0);

  /* get network part of withnetaddr */
  tmp = ntohl(withnetaddr->sin_addr.s_addr);
  net1 = gd_inet_wholenetof(withnetaddr->sin_addr);
  for (ifp = ifnet; ifp; ifp = ifp->int_next) {
    if ((ifp->int_netmask & net1) == ifp->int_net) {
      net1 = tmp & ifp->int_subnetmask;
      break;
    }
  }
  /* search for ifp */
  for (ifp = ifnet; ifp; ifp = ifp->int_next) {
    if (ifp->int_flags & IFF_POINTOPOINT) {
      tmp = ntohl(in_addr_ofs(&ifp->int_dstaddr).s_addr);
      net2 = gd_inet_wholenetof(in_addr_ofs(&ifp->int_dstaddr));
      if ((ifp->int_netmask & net2) == ifp->int_net)
         net2 = tmp & ifp->int_subnetmask;
    }
    else {
      tmp = ntohl(in_addr_ofs(&ifp->int_addr).s_addr);
      net2 = gd_inet_wholenetof(in_addr_ofs(&ifp->int_addr));
      if ((ifp->int_netmask & net2) == ifp->int_net)
        net2 = tmp & ifp->int_subnetmask;
    }
    if (net1 == net2)
      break;
  }
  return(ifp);
}
#endif /* sgi */

#ifdef notdef
/* used for DEBUGing */
if_print()
{
  register struct interface *ifp;

  for (ifp = ifnet; ifp; ifp = ifp->int_next) {
    if_display("if_print", ifp);
  }
}
#endif	notdef

#ifdef sgi
/*
 * Find the interface with this name.
 */
struct interface *
if_ifwithname(if_name)
	char *if_name;
{
	register struct interface *ifp;

	for (ifp = ifnet; ifp; ifp = ifp->int_next) {
		if (0 != ifp->int_name && !strcmp(ifp->int_name, if_name))
			break;
	}
	return (ifp);
}

/* if_maint() keeps maintains the addresses and names of configured
 * internet interfaces. New entries are added to the head of the global
 * linked list of interface tables.
 *
 * External variables:
 * ifnet - pointer to interface list
 * n_interfaces - number of direct internet interfaces (re-set here) 
 */ 

if_maint()
{
	int	sock, n_intf;
	struct	in_addr if_addr;
	int	bufsize;
	struct	ifconf ifc;
	struct	ifreq  *ifreq_buf;	/* buffer for receiving interface
				 	 * configuration info from ioctl */
	struct	ifreq ifreq, *ifr;
	struct	interface *ifp, *test;
	u_long	a;
	struct	sockaddr_in *sin;

	/*
	 * Determine internet addresses of all internet interfaces, except
	 * ignore loopback interface.
	 *
	 * First must open a temporary socket to get a valid socket
	 * descriptor for ioctl call.
	 */

	if ((sock = getsocket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		p_error("if_maint: no info socket ");
		return;
	}

	/*
	 * Get interface configuration. Allocate buffer for config.
	 * If insufficient double size
	 */
	bufsize = 10 * sizeof(struct ifreq) + 1; /* ioctl assumes >size ifreq */
	for (;;) {
	    ifreq_buf = (struct ifreq *)malloc((unsigned)bufsize);
	    if (ifreq_buf == 0) {
		syslog(LOG_ERR, "if_maint: malloc: out of memory\n");
		return;
	    }
	    ifc.ifc_len = bufsize;	/*sizeof(ifreq_buf);*/
	    ifc.ifc_req = ifreq_buf;

	    if (ioctl(sock, SIOCGIFCONF, (char *)&ifc) ) {
		p_error("if_maint: ioctl SIOCGIFCONF:\n");
		return;
	    }
	    /*
	     * if spare buffer space for at least
	     * one more interface all found
	     */
	    if ((bufsize - ifc.ifc_len) > sizeof (struct ifreq)) 
		break;
	    /* else double buffer size and retry*/
	    free((char *)ifreq_buf);
	    if (bufsize > 40 * sizeof(struct ifreq)) {
		syslog(LOG_ERR, "if_maint: more than 39 interfaces\n");
		return;
	    }
	    bufsize <<= 1;
	}

	/*
	 * Enumerate the interfaces and add any interfaces we don't know.
	 */
	n_intf = ifc.ifc_len / sizeof(struct ifreq);	/* # of interfaces */
	for (ifr = &ifreq_buf[0]; n_intf > 0; n_intf--, ifr++) {
	    if (ifr->ifr_addr.sa_family != AF_INET) {
		continue;
	    }
	    if_addr.s_addr =
		((struct sockaddr_in *)&ifr->ifr_addr)->sin_addr.s_addr;
	    if ((unsigned)gd_inet_netof(if_addr) == (unsigned)LOOPBACKNET) {
		continue;	/* ignore loopback interface */
	    }
	    if (if_ifwithname(ifr->ifr_name)) {
		continue;	/* next, already know about this interface */
	    }

	    /*
	     * Don't know about this interface, so add it.
	     */
	    ifp = (struct interface *)malloc(sizeof (struct interface));
	    if (ifp == 0) {
		syslog(LOG_ERR, "if_maint: malloc: out of memory\n");
		continue;
	    }
	    /* save name for future ioctl calls */
	    ifp->int_name = malloc((unsigned)strlen(ifr->ifr_name) + 1);
	    if (ifp->int_name == 0) {
		syslog(LOG_ERR, "if_maint: malloc: out of memory\n");
		continue;
	    }
	    (void) strcpy(ifp->int_name, ifr->ifr_name);
	    (void) strcpy(ifreq.ifr_name, ifr->ifr_name); /* for ioctl calls */

	    /* interface address */
	    ifp->int_addr = ifr->ifr_addr;

	    /* Get interface flags */
	    if (ioctl(sock, SIOCGIFFLAGS, (char *)&ifreq)) {
		(void) sprintf(err_message,
		    "if_maint: %s: ioctl SIOCGIFFLAGS:\n", ifreq.ifr_name);
		p_error(err_message);
		continue;
	    }
	    /*
	     * Mask off flags that we don't understand as some systems use
	     * them differently than we do.
	     */
	    ifp->int_flags = IFF_INTERFACE | (ifreq.ifr_flags & IFF_MASK);

	    /*
	     * if interface is marked down, we will include it and try again
	     * later.
	     */

	    /* get interface metric */
	    if (ioctl(sock, SIOCGIFMETRIC, (char *)&ifreq) < 0) {
		(void) sprintf(err_message,
		    "if_maint: %s: ioctl SIOCGIFMETRIC:\n", ifreq.ifr_name);
		p_error(err_message);
		ifreq.ifr_metric = 0;
	    }
	    ifp->int_metric = (ifreq.ifr_metric >= 0) ? ifreq.ifr_metric : 0;
	    /*
	     * TBD: should the metric be bump up like routed/startup$ifinit()?
	     */
	    if (ifp->int_flags & IFF_POINTOPOINT) {
		if (ioctl(sock, SIOCGIFDSTADDR, (char *)&ifreq) < 0) {
		    (void) sprintf(err_message,
			"if_maint: %s: ioctl SIOCGIFDSTADDR:\n",
			ifreq.ifr_name);
		    p_error(err_message);
		    continue;
		}
		ifp->int_dstaddr = ifreq.ifr_dstaddr;
	    }
	    if (ifp->int_flags & IFF_BROADCAST) {
		if (ioctl(sock, SIOCGIFBRDADDR, (char *)&ifreq) < 0) {
		    (void) sprintf(err_message,
			"if_maint: %s: ioctl SIOGIFBRDADDR:\n", ifreq.ifr_name);
		    p_error(err_message);
		    continue;
		}
		ifp->int_broadaddr = ifreq.ifr_broadaddr;
	    }

	    /* get netmask */
	    if (ioctl(sock, SIOCGIFNETMASK, (char *)&ifreq) < 0) {
		(void) sprintf(err_message,
		    "if_maint: %s: ioctl SIOGIFNETMASK:\n", ifreq.ifr_name);
		p_error(err_message);
		continue;
	    }
	    sin = (struct sockaddr_in *)&ifreq.ifr_addr;
	    ifp->int_subnetmask = ntohl(sin->sin_addr.s_addr);

	    sin = (struct sockaddr_in *)&ifp->int_addr;
	    a = ntohl(sin->sin_addr.s_addr);
	    if (IN_CLASSA(a)) {
		ifp->int_netmask = IN_CLASSA_NET;
	    } else if (IN_CLASSB(a)) {
		ifp->int_netmask = IN_CLASSB_NET;
	    } else {
		ifp->int_netmask = IN_CLASSC_NET;
	    }
	    if (ifp->int_subnetmask == 0) {
		ifp->int_subnetmask = ifp->int_netmask;
	    } else if (ifp->int_subnetmask != ifp->int_netmask) {
		ifp->int_flags |= IFF_SUBNET;
	    }
	    ifp->int_net = a & ifp->int_netmask;
	    ifp->int_subnet = a & ifp->int_subnetmask;
	    ifp->int_active_gw = NULL;

	    /*
	     * Add interface to the end of the direct interface list.
	     */
	    *ifnext = ifp;
	    ifnext = &ifp->int_next;
	    ifp->int_next = NULL; 

	    /*
	     * if multiple interfaces with same net number exit as the
	     * routing tables assume at most one interface per net.
	     * NOT TRUE!  Point-to-point`s are OK.
	     */
	    for (test = ifnet; test != ifp; test = test->int_next)
		if ((ifp->int_subnet == test->int_subnet) &&
		  ((ifp->int_flags|test->int_flags) & IFF_POINTOPOINT) == 0) {
		    syslog(LOG_ERR, "if_maint: Exit as multiple non PTP");
		    syslog(LOG_ERR, "interfaces to net of interface %s\n",
		    	   inet_ntoa(if_addr));
		    quit();	/* a little drastic */
	        }
	    n_interfaces++;	/* number of internet interfaces */

	    TRACE_INT("\n");
	    if_display("if_maint", ifp);

	    if (doing_rip && (ifp->int_flags & IFF_POINTOPOINT)
	      && (rip_supplier < 0)) {
		rip_supplier = TRUE;
		TRACE_INT("if_maint: PointoPoint RIP supplier to: %s\n",
		    ifp->int_name);
	    }
	    if (doing_hello && (ifp->int_flags & IFF_POINTOPOINT)
	      && (hello_supplier < 0)) {
		hello_supplier = TRUE;
		TRACE_INT("if_maint: PointoPoint HELLO supplier to: %s\n",
		ifp->int_name);
	    }

	    /*
	     * initialize the interior routing table with direct nets as
	     * per the newly configurated interface.
	     */
	    if ( !(rip_supplier || hello_supplier)
	      || !(doing_rip || doing_hello) ) {
		ifp->int_flags |= IFF_NOAGE;
		TRACE_INT("if_maint: interface %s: %s marked passive\n",
		    ifp->int_name, inet_ntoa(sock_inaddr(&ifp->int_addr)));
	    }
	    if (_IFF_UP_RUNNING(ifp->int_flags)) {
		rt_ifup(ifp);
	    } else {
		rt_ifdown(ifp, TRUE);
	    }

	}	/* for each configured interface */

	if (doing_rip && (n_interfaces > 1) && (rip_supplier < 0)) {
	    rip_supplier = TRUE;
	    syslog(LOG_NOTICE,
		"if_maint: Acting as RIP supplier to our direct nets");
	    TRACE_TRC("if_maint: Acting as RIP supplier to our direct nets\n");
	}
	if (doing_hello && (n_interfaces > 1) && (hello_supplier < 0)) {
	    hello_supplier = TRUE;
	    syslog(LOG_NOTICE,
		"if_maint: Acting as HELLO supplier to our direct nets");
	    TRACE_TRC("if_maint: Acting as HELLO supplier to our direct nets\n");
	}
	free((char *)ifreq_buf);
	(void) close(sock);

	/*
	 * There probably is no need to call rt_NRadvise_init() to set up
	 * ifflags since new interfaces coming abroad now "must" be point-
	 * to-point links.
	 */

} /* if_maint */
#endif /* sgi */

#ifndef	NSS
/*
 * if_check() checks the current status of all interfaces
 * If any interface has changed status, then the interface values
 * are re-read from the kernel and re-set.
 */

if_check()
{
  register struct interface *ifp;
  struct ifreq ifrequest;
  int  if_change = FALSE;
  struct sockaddr_in *sin;
  u_long a;
  int info_sock;

#ifdef sgi
  if_maint();	/* check for any newly configured interfaces */
#endif

  if ((info_sock = getsocket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    p_error("if_check: no info socket ");
    return;
  }
  for (ifp = ifnet; ifp != NULL; ifp = ifp->int_next) {
    /* get interface status flags */
    (void) strcpy(ifrequest.ifr_name, ifp->int_name);
    if (ioctl(info_sock, SIOCGIFFLAGS, (char *)&ifrequest)) {
      (void) sprintf(err_message,"if_check: %s: ioctl SIOCGIFFLAGS:", ifp->int_name);
      p_error(err_message);
    } else {
#ifdef sgi
      if_change = FALSE;
      /*
       * Changes requiring action:
       *	(1) interface toggles up/down state.
       *	(2) interface is to somewhere else, as can happen with SLIP.
       */
      if ((ifrequest.ifr_flags & (IFF_UP|IFF_RUNNING))
	!= (ifp->int_flags & (IFF_UP|IFF_RUNNING))) {
	  if_change = TRUE;
      }

      if ( (ifp->int_flags & IFF_POINTOPOINT)
	&& (_IFF_UP_RUNNING(ifp->int_flags))
	&& (_IFF_UP_RUNNING(ifrequest.ifr_flags)) ) {
	  short tmp_if_flags;
	  tmp_if_flags = ifrequest.ifr_flags;
	  if (ioctl(info_sock, SIOCGIFDSTADDR, (char *)&ifrequest) < 0) {
	      (void) sprintf(err_message,"if_check: %s: ioctl SIOCGIFDSTADDR:",
	  	  ifp->int_name);
	      p_error(err_message);
	  } else {
	      if (!same(&ifp->int_dstaddr, &ifrequest.ifr_dstaddr)) {
	          ifp->int_dstaddr = ifrequest.ifr_dstaddr;
	          if_change = TRUE;
	      }
          }
	  ifrequest.ifr_flags = tmp_if_flags;	/* set it back */
      }

      if ( if_change ) {
        if (_IFF_UP_RUNNING(ifrequest.ifr_flags)) {
#else /* sgi */
      if ((ifrequest.ifr_flags & IFF_UP) != (ifp->int_flags & IFF_UP)) {
        if_change = TRUE;
        if (ifrequest.ifr_flags & IFF_UP) {
#endif /* sgi */
          ifp->int_flags = IFF_INTERFACE |
                           (ifrequest.ifr_flags & IFF_MASK) |
                           (ifp->int_flags & IFF_KEEPMASK);

#if	defined(INT_METRIC)
          (void) strcpy(ifrequest.ifr_name, ifp->int_name);
          if (ioctl(info_sock, SIOCGIFMETRIC, (char *)&ifrequest) < 0) {
            (void) sprintf(err_message,"if_check: %s: ioctl SIOCGIFMETRIC:", ifp->int_name);
            p_error(err_message);
          } else {
            ifp->int_metric = (ifrequest.ifr_metric >= 0) ?
                                 ifrequest.ifr_metric : 0;
          }
#else	defined(INT_METRIC)
          ifp->int_metric =  0;
#endif	defined(INT_METRIC)
#ifdef sgi
	  /* PTP link dst address has already been set up above.
	   */
#else /* sgi */
          if (ifp->int_flags & IFF_POINTOPOINT) {
            (void) strcpy(ifrequest.ifr_name, ifp->int_name);
            if (ioctl(info_sock, SIOCGIFDSTADDR, (char *)&ifrequest) < 0) {
              (void) sprintf(err_message,"if_check: %s: ioctl SIOCGIFDSTADDR:", ifp->int_name);
              p_error(err_message);
            } else {
              ifp->int_dstaddr = ifrequest.ifr_dstaddr;
            }
          }
#endif /* sgi */
          (void) strcpy(ifrequest.ifr_name, ifp->int_name);
          if (ioctl(info_sock, SIOCGIFADDR, (char *)&ifrequest) < 0) {
            (void) sprintf(err_message,"if_check: %s: ioctl SIOCGIFADDR:", ifp->int_name);
            p_error(err_message);
          } else {
            ifp->int_addr = ifrequest.ifr_addr;
          }
          if (ifp->int_flags & IFF_BROADCAST) {
#ifdef SIOCGIFBRDADDR
            (void) strcpy(ifrequest.ifr_name, ifp->int_name);
            if (ioctl(info_sock, SIOCGIFBRDADDR, (char *)&ifrequest) < 0) {
              (void) sprintf(err_message,"if_check: %s: ioctl SIOCGIFBRDADDR:", ifp->int_name);
              p_error(err_message);
            } else {
#ifdef	SUN3_3PLUS
              ifp->int_broadaddr = ifrequest.ifr_addr;
#else	SUN3_3PLUS
              ifp->int_broadaddr = ifrequest.ifr_broadaddr;
#endif	SUN3_3PLUS
            }
#else !SIOCGIFBRDADDR
            ifp->int_broadaddr = ifp->int_addr;
            sin = (struct sockaddr_in *)&ifp->int_addr;
            a = ntohl(sin->sin_addr.s_addr);
            sin = (struct sockaddr_in *)&ifp->int_broadaddr;
            if (IN_CLASSA(a))
              sin->sin_addr.s_addr = htonl(a & IN_CLASSA_NET);
            else if (IN_CLASSB(a))
              sin->sin_addr.s_addr = htonl(a & IN_CLASSB_NET);
            else
              sin->sin_addr.s_addr = htonl(a & IN_CLASSC_NET);
#endif SIOCGIFBRDADDR
          }
          (void) strcpy(ifrequest.ifr_name, ifp->int_name);
#ifdef	SIOCGIFNETMASK
          if (ioctl(info_sock, SIOCGIFNETMASK, (char *)&ifrequest) < 0) {
            (void) sprintf(err_message,"if_check: %s: ioctl SIOCGIFNETMASK:", ifp->int_name);
            p_error(err_message);
            ifp->int_subnetmask = (u_long) 0;
          } else {
            sin = (struct sockaddr_in *)&ifrequest.ifr_addr;
            ifp->int_subnetmask = ntohl(sin->sin_addr.s_addr);
          }
#else	SIOCGIFNETMASK
          sin = (struct sockaddr_in *)&ifp->int_addr;
          a = ntohl(sin->sin_addr.s_addr);
          if (IN_CLASSA(a)) {
            ifp->int_subnetmask = IN_CLASSA_NET;
          } else if (IN_CLASSB(a)) {
            ifp->int_subnetmask = IN_CLASSB_NET;
         } else {
            ifp->int_subnetmask = IN_CLASSC_NET;
         }
#endif	SIOCGIFNETMASK
          sin = (struct sockaddr_in *)&ifp->int_addr;
          a = ntohl(sin->sin_addr.s_addr);
          if (IN_CLASSA(a)) {
            ifp->int_netmask = IN_CLASSA_NET;
          } else if (IN_CLASSB(a)) {
            ifp->int_netmask = IN_CLASSB_NET;
          } else {
            ifp->int_netmask = IN_CLASSC_NET;
          }
          if (ifp->int_subnetmask == 0) {
            ifp->int_subnetmask = ifp->int_netmask;
          } else if (ifp->int_subnetmask != ifp->int_netmask) {
            ifp->int_flags |= IFF_SUBNET;
          }
          ifp->int_net = a & ifp->int_netmask;
          ifp->int_subnet = a & ifp->int_subnetmask;
          syslog(LOG_NOTICE, "if_check: %s, address %s up",
                             ifp->int_name, inet_ntoa(sock_inaddr(&ifp->int_addr)));
          TRACE_INT("if_check: %s, address %s up at %s",
                             ifp->int_name, inet_ntoa(sock_inaddr(&ifp->int_addr)), strtime);
          if_display("if_check", ifp);
          rt_ifup(ifp);
        } else {
          syslog(LOG_NOTICE, "if_check: %s, address %s down",
                             ifp->int_name, inet_ntoa(sock_inaddr(&ifp->int_addr)));
          TRACE_INT("if_check: %s, address %s down at %s",
                             ifp->int_name, inet_ntoa(sock_inaddr(&ifp->int_addr)), strtime);
          ifp->int_flags = IFF_INTERFACE |
                           (ifrequest.ifr_flags & IFF_MASK) |
                           (ifp->int_flags & IFF_KEEPMASK);
          rt_ifdown(ifp, FALSE);
        }
      }
    }
  }
  if (if_change) {
    register struct rt_entry *rt;
    register struct rthash *rh;

    for (rh = nethash; rh < &nethash[ROUTEHASHSIZ]; rh++) 
      for (rt = rh->rt_forw; rt != (struct rt_entry *)rh; rt = rt->rt_forw) {
        if ((rt->rt_state & RTS_INTERIOR) == 0)
          continue;
#ifdef sgi
        if (_IFF_UP_RUNNING(rt->rt_ifp->int_flags))
#else
        if (rt->rt_ifp->int_flags & IFF_UP)
#endif
          rt->rt_flags |= RTF_UP;
        else
          rt->rt_flags &= ~RTF_UP;
      }
  }
  (void) close(info_sock);
}


/*
 *	if_display():
 *		Log the configuration of the interface
 */
if_display(name, ifp)
  char *name;
  struct interface *ifp;
{
#ifdef sgi
  TRACE_INT("%s: interface %s: %s, addr %s, metric %d",
    name, ifp->int_name, _IFF_UP_RUNNING(ifp->int_flags) ? "up" : "down",
    inet_ntoa(sock_inaddr(&ifp->int_addr)), ifp->int_metric);
#else
  TRACE_INT("%s: interface %s: %s, addr %s, metric %d",
    name, ifp->int_name, (ifp->int_flags & IFF_UP) ? "up" : "down", inet_ntoa(sock_inaddr(&ifp->int_addr)), ifp->int_metric);
#endif
  if (ifp->int_flags & IFF_BROADCAST) {
    TRACE_INT(", broadaddr %s, ", inet_ntoa(sock_inaddr(&ifp->int_broadaddr)));
  }
  if (ifp->int_flags & IFF_POINTOPOINT) {
    TRACE_INT(", dstaddr %s, ", inet_ntoa(sock_inaddr(&ifp->int_dstaddr)));
  }
  TRACE_INT("\n%s: interface %s: ", name, ifp->int_name);
  TRACE_INT("net %s, ", gd_inet_ntoa(htonl(ifp->int_net)));
  TRACE_INT("netmask %s, ", gd_inet_ntoa(htonl(ifp->int_netmask)));
  TRACE_INT("\n%s: interface %s: ", name, ifp->int_name);
  TRACE_INT("subnet %s, ", gd_inet_ntoa(htonl(ifp->int_subnet)));
  TRACE_INT("subnetmask %s\n", gd_inet_ntoa(htonl(ifp->int_subnetmask)));
}
#endif	NSS

/*
 * Find the interface with address addr.
 */

#ifdef sgi
struct interface *
if_ifwithaddr(addr)
	struct sockaddr *addr;
{
	register struct interface *ifp;
	struct sockaddr_in *intf_addr;

	for (ifp = ifnet; ifp; ifp = ifp->int_next) {
		if (ifp->int_flags & IFF_REMOTE)
			continue;
		if (ifp->int_addr.sa_family != addr->sa_family)
			continue;
		if (ifp->int_flags & IFF_POINTOPOINT) {
			if (same(&ifp->int_dstaddr, addr))
				break;
			else
				continue;
		}
		if (same(&ifp->int_addr, addr))
			break;
		if ((ifp->int_flags & IFF_BROADCAST) &&
		    same(&ifp->int_broadaddr, addr))
			break;
	}
	return (ifp);
}
#else /* sgi */
struct interface *
if_ifwithaddr(withaddraddr)
	struct sockaddr *withaddraddr;
{
  register struct interface *ifp;
  struct sockaddr_in *addr = (struct sockaddr_in *) withaddraddr;
  struct sockaddr_in *intf_addr;

  for (ifp = ifnet; ifp; ifp = ifp->int_next) {
    if (ifp->int_flags & IFF_REMOTE) {
      continue;
    }
    if (ifp->int_addr.sa_family != withaddraddr->sa_family) {
      continue;
    }
    if (ifp->int_flags & IFF_POINTOPOINT) {
      intf_addr = (struct sockaddr_in *)&ifp->int_dstaddr;
      if (!bcmp((char *)&intf_addr->sin_addr, (char *)&addr->sin_addr, sizeof(struct in_addr))) {
        break;
      } else {
        continue;
      }
    }
    intf_addr = (struct sockaddr_in *)&ifp->int_addr;
    if (!bcmp((char *)&intf_addr->sin_addr, (char *)&addr->sin_addr, sizeof(struct in_addr))) {
      break;
    }
    intf_addr = (struct sockaddr_in *)&ifp->int_broadaddr;
    if (ifp->int_flags & IFF_BROADCAST) {
    }
    if ((ifp->int_flags & IFF_BROADCAST) &&
      !bcmp((char *)&intf_addr->sin_addr, (char *)&addr->sin_addr, sizeof(struct in_addr)))
      break;
  }
  return (ifp);
}
#endif /* sgi */

#ifdef sgi
/*
 * Find the Interface with this address. Don't do anything special for
 * Point-to-Point links (i.e. looking at the destination address).
 * Remember that point-to-point interfaces may have duplicate addresses.
 */
struct interface *
if_ifwithint_addr(addr)
	struct sockaddr *addr;
{
	register struct interface *ifp;

	for (ifp = ifnet; ifp; ifp = ifp->int_next) {
		if (ifp->int_flags & IFF_REMOTE)
			continue;
		if (ifp->int_addr.sa_family != addr->sa_family)
			continue;
		if (same(&ifp->int_addr, addr))
			break;
	}
	return (ifp);
}
#endif

#ifndef	NSS
/*
 * update the active gw list on argument interface.
 */

if_updateactivegw(ifptr, actgw_addr, gw_proto)
	struct interface *ifptr;
	u_long actgw_addr;
	int gw_proto;
{
  struct active_gw *agp, *tmpactgw;
  int found_gw = 0;

  for (agp = ifptr->int_active_gw; agp; agp = agp->next) {
    if (actgw_addr == agp->addr) {
      found_gw++;
      break;
    }
    if (agp->next == NULL)
      break;
  }
  if (found_gw != 0) {
    agp->timer = 0;
    agp->proto |= gw_proto;
    return(agp->proto);
  }
  /*
   * this active gateway wasn't recorded yet!  Add it.
   * we have agp pointing to the last element, so no need to
   * traverse again.
   */
  tmpactgw = (struct active_gw *)malloc((unsigned)sizeof(struct active_gw));
  if (tmpactgw <= (struct active_gw *)0) {
    syslog(LOG_WARNING, "if_updateactivegw: out of memory");
    return(0);
  }
  tmpactgw->proto = gw_proto;
  tmpactgw->addr = actgw_addr;
  tmpactgw->timer = 0;
  if (agp == NULL) {		/* first one */
    ifptr->int_active_gw = tmpactgw;
    tmpactgw->back = ifptr->int_active_gw;
  }
  else {
    agp->next = tmpactgw;
    tmpactgw->back = agp;
  }
  tmpactgw->next = NULL;
  tmpactgw = NULL;		/* just to be safe */
  return (gw_proto);
}
#endif	NSS
