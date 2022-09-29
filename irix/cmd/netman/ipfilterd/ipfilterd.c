/*
 * ipfilterd.c
 * Daemon to filter incoming IP packets and return a verdict to IP,
 *
 * Copyright 1990, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */
#define _BSD_SIGNALS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bstring.h>
#include <getopt.h>
#include <syslog.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <netinet/igmp.h>
#include <net/if.h>
#include <netinet/ipfilter.h>
#include <sys/capability.h>
#include "exception.h"
#include "expr.h"
#include "protocol.h"
#include "snooper.h"

#define MAX_FILTERERRS	10
#define CMDBUFSIZ	256	/* maximum command line length */
#define MAXFILTERS	1000	/* maximum number of discrete filters */

int maxfilters = MAXFILTERS;

char filename[] = "/etc/ipfilterd.conf";	/* filter configuration file */

struct parsefilter {
	int		expr_flag;		/* filter on protocols?	   */
	struct expr 	*expr;			/* protocol filter 	   */
	int		if_flag;		/* filter on interface?	   */
	char		interface[IFNAMSIZE];	/* interface name 	   */
	u_char		accept;			/* verdict 		   */
};

/* 
 * daemon cache of configured filters 
*/
struct parsefilter	filtercache[MAXFILTERS]; 

/*
 * Stuff for debugging; uses SYSLOG after becoming daemon, printfs before
 */

unsigned int		pollcount = 0;		/* number times daemon polled */
unsigned int		foundfverdict = 0;	/* number times a packet      */
						/* matched a filter           */
unsigned int		foundiverdict = 0;	/* number times a packet      */
						/* matched a n I/F filter     */
unsigned int		defaultverdict = 0;	/* number times a packet      */
						/* passes by default 	      */
						/* (doesn't match any filter) */
int			debug = 0;		/* turn on verbose syslogging */

/*
 * Supported protocols (the IP family), indented to show nested layers
 */
/*
 * pv: 500662 - changed the supported protocols to include
 * ip, icmp, igmp, udp, and tcp only
 */
extern Protocol 
            ip_proto,
                icmp_proto, igmp_proto, tcp_proto, udp_proto;

Protocol *protocols[] = {
            &ip_proto,
                &icmp_proto, &igmp_proto, &tcp_proto, &udp_proto, 0
};

char f_interface[IFNAMSIZE];	/* used for building filters */
char p_interface[IFNAMSIZE];	/* inbound packet interface */

/*
 * Forward declarations
 */

void parsefile(Snooper *sn, FILE *fp);
unsigned char filterpacket(Snooper *sn, char *packet);
unsigned int scanprotoname(char *prot, char **namep, Protocol **prp);

/* supporting SIGHUP */
static void config(int);
Snooper			*sn = 0;
int			fd;	/* the ipfilter device file */

