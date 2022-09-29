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
static char *rcsid = "$Header: /proj/irix6.5.7m/isms/eoe/cmd/bsd/gated/RCS/rt_control.c,v 1.1 1989/09/18 19:02:13 jleong Exp $";
#endif	not lint

#include "include.h"

/*
 * control_lookup() looks up a restricted network for an exact match.
 */

struct restrictlist *
control_lookup(table, dst)
	int table;
	struct sockaddr_in *dst;
{
  register struct restrictlist *rt;
  register struct restricthash *rh;
  register unsigned hash;
  struct afhash h;

  if (dst->sin_family != AF_INET)
    return ((struct restrictlist *)NULL);

  (*afswitch[dst->sin_family].af_hash)(dst, &h);
  hash = h.afh_nethash;
  rh = &rt_restrict[hash & ROUTEHASHMASK];

  for (rt = rh->rt_forw; rt != (struct restrictlist *)rh; rt = rt->rt_forw) {
    if (rt->rhash != hash)
      continue;
    if ((equal(&rt->rdst, dst)) && (rt->flags & table))
      return (rt);
  }
  return ((struct restrictlist *)NULL);
}

/*
 * control_add() adds a route to either the announce or forbid control tables.
 */

control_add(table, control_net)
	int table;			/* interior or exterior table */
	struct restrictlist *control_net;
{
  register struct restrictlist *rt;
  struct restricthash *rh;
  unsigned hash;
  struct afhash h;

  if (control_net->rdst.sin_family != AF_INET)
    return;

  (*afswitch[control_net->rdst.sin_family].af_hash)(&control_net->rdst, &h);
  hash = h.afh_nethash;
  rh = &rt_restrict[hash & ROUTEHASHMASK];

  rt = (struct restrictlist *)malloc(sizeof(*rt));
  if (rt == 0) {
    syslog(LOG_ERR, " control_add: malloc: out of memory\n");
    return;
  }
  rt->rhash = hash;
  rt->rdst = control_net->rdst;
  rt->regpmetric = control_net->regpmetric;
  rt->rproto = control_net->rproto;
  rt->flags = table;
  bcopy((char *)control_net->rintf, (char *)rt->rintf, sizeof(rt->rintf));
  insque(rt, rh);
}

/*
 *	rt_resinit():
 *		Parse the following clauses:
 *			announce, noannounce, announcehost, noannouncehost
 *			listen, donotlisten, listenhost, donotlistenhost
 *
 */

