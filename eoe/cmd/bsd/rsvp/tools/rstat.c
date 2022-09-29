/*
 *  @(#) $Id: rstat.c,v 1.7 1997/11/14 07:36:12 mwang Exp $
 */
/********************************************************************
 *
 *            rstat - Display RSVP status 
 *                                                                          
 *              Written by: Bob Braden (braden@isi.edu)
 *              USC Information Sciences Institute
 *              Marina del Rey, California
 *		September 1995                                  
 *                                                                          
 *  Copyright (c) 1995 by the University of Southern California
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

/*                            rstat.c
 *
 *      This is intended to give the kind of local status display that
 *	netstat provides, but for RSVP specifically.  It may be extended
 *	in the future to query remote RSVPs, and to do other fancy
 *	things.
 *
 *      rstat sends a UDP probe packet to the local RSVP daemon, and
 *	then gathers and formats the reply packet(s).
 */

#include "rsvp_daemon.h"
#include "rsvp_mstat.h"

char *version = "R1.4 May 2 1996";
char *Usage_text = "\
    Usage: rstat [-p] [-f] [-n] [-h node_name ]\n\
        -p => show path state\n\
        -f => show flowspecs and sender Tspecs\n\
        -n => show only numeric (dotted-decimal) IP addresses\n\
        -h => get status from specified node (default is self)\n";

/*
 *  Global control variables
 *
 */
int	Xdebug= 0;
int	Show_Path = 0;
int	Show_Flowspecs = 0;
int	Printed_header = 0;

char	From_name[128] = "";
char	Node_name[128] = "";
char	probe_buff[4];

char *Type_name[] = {"", "PATH  ", "RESV  ", "PATH-ERR", "RESV-ERR",
			 "PATH-TEAR", "RESV-TEAR"};

#include <setjmp.h>
extern jmp_buf savej;  /* longjmp environ for receiving response; used the one in rapi_fmt.c - mwang */
#define	RESP_TIMEOUT	4	/* response timeout time */

#define WFp(rp) ((ResvWF_t *)(rp))
#define FFp(rp) ((ResvFF_t *)(rp))
#define SEp(rp) ((ResvSE_t *)(rp))

char *Heading = "\
Iface      Session                 Sender                  Neighbor\n";
char	SpecFormat[] = "%30s>> %s\n";

int udpsock = -1;
struct sockaddr_in sin;

#define NIFACES 8
#define NSTATS 32

/*	Define queue element structure.  This precedes a PathInfo_t or
 *	ResvInfo_t area and is used to sort them into order.
 */
typedef struct Sess_Elem {
	struct Sess_Elem	*selm_next;	/* chain pointer */
	int			selm_size;	/* #bytes of data following */
	int			selm_type;	/* path/resv data */
	Session_IPv4		selm_session;
}   sess_element_t;

sess_element_t	selm_head;

/* Forward and external declarations
 */
u_long resolve_name();
extern char *fmt_hostname(), *fmt_filtspec(), *fmt_flowspec(), *fmt_tspec();
extern void ntoh_flowspec(FLOWSPEC *), ntoh_tspec(SENDER_TSPEC *);
#define ip_convert(addr)  fmt_hostname(&(addr))
extern	IP_NumOnly;

char	*fmt_session();
u_long	default_if_addr();
void	queue_info(char *, int, Session_IPv4 *, int);
void	recvstatus(void);
void	print_status();
void	hexf(FILE *,  char *, int);
int	ntoh_Path(PathInfo_t *);
int	ntoh_Resv(ResvInfo_t *);
int	ntoh_session_ipv4(Session_IPv4 *);
int	ntoh_policydata(POLICY_DATA *);
int	ntoh_filterspec(FILTER_SPEC *);
char	*filtsptoa(FILTER_SPEC *);
ResvInfo_t *Next_Resv_Session(ResvInfo_t *);
PathInfo_t *Next_Path_Session(PathInfo_t *);
void	print_sess_Path();
void	print_sess_Resv();
void	print_header();
void	timeout();
char	*prune_hostname();
void	sort_ifaces(char **, int, int);