int main(int argc, char **argv)
{
	struct ipfilter_ioctl	ipfioctl;
	int			pid;
	int			loopcount = 0;
	int			i;
	Protocol		**prp;
	cap_t			ocap;
	cap_value_t		cap_network_mgt = CAP_NETWORK_MGT;
	struct sigvec		sv;
	int 			mask;

        int ch;

        while ((ch = getopt(argc, argv, "d")) != EOF) {
                switch (ch) {
                case 'd':
                        debug++;
                        break;
                default:
                        fprintf(stderr, "usage: ipfilterd [ -d ]\n");
                        exit(1);
                }
        }

	/*
	 * Open the filter configuration file, initialize the supported 
	 * protocols, and compile the filters.
	 */
	for (prp = protocols; *prp; prp++) {
		if (!pr_init(*prp))
			exc_raise(0, "cannot initialize %s protocol",
				  (*prp)->pr_name);
	}

	config(0);
	
	/*
 	 * get rid of the controlling tty so we can become a daemon
 	 */
	if (setpgrp() < 0)
		perror("setpgrp");
	if ((i = open("/dev/tty", O_RDWR)) >= 0) {
		(void)ioctl(i, TIOCNOTTY, (char *)0);
		(void)close(i);
	}
	/*
 	 * become a daemon
 	 */
	if((pid = fork()) < 0) {
		perror("ipfilterd: fork:");
		exit(1);
	}
	if (pid) 				/* parent */
		exit(0);		
	/*
 	 * Child; now in background and owned by init(). 
	 * Close open file descriptors and initialize with syslogd.
	 */
	exc_openlog("ipfilterd", LOG_PID, LOG_DAEMON);
	if(debug)
		syslog(LOG_DAEMON, "ipfilterd active");

	/* handle the HUP signal - reread ipfilterd.conf */
	memset((void *) &sv, '\0', sizeof (sv));
	sv.sv_mask = 0;
	sv.sv_handler = config;
	sigvec(SIGHUP, &sv, (struct sigvec *)0);
	/*
 	 * Post ipfilter ioctl()s forever. Log any errors. 
 	 * The issuing of the ioctl completes the filter request from 
	 * the kernel; the completion of the ioctl provides a new request 
	 * When there are no pending requests, the ioctl() sleeps in the kernel.
	 * If the sleep is interrupted, post a fresh ioctl (no verdict).
 	 */
	bzero((caddr_t)&ipfioctl, sizeof (struct ipfilter_ioctl));
	for ( ; ; ) {
		ocap = cap_acquire(1, &cap_network_mgt);
		if (ioctl(fd, ZIOCIPFILTER, &ipfioctl) < 0 ) {
			cap_surrender(ocap);
			bzero((caddr_t)&ipfioctl,
				sizeof (struct ipfilter_ioctl));
			loopcount++;
			if(loopcount == MAX_FILTERERRS) {
				syslog(LOG_ERR, "ipfilterd loopcount: %m");
			}
			continue;
		}
		cap_surrender(ocap);
		loopcount = 0;
		/*
 		 * Process the new filter request and loop again.
 		 */
		strncpy((caddr_t)p_interface, (char *)ipfioctl.ifname, 
			 IFNAMSIZE);
                mask = sigblock(sigmask(SIGHUP)); 
		ipfioctl.verdict = (u_char)filterpacket(sn, 
				                      (char *)ipfioctl.packet);
		sigsetmask(mask); 
	}
}

static      ExprSource 	src;