rt_resinit(fp)
	FILE *fp;
{
  char *cap();
  char *keyword;
  char *savekeyword;
  char *option;
  char buf[BUFSIZ];
  struct 	sockaddr_in addr_in;
  struct  restrictlist res_net;
  register struct bits *p;
  int	fatalerror = FALSE, intfc, error = FALSE, line = 0, protocols;
  int i;
  char nethost[MAXHOSTNAMELENGTH+1];
  struct interface *tifp;
  int global_flag_set = 0;

  rewind(fp);
  announcethesenets = 0;
  donotannounce = 0;
  glob_announcethesenets = 0;
  glob_donotannounce = 0;
  donotlisten = 0;
  islisten = 0;

  while (fgets(buf, sizeof(buf), fp) != NULL) {
    line++;
    bzero((char *)&addr_in, sizeof(addr_in));
    bzero((char *)&res_net, sizeof(res_net));
    if (error == TRUE && fatalerror == FALSE) {
      fatalerror = TRUE;
    }
    error = FALSE;
    if ((buf[0] == '#') || (buf[0] == '\n')) {
      continue;
    }
    if ( ((keyword = cap(buf)) == (char *)NULL) || (*keyword == '#') ) {
      continue;
    }
    if (((strcasecmp(keyword, "announce") == 0) ||
        (strcasecmp(keyword, "announcehost") == 0) ||
        (strcasecmp(keyword, "noannounce") == 0) ||
        (strcasecmp(keyword, "noannouncehost") == 0) ||
        (strcasecmp(keyword, "listen") == 0) ||
        (strcasecmp(keyword, "listenhost") == 0) ||
        (strcasecmp(keyword, "donotlisten") == 0) ||
        (strcasecmp(keyword, "donotlistenhost") == 0))) {
      savekeyword = calloc((unsigned)strlen(keyword)+1, sizeof(char));
      (void) strcpy(savekeyword, keyword);
      if ((option = cap(buf)) == (char *)NULL) {
        printf("rt_resinit: unexpected EOLN at line %d\n",line);
        error = TRUE;
        continue;
      }
      if ((strcasecmp(savekeyword, "announcehost") == 0) ||
          (strcasecmp(savekeyword, "noannouncehost") == 0) ||
          (strcasecmp(savekeyword, "listenhost") == 0) ||
          (strcasecmp(savekeyword, "donotlistenhost") == 0)) {
        (void) strcpy(nethost, "host");
      } else {
        (void) strcpy(nethost, "net");
      }
      if (getnetorhostname(nethost, option, &addr_in) <= 0) {
        printf("rt_resinit: invalid %s %s at line %d\n",
                    nethost, option, line);
        error = TRUE;
        continue;
      }
      res_net.rdst = addr_in;
      if ((option = cap(buf)) == (char *)NULL) {
        printf("rt_resinit: unexpected EOLN at line %d\n",line);
        error = TRUE;
        continue;
      }
      if (((strcasecmp(savekeyword, "listenhost") == 0) ||
          (strcasecmp(savekeyword, "listen") == 0)) &&
          (strcasecmp(option,"gateway") != 0)) {
        printf("rt_resinit: 'gateway' expected at line %d\n",line);
        error = TRUE;
        continue;
      } else {
        if ((strcasecmp(savekeyword, "listenhost") != 0) &&
            (strcasecmp(savekeyword, "listen") != 0)) {
          if (strcasecmp(option,"intf") != 0) {
            printf("rt_resinit: 'intf' expected at line %d\n",line);
            error = TRUE;
            continue;
          }
        }
      }
      intfc = 0;
      while ((option = cap(buf)) != (char *)NULL) {
        if (strcasecmp(option, "proto") == 0) {
          if (intfc == 0) {
	    printf("rt_resinit: No interface list at line %d\n",line);
            error = TRUE;
          }
          break;
        }
        if (strcasecmp(option, "all") == 0) {
          if (intfc != 0) {
            printf("rt_resinit: 'all' unexpected at line %d\n",line);
            error = TRUE;
          } else {
            if ((option = cap(buf)) == (char *)NULL) {
              printf("rt_resinit: unexpected EOLN at line %d\n",line);
              error = TRUE;
            } else {
              if (strcasecmp(option, "proto") != 0) {
                printf("rt_resinit: 'proto' expected at line %d\n",line);
                error = TRUE;
              }
            }
          }
          global_flag_set++;
          break;
        }
        if (!getnetorhostname("host",option,&addr_in)) {
          printf("rt_resinit: invalid host %s at line %d\n",
                          option, line);
          error = TRUE;
          break;
        }
        if ((strcasecmp(savekeyword, "listenhost") != 0) &&
            (strcasecmp(savekeyword, "listen") != 0)) {
          if ((tifp = if_ifwithaddr((struct sockaddr *)&addr_in)) <= (struct interface *)0) {
            printf("rt_resinit: invalid interface %s at line %d\n",
                          option, line);
            error = TRUE;
            break;
          }
        }
        if (intfc >= MAXINTERFACE) {
          printf("rt_resinit: max number of interfaces at line %d\n",line);
          error = TRUE;
          break;
        }
        res_net.rintf[intfc] = addr_in.sin_addr.s_addr;
        intfc++;
        if ((strcasecmp(savekeyword, "noannounce") == 0) ||
            (strcasecmp(savekeyword, "noannouncehost") == 0)) {
          if (tifp->int_flags & IFF_ANNOUNCE) {
            printf("rt_resinit: announce and noannounce specified ");
            printf("for interface %s at line %d\n", option, line);
            error = TRUE;
	    fatalerror = TRUE;
            break;
          } else {
            tifp->int_flags |= IFF_NOANNOUNCE;
          }
        } else if ((strcasecmp(savekeyword, "announce") == 0) || (strcasecmp(savekeyword, "announcehost") == 0)) {
          if (tifp->int_flags & IFF_NOANNOUNCE) {
            printf("rt_resinit: announce and noannounce specified ");
            printf("for interface %s at line %d\n", option, line);
            error = TRUE;
	    fatalerror = TRUE;
            break;
          } else {
            tifp->int_flags |= IFF_ANNOUNCE;
          }
        }
      }
      if (error == TRUE) {
        continue;
      }
      if (option == (char *)NULL) {
        printf("rt_resinit: unexpected EOLN at line %d\n",line);
        error = TRUE;
      }
      protocols = 0;
      while ((option = cap(buf)) != (char *)NULL) {
        if (strcasecmp(option, "egpmetric") == 0) {
          if (protocols == 0) {
            printf("rt_resinit: No protocol list at line %d\n",line);
            error = TRUE;
          }
          break;
        }
        if (strcasecmp(option, "all") == 0) {
          if (protocols != 0) {
            printf("rt_resinit: 'all' unexpected at line %d\n",line);
            error = TRUE;
          } else {
            if ((option = cap(buf)) == (char *)NULL) {
              printf("rt_resinit: unexpected EOLN at line %d\n",line);
              error = TRUE;
            } else {
              if (strcasecmp(option, "egpmetric") != 0) {
                printf("rt_resinit: 'egpmetric' expected at line %d\n",line);
                error = TRUE;
              }
            }
          }
#ifndef	NSS
          res_net.rproto = (RTPROTO_RIP|RTPROTO_HELLO|RTPROTO_EGP);
#else	NSS
          res_net.rproto = (RTPROTO_IGP|RTPROTO_EGP);
#endif	NSS
          break;
        }
#ifdef	NSS
        if (strcasecmp(option, "igp") == 0)
          res_net.rproto |= RTPROTO_IGP;
#else	NSS
        if (strcasecmp(option, "rip") == 0)
          res_net.rproto |= RTPROTO_RIP;
#endif	NSS
        else if (strcasecmp(option, "hello") == 0)
          res_net.rproto |= RTPROTO_HELLO;
        else if (strcasecmp(option, "egp") == 0)
          res_net.rproto |= RTPROTO_EGP;
        else if ((strcasecmp(option, "#") == 0) || (strcasecmp(option, "\n") == 0)) {
          if (res_net.rproto & RTPROTO_EGP) {
            printf("rt_resinit: 'egpmetric' expected at line %d\n",line);
            error = TRUE;
          }
          break;
        }
        else {
          printf("rt_resinit: unknown protocol %s at line %d\n",
                     option, line);
          error = TRUE;
          break;
        }
        protocols++;
      }
      if (error == TRUE)
        continue;
      if (((option == (char *)NULL) || (strcasecmp(option, "#") == 0)) &&
          ((protocols == 0) || (res_net.rproto & RTPROTO_EGP))) {
        printf("rt_resinit: unexpected EOLN at line %d\n",line);
        error = TRUE;
        continue;
      }
      if (res_net.rproto & RTPROTO_EGP) {
        if ((option = cap(buf)) == (char *)NULL) {
          printf("rt_resinit: unexpected EOLN at line %d\n",line);
          error = TRUE;
          continue;
        }
        res_net.regpmetric = atoi(option);
        if ((res_net.regpmetric >= 255) || (res_net.regpmetric < 0)) {
          printf("rt_resinit: invalid EGP metric %d at line %d\n",
                    res_net.regpmetric, line);
          error = TRUE;
          continue;
        }
      }
      if ((strcasecmp(savekeyword, "announce") == 0) ||
          (strcasecmp(savekeyword, "announcehost") == 0)) {
        if (res_net.rproto & RTPROTO_EGP) {
          n_remote_nets++;
        }
        if (global_flag_set != 0) {
          glob_announcethesenets++;
        }
        announcethesenets++;
        control_add(RT_ANNOUNCE, &res_net);
      }
      if ((strcasecmp(savekeyword, "noannounce") == 0) ||
          (strcasecmp(savekeyword, "noannouncehost") == 0)) {
        if (global_flag_set != 0) {
          glob_donotannounce++;
        }
        donotannounce++;
        control_add(RT_NOANNOUNCE, &res_net);
      }
      if ((strcasecmp(savekeyword, "donotlisten") == 0) ||
          (strcasecmp(savekeyword, "donotlistenhost") == 0)) {
        donotlisten++;
        control_add(RT_NOLISTEN, &res_net);
      }
      if ((strcasecmp(savekeyword, "listen") == 0) ||
          (strcasecmp(savekeyword, "listenhost") == 0)) {
        islisten++;
        control_add(RT_SRCLISTEN, &res_net);
      }
      TRACE_INT("rt_resinit: %s %s intf ",
		    savekeyword, inet_ntoa(res_net.rdst.sin_addr));
      if ( global_flag_set ) {
      	TRACE_INT("all ");
      } else {
        for(i = 0; i < intfc; i++) {
          TRACE_INT("%s ",gd_inet_ntoa(res_net.rintf[i]));
        }
      }
      TRACE_INT("proto ");
      for (p = protobits; p->t_bits; p++) {
        if ((res_net.rproto & p->t_bits) == 0)
          continue;
        TRACE_INT("%s ", p->t_name);
      }
      if ( res_net.rproto & RTPROTO_EGP) {
        TRACE_INT("egpmetric %d", res_net.regpmetric);
      }
      TRACE_INT("\n");
      global_flag_set = 0;
      free((char *)savekeyword);
    }
  }
  if (((glob_announcethesenets != 0) && ((glob_donotannounce !=0) ||
       (donotannounce != 0))) || ((glob_donotannounce != 0) &&
       (announcethesenets != 0))) {
    printf("The \"Do not announce\" list and \"announce\" list both\n");
    printf("initialized globally.  You can only have one or the other\n");
    printf("applied globally.  You can have both on different interfaces.\n");
    fatalerror = TRUE;
  }
  if (fatalerror == TRUE) {
    printf("rt_resinit: %s: initialization error\n", EGPINITFILE);
    quit();
  }
}