#define fspectoa(x)	fmt_flowspec((Obj_Class(x) != class_NULL)? (x):NULL)
#define ftspectoa(x)	(Obj_Class(x) != class_NULL)? fmt_tspec(x): "TS[]"
#define From_addr(hop, w)  (((&(hop))->s_addr == INADDR_ANY)? "API" : \
					prune_hostname(ip_convert(hop), w))

#define Iface_name(ifp) ((!strcmp((ifp), "local"))? "API" : (ifp))

#define sess_data(x) (char *)((sess_element_t *)(x)+1)

#define Sess_LT(x, y) ((x)->sess_destaddr.s_addr < (y)->sess_destaddr.s_addr ||\
	((x)->sess_destaddr.s_addr == (y)->sess_destaddr.s_addr && \
	 (x)->sess_destport < (y)->sess_destport) )

#define Sess_EQ(x, y) ((x)->sess_destaddr.s_addr == (y)->sess_destaddr.s_addr \
		&& (x)->sess_destport == (y)->sess_destport )

#define NotNULL(x)  (Obj_Class(x) != class_NULL)


int
main(argc, argv)
	int argc ;
	char **argv ;
	{
  	extern char	*optarg;
	u_int		c;
	char		*rmsuffix(), *tp;
	u_int32_t	host;
	u_int16_t	port;
	int		one = 1, i;
	int		sin_len, *sin_lenp = &sin_len;
	void		(*prev_func)();
    
	IP_NumOnly = 0;

	/*
	 *	Process command-line options
	 */
	while ((c = getopt(argc, argv, "fh:npx")) != -1) {
		switch (c) {
        
		case 'x':   /* -x -- Debug flag  */
			Xdebug = 1 ;
			break ;
       
		case 'p':   /* -p -- Show path state */
			Show_Path = 1;
			break ;

		case 'n':   /* -n -- Noconversion of host addrs to names */
			IP_NumOnly = 1;
			break;
     
		case 'h':  /* -h <host/router> */
			strcpy(Node_name, optarg);
			break;
 
		case 'f':  /* -f -- Show flowspecs */
			Show_Flowspecs = 1;
			break;

		default:    
    			fprintf(stderr, Usage_text);
			exit(1) ;
		}
	}


	/*
	 *  Init socket
	 */
	if ((udpsock = socket(AF_INET, SOCK_DGRAM, 0))< 0) {
		perror("Socket Error") ;
		exit(1) ;
	}
	if (setsockopt(udpsock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one))){
		perror("sockopt");
		exit(1);
	}
	bzero((char *) &sin, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = 0;	/* Wildcard port */
	if (bind(udpsock,(struct sockaddr *)&sin,sizeof(struct sockaddr_in))
									< 0) {
		perror("Bind error");
		exit(1);
	}

	if (Node_name[0] != '\0') {
		/* 
		 *  Convert node address: 	<host>[ {:|/}<port> ]
		 */
		if (tp = strpbrk(Node_name, ":/")) {
			*tp = '\0';
			port = htons((u_int16_t)atoi(tp+1));
		}
		else
			port = htons(STAT_PROBE_PORT);
        	host = resolve_name(Node_name);
		if (host == INADDR_ANY) {
                	fprintf(stderr, "Unknown node %s\n", Node_name);
                	return (-1);
		}
        }
	else {
		port = htons(STAT_PROBE_PORT);
		host = default_if_addr(udpsock);
	}
	if (Xdebug) {
		struct in_addr addr;

		addr.s_addr = host;
		printf("Probe: %s:%d\n", inet_ntoa(addr), port);
		fflush(stdout);
	}
	sin.sin_addr.s_addr = host;
	sin.sin_port = port;
	if (IN_MULTICAST(ntoh32(host))) {
		printf("Multicast node address not allowed\n");
		exit(-1);
	}
	*(int *)probe_buff = hton32(TYPE_RSVP_PROBE);
	if (sendto(udpsock, probe_buff, sizeof(probe_buff), 0,
				&sin, sizeof(struct sockaddr_in)) < 0) {
		perror("sendto");
		exit(-1);
	}

	if (Xdebug) {
		recvstatus();  /* Path */
		recvstatus();  /* Resv */
	} else {
		if (setjmp(savej) == 0) {
			prev_func = signal(SIGALRM, timeout);
			alarm(RESP_TIMEOUT);
			recvstatus();  /* Path */
			recvstatus();  /* Resv */
			alarm(0);
    		}
		signal(SIGALRM, prev_func);
	}
	print_status();
}

