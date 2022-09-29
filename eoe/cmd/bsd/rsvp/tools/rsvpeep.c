/*
 *  @(#) $Id: rsvpeep.c,v 1.7 1998/12/09 19:22:33 michaelk Exp $
 */
/********************************************************************
 *
 *            rsvpeep - Display RSVP status packets
 *                                                                          
 *              Written by: Bob Braden (braden@isi.edu)
 *              USC Information Sciences Institute
 *              Marina del Rey, California
 *		November 1993                                   
 *                                                                          
 *  Copyright (c) 1993 by the University of Southern California
 *  All rights reserved.
 *
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation in source and binary forms for non-commercial purposes
 *  and without fee is hereby granted, provided that the above copyright
 *  notice appear in all copies and that both the copyright notice and
 *  this permission notice appear in supporting documentation. and that
 *  any documentation, advertising materials, and other materials related
 *  to such distribution and use acknowledge that the software was
 *  developed by the University of Southern California, Information
 *  Sciences Institute.  The name of the University may not be used to
 *  endorse or promote products derived from this software without
 *  specific prior written permission.
 *
 *  THE UNIVERSITY OF SOUTHERN CALIFORNIA makes no representations about
 *  the suitability of this software for any purpose.  THIS SOFTWARE IS
 *  PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,
 *  INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *  Other copyrights might apply to parts of this software and are so
 *  noted when applicable.
 *
 ********************************************************************/

/* Older versions of rsvpd send junk in FF reports which may crash
 * this program. Drive defensively...
 */
#define BAD_FF_REPORTS
 
/*                            rsvpeep.c
 *
 *      Display routine for status packets from RSVP daemon.  This is a
 *	simple ASCII-based display program, an alternative to the
 *	 window-based dmap program.
 *
 *      rsvpeep listens for status summary messages to a specific
 *      multicast/unicast address and port, and displays either all
 *	status packets it receives, or else a specific subset.
 */

#include <cap_net.h>
#include "rsvp_daemon.h"
#include "rapi_lib.h"
#include "rsvp_mstat.h"

char *version = "R3.3  May 96";



/*
 *  Global control variables
 *
 */
int Xdebug= 0;
u_long src_host = 0;
int Time_Stamp = 0;

#define MAX_NAMES 20
char *Names[MAX_NAMES];
int Num_names = 0;
char lastName[128] = "";

Session_IPv4 last_session;	/* initialized to zero below */


#define ADDR_NEQ(a, b) ((* (u_long *)&(a)->sess_destaddr != \
				*(u_long *)&(b)->sess_destaddr)|| \
			((a)->sess_destport != (b)->sess_destport))

#define filtsptoa(x)	(Obj_Class(x) != class_NULL)? fmt_filtspec(x):"*/*[*]"
#define fspectoa(x)	fmt_flowspec((Obj_Class(x) != class_NULL)? (x):NULL)
#define ftspectoa(x)	(Obj_Class(x) != class_NULL)? fmt_tspec(x): "TS[]"

char Stat_addr[64];

char *Heading = "\
 Style  Iface     Next/Prev Hop    Filterspec          Flowspec\n";

int udpsock = -1;
struct sockaddr_in sin;

#define NIFACES 8
#define NSTATS 32

/* Forward and external declarations
 */
u_long resolve_name();

extern char *fmt_hostname(), *fmt_filtspec(), *fmt_flowspec(), *fmt_tspec();
extern void ntoh_flowspec(FLOWSPEC *), ntoh_tspec(SENDER_TSPEC *);
#define ip_convert(addr)  fmt_hostname(&(addr))

void	recvstatus(void);
void	print_Path(PathInfo_t *, int);
void	print_Resv(ResvInfo_t *, int);
void	print_session(Session_IPv4 *, POLICY_DATA *);
int	wrong_name(char *);
char   *argscan(int *, char ***);
void	hexf(FILE *,  char *, int);
int	ntoh_info(RSVPInfo_t *);
int	ntoh_session_ipv4(Session_IPv4 *);
int	ntoh_policydata(POLICY_DATA *);
int	ntoh_filterspec(FILTER_SPEC *);


char *Type_name[] = {"", "PATH  ", "RESV  ", "PATH-ERR", "RESV-ERR",
			 "PATH-TEAR", "RESV-TEAR"};