/*
 *	rt_ASinit():
 *		Parse the following clauses:
 *			announcetoAS, noannouncetoAS
 *			validAS
 *
 */

rt_ASinit(fp, init_flag)
	FILE *fp;
	int init_flag;
{
  char *cap();
  char *keyword;
  char buf[BUFSIZ];
  struct 	sockaddr_in addr_in;
  struct  restrictlist res_net;
  int	fatalerror = FALSE, error = FALSE, line = 0;
  int value;
  struct as_list *my_sendas = NULL;
  struct as_valid *my_validas = NULL;
  struct as_entry *tmp_entry, *ptr_entry;
  struct as_list *tmp_list, *free_sendas;
  struct as_valid *tmp_valid, *free_validas;

  rewind(fp);

  while (fgets(buf, sizeof(buf), fp) != NULL) {
    line++;
    bzero((char *)&addr_in, sizeof(addr_in));
    bzero((char *)&res_net, sizeof(res_net));
    if (error == TRUE && fatalerror == FALSE) {
      fatalerror = TRUE;
    }
    error = FALSE;
    if ((buf[0] == '#') || (buf[0] == '\n')) {
      continue;
    }
    if ( ((keyword = cap(buf)) == (char *)NULL) || (*keyword == '#') ) {
      continue;
    }
    if ((strcasecmp(keyword, "announcetoAS") == 0) ||
        (strcasecmp(keyword, "noannouncetoAS") == 0)) {
      struct as_list *ptr_list;
      int as_type;

      if (strncasecmp(keyword, "no",2) == 0) {
        as_type = AS_DONOTSEND;
      } else {
        as_type = AS_SEND;
      }

      if ( (tmp_list = (struct as_list *) calloc(1, sizeof(struct as_list))) == NULL ) {
        TRACE_TRC("rt_ASinit: out of memory\n");
        syslog(LOG_EMERG, "rt_ASinit: out of memory");
        fatalerror = TRUE;
        break;
      }
      if ((value = parse_numeric(buf, (int)65535, "rt_ASinit", keyword, FALSE)) < 0) {
        free((char *)tmp_list);
        error = TRUE;
        continue;
      }
      tmp_list->as = value;
      tmp_list->flags = as_type;
      for ( ptr_list = my_sendas; ptr_list; ptr_list = ptr_list->next) {
        if (ptr_list->as == tmp_list->as) {
          TRACE_EXT("rt_ASinit: duplicate %sannouncetoAS in line %d, %sannouncetoAS already specified for AS %d\n",
                    tmp_list->flags & AS_DONOTSEND ? "no" : "",
                    line,
                    ptr_list->flags & AS_DONOTSEND ? "no" : "",
                    tmp_list->as);
          syslog(LOG_ERR,"rt_ASinit: duplicate %sannouncetoAS in line %d, %sannouncetoAS already specified for AS %d",
                    tmp_list->flags & AS_DONOTSEND ? "no" : "",
                    line,
                    ptr_list->flags & AS_DONOTSEND ? "no" : "",
                    tmp_list->as);
          error = TRUE;
          continue;
        }
      }
      if ( ( (keyword = cap(buf)) != (char *)NULL ) && 
           ( !strcasecmp(keyword,"restrict") || !strcasecmp(keyword, "norestrict")) ) {
        if ( strncasecmp(keyword, "no", 2) ) {
          tmp_list->flags |= AS_RESTRICT;
        }
      } else {
        syslog(LOG_ERR, "rt_ASinit: invalid keyword: %s", keyword);
        TRACE_TRC("rt_ASinit: invalid keyword: %s\n", keyword);
        free((char *)tmp_list);
        error = TRUE;
        continue;
      }
      if ( ((keyword = cap(buf)) != (char *)NULL) && (strcasecmp(keyword,"ASlist") == 0) ) {
        while( (value = parse_numeric(buf, (int)65535, "rt_ASinit", "ASlist", FALSE)) >= 0 ) {
          if ((tmp_entry = (struct as_entry *) calloc(1, sizeof(struct as_entry))) == NULL ) {
            TRACE_TRC("rt_ASinit: out of memory\n");
            syslog(LOG_EMERG, "rt_ASinit: out of memory");
            fatalerror = TRUE;
            break;
          }
          tmp_entry->as = value;
          tmp_entry->flags = tmp_list->flags;
          tmp_entry->next = tmp_list->as_ptr;
          tmp_list->as_ptr = tmp_entry;
        }
      } else {
        syslog(LOG_ERR, "rt_ASinit: invalid keyword: %s", keyword);
        TRACE_TRC("rt_ASinit: invalid keyword: %s\n", keyword);
        free((char *)tmp_list);
        error = TRUE;
        continue;
      }
      tmp_list->next = my_sendas;
      my_sendas = tmp_list;
      TRACE_EXT("rt_ASinit: %sannouncetoAS %d %srestrict ASlist",
                as_type == AS_DONOTSEND ? "no" : "",
                tmp_list->as,
                (tmp_list->flags & AS_RESTRICT) ? "no" : "");
      for ( tmp_entry = tmp_list->as_ptr; tmp_entry ; tmp_entry = tmp_entry->next) {
        TRACE_EXT(" %d", tmp_entry->as);
      }
      TRACE_EXT("\n");
    } else if (strcasecmp(keyword, "validAS") == 0) {
      if ( (tmp_valid = (struct as_valid *) calloc(1, sizeof(struct as_valid))) == NULL ) {
        TRACE_TRC("rt_ASinit: out of memory\n");
        syslog(LOG_EMERG, "rt_ASinit: out of memory");
        fatalerror = TRUE;
        break;
      }
      if ((keyword = cap(buf)) == (char *)NULL) {
        TRACE_TRC("rt_ASinit: unexpected EOLN at line %d\n",line);
        syslog(LOG_ERR,"rt_ASinit: unexpected EOLN at line %d\n",line);
        free((char *)tmp_valid);
        error = TRUE;
        continue;
      }
      if (getnetorhostname("net", keyword, &addr_in) <= 0) {
        TRACE_TRC("rt_ASinit: invalid net %s at line %d\n", keyword, line);
        syslog(LOG_ERR, "rt_ASinit: invalid net %s at line %d\n", keyword, line);
        free((char *)tmp_valid);
        error = TRUE;
        continue;
      }
      tmp_valid->dst = addr_in.sin_addr;
      if ( ((keyword = cap(buf)) != (char *)NULL) && (strcasecmp(keyword,"AS") == 0) ) {
        if ((value = parse_numeric(buf, (int)65535, "rt_ASinit", "validAS", FALSE)) < 0) {
          free((char *)tmp_valid);
          error = TRUE;
          continue;
        }
        tmp_valid->as = value;
      } else {
        syslog(LOG_ERR, "rt_ASinit: invalid keyword: %s", keyword);
        TRACE_TRC("rt_ASinit: invalid keyword: %s\n", keyword);
        free((char *)tmp_valid);
        error = TRUE;
        continue;
      }
      if ( ((keyword = cap(buf)) != (char *)NULL) && (strcasecmp(keyword,"metric") == 0) ) {
        if ((value = parse_numeric(buf, DELAY_INFINITY, "rt_ASinit", "metric", FALSE)) < 0) {
          free((char *)tmp_valid);
          error = TRUE;
          continue;
        }
        tmp_valid->metric = value;
      } else {
        syslog(LOG_ERR, "rt_ASinit: invalid keyword: %s", keyword);
        TRACE_TRC("rt_ASinit: invalid keyword: %s\n", keyword);
        free((char *)tmp_valid);
        error = TRUE;
        continue;
      }
      tmp_valid->next = my_validas;
      my_validas = tmp_valid;
      TRACE_EXT("rt_ASinit: validAS %s AS %d metric %d\n",inet_ntoa(tmp_valid->dst), tmp_valid->as, tmp_valid->metric);
    }
  }
  if (!fatalerror) {
    free_sendas = sendas;
    sendas = my_sendas;
    free_validas = validas;
    validas = my_validas;
  } else {
    if (init_flag) {
      printf("rt_ASinit: %s: initialization error\n", EGPINITFILE);
      quit();
    } else {
      printf("rt_ASinit: %s: reinitialization error - changes ignored\n", EGPINITFILE);
    }
    free_sendas = my_sendas;
    free_validas = my_validas;
  }

  for ( ; free_validas; free_validas = tmp_valid) {
    tmp_valid = free_validas->next;
    free((char *)free_validas);
  }
  for ( ; free_sendas; free_sendas = tmp_list) {
    tmp_list = free_sendas->next;
    ptr_entry = free_sendas->as_ptr;
    for ( ; ptr_entry; ptr_entry = tmp_entry) {
      tmp_entry = ptr_entry->next;
      free((char *)ptr_entry);
    }
    free((char *)free_sendas);
  }
  /*
   *	If we are re-reading the AS info, correct any pointers to the AS
   *	list 
   */
  if (!init_flag) {
    if (mysystem) {
      struct as_list *tmp_list;

      for (tmp_list = sendas; tmp_list; tmp_list = tmp_list->next) {
        if (tmp_list->as == mysystem) {
          my_aslist = tmp_list;
        }
      }
    }
    if (doing_egp) {
      struct egpngh *ngp;

      for (ngp = egpngh; ngp; ngp = ngp->ng_next) {
        if (ngp->ng_asin) {
          struct as_list *tmp_list;

          for (tmp_list = sendas; tmp_list; tmp_list = tmp_list->next) {
            if (tmp_list->as == ngp->ng_asin) {
              ngp->ng_aslist = tmp_list;
              break;
            }
          }
        }
      }
    }
  }
}