void timeout()
        {
	printf("...timeout\n");
	fflush(stdout);
	longjmp(savej, 1);
}

/*
 *  recvstatus():  Receive UDP packets and display status
 */
void
recvstatus()
	{
	struct sockaddr_in from;
	int		fromlen = sizeof(struct sockaddr_in);
	char		buff[STAT_MSG_SIZE];
	RSVPInfo_t	*Infop = (RSVPInfo_t *) buff;
	ResvInfo_t	*r_sesp, *new_r_sesp;
	PathInfo_t	*p_sesp, *new_p_sesp;
	int		i, n, size;

	n = recvfrom(udpsock, buff, sizeof(buff), 0,
		    		(struct sockaddr *)&from, &fromlen);
	if (n < 0) {
		perror("recvfrom");
		exit(1);
	}
	strcpy(From_name, ip_convert(from.sin_addr));

	print_header();
	NTOH32(Infop->type);
	NTOH32(Infop->nSession);

	switch (Infop->type) {	/* first fullword is type */
            case TYPE_RSVP_VPATH:
		if (Xdebug) {
			printf("RCVD PATH:\n");
			hexf(stdout, buff, n);
			fflush(stdout);
		}
		p_sesp = &Infop->Session_un.pathInfo;
		for (i= 0; i<Infop->nSession; i++) {
			ntoh_Path(p_sesp);
			new_p_sesp = Next_Path_Session(p_sesp);
			size = (char *) new_p_sesp - (char *)p_sesp;
			queue_info((char *)p_sesp, size,
					&p_sesp->path_session, Infop->type);
			p_sesp = new_p_sesp;
		}
		break;

            case TYPE_RSVP_VRESV:
		if (Xdebug) {
			printf("RCVD RESV:\n");
			hexf(stdout, buff, n);
		}
		r_sesp = &Infop->Session_un.resvInfo;
		for (i= 0; i< Infop->nSession; i++) {
			ntoh_Resv(r_sesp);
			new_r_sesp = Next_Resv_Session(r_sesp);
			size = (char *) new_r_sesp - (char *)r_sesp;
			queue_info((char *)r_sesp, size,
					&r_sesp->resv_session, Infop->type);
			r_sesp = new_r_sesp;
		}
		break;

	    default:
		fprintf(stderr, "Unknown RSVP mstat pkt type %d from host %s\n",
				Infop->type, inet_ntoa(from.sin_addr));
		if (Xdebug)
			hexf(stdout, buff, n);
		break;
	}		
}


void
queue_info(char *data, int size, Session_IPv4 *sessp, int type)
	{
	sess_element_t	*selmp, **p;

	selmp = (sess_element_t *) malloc(size+sizeof(sess_element_t));
	selmp->selm_size = size;
	selmp->selm_type = type;
	selmp->selm_session = *sessp;
	memcpy(sess_data(selmp), data, size);

	p = &selm_head.selm_next;
	while(*p != NULL && Sess_LT(&(*p)->selm_session, sessp)){
		p = &((*p)->selm_next);
	}
	selmp->selm_next = *p;
	*p = selmp;
}