void
parsefile(Snooper *sn, FILE *fp)
{
	char 		buffer[CMDBUFSIZ];
	char 		*keyword;
	char 		*ifname;
	int		iface = 0;
	u_char		acceptit;
	int		i = 0;
	ExprError	err;

	openlog("ipfilterd", LOG_PID, LOG_DAEMON); /* ...but not daemon yet */

	bzero((caddr_t)filtercache, (sizeof(struct parsefilter) * MAXFILTERS));
	src.src_line = 1;	/* avoid deletion in ex_dropsrc */
        src.src_path = filename;
        while(fgets(buffer, CMDBUFSIZ, fp)) {
                src.src_line++;
		if (strstr(buffer, "-i") != (char *) 0)
			iface = 1;
		else
			iface = 0;
                keyword = strtok(buffer, " \t");
                if (keyword == NULL || *keyword == '#') {
                        continue;
		}
		if (strcmp(keyword, "define") == 0) {

        		char *expr, *name;
        		Protocol *pr;
        		int explen, namlen;
        		ExprSource dsrc;
			char *prot;

        		prot = strtok(0, " \t\n");
        		if (prot == 0) {
                		exc_perror(0, "missing macro definition");
                		continue;
        		}
        		expr = strtok(0, "\n");
        		if (expr == 0) {
                		exc_perror(0, "missing expression after %s",
					   prot);
                		continue;
        		}
        		if (!scanprotoname(prot, &name, &pr)) {
				syslog(LOG_ERR, "can't scan proto %s", prot);
                		continue;
			}
        		namlen = strlen(name);
        		name = strndup(name, namlen);
        		explen = strlen(expr);
        		expr = strndup(expr, explen);
        		dsrc.src_path = src.src_path;
        		dsrc.src_line = src.src_line;
        		dsrc.src_buf = expr;
        		pr_addmacro(pr, name, namlen, expr, explen, &dsrc);
			continue;
		}
                else if (strcmp(keyword, "accept") == 0) {
                        acceptit = IPF_ACCEPTIT;
			if (debug)
				printf("accept keyword\n");
		}
                else if (strcmp(keyword, "reject") == 0) {
                        acceptit = IPF_DROPIT;
			if (debug)
				printf("reject keyword\n");
		}
		else if (strcmp(keyword, "grab") == 0) {
			acceptit = IPF_GRABIT;
			if (debug)
				printf("grab keyword\n");
		}
		else {
			printf("unknown keyword %s", keyword);
			continue;
		}
		if (iface) {
			if (debug)
				printf("interface specified\n");
        		ifname = strtok(0, " \t");      /* get to "-i" token */
        		ifname = strtok(0, " \t");  /* get to interface name */
		        	/* pull out interface info */
			strncpy(f_interface, ifname, IFNAMSIZE);
			if (debug)
				printf("interface name %s\n", f_interface); 
		}
                src.src_buf = strtok(0, "\n");
                if ((src.src_buf == NULL) && (iface == 0)) {
			exc_perror(0, "missing expression"); 
                        continue;
                }
		if (debug) {
			if (src.src_buf != NULL)
				printf("filter: %s\n", src.src_buf);
			else
				printf("interface filter only\n");
		}
		if (src.src_buf != NULL) {	/* protocol part */
                	if (!(sn_compile(sn, &src, &err))) {
                        	exc_perror(0, exc_message); 
                        	continue;
                	}
			filtercache[i].expr_flag = 1;
                	filtercache[i].expr = sn->sn_expr;
		} else  filtercache[i].expr_flag = 0;	/* no proto; I/F only */
		if (iface) {
			filtercache[i].if_flag = 1;
			strncpy(filtercache[i].interface, f_interface, 
			   	IFNAMSIZE);
		}
                filtercache[i].accept = acceptit;
		i++;
		sn->sn_expr = 0;
		if (i >= MAXFILTERS) {
			if (debug)
				printf("Maximum number of filters (%d) reached; any remaining filters ignored.\n", MAXFILTERS);
			else
				fprintf(stderr, "Maximum number of filters (%d) reached; any remaining filters ignored.\n", MAXFILTERS);
			syslog(LOG_ERR, "Maximum number of filters (%d) reached; any remaining filters ignored.", MAXFILTERS);
			break;
		}
        };
	maxfilters = i;

	closelog();
}


void
ipfilter_verdict_print(char *buf, struct parsefilter *filter)
{
    struct ip *ip = (struct ip *)buf;
    int hlen, sport=0, dport=0;
  
    hlen = ip->ip_hl<<2;
    switch(ip->ip_p) {
    case IPPROTO_TCP:
	    sport = ((struct tcphdr *)(buf+hlen))->th_sport;
	    dport = ((struct tcphdr *)(buf+hlen))->th_dport;
	    break;
    case IPPROTO_UDP:
	    sport = ((struct udphdr *)(buf+hlen))->uh_sport;
	    dport = ((struct udphdr *)(buf+hlen))->uh_dport;
	    break;
    case IPPROTO_ICMP:
	    sport = (u_short)(((struct icmp *)(buf+hlen))->icmp_type);
	    dport = (u_short)(((struct icmp *)(buf+hlen))->icmp_code);
	    break;
    case IPPROTO_IGMP: 
	    sport = (u_short)(((struct igmp *)(buf+hlen))->igmp_type);
	    dport = (u_short)(((struct igmp *)(buf+hlen))->igmp_code);
	    break;
    default:
	    break;
    }
    syslog(LOG_DEBUG, "iface=%s,p=0x%x,src=0x%x,dst=0x%x,spt/ty=%d,dpt/cd=%d,verdict=%d\n",
	   filter->interface, ip->ip_p, ip->ip_src.s_addr, ip->ip_dst.s_addr,
	   sport, dport, filter->accept);
}