/*
 * given a route entry, protocol, and interface, check and see
 * if the route is valid.  If it is, return a 1.  If not, return 0.
 */
is_valid(rt, protocol, intfp)
	struct rt_entry *rt;
	int protocol;
	struct interface *intfp;
{
  int intfc;
  u_long intfcaddr;

  if (intfp == (struct interface *)NULL) {
    return(0);
  }

  intfcaddr = (intfp->int_flags & IFF_POINTOPOINT) ?
		     sock_inaddr(&intfp->int_dstaddr).s_addr :
		     sock_inaddr(&intfp->int_addr).s_addr;

  if ((glob_announcethesenets != 0) || (intfp->int_flags & IFF_ANNOUNCE)) {
    if (rt->rt_announcelist == (struct restrictlist *)NULL) {
      return(0);
    }
    if ((rt->rt_announcelist->rproto & protocol) == 0) {
      return(0);
    }
    intfc = 0;
    while ((rt->rt_announcelist->rintf[intfc] != 0) &&
           (intfc < MAXINTERFACE)) {
      if (rt->rt_announcelist->rintf[intfc] == intfcaddr) {
        break;
      }
      intfc++;
    }
    if ((rt->rt_announcelist->rintf[intfc] == 0) &&
        (intfc != 0))
      return(0);
  } else {
    if ((glob_donotannounce != 0) || (intfp->int_flags & IFF_NOANNOUNCE)) {
      if (rt->rt_noannouncelist != (struct restrictlist *)NULL) {
        if (rt->rt_noannouncelist->rproto & protocol) {
          intfc = 0;
          while ((rt->rt_noannouncelist->rintf[intfc] != 0) &&
                 (intfc < MAXINTERFACE)) {
            if (rt->rt_noannouncelist->rintf[intfc] == intfcaddr) {
              break;
            }
            intfc++;
          }
          if ((rt->rt_noannouncelist->rintf[intfc] != 0) || (intfc == 0)) {
            return(0);
          }
        }
      }
    }
  }
  return(1);
}