PathInfo_t *
Next_Path_Session(PathInfo_t *sesp)
	{
	PathSender_t *Pp;
	int j;

	Pp = &sesp->path_sender;
	for (j=0; j < sesp->nSender; j++) {
		Pp = (PathSender_t *) 	
			Next_Object(
				Next_Object(
					Next_Object(&Pp->pathS_policy)));
	}
	return((PathInfo_t *)Pp);
}

ResvInfo_t *
Next_Resv_Session(ResvInfo_t *sesp)
	{
	int	style = sesp->resv_style;
	char	*rp;
	int	j, k;

	rp = (char *) Next_Object(&sesp->resv_policy);
	for (j=0; j < sesp->nStruct; j++) {
		switch (style) {

		case RAPI_RSTYLE_WILDCARD:
			rp = (char *) Next_Object(
				Next_Object(&WFp(rp)->WF_flowspec));
			break;
	
		case RAPI_RSTYLE_FIXED:
			rp = (char *) Next_Object(
				Next_Object(&FFp(rp)->FF_flowspec));
			break;

		case RAPI_RSTYLE_SE:
			rp = (char *) Next_Object(&SEp(rp)->SE_flowspec);
			for (k = 0; k < ((ResvSE_t *)rp)->nSE_filts; k++)
				rp = (char *) Next_Object(rp);
			break;
				
		default:
			break;
		}
	}
	return((ResvInfo_t *) rp);
}

/*	Display status from sorted queue of session elements
 *	List has form: [Resv1] Path1 [Resv2] Path2 ...
 *
 */
void
print_status()
	{
	sess_element_t *sp, *tp;

	for (sp = selm_head.selm_next; sp; sp = sp->selm_next) {
		if (sp->selm_type == TYPE_RSVP_VRESV) {
			print_sess_Resv((ResvInfo_t *)sess_data(sp));
			tp = sp->selm_next;
			if (tp == NULL || tp->selm_type != TYPE_RSVP_VPATH ||
				!Sess_EQ(&sp->selm_session, &tp->selm_session)
				) {
				printf("!? missing path state\n");
				return;
			}
			sp = tp;
			if (Show_Path)
				print_sess_Path((PathInfo_t *)sess_data(sp));
		}
		else if (((PathInfo_t *)sess_data(sp))->nSender) {
			/* No Resv data for this session,
			 * but there is path data.
			 */
			printf(" (no resv) %-22s\n",
				fmt_session(&((PathInfo_t *)
					sess_data(sp))->path_session));
			if (Show_Path)
				print_sess_Path((PathInfo_t *)sess_data(sp));
		}
		printf("\n");
	}		

}


/*
 *  Display Path State from vector of n per-session data items
 */
void
print_sess_Path(sesp)
	PathInfo_t *sesp;
	{
	PathSender_t *Pp;
	int	j;

	Pp = &sesp->path_sender;
	for (j=0; j < sesp->nSender; j++) {
		FILTER_SPEC *fip =
				(FILTER_SPEC *)Next_Object(&Pp->pathS_policy);
		FLOWSPEC *fsp =  (FLOWSPEC *)  Next_Object(fip);

		printf("-- Path -- %-22s  %-22s  %-13s\n",
			fmt_session(&sesp->path_session),
			filtsptoa(fip),
			From_addr(Pp->pathS_phop, 13));

		if ((Show_Flowspecs) && NotNULL(fsp))
			printf(SpecFormat, "", ftspectoa(fsp));
		Pp = (PathSender_t *) Next_Object(fsp);
	}
}

                                             
/*
 *  Display reservation state from vector of (session, style) pair.
 */