unsigned char
filterpacket(Snooper *sn, char *packet)
{
        SnoopPacket *sp;
        char buf[sizeof(SnoopHeader) + IPFILTERSIZE];
        struct ip *ip;
        int len, i;

        sp = (SnoopPacket *) buf;
	sp->sp_hdr.snoop_packetlen = IPFILTERSIZE;
	sp->sp_hdr.snoop_flags = 0;
        bcopy(packet, sp->sp_data, IPFILTERSIZE);
        ip = (struct ip *) packet;
        len = (int)ip->ip_hl << 2;
        switch (ip->ip_p) {
          case IPPROTO_TCP:
		len += sizeof(struct tcphdr);
		break;
          case IPPROTO_UDP:
		len += sizeof(struct udphdr);
		break;
          case IPPROTO_ICMP:
		len += sizeof(struct icmp);
		break;
	  case IPPROTO_IGMP:
		len += sizeof(struct igmp);
		break;
	 default:
		break;
        }
        for (i = 0; i < maxfilters; i++) {
		/* check interface first */
		if(filtercache[i].if_flag) {	 /* if filtering on interface */
		    	if (strncmp(p_interface, filtercache[i].interface,
				    strlen(p_interface)) != 0) 
				continue;  	    /* no match; try next one */
		}
		if (!filtercache[i].expr_flag) {	   /* I/F filter only */
			if(debug)
				syslog(LOG_DEBUG,
					"packet verdict from I/F %d",
					++foundiverdict);
                        return(filtercache[i].accept);
		}
                if(pr_test(sn->sn_rawproto, filtercache[i].expr, sp, len, sn)) {
			if(debug) {
				syslog(LOG_DEBUG,
				       "packet verdict from filters %d",
				       ++foundfverdict);
				if (debug > 1) 
					ipfilter_verdict_print(packet,
							     &filtercache[i]);
			}
                        return(filtercache[i].accept);
		}
        }	
	if (debug)
		syslog(LOG_DEBUG, "packet dropped by default %d",
			++defaultverdict);
        return (u_char)(IPF_DROPIT);
}

unsigned int
scanprotoname(char *prot, char **namep, Protocol **prp)
{
        char *name;
        Protocol *pr;

        name = strchr(prot, '.');
        if (name == 0) {
                syslog(LOG_ERR, "missing protocol before %s", prot);
                return 0;
        }
        *name++ = '\0';
        pr = findprotobyname(prot, name-prot-1);
        if (pr == 0) {
                syslog(LOG_ERR, "unknown protocol %s", prot);
                return 0;
        }
        *namep = name;
        *prp = pr;
        return 1;
}

static void
config(int sig)
{
	FILE 			*fltrs;
	int			i;
    
	if (sig == SIGHUP) {
		syslog(LOG_INFO, "received SIGHUP: reconfiguring");
	}
	
	if ((fltrs = fopen(filename, "r")) == 0) {
		perror("ipfilterd: open conf file");
		exit(1);
	}
	
	if (sn) {
		sn_destroy(sn);
		for (i = 0; i < maxfilters; i++) {
			ex_destroy(filtercache[i].expr);
		}
	}
	sn = nullsnooper(&ip_proto);	
	parsefile(sn, fltrs);
	if(maxfilters == 0) {
		fprintf(stderr,"ipfilterd: no filter expressions in %s\n",
			filename);
		exit(1);
	}
	if (debug)
		printf("maxfilters = %d\n", maxfilters);
	fclose(fltrs);
	
	/*
	 * Open /dev/ipfilter to turn filtering on and to respond to 
	 * filter requests from kernel.
	 */
	if (sig == SIGHUP) {
		close(fd);		/* flush kernel filters */
	}
	if ((fd = open ("/dev/ipfilter", O_RDWR)) < 0) {
		perror("ipfilterd: open of /dev/ipfilter failed: ");
		exit(1);
	}
}    