#ifndef	NSS
/*
 *  given a route entry, protocol, interface pointer, and
 *  an address, check and see if the INPUT route is ok. Return
 *  0 if the input packet is NOT valid. Return 1 if it is valid.
 */
is_valid_in(rt, protocol, intfp, whofrom)
	struct rt_entry *rt;
	int protocol;
	struct interface *intfp;
	struct sockaddr_in *whofrom;
{
  int intfc;
  u_long intfcaddr;

  intfcaddr = (intfp->int_flags & IFF_POINTOPOINT) ?
		     sock_inaddr(&intfp->int_dstaddr).s_addr :
		     sock_inaddr(&intfp->int_addr).s_addr;

  if (intfp == (struct interface *)NULL)
    return(0);

  /*
   * Since the kernel doesn't know about these neat restrictions on
   * input, return VALID for a redirect.  This will keep the gated
   * tables more consistent with the kernel's.
   */
  if (protocol & RTPROTO_REDIRECT)
    return(1);

  if (donotlisten != 0) {
    if (rt->rt_listenlist != (struct restrictlist *)NULL) {
      if (rt->rt_listenlist->rproto & protocol) {
        intfc = 0;
        while ((rt->rt_listenlist->rintf[intfc] != 0) &&
               (intfc < MAXINTERFACE)) {
          if (rt->rt_listenlist->rintf[intfc] == intfcaddr)
            break;
          intfc++;
        }
        if ((rt->rt_listenlist->rintf[intfc] != 0) ||
            (intfc == 0))
          return(0);
      }
    }
  }
  if (islisten != 0) {
    if (rt->rt_srclisten != (struct restrictlist *)NULL) {
      if ((rt->rt_srclisten->rproto & protocol) == 0)
        return(0);
      intfc = 0;
      while ((rt->rt_srclisten->rintf[intfc] != 0) &&
             (intfc < MAXINTERFACE)) {
        if (rt->rt_srclisten->rintf[intfc] == whofrom->sin_addr.s_addr)
          break;
        intfc++;
      }
      if ((rt->rt_srclisten->rintf[intfc] == 0) && (intfc != 0))
        return(0);
    }
  }
  return(1);
}
#endif	NSS

