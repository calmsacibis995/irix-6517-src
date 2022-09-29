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
static char *rcsid = "$Header: /proj/irix6.5.7m/isms/eoe/cmd/bsd/gated/RCS/init.c,v 1.2 1990/01/09 15:35:49 jleong Exp $";
#endif	not lint

/*
 * init.c
 *
 * Functions: init_if, init_sock, getsocket, init_egpngh, getnetorhostname,
 *	      init_egp, rip_init, get_rip_socket
 */
/* Initialization routines */


#include "include.h"

extern u_long inet_addr();

#ifndef	NSS
/* init_if() determines addresses and names of internet interfaces configured.
 * Results returned in global linked list of interface tables.
 * The interface names are required for later ioctl calls re interface status.
 *
 * External variables:
 * ifnet - pointer to interface list
 * n_interfaces - number of direct internet interfaces (set here) 
 */ 

init_if()
{
  int	i, j, init_ifs, n_intf;
  struct in_addr initifaddr;
  int	bufsize;
  struct ifreq  *ifreq_buf;	/* buffer for receiving interace
				 * configuration info from ioctl */
  struct ifconf ifconf_req;
  struct interface *ifp;
#ifdef sgi
  struct interface *test;
#endif
  struct ifreq if_info;
  u_long a;
  struct sockaddr_in *sin;

/*
 * Determine internet addresses of all internet interfaces, except
 * ignore loopback interface.
 */

/*
 * First must open a temporary socket to get a valid socket descriptor for
 * ioctl call.
 */

  if ((init_ifs = getsocket(AF_INET, SOCK_DGRAM, 0)) < 0)
    quit();

  /*
   * get interface configuration.
   * allocate buffer for config. if
   * insufficient double size
   */
  bufsize = 10 * sizeof(struct ifreq) + 1; /* ioctl assumes > size ifreq */
  for (;;) {
    ifreq_buf = (struct ifreq *)malloc((unsigned)bufsize);
    if (ifreq_buf == 0) {
      syslog(LOG_ERR, "init_if: malloc: out of memory\n");
      quit();
    }
    ifconf_req.ifc_len = bufsize; /*sizeof(ifreq_buf);*/
    ifconf_req.ifc_req = ifreq_buf;

    if (ioctl(init_ifs, SIOCGIFCONF, (char *)&ifconf_req) ) {
      p_error("init_if: ioctl SIOCGIFCONF:\n");
      quit();
    }
    /*
     * if spare buffer space for at least
     * one more interface all found
     */
    if ((bufsize - ifconf_req.ifc_len) > sizeof (struct ifreq)) 
      break;
    /* else double buffer size and retry*/
    free((char *)ifreq_buf);
    if (bufsize > 40 * sizeof(struct ifreq)) {
      syslog(LOG_ERR, "init_if: more than 39 interfaces\n");
      quit();
    }
    bufsize <<= 1;
  }
  n_intf = ifconf_req.ifc_len/sizeof(struct ifreq); /* number of interfaces */
  j = 0;	/* internet interface index */
  for (i = 0; i < n_intf; i++) {
    if (ifreq_buf[i].ifr_addr.sa_family != AF_INET) {
      continue;
    }
    initifaddr.s_addr = ((struct sockaddr_in *)&ifreq_buf[i].ifr_addr)
						       ->sin_addr.s_addr;
    if ((unsigned)gd_inet_netof(initifaddr) == (unsigned)LOOPBACKNET) {
      continue;	/* ignore loopback interface */
    }
    ifp = (struct interface *)malloc(sizeof (struct interface));
    if (ifp == 0) {
      syslog(LOG_ERR, "init_if: malloc: out of memory\n");
      quit();
    }
    /* save name for future ioctl calls */
    ifp->int_name = malloc((unsigned)strlen(ifreq_buf[i].ifr_name) + 1);
    if (ifp->int_name == 0) {
      syslog(LOG_ERR, "init_if: malloc: out of memory\n");
      quit();
    }
    /* Get interface flags */
    (void) strcpy(if_info.ifr_name, ifreq_buf[i].ifr_name);
    if (ioctl(init_ifs, SIOCGIFFLAGS, (char *)&if_info)) {
      (void) sprintf(err_message,"if_check: %s: ioctl SIOCGIFFLAGS:\n", ifreq_buf[i].ifr_name);
      p_error(err_message);
      quit();
    }
    /*
     * Mask off flags that we don't understand as some systems use
     * them differently than we do.
     */
    ifp->int_flags = IFF_INTERFACE | (if_info.ifr_flags & IFF_MASK);
    /*
     * if interface is marked down, we will include it and try again
     * later.
     */
    /* get interface metric */
    (void) strcpy(if_info.ifr_name, ifreq_buf[i].ifr_name);
#if	defined(INT_METRIC)
    if (ioctl(init_ifs, SIOCGIFMETRIC, (char *)&if_info) < 0) {
      (void) sprintf(err_message,"init_if: %s: ioctl SIOCGIFMETRIC:\n", ifreq_buf[i].ifr_name);
      p_error(err_message);
      quit();
    }
    ifp->int_metric = (if_info.ifr_metric >= 0) ? if_info.ifr_metric : 0;
#else	defined(INT_METRIC)
    ifp->int_metric =  0;
#endif	defined(INT_METRIC)
    if (ifp->int_flags & IFF_POINTOPOINT) {
      if (ioctl(init_ifs, SIOCGIFDSTADDR, (char *)&if_info) < 0) {
        (void) sprintf(err_message,"init_if: %s: ioctl SIOCGIFDSTADDR:\n", ifreq_buf[i].ifr_name);
        p_error(err_message);
        quit();
      }
      ifp->int_dstaddr = if_info.ifr_dstaddr;
    }
    if (ifp->int_flags & IFF_BROADCAST) {
#ifdef SIOCGIFBRDADDR
      if (ioctl(init_ifs, SIOCGIFBRDADDR, (char *)&if_info) < 0) {
        (void) sprintf(err_message,"init_if: %s: ioctl SIOGIFBRDADDR:\n", ifreq_buf[i].ifr_name);
        p_error(err_message);
        quit();
      }
#ifdef SUN3_3PLUS
      ifp->int_broadaddr = if_info.ifr_addr;
#else
      ifp->int_broadaddr = if_info.ifr_broadaddr;
#endif
#else !SIOCGIFBRDADDR
      /*
       *  Assume 4.2BSD with INADDR_ANY == 0
       */
      ifp->int_broadaddr = ifreq_buf[i].ifr_addr;
      sin = (struct sockaddr_in *)&ifreq_buf[i].ifr_addr;
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
    /* get netmask */
    (void) strcpy(ifp->int_name, ifreq_buf[i].ifr_name);
    ifp->int_addr = ifreq_buf[i].ifr_addr;
#ifdef	SIOCGIFNETMASK 
    if (ioctl(init_ifs, SIOCGIFNETMASK, (char *)&ifreq_buf[i]) < 0) {
      (void) sprintf(err_message,"init_if: %s: ioctl SIOGIFNETMASK:\n", ifreq_buf[i].ifr_name);
      p_error(err_message);
      continue;
    }
    sin = (struct sockaddr_in *)&ifreq_buf[i].ifr_addr;
    ifp->int_subnetmask = ntohl(sin->sin_addr.s_addr);
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
    ifp->int_active_gw = NULL;
		
#ifdef sgi
    /*
     * Add interface to the end of the direct interface list.
     */
    *ifnext = ifp;			/* last pointer points to new if */
    ifnext = &ifp->int_next;		/* re-set pointer to last pointer */
    ifp->int_next = NULL;		/* mark end of list */
#else
    ifp->int_next = ifnet;
    ifnet = ifp;
#endif

    TRACE_INT("\n");
    if_display("init_if", ifp);

    if (doing_rip && (ifp->int_flags & IFF_POINTOPOINT) && (rip_supplier < 0)) {
      rip_supplier = TRUE;
      TRACE_INT("init_if: PointoPoint RIP supplier to: %s\n", ifp->int_name);
    }
    if (doing_hello && (ifp->int_flags & IFF_POINTOPOINT) && (hello_supplier < 0)) {
      hello_supplier = TRUE;
      TRACE_INT("init_if: PointoPoint HELLO supplier to: %s\n", ifp->int_name);
    }
    /*
     * if multiple interfaces with same net number exit as the routing tables
     * assume at most one interface per net.
     * NOT TRUE!  Point-to-point`s are OK.
     */
#ifdef sgi
    for (test = ifnet; test != ifp; test = test->int_next)
       if ((ifp->int_subnet == test->int_subnet) &&
           ((ifp->int_flags|test->int_flags) & IFF_POINTOPOINT) == 0) {
         syslog(LOG_ERR, "init_if: Exit as multiple non PTP");
         syslog(LOG_ERR, "interfaces to net of interface %s\n",
		inet_ntoa(initifaddr));
         quit();
       }
#else
    for (ifp = ifnet->int_next; ifp != 0; ifp = ifp->int_next)
       if ((ifnet->int_subnet == ifp->int_subnet) &&
           ((ifnet->int_flags|ifp->int_flags) & IFF_POINTOPOINT) == 0) {
         syslog(LOG_ERR, "init_if: Exit as multiple non PTP");
         syslog(LOG_ERR, "interfaces to net of interface %s\n",
                 inet_ntoa(initifaddr));
         quit();
       }
#endif
    j++;
  }
  n_interfaces = j;	/* number internet interfaces */
  if (n_interfaces == 0) {
    syslog(LOG_ERR, "if_init: Exit no interfaces\n");
    quit();
  }
  if (doing_rip && (n_interfaces > 1) && (rip_supplier < 0)) {
    rip_supplier = TRUE;
    syslog(LOG_NOTICE, "if_init: Acting as RIP supplier to our direct nets");
    TRACE_TRC("if_init: Acting as RIP supplier to our direct nets\n");
  }
  if (doing_hello && (n_interfaces > 1) && (hello_supplier < 0)) {
    hello_supplier = TRUE;
    syslog(LOG_NOTICE, "if_init: Acting as HELLO supplier to our direct nets");
    TRACE_TRC("if_init: Acting as HELLO supplier to our direct nets\n");
  }
  free((char *)ifreq_buf);
  (void) close(init_ifs);
}
#endif	NSS


/*
 * getsocket gets a socket, retries later if no buffers at present
 */

getsocket(domain, type, protocol)
	int domain, type, protocol;
{
  int retry, getsocks;

  retry = 2;		/* if no buffers a retry might work */
  while ((getsocks = socket(domain, type, protocol)) < 0 && retry--) {
    p_error("getsocket: socket");
    sleep(5);
  }
  if (getsocks < 0) {
    return (-1);
  } else {
    return (getsocks);
  }
}

/*
 * init_egpngh() reads autonomous system number and names of EGP neighbors 
 * from EGPINITFILE .
 * The format is: # comment
 *		  autonomoussystem value
 *		  egpneighbor name
 *		  egpmaxacquire value
 * where name is either a symbolic name in /etc/hosts or an internet address
 * in dot notation and value is the maximum number of EGP neighbors to acquire.
 * All EGP neighbors must be on the same net.
 * A state table is allocated for the neighbor and added to a linked list
 * terminated by a NULL pointer.
 *
 * External variables:
 *   mysystem - autonomous sytem number
 *   maxacq - maximum number of EGP neighbors to acquire
 *   nneigh - # egp neighbors
 *   egpngh - pointer to start of linked list of egp neighbor state tables.
 */

init_egpngh(fp)
	FILE *fp;
{
  char name[MAXHOSTNAMELENGTH+1];
  int	error = FALSE, c;
  struct egpngh	*ngp = NULL, *ngp_last, *ngp_next;
  struct egpngh *parse_egpneighbor();

  nneigh = 0;
  egpngh = NULL;

  rewind(fp);
  while(fscanf(fp, "%s", name) != EOF) {	/* read first word of line and
						   compare to key words */
    if (strcasecmp(name, "autonomoussystem") == 0) {
#if	defined(SSCANF_BROKEN)
      char sysnum[MAXHOSTNAMELENGTH+1];

      if (fscanf(fp, "%s", sysnum) != 1) {
        error = TRUE;
      } else {
        mysystem = atoi(sysnum);
      }
#else	defined(SSCANF_BROKEN)
      if (fscanf(fp, " %d", &mysystem) != 1) {
        error = TRUE;
      }
#endif	defined(SSCANF_BROKEN)
      if (!error) {
        if (sendas) {
          struct as_list *tmp_list;

          for (tmp_list = sendas; tmp_list; tmp_list = tmp_list->next) {
            if (tmp_list->as == mysystem) {
              my_aslist = tmp_list;
            }
          }
        }
      }
    } else if (strcasecmp(name, "egpmaxacquire") == 0) {
#if	defined(SSCANF_BROKEN)
      char maxnum[MAXHOSTNAMELENGTH+1];

      if (fscanf(fp, "%s", maxnum) != 1) {
        error = TRUE;
      } else {
        maxacq = atoi(maxnum);
      }
#else	defined(SSCANF_BROKEN)
      if (fscanf(fp, " %d", &maxacq) != 1) {
        error = TRUE;
      }
#endif	SSCANF_BROKEN
    } else if (strcasecmp(name, "egpneighbor") == 0) {
      if ( ngp = parse_egpneighbor(fp) ) {
#ifdef	notdef
	/*
	 * add to end of egp neighbor table linked list so order same as in
	 * EGPINITFILE, although simpler to add to front. 
	 */
	if (egpngh == NULL) {
		egpngh = ngp;	/* first neighbor */
	} else {
		egpngh_last->ng_next = ngp;
	}
	egpngh_last = ngp;
#endif	notdef
	/*
	 * Add to the EGP neighbor table in sorted order
	 */
	if (egpngh == NULL) {
		egpngh = ngp;	/* first on the list */
		ngp->ng_next = NULL;
	} else if (ngp->ng_addr.s_addr <= egpngh->ng_addr.s_addr) {
		ngp->ng_next = egpngh;
		egpngh = ngp;
	} else {
		ngp_last = egpngh;
		for (ngp_next = egpngh->ng_next; ngp_next; ngp_next = ngp_next->ng_next) {
			if (ngp->ng_addr.s_addr <= ngp_next->ng_addr.s_addr) {
				break;
			}
			ngp_last = ngp_next;
		}
		ngp->ng_next = ngp_next;
		ngp_last->ng_next = ngp;
	}
        nneigh++;
#if SAMENET
        if (nneigh > 1) {
          if (gd_inet_netof(ngp->ng_addr) != gd_inet_netof(egpngh->ng_addr)) {
            syslog(LOG_ERR, "init_egpngh: EGP neighbors on different nets");
            error = TRUE;
          }
        }
#endif SAMENET
      } else {
        error = TRUE;
      }
      (void) ungetc('\n', fp);
    }    /* end else egpneighbor */
    do c = fgetc(fp); while (c != '\n' && c != EOF); /* next line */
  }			/* end while */
  if (mysystem == 0) {
    syslog(LOG_ERR, "init_egpngh: autonomous system # 0\n");
    error = TRUE;
  }
  if ((maxacq == 0) && (nneigh > 0)) {
    maxacq = nneigh;
    TRACE_INT("init_egpngh: egpmaxacquire not specified, %d assumed\n",
              maxacq);
    syslog(LOG_WARNING, "init_egpngh: egpmaxacquire not specified, %d assumed",
              maxacq);
  }
  if (maxacq > nneigh) {
    syslog(LOG_ERR, "init_egpngh: egpmaxacquire = %d > # neighbors = %d\n",
                         maxacq, nneigh);
    error = TRUE;
  }
  if (nneigh == 0) {
    syslog(LOG_ERR, "init_egpngh: no EGP neighbors.\n");
    error = TRUE;
  }
  if (error) {
    syslog(LOG_ERR, "init_egpngh: %s: initialization error\n", EGPINITFILE);
    quit();
  } else {
    TRACE_INT("init_egpngh: autonomoussystem %d\n", mysystem);
    TRACE_INT("init_egpngh: egpmaxacquire %d\n", maxacq);
    for (ngp = egpngh; ngp; ngp = ngp->ng_next) {
      TRACE_INT("init_egpngh: egpneighbor %s", inet_ntoa(ngp->ng_addr));
      if (ngp->ng_flags & NG_INTF) {
        TRACE_INT(" intf %s", inet_ntoa(ngp->ng_myaddr));
      }
      if (ngp->ng_flags & NG_SADDR) {
        TRACE_INT(" source net %s", inet_ntoa(ngp->ng_saddr));
      }
      if (ngp->ng_flags & NG_GATEWAY) {
        TRACE_INT(" gateway %s", inet_ntoa(ngp->ng_gateway));
      }
      if (ngp->ng_flags & NG_METRICIN) {
        TRACE_INT(" metricin %d", ngp->ng_metricin);
      }
      if (ngp->ng_flags & NG_METRICOUT) {
        TRACE_INT(" metricout %d", ngp->ng_metricout);
      }
      if (ngp->ng_flags & NG_ASIN) {
        TRACE_INT(" asin %d", ngp->ng_asin);
      }
      if (ngp->ng_flags & NG_ASOUT) {
        TRACE_INT(" asout %d", ngp->ng_asout);
      }
      if (ngp->ng_flags & NG_AS) {
        TRACE_INT(" as %d", ngp->ng_as);
      }
      if (ngp->ng_flags & NG_DEFAULTOUT) {
        TRACE_INT(" defaultout %d", ngp->ng_defaultmetric);
      }
      if (ngp->ng_flags & NG_DEFAULTIN) {
        TRACE_INT(" defaultin");
      }
      if (ngp->ng_flags & NG_NOGENDEFAULT) {
        TRACE_INT(" nogendefault");
      }
      if (ngp->ng_flags & NG_VALIDATE) {
	TRACE_INT(" validate");
      }
      TRACE_INT("\n");
    }
  }
}

/*
 * Parse an egpneighbor clause in the format:
 *
 *	egpneighbor <address> <options>
 *
 * Where the options are:
 *	metricin <metric>	All incoming nets will be set to this metric
 *	egpmetricout <metric>	All outgoing nets will be set to this metric
 *	ASin <as>		Verify that neighbor uses this AS #
 *	ASout <as>		Use this AS number with this neighbor
 *	AS <as>			AS that is assigned to routes from this neighbor
 *	nodefaultgen		Do not consider this neighbor when
 *				generating internal default
 *	acceptdefault		Allow default in an incoming packet
 *	defaultout		Send internally generated default to
 *				this neighbor
 *	validate		All nets from this neighbor must have
 *				validate statements 
 *	intf <address>		Interface to use when neighbor is off net
 *	sourcenet <net>		IP Source Net that gets polled for
 *	gateway <address>	IP address of local gateway to use instead
 *				gateway specified by neighbor
 */
struct egpngh *parse_egpneighbor(fp)
	FILE *fp;
{
	struct sockaddr_in  egpnghaddr, intf_addr, source_net, gateway_addr;
	struct egpngh *ngp;
	int metricin = 0;
	u_char metricout = 0;
	u_char defaultmetric = 0;
	u_short asin = 0;
	u_short asout = 0;
	u_short as = 0;
	u_int flags = 0;
	int value;
	char buf[BUFSIZ];
	char *option;
	struct keywds {
		char *kw_name;
		int  kw_flags;
	};
	struct keywds *kp;
	static struct keywds keywords[] = {
		{ "metricin",		NG_METRICIN },
		{ "egpmetricout",	NG_METRICOUT },
		{ "ASin", 		NG_ASIN },
		{ "ASout",		NG_ASOUT },
		{ "AS",			NG_AS },
		{ "nogendefault",	NG_NOGENDEFAULT },
		{ "acceptdefault",	NG_DEFAULTIN },
		{ "defaultout", 	NG_DEFAULTOUT },
		{ "validate",		NG_VALIDATE },
		{ "intf",		NG_INTF },
		{ "sourcenet",		NG_SADDR },
		{ "gateway",		NG_GATEWAY },
		{ (char *) 0, 0 } };

	extern char *cap();

	bzero((char *)&egpnghaddr, sizeof(egpnghaddr));
	bzero((char *)&intf_addr, sizeof(intf_addr));
	bzero((char *)&source_net, sizeof(source_net));
	bzero((char *)&gateway_addr, sizeof(gateway_addr));

	if ((fgets(buf, sizeof(buf), fp) == NULL) || ((option = cap(buf)) == NULL)) {
		syslog(LOG_ERR, "parse_egpneighbor: missing neighbor name or address");
		return((struct egpngh *)0);
	}
	if (!getnetorhostname("host", option, &egpnghaddr)) {
		syslog(LOG_ERR, "parse_egpneighbor: invalid neighbor name or address %s\n", option);
		return((struct egpngh *)0);
	}
	while ( (option = cap(buf)) != NULL && (*option != '#') ) {
		for (kp = keywords; kp->kw_name; kp++ ) {
			if ( strcasecmp(kp->kw_name, option) == 0 ) {
				break;
			}
		}
		if ( kp->kw_flags == 0 ) {
			syslog(LOG_ERR,	"parse_egpneighbor: invalid parameter: %s",
				option);
			return((struct egpngh *)0);
		}
		flags |= kp->kw_flags;
		switch (kp->kw_flags) {
			case NG_METRICIN:
				value = parse_numeric(buf, DELAY_INFINITY, "parse_egpneighbor", kp->kw_name, TRUE);
				if ( value < 0) {
					return((struct egpngh *)0);
				}
				metricin = value;
				break;
			case NG_METRICOUT:
				value = parse_numeric(buf, 255, "parse_egpneighbor", kp->kw_name, TRUE);
				if ( value < 0 ) {
					return((struct egpngh *)0);
				}
				metricout = (u_char) value;
				break;
			case NG_DEFAULTOUT:
				value = parse_numeric(buf, 255, "parse_egpneighbor", kp->kw_name, TRUE);
				if ( value < 0 ) {
					return((struct egpngh *)0);
				}
				defaultmetric = (u_char) value;
				break;
			case NG_ASIN:
				value = parse_numeric(buf, (int)65535, "parse_egpneighbor", kp->kw_name, TRUE);
				if ( value < 0 ) {
					return((struct egpngh *)0);
				}
				asin = (u_short) value;
				break;
			case NG_ASOUT:
				value = parse_numeric(buf, (int)65535, "parse_egpneighbor", kp->kw_name, TRUE);
				if ( value < 0 ) {
					return((struct egpngh *)0);
				}
				asout = (u_short) value;
				break;
			case NG_AS:
				value = parse_numeric(buf, (int)65535, "parse_egpneighbor", kp->kw_name, TRUE);
				if ( value < 0 ) {
					return((struct egpngh *)0);
				}
				as = (u_short) value;
				break;
			case NG_INTF:
				if (((option = cap(buf)) == NULL) || (*option == '#') ) {
					syslog(LOG_ERR, "parse_egpneighbor: missing interface name or address");
					return((struct egpngh *)0);
				}
				if (!getnetorhostname("host", option, &intf_addr)) {
					syslog(LOG_ERR, "parse_egpneighbor: invalid interface name or address %s\n", option);
					return((struct egpngh *)0);
				}
				if (if_ifwithaddr((struct sockaddr *)&intf_addr) <= (struct interface *)0) {
					syslog(LOG_ERR, "parse_egpneighbor: invalid interface %s\n", inet_ntoa(intf_addr.sin_addr));
					TRACE_TRC("parse_egpneighbor: invalid interface %s\n", inet_ntoa(intf_addr.sin_addr));
					return((struct egpngh *)0);
				}
				break;
			case NG_SADDR:
				if (((option = cap(buf)) == NULL) || (*option == '#') ) {
					syslog(LOG_ERR, "parse_egpneighbor: missing source net name or address");
					return((struct egpngh *)0);
				}
				if (!getnetorhostname("net", option, &source_net)) {
					syslog(LOG_ERR, "parse_egpneighbor: invalid source net name or address %s\n", option);
					return((struct egpngh *)0);
				}
				break;
			case NG_GATEWAY:
				if (((option = cap(buf)) == NULL) || (*option == '#') ) {
					syslog(LOG_ERR, "parse_egpneighbor: missing gateway name or address");
					return((struct egpngh *)0);
				}
				if (!getnetorhostname("host", option, &gateway_addr)) {
					syslog(LOG_ERR, "parse_egpneighbor: invalid gateway name or address %s\n", option);
					return((struct egpngh *)0);
				}
				if (if_withnet(&gateway_addr) <= (struct interface *)0) {
					syslog(LOG_ERR, "parse_egpneighbor: invalid gateway %s\n", inet_ntoa(gateway_addr.sin_addr));
					TRACE_TRC("parse_egpneighbor: invalid gateway %s\n", inet_ntoa(gateway_addr.sin_addr));
					return((struct egpngh *)0);
				}
				break;
		}
	}
	ngp = (struct egpngh *) malloc(sizeof(struct egpngh));
	if (ngp == NULL) {
		syslog(LOG_ERR, "parse_egpneighbor: malloc: out of memory\n");
		quit();
	}
	ngp->ng_next = NULL;
	ngp->ng_addr = egpnghaddr.sin_addr;
	ngp->ng_flags = flags;
	ngp->ng_metricin = metricin;
	ngp->ng_metricout = metricout;
	ngp->ng_asin = asin;
	ngp->ng_asout = asout;
	ngp->ng_as = as;
	ngp->ng_defaultmetric = defaultmetric;
	ngp->ng_myaddr = intf_addr.sin_addr;
	ngp->ng_saddr = source_net.sin_addr;
	ngp->ng_gateway = gateway_addr.sin_addr;
	return(ngp);
}

/*
 * Given an input buffer, parse the next argument as a numeric with a
 * range of 0 to upper.
 */

parse_numeric(buf, upper, caller, keyword, missing)
char *buf, *keyword, *caller;
int upper, missing;
{
  char *option;
  int temp;
  extern char *cap();

  if ( (option = cap(buf)) == NULL ) {
    if (missing) {
      syslog(LOG_ERR, "%s: missing value for parameter %s", caller, keyword);
      TRACE_TRC("%s: missing value for parameter %s\n", caller, keyword);
    }
    return(-1);
  } else if ( (sscanf(option, "%d", &temp) != 1) || ( temp < 0 ) || ( temp > upper) ) {
    syslog(LOG_ERR, "%s: invalid value (%s) for parameter %s", caller, option, keyword);
    TRACE_TRC("%s: invalid value (%s) for parameter %s\n", caller, option, keyword);
    return(-1);
  } 
  return(temp);
}


/*
 * Given host or net name or internet address in dot notation assign the
 * internet address in byte format.
 * source is ../routed/startup.c with minor changes to detect syntax errors.
 *
 * Unfortunately the library routine inet_addr() does not detect mal formed
 * addresses that have characters or byte values > 255.
 */

getnetorhostname(type, name, sin)
	char *type, *name;
	struct sockaddr_in *sin;
{
  if (strcasecmp(type, "net") == 0) {
    struct netent *np;
    u_long n;

    if ((n = gd_inet_isnetwork(name)) == (u_long) -1) {
      if (np = getnetbyname(name)) {
        if (np->n_addrtype != AF_INET) {
          return (0);
        }
        n = np->n_net;
      } else {
        return(0);
      }
    }
#ifdef	notdef
    if (n < AMSK) {
      n <<= IN_CLASSA_NSHIFT;
    } else if (n < (BMSK << 8)) {
      n <<= IN_CLASSB_NSHIFT;
    } else if (n < (CMSK << 16)) {
      n <<= IN_CLASSC_NSHIFT;
    }
#else	notdef
    if (n) {
      while (!(n & 0xff000000)) {
      	n <<= 8;
      }
    }
#endif	notdef
    sin->sin_family = AF_INET;
    sin->sin_addr = gd_inet_makeaddr(n, 0, FALSE);
    return (1);
  }				
  if (strcasecmp(type, "host") == 0) {
    struct hostent *hp;

    if ((sin->sin_addr.s_addr = inet_addr(name)) == (u_long) -1) {
      if (hp = gethostbyname(name)) {
        bcopy(hp->h_addr, (char *)&sin->sin_addr, hp->h_length);
        sin->sin_family = hp->h_addrtype;
        return (1);
      }
      return(0);
    }
    sin->sin_addr.s_addr = inet_addr(name);
    sin->sin_family = AF_INET;
    return (1);
  }
  return (0);
}

/*
 * init_egp() initializes the state tables for each potential EGP neighbor
 */

init_egp()
{
  reg	egpng		*ngp;
  struct	interface	*ifp;
  struct	sockaddr_in	dst;

  egpsleep = MINHELLOINT + HELLOMARGIN;
  rt_maxage = RT_MINAGE;		/* minimum time before routes are
                                           deleted when not updated */
  for (ngp = egpngh; ngp != NULL; ngp = ngp->ng_next) {
    ngp->ng_state = NGS_ACQUISITION;
    ngp->ng_sid = 1;
    ngp->ng_retry = 0;
    ngp->ng_hint = MINHELLOINT + HELLOMARGIN;
    ngp->ng_htime = gatedtime;
    ngp->ng_rcmd = 0;
    ngp->ng_aslist = (struct as_list *)NULL;

    /* Check that I have a direct net to neighbor */
    if ( (ngp->ng_flags & NG_INTF) == 0 ) {
      if ( (ngp->ng_flags & NG_GATEWAY) == 0 ) {
	dst.sin_family = AF_INET;
	dst.sin_addr = ngp->ng_addr;
	if ((ifp = if_withnet(&dst) ) != NULL) {
	  ngp->ng_myaddr = sock_inaddr(&ifp->int_addr);
	} else {
	  TRACE_INT("init_egp: no direct net to neighbor %s\n",
			       inet_ntoa(ngp->ng_addr));
	  quit();
	}
      } else {
	dst.sin_family = AF_INET;
	dst.sin_addr = ngp->ng_gateway;
	if ((ifp = if_withnet(&dst) ) != NULL) {
	  ngp->ng_myaddr = sock_inaddr(&ifp->int_addr);
	} else {
	  TRACE_INT("init_egp: no direct net to neighbor %s's gateway\n",
			       inet_ntoa(ngp->ng_addr));
	  quit();
	}
      }
    }

    if ((ngp->ng_flags & NG_SADDR) == 0) {
      ngp->ng_saddr = gd_inet_makeaddr(gd_inet_wholenetof(ngp->ng_addr),0,FALSE);
    }

  }
}


#ifndef	NSS
/*
 * initialize all the RIP stuff...
 */

rip_init()
{
  int ripinits;
#ifdef vax11c
  static struct servent staticsp;
#endif

  sp = getservbyname("router", "udp");
  if (sp == NULL) {
    syslog(LOG_ERR,"No service for router available\n");
    quit();
  }

/*\
 *	Fix "911" bug in which the port gets corrupted. This is
 *	happening because getservbyname() returns a pointer to
 *	an internal static area, and other users of getserv...()
 *	(like NAMED) change this after us.
\*/
#ifdef vax11c
  bcopy(sp, &staticsp, sizeof(staticsp));
  sp = &staticsp;
#endif
  addr.sin_family = AF_INET;
  addr.sin_port = sp->s_port;
  ripinits = get_rip_socket(AF_INET, SOCK_DGRAM, &addr);
  if (ripinits < 0)
    quit();
  return(ripinits);
}

get_rip_socket(domain, type, sin)
	int domain, type;
	struct sockaddr_in *sin;
{
  int ripsocks;
  unsigned int on = 1;

  if ((ripsocks = socket(domain, type, 0)) < 0) {
    p_error("get_rip_socket: socket");
    return (-1);
  }
#ifdef SO_BROADCAST
  if (setsockopt(ripsocks, SOL_SOCKET, SO_BROADCAST, (char *)&on, sizeof(on)) < 0) {
    (void) close(ripsocks);
    return (-1);
  }
#endif SO_BROADCAST
#ifdef SO_RCVBUF
  on = 48*1024;
  if (setsockopt(ripsocks, SOL_SOCKET, SO_RCVBUF, (char *)&on, sizeof(on)) < 0) {
    p_error("get_rip_socket: setsockopt SO_RCVBUF");
  } else {
    TRACE_TRC("get_rip_socket: RIP receive buffer size set to %dK\n", on/1024);
  }
#endif SO_RCVBUF
  if (bind(ripsocks, (struct sockaddr *)sin, sizeof (*sin)) < 0) {
    p_error("get_rip_socket: bind");
    (void) close(ripsocks);
    return (-1);
  }
  return (ripsocks);
}


get_egp_socket(domain, type, protocol)
	int domain, type, protocol;
{
  int egpsocks;
  unsigned int value;

  if ((egpsocks = socket(domain, type, protocol)) < 0) {
    p_error("get_egp_socket: socket");
    return (-1);
  }
#ifdef SO_RCVBUF
  value = 4*EGPMAXPACKETSIZE;
  if (setsockopt(egpsocks, SOL_SOCKET, SO_RCVBUF, (char *)&value, sizeof(value)) < 0) {
    p_error("get_egp_socket: setsockopt SO_RCVBUF");
  } else {
    TRACE_TRC("get_egp_socket: EGP receive buffer size set to %dK\n", value/1024);
  }
  value = 4*EGPMAXPACKETSIZE;
  if (setsockopt(egpsocks, SOL_SOCKET, SO_SNDBUF, (char *)&value, sizeof(value)) < 0) {
    p_error("get_egp_socket: setsockopt SO_SNDBUF");
  } else {
    TRACE_TRC("get_egp_socket: EGP send buffer size set to %dK\n", value/1024);
  }
#endif SO_RCVBUF
  return (egpsocks);
}
#endif	NSS