void
Usage() {
    fprintf(stderr, 
  "Usage: rsvpeep [-x] [-n] [ -d <mcast addr>:<port> ] [ node-name ] ...\n");
}

int
main(argc, argv)
    int argc ;
    char **argv ;
    {
    char  *argscan(), *rmsuffix(), *tp;
    u_int32_t host;
    u_int16_t port;
    struct timeval timenow;
    struct ip_mreq mreq;
    u_char ttlval = STAT_MCAST_TTL;
    int one = 1, i;
    extern IP_NumOnly;
    
    bzero(&last_session, sizeof(Session_IPv4));
    strcpy(Stat_addr, STAT_DEST_ADDR);
    IP_NumOnly = 0;

    if (--argc > 0 && **++argv == '?') {
        Usage() ;
        exit(0) ;
    }

    while (argc > 0 && **argv == '-') {
	switch (argv[0][1]) {
        
	case 'x':   /* -x -- Debug flag  */
		Xdebug = 1 ;
		break ;
       
	case 'd':   /* -d -- Destination for status packets <mcast addr>:<port>  */
		strcpy(Stat_addr, argscan(&argc, &argv));
		break ;

	case 'n':   /* -n -- Suppress conversion of host addrs to names */
		IP_NumOnly = 1;
		break;
     
	case 't':  /* -t: timestamp arriving packets */
		Time_Stamp = 1;
		break;

	default:    
		Usage() ;
		exit(1) ;
	};
	argv++;
	argc--;
    }

    if (argc > MAX_NAMES) {
	printf("Too many names\n");
	exit(1);
    }
    while (argc > 0) {
	Names[Num_names++] = *argv++;
	argc--;
    }

	/* 
	 *  Convert data collection identifier/mcast group: <host>.<port>
	 */
    if (tp = rmsuffix(Stat_addr)) {
        port = htons((u_int16_t)atoi(tp));
        host = resolve_name(Stat_addr);
    }
    else {
	port = htons((u_int16_t)atoi(*argv));
	host = INADDR_ANY;
    }

    /*
     *  Setup socket to wait for data packets
     */
    bzero((char *) &sin, sizeof(struct sockaddr_in));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = host;
    sin.sin_port = port;
    if ((udpsock = socket(AF_INET, SOCK_DGRAM, 0))< 0) {
	perror("Socket Error") ;
	exit(1) ;
    }
    if (setsockopt(udpsock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one))){
        perror("sockopt");
        exit(1);
    }
    if (bind(udpsock,(struct sockaddr *)&sin,sizeof(struct sockaddr_in)) < 0) {
	perror("Bind error");
	exit(1);
    }
    if (IN_MULTICAST(ntoh32(host))) {
        * (u_long *) &mreq.imr_multiaddr = host;
        * (u_long *) &mreq.imr_interface = INADDR_ANY;
        if (0 > cap_setsockopt(udpsock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                        &mreq, sizeof(mreq))) {
                perror("Can't add to multicast group");
                exit(1);
        }
    }

    gettimeofday(&timenow, NULL) ; 

    printf("rsvpeep starting  %s\n", ctime(&timenow.tv_sec));
    printf(Heading);

    for (;;) {
	recvstatus();
    }

}


/*
 *  recvstatus():  Receive UDP packets and display status
 */