/*
 *  Determine if it is allowed to send the specified
 *  route to the specified AS.  Return values are:
 *  
 *  2	OK to send - announce clauses apply
 *  1	OK to send - announce clauses do not apply
 *  0	restricted from sending
 *  -1  no restrictions
 *
 */

sendAS(aslist, as)
struct as_list *aslist;
u_short as;
{
  if (aslist) {
    struct as_entry *tmp_entry;

    for (tmp_entry = aslist->as_ptr; tmp_entry; tmp_entry = tmp_entry->next) {
      if ( tmp_entry->as == as ) {
        if (tmp_entry->flags & AS_DONOTSEND) {
          return(0);
        }
        if (tmp_entry->flags & AS_SEND) {
          if (tmp_entry->flags & AS_RESTRICT) {
            return(2);
          } else {
            return(1);
          }
        }
      }
    }
    if (aslist->flags & AS_SEND) {
      return(0);
    } else {
      if (aslist->flags & AS_RESTRICT) {
        return(2);
      } else {
        return(1);
      }
    }
  } else {
    return(-1);
  }
}
/*
 *	Given a network number return 1 if it is not a valid network,
 *	i.e it is a martian.  This does not check for DEFAULT or
 *	non-class A,B or C networks.
 */

is_martian(network)
struct in_addr network;
{
	struct advlist *mptr;

	for ( mptr = martians; mptr; mptr = mptr->next ) {
		if ( network.s_addr == mptr->destnet.s_addr ) {
			return(1);
		}
	}
	return(0);
}


martian_init()
{
	char **ptr;
	struct sockaddr_in net;
	struct advlist *mptr;

	MARTIAN_NETS;		/* Expand the table of martian nets */

	martians = (struct advlist *) 0;

	for ( ptr = martian_nets; *ptr; ptr++ ) {
		if ((mptr = (struct advlist *) calloc(1, sizeof(struct advlist))) == NULL) {
			syslog(LOG_EMERG, "martian_init: out of memory");
			quit();
		}
		if (!getnetorhostname("host", *ptr, &net)) {
			syslog(LOG_ERR, "is_martian: invalid net %s in martian table\n", *ptr);
			continue;
		}
		mptr->destnet = net.sin_addr;
		mptr->next = martians;
		martians = mptr;
	}
}