void
print_sess_Resv(sesp)
	ResvInfo_t *sesp;
	{
	char		*rp, *bp;
	int		i, j, k;
	FILTER_SPEC	*fip;
	FLOWSPEC	*fsp;
	char		*ifcp;
	int		style = sesp->resv_style;
	POLICY_DATA	*ap = &sesp->resv_policy;
	int		N = sesp->nStruct;
	char		**vec = (char **) malloc(sizeof(char *) * N);

	if (N == 0)
		return; 
	bp = rp = (char *) Next_Object(ap);

	for (i = 0; i < N; i++) {
	    vec[i] = rp;
	    switch (style) {
		case RAPI_RSTYLE_WILDCARD:
			rp = (char *) Next_Object(Next_Object(
							&WFp(rp)->WF_flowspec));
			break;
	
		case RAPI_RSTYLE_FIXED:
			rp = (char *) Next_Object(Next_Object(
							&FFp(rp)->FF_flowspec));
			break;

		case RAPI_RSTYLE_SE:
			rp = (char *) Next_Object(&SEp(rp)->SE_flowspec);
			for (k = 0; k < ((ResvSE_t *)rp)->nSE_filts; k++)
				rp = (char *) Next_Object(rp);
			break;
				
		default:
			break;
		}
	}
	switch (style) {
		case RAPI_RSTYLE_WILDCARD:
			sort_ifaces(vec, N,
				0 /* offsetof(ResvWF_t, WF_if) */ );
			break;

		case RAPI_RSTYLE_FIXED:
			sort_ifaces(vec, N,
				0 /* offsetof(ResvFF_t, FF_if) */ );
			break;
	
		case RAPI_RSTYLE_SE:
			sort_ifaces(vec, N,
				0 /* offsetof(ResvSE_t, SE_if) */ );
 			break;
	}

	for (j = 0; j < N; j++) {
		rp = vec[j];
		switch (style) {

		case RAPI_RSTYLE_WILDCARD:
			fsp = &WFp(rp)->WF_flowspec;
			fip = (FILTER_SPEC *) Next_Object(fsp);
				
			printf("%-5s  WF  %-22s  %-22s  %-13s\n",
				Iface_name(WFp(rp)->WF_if),
				fmt_session(&sesp->resv_session),
				filtsptoa(fip),
				From_addr(WFp(rp)->WF_nexthop, 13));
			if ((Show_Flowspecs) && NotNULL(fsp))
				printf(SpecFormat, "", fspectoa(fsp));
			break;

		case RAPI_RSTYLE_FIXED:
			fsp = &FFp(rp)->FF_flowspec;
			fip = (FILTER_SPEC *) Next_Object(fsp);
			if (j == 0 ||  strcmp(FFp(rp)->FF_if,
					FFp(vec[j-1])->FF_if))
				printf("%-5s  FF  %-22s  %-22s  %-13s\n",
					Iface_name(FFp(rp)->FF_if),
					fmt_session(&sesp->resv_session),
					filtsptoa(fip),
					From_addr(FFp(rp)->FF_nexthop, 13));

			else
				printf("%33s  %-22s\n",
						"", filtsptoa(fip));
			if ((Show_Flowspecs) && NotNULL(fsp))
				printf(SpecFormat, "", fspectoa(fsp));
			break;

		case RAPI_RSTYLE_SE:
				

		default:
			break;
		}
	}
}

void
print_header()
	{
	if (Printed_header)
		return;
	Printed_header = 1;

	if (Node_name[0] != '\0')
		printf("RSVP status on node: %s\n", From_name);
	printf(Heading);
/*** seems to look better without this...
	if (Show_Flowspecs)
		printf(SpecFormat, "", "[Flowspec/Tspec]");
	else
****/
		printf("\n");
}

char *
fmt_session(sadrp)
	Session_IPv4 *sadrp;
	{
	static char sess_buff[32];
 
	sprintf(sess_buff, "%.15s/%d",
		ip_convert(sadrp->sess_destaddr),
		sadrp->sess_destport);
	return(sess_buff);
}

/*
 * Convert RSVPInfo_t datastructure from network to local format
 * 	Return 0 success, -1 fail
 */