void
recvstatus()
{
	struct sockaddr_in from;
	int fromlen = sizeof(struct sockaddr_in);
	char buff[STAT_MSG_SIZE];
	RSVPInfo_t	*Infop = (RSVPInfo_t *) buff;
	struct timeval timenow;
	int n;

	n = recvfrom(udpsock, buff, sizeof(buff), 0,
		     (struct sockaddr *)&from, &fromlen);

	if (n < 0) {
		perror("recvfrom");
		exit(1);
	}
	/* If this is not a node we want, return.
	 * Else parse and display the packet.
	 */
	if (src_host && from.sin_addr.s_addr != src_host) return; /* XXX */

#if BYTE_ORDER == LITTLE_ENDIAN
	if (ntoh_info(Infop))
	    return;
#endif
	switch (Infop->type) {	/* first fullword is type */
            case TYPE_RSVP_VPATH:
		if (wrong_name(Infop->name)) return;
			 /* Wanted particular node, and this is not it. */

		if (Xdebug) {
			printf("RCVD PATH:\n");
			hexf(stdout, buff, n);
			fflush(stdout);
		}
		if (Time_Stamp) {
			gettimeofday(&timenow, NULL) ; 
			printf("\n%.19s.%06ld\n",
				ctime(&timenow.tv_sec), timenow.tv_usec);
		}
		if ((lastName[0] == '\0')||strcmp(Infop->name, lastName)) {
			bzero(&last_session, sizeof(Session_IPv4));
			printf("\nNODE= %s\n", Infop->name);
		}
		strcpy(lastName, Infop->name);
		print_Path(&Infop->Session_un.pathInfo, Infop->nSession);
		break;

            case TYPE_RSVP_VRESV:
		if (wrong_name(Infop->name)) return;
			 /* Wanted particular node, and this is not it. */
		if (Xdebug) {
			printf("RCVD RESV:\n");
			hexf(stdout, buff, n);
		}
		if (Time_Stamp) {
			gettimeofday(&timenow, NULL) ; 
			printf("\n%.19s.%06ld\n",
				ctime(&timenow.tv_sec), timenow.tv_usec);
		}

		if ((lastName[0] == '\0')||strcmp(Infop->name, lastName)) {
			bzero(&last_session, sizeof(Session_IPv4));
			printf("\nNODE= %s\n", Infop->name);
		}
		strcpy(lastName, Infop->name);
		print_Resv(&Infop->Session_un.resvInfo, Infop->nSession);
		break;

	    default:
		fprintf(stderr, "Unknown RSVP mstat pkt type %d from host %s\n",
				Infop->type, inet_ntoa(from.sin_addr));
		if (Xdebug)
			hexf(stdout, buff, n);
		break;
	}		
}



/*
 *  Display Path State from vector of n per-session data items
 */
void
print_Path(sesp, n)
	PathInfo_t *sesp;
        int n;
	{
	PathSender_t *Pp;
	int i, j;

	if (n == 0) {
		printf("  PATH  (Session Gone)\n");
		return;
	}
	for (i= 0; i<n; i++) {
		if (ADDR_NEQ(&sesp->path_session, &last_session))
			print_session(&sesp->path_session, NULL /* XXX */);
		last_session = sesp->path_session;

		Pp = &sesp->path_sender;
		for (j=0; j < sesp->nSender; j++) {
			FilterSpec *fip = (FILTER_SPEC *) 
						Next_Object(&Pp->pathS_policy);
			FlowSpec   *fsp =  (FLOWSPEC *)  Next_Object(fip);

			printf("  %4.4s  %-9.9s %-16.16s ",
				(j==0)?"PATH":"\"  ",
				Pp->pathS_in_if,
				ip_convert(Pp->pathS_phop));
			printf("%-22s \t%s\n", filtsptoa(fip), ftspectoa(fsp));
			Pp = (PathSender_t *) Next_Object(fsp);
		}
		sesp = (PathInfo_t *)Pp;
	}
}

#define WFp(rp) ((ResvWF_t *)(rp))
#define FFp(rp) ((ResvFF_t *)(rp))
#define SEp(rp) ((ResvSE_t *)(rp))
                                             
/*
 *  Display reservations state from vector of session data
 */
void
print_Resv(sesp, n)
	ResvInfo_t *sesp;	/* First session */
	int n;			/* Number of sessions */
	{
	char *rp;
	int i, j, k;
	FILTER_SPEC *filtp;
	struct in_addr ia;
	if (n == 0) {
		printf("  Resv  (Session gone)\n");
		return;
	}
	for (i= 0; i<n; i++) {
		int		style = sesp->resv_style;
		Authentication	*ap = &sesp->resv_policy;

		if (ADDR_NEQ(&sesp->resv_session, &last_session))
			print_session(&sesp->resv_session, ap);
		last_session = sesp->resv_session;

		rp = (char *) Next_Object(ap);
		for (j=0; j < sesp->nStruct; j++) {
			switch (style) {

			case RAPI_RSTYLE_WILDCARD:
				filtp = (FILTER_SPEC *)
					Next_Object(&WFp(rp)->WF_flowspec);

				printf("   WF   %-9.9s %-16.16s %s %s\n",
					WFp(rp)->WF_if,
					ip_convert(WFp(rp)->WF_nexthop),
					filtsptoa(filtp),
					fspectoa(&WFp(rp)->WF_flowspec));
				rp = (char *) Next_Object(filtp);
				break;

			case RAPI_RSTYLE_FIXED:
				filtp = (FILTER_SPEC *)
					Next_Object(&FFp(rp)->FF_flowspec);

				printf("   FF   %-9.9s %-16.16s %-.22s \t%s\n",
					FFp(rp)->FF_if,
#ifdef BAD_FF_REPORTS
				       	inet_ntoa(FFp(rp)->FF_nexthop),
#else
					ip_convert(FFp(rp)->FF_nexthop),
#endif
					filtsptoa(filtp),
				        fspectoa(&FFp(rp)->FF_flowspec));
				rp = (char *) Next_Object(filtp);
				break;

			case RAPI_RSTYLE_SE:
				filtp = (FILTER_SPEC *)
					Next_Object(&SEp(rp)->SE_flowspec);
				printf("   SE   %-9.9s %-16.16s %-.22s \t%s\n",
					SEp(rp)->SE_if,
					ip_convert(SEp(rp)->SE_nexthop),
					filtsptoa(filtp),
				        fspectoa(&SEp(rp)->SE_flowspec));
				filtp = (FILTER_SPEC *) Next_Object(filtp);
				for (j = 1; j < SEp(rp)->nSE_filts; j++) {
					printf("   SE   %26s %-.22s\n",
							"", filtsptoa(filtp));
					filtp = (FILTER_SPEC *) 
							Next_Object(filtp);
				}
				rp = (char *) filtp;
				break;
				

			default:
				break;
			}
		}
		sesp= (ResvInfo_t *) rp;
	}
}

void
print_session(sadrp, authp)
	Session_IPv4 *sadrp;
	POLICY_DATA *authp;
	{
	struct timeval timenow;
	struct timezone tz;

	gettimeofday(&timenow,NULL ) ; 
	printf("SESSION (dest) = %s:%d  %.19s.%06ld  Auth= %s\n",
		ip_convert(sadrp->sess_destaddr),
		sadrp->sess_destport,
		ctime(&timenow.tv_sec), timenow.tv_usec,
		"(none)" /* XXXX */
		);
}

int
wrong_name( cp)
	char *cp;
	{
	int i;

	if (Num_names == 0) return(0);
	for (i=0; i<Num_names; i++)
		if (!strcmp(cp, Names[i])) return(0);
        return(1);
}

char *argscan(argcp, argvp)
    int *argcp;
    register char ***argvp;
    {
    register char *cp;
    
    if (*(cp = 2+**argvp)) 
        return(cp);
    else if  (--*argcp > 0)
        return(*++*argvp);
    Usage();
    exit(1);
    /* keep compiler happy - mwwang */
    return 0;
}
   
void
hexf(fp,  p, len)                
    FILE *fp;
    char *p ;
    register int len ;
    {
    char *cp = p;
    int   wd ;
    u_long  temp ;
    
    while (len > 0) {
        fprintf(fp, "x%2.2x: ", cp-p);
        for (wd = 0; wd<4; wd++)  {
            memcpy((char *) &temp, cp, sizeof(u_long) );
            if (len > 4) {
                fprintf(fp, " x%8.8lx", temp) ;
                cp += sizeof(long);
                len -= sizeof(long);
                }
            else {
                fprintf(fp, " x%*.*x", 2*len, 2*len, temp);
		len = 0;
                break ;
            }
        }
        fprintf(fp, "\n") ;
    }
}

/*
 * Convert RSVPInfo_t datastructure from network to local format
 * return 0 success, -1 fail
 */