int
ntoh_Path(PathInfo_t *pp)
	{
#if BYTE_ORDER == LITTLE_ENDIAN
	int i, j;
	PathSender_t *sp;
	Object_header *p;

	if (ntoh_session_ipv4(&pp->path_session))
		return -1;
	NTOH32(pp->path_R);
	NTOH32(pp->nSender);

	sp = &pp->path_sender;
	for (j = 0; j < pp->nSender; j++) {
		/* pathS_phop stays in network format.. */
		NTOH32(sp->pathS_routes);
		NTOH32(sp->pathS_ttd);
		p = (Object_header *)&sp->pathS_policy;
		if (ntoh_policydata((POLICY_DATA *) p))
		    return -1;
		p = Next_Object(p);
		if (ntoh_filterspec((FILTER_SPEC *) p))
		    return -1;
		p = Next_Object(p);
		NTOH16(Obj_Length(p));
		ntoh_tspec((SENDER_TSPEC *) p);
		p = Next_Object(p);
		sp = (PathSender_t *) p;
	}
#endif /* LITTLE_ENDIAN */
	return 0;
}

	
int
ntoh_Resv(ResvInfo_t *rp)
	{
#if BYTE_ORDER == LITTLE_ENDIAN
	int i, j;
	Object_header *p;

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
		NTOH16(Obj_Length(&sp->WF_flowspec));
		ntoh_flowspec(&sp->WF_flowspec);
		p = Next_Object(&sp->WF_flowspec);
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
		NTOH16(Obj_Length(&sp->FF_flowspec));
	    	ntoh_flowspec(&sp->FF_flowspec);
		p = Next_Object(&sp->FF_flowspec);
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
		NTOH16(sp->nSE_filts);
		NTOH16(Obj_Length(&sp->SE_flowspec));
	    	ntoh_flowspec(&sp->SE_flowspec);
		p = Next_Object(&sp->SE_flowspec);
		for (j = 0; j < sp->nSE_filts; j++) {
		    if (ntoh_filterspec((FILTER_SPEC *) p))
			return -1;
		    p = Next_Object(p);
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
#endif /* LITTLE_ENDIAN */
	return 0;
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
    NTOH16(Obj_Length(&p->policy_d_header));
    return 0;
}

int
ntoh_filterspec(FILTER_SPEC *p)
{
    NTOH16(Obj_Length(&p->filt_header));
    /* address, port stay in network format */
    return 0;
}


char *
filtsptoa(FILTER_SPEC *filtp)
	{
	static char tmp[256];
	char	*cp, *P, *Q;
	int	n;

	if (Obj_Class(filtp) == class_NULL)
		return  "*/*[*]";
	cp = fmt_filtspec(filtp);
	if ((n = strlen(cp)) <= 22)
		return cp;
	else if (n > 255)
		return "!!too long";

	strcpy(tmp, cp);
	while (strlen(tmp) > 22) {
		P = strrchr(tmp, '/');
		Q = strrchr(tmp, '.');
		strcpy(Q, P);
	}
	return tmp;
}

/*   Prune trailing segments from host name, in order to fit into
 *	specified width.  Modifies input string in place, returns
 *	its address.
 */
char *
prune_hostname(char *cp, int wid)
	{
	char	*p;

	if (isdigit(*cp))
		return(cp);	/* Can't prune dotted-decimal */
	while (strlen(cp) > wid) {
		if (p = strrchr(cp, '.'))
			*p = '\0';
		else
			cp[wid] = '\0';
	}		
	return(cp);
}

/*	Sort list of Resv entries into lexicographic order by interface names
 */
void
sort_ifaces(char **vec, int N, int offset) {
	int i, j;
	char *temp;

	for (i = 0; i < N; i++)
		for (j = i+1; j < N; j++) {
			if (strcmp(vec[j]+offset, vec[i]+offset) > 0) {
				temp = vec[i];
				vec[i] = vec[j];
				vec[j] = temp;
			}
		}
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
 * this function is not implemented for now - mwang.
 * See comments at the bottom of rsvpeep.c
 */
void
log(int severity, int syserr, const char *format, ...)
{

}