int
ntoh_info(RSVPInfo_t *ip)
{
    int i, j;
    ResvInfo_t *rp;	/* declare early for compiler - mwang */

    NTOH32(ip->type);
    NTOH32(ip->nSession);

    switch(ip->type) {
    case TYPE_RSVP_VPATH: {
	PathInfo_t *pp = &ip->Session_un.pathInfo;
	PathSender_t *sp;
	char *p;

	for (i = 0; i < ip->nSession; i++) {
	    if (ntoh_session_ipv4(&pp->path_session))
		return -1;
	    NTOH32(pp->path_R);
	    NTOH32(pp->nSender);

	    sp = &pp->path_sender;
	    for (j = 0; j < pp->nSender; j++) {
		/* pathS_phop stays in network format.. */
		NTOH32(sp->pathS_routes);
		NTOH32(sp->pathS_ttd);
		p = (char *)&sp->pathS_policy;
		if (ntoh_policydata((POLICY_DATA *) p))
		    return -1;
		p = (char *)Next_Object(p);
		if (ntoh_filterspec((FILTER_SPEC *) p))
		    return -1;
		p = (char *)Next_Object(p);
		NTOH16(Obj_Length(p));
		ntoh_tspec((SENDER_TSPEC *) p);
		p = (char *)Next_Object(p);
		sp = (PathSender_t *) p;
	    }
	    pp = (PathInfo_t *)sp;
	}
	return 0;
    }
	
    case TYPE_RSVP_VRESV: {
	int i, j;
	char *p;

	rp = &ip->Session_un.resvInfo;

	NTOH32(rp->resv_style);
	NTOH32(rp->resv_R);
	NTOH32(rp->nStruct);
	if (ntoh_session_ipv4(&rp->resv_session))
	    return -1;
	if (ntoh_policydata(&rp->resv_policy))
	    return -1;

	switch (rp->resv_style) {
	case RAPI_RSTYLE_WILDCARD: {
	    ResvWF_t *sp = (ResvWF_t *) Next_Object(&rp->resv_policy);

	    for (i = 0; i < rp->nStruct; i++) {
		NTOH32(sp->WF_ttd);
	    	/* NTOH32(sp->WF_nexthop); */
		NTOH16(Obj_Length(&sp->WF_flowspec));
	    	ntoh_flowspec(&sp->WF_flowspec);
		p = (char *)Next_Object(&sp->WF_flowspec);
		if (ntoh_filterspec((FILTER_SPEC *) p))
		    return -1;
		sp = (ResvWF_t *) Next_Object(p);
	    }
	    return 0;
	}

	case RAPI_RSTYLE_FIXED: {
	    ResvFF_t *sp = (ResvFF_t *) Next_Object(&rp->resv_policy);

	    for (i = 0; i < rp->nStruct; i++) {
		NTOH32(sp->FF_ttd);
		/* nexthop stays in network format for printer */
	    	/* NTOH32(sp->FF_nexthop); */
		NTOH16(Obj_Length(&sp->FF_flowspec));
	    	ntoh_flowspec(&sp->FF_flowspec);
		p = (char *)Next_Object(&sp->FF_flowspec);
		if (ntoh_filterspec((FILTER_SPEC *) p))
		    return -1;
		sp = (ResvFF_t *) Next_Object(p);
	    }
	    return 0;
	}

	case RAPI_RSTYLE_SE: {
	    ResvSE_t *sp = (ResvSE_t *) Next_Object(&rp->resv_policy);

	    for (i = 0; i < rp->nStruct; i++) {
		NTOH32(sp->SE_ttd);
	    	/* NTOH32(sp->SE_nexthop); */
		NTOH16(sp->nSE_filts);
		NTOH16(Obj_Length(&sp->SE_flowspec));
	    	ntoh_flowspec(&sp->SE_flowspec);
		p = (char *)Next_Object(&sp->SE_flowspec);
		for (j = 0; j < sp->nSE_filts; j++) {
		    if (ntoh_filterspec((FILTER_SPEC *) p))
			return -1;
		    p = (char *)Next_Object(p);
		}
		sp = (ResvSE_t *) p;
	    }
	    return 0;
	}

	default:
	    /* Unknown STYLE */
	    fprintf(stderr, "Unknown RSVPInfo reservation style... (%d)\n",
		    rp->resv_style);
	    return -1;
	} /* resv_style */

    default:
	/* Unknown INFO packet type */
	return 0;
    }
    } /* pkt type */

    /* return 0; compiler complains unreachable - mwang */
}

int
ntoh_session_ipv4(Session_IPv4 *p)
{
    /* session address stays in network format */
    NTOH16(p->sess_destport);
    return 0;
}

int
ntoh_policydata(POLICY_DATA *p)
{
    NTOH16(p->policy_d_header.obj_length);
    return 0;
}

int
ntoh_filterspec(FILTER_SPEC *p)
{
    NTOH16(p->filt_header.obj_length);
    /* XXX how do you know what kind of filter it is? */
    /* address, port stay in network format for fmt_filtspec */
    return 0;
}

/*
 * this function is not implemented for now - mwang.
 * There is another log function in rsvp_debug.c but its hard to
 * integrate that one with this.
 */
void
log(int severity, int syserr, const char *format, ...)
{

}

