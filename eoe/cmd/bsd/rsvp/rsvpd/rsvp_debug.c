
/*
 * @(#) $Id: rsvp_debug.c,v 1.7 1998/11/25 08:43:14 eddiem Exp $
 */

/***************************** rsvp_debug.c ******************************
 *                                                                       *
 *                                                                       *
 * Code to format a trace of events and internal state for debugging,    *
 * and code to log debugging, warning, and informational messages.       *
 *                                                                       *
 *                                                                       *
 *************************************************************************/
/****************************************************************************

            RSVPD -- ReSerVation Protocol Daemon

                USC Information Sciences Institute
                Marina del Rey, California

		Original Version: Shai Herzog, Nov. 1993.
		Current Version:  Steven Berson & Bob Braden, May 1996

  Copyright (c) 1996 by the University of Southern California
  All rights reserved.

  Permission to use, copy, modify, and distribute this software and its
  documentation in source and binary forms for any purpose and without
  fee is hereby granted, provided that both the above copyright notice
  and this permission notice appear in all copies, and that any
  documentation, advertising materials, and other materials related to
  such distribution and use acknowledge that the software was developed
  in part by the University of Southern California, Information
  Sciences Institute.  The name of the University may not be used to
  endorse or promote products derived from this software without
  specific prior written permission.

  THE UNIVERSITY OF SOUTHERN CALIFORNIA makes no representations about
  the suitability of this software for any purpose.  THIS SOFTWARE IS
  PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,
  INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

  Other copyrights might apply to parts of this software and are so
  noted when applicable.

********************************************************************/

#include "rsvp_daemon.h"
#ifdef __STDC__
#include <stdarg.h>
#include <stdio.h>
#else
#include <varargs.h>
#endif

#define iptoname(p)	fmt_hostname(p)
/*
 *	External declarations
 */
char	*fmt_tspec(SENDER_TSPEC *);
char	*fmt_filtspec(FILTER_SPEC *);
char	*fmt_flowspec(FLOWSPEC *);
char	*fmt_adspec(ADSPEC *);
char	*fmt_hostname(struct in_addr *);
int	vfprintf(FILE * stream, const char *format, va_list ap);
void	fmt_object();
char	*strtcpy();
char	*fmt_style(style_t);
char	*cnv_flags(char *, u_char);

/* these are defined in rsvp_main.c - mwang */
extern int	debug_filter;
extern int	debug_filter_num;
extern int	debug_filters[];

/*
 *	Forward declarations
 */
void	print_senders(), print_flwds();
void	dump_PSB(PSB *), dump_RSB(RSB *), dump_TCSB(TCSB *, int);
void	dump_session_state(Session *);
void	dump_filtstar(FiltSpecStar *, FLOWSPEC *);

/*
 * Exported variables.
 */
char s1[19];		/* buffers to hold the string representations  */
char s2[19];		/* of IP addresses, to be passed to inet_fmt() */
char s3[19];		/* or inet_fmts().                             */
char s4[19];
#ifndef __sgi
/* char buff for bm_expand is defined in rsvp_rsrr.c - mwang */
char s[RSRR_MAX_VIFS];  /* char buff for bm_expand() */
#endif

char   *r_type[] = {"RSVP_Unknown", "PATH", "RESV", "PATH_ERR",
				 "RESV_ERR", "PTEAR", "RTEAR","CONFIRM"};

/*
 * dump_ds(): This function is called to log and possibly multicast the
 *	internal state.
 */
void
dump_ds(int force) {
	Session	*dst;
	static int last_print = 0;
	int	i;

	/*
	 * Don't print state too often, wait at least DUMP_DS_TIMO, unless a
	 * force option is specified.
	 */
	if (!force && (abs((int) (time_now - last_print)) < DUMP_DS_TIMO))
		return;

	/* If flag set, multicast the state to remote display programs
	 */
	if (debug & DEBUG_MCAST_STATE) {
		mcast_rsvp_state();
		last_print = time_now;
	}

	/* If state dump flag bit not set, exit now.
	 */
	if (!IsDebug(DEBUG_DS_DUMP))
		return;

	last_print = time_now;
	log(LOG_DEBUG, 0,
		"\n%12s >>>>>>>>>>  Internal STATE: <<<<<<< %u <<<<<<\n",
		rsvp_timestamp(),time_now);

	/* Dump each session */
	for (i = 0; i < SESS_HASH_SIZE; i++)
		for (dst = session_hash[i]; dst; dst = dst->d_next)
			dump_session_state(dst);
	return;
}

void
dump_session_state(Session *dst) {
	PSB 		*snd;
	RSB		*rp;
	TCSB		*kp;
	int		 i;

	fmt_object(dst->d_session, 0);

	if (debug_filter) {
		for (i = 0; i < debug_filter_num; i++) {
			if (debug_filters[i] == dst->d_addr.s_addr)
				break;
		}
		if (i == debug_filter_num)
			return;
	}
	log(LOG_DEBUG, 0, "   Refresh intervals: Path R= %d  Resv R=%d\n",
		dst->d_Rtimop, dst->d_Rtimor);
	for (snd = dst->d_PSB_list; snd != NULL; snd = snd->ps_next)
		dump_PSB(snd);
	for (rp = dst->d_RSB_list; rp != NULL; rp = rp->rs_next)
		dump_RSB(rp);
	for (i = 0; i <= if_num; i++) {
		for (kp = dst->d_TCSB_listv[i]; kp != NULL; kp = kp->tcs_next)
			dump_TCSB(kp, i);
	}
	log(LOG_DEBUG, 0, 
		"   ------------- End of Dest state dump ---------------\n\n");
}

void
dump_PSB(PSB *sp) {
	struct in_addr phop = sp->ps_phop;
	Fobject		*fop;

	log(LOG_DEBUG, 0,
	"   Sender: %s   PHOP: <%s LIH=%d>   TTD: %d\n", 
		fmt_filtspec(sp->ps_templ),
 		(IsAPI(sp)) ? "(API)" : iptoname(&phop), sp->ps_LIH,
		sp->ps_ttd);
	log(LOG_DEBUG, 0,
	"     In_if %d=>%s  Outlist 0x%x   flags %s  ip_ttl %d\n",
		sp->ps_in_if, vif_name(sp->ps_in_if), sp->ps_outif_list,
		cnv_flags("SRPULE?N", sp->ps_flags), sp->ps_ip_ttl);
	log(LOG_DEBUG, 0,
	"     %s", fmt_tspec(sp->ps_tspec));
	if (sp->ps_adspec)
		log(LOG_DEBUG, 0, "   %s\n", fmt_adspec(sp->ps_adspec));
	else 
		log(LOG_DEBUG, 0, "\n");
	for (fop = sp->ps_UnkObjList; fop; fop= fop->Fobj_next)
		fmt_object(&fop->Fobj_objhdr, 0);
	if (sp->ps_BSB_Qb)
		log(LOG_DEBUG, 0,
		"      Blockade: Tb= %d  Qb= %s\n", sp->ps_BSB_Tb,
			fmt_flowspec(sp->ps_BSB_Qb));
}

void
dump_RSB(RSB *rp)
	{
	char		temp[256];
	Fobject		*fop;

	sprintf(temp, "Iface %d=>%s Nhop <%s LIH=%d>",
			rp->rs_OIf, vif_name(rp->rs_OIf),
			iptoname(&rp->rs_nhop),
			rp->rs_rsvp_nhop.hop4_LIH);
	log(LOG_DEBUG, 0, "   %s Resv:   %s  TTD %d\n",
				fmt_style(rp->rs_style), temp, rp->rs_ttd);
	if (rp->rs_scope)
		fmt_object(rp->rs_scope, RSVP_RESV);
	if (rp->rs_confirm)
		fmt_object(rp->rs_confirm, RSVP_RESV);
	else if (rp->rs_scope)
		log(LOG_DEBUG, 0, "\n");
	for (fop = rp->rs_UnkObjList; fop; fop= fop->Fobj_next)
		fmt_object(&fop->Fobj_objhdr, 0);
	dump_filtstar(rp->rs_filtstar, rp->rs_spec);
}

void 
dump_TCSB(TCSB *kp, int in)
	{
	struct in_addr ina;

	if (in == if_num)
		log(LOG_DEBUG, 0,
			"   Kernel reservation: API  Rhandle %x\n",
			kp->tcs_rhandle);
	else {
		ina.s_addr = IF_toip(in);
		log(LOG_DEBUG, 0,
			"   Kernel reservation: Iface %d (%s) Rhandle %x \n",
			in, inet_ntoa(ina), kp->tcs_rhandle);
	}
	dump_filtstar(kp->tcs_filtstar, kp->tcs_spec);
}

void
dump_filtstar(FiltSpecStar *fstp, FLOWSPEC *specp)
	{
	int i;

	if (fstp->fst_count == 0) {
		log(LOG_DEBUG, 0, "      Flowspec %s\n",  fmt_flowspec(specp));
		return;
	}
	for (i=0; i < fstp->fst_count; i++) {
	    if (fstp->fst_Filtp(i) == NULL)
		continue;
	    if (i)
		log(LOG_DEBUG, 0, "             %s\n",
				fmt_filtspec(fstp->fst_Filtp(i)));
	    else
		log(LOG_DEBUG, 0, "      Filter %s    Flowspec %s\n",
				fmt_filtspec(fstp->fst_filtp0), 
				fmt_flowspec(specp));

	}
}


/*
 * print_rsvp(): Log a dump of an RSVP message.
 */
void
print_rsvp(struct packet *pkt)
	{
	packet_map	*mapp = pkt->pkt_map;
	struct in_addr	destaddr;
	enum byteorder	old_order = pkt->pkt_order;
	SCOPE		*scp;
	Fobject		*fop;
	int             i;

	ntoh_packet(pkt);
	/* It would be nicer to make copy if necessary to convert byte order,
	 *	avoid converting back at end.
	 */
 
	if (!(mapp->rsvp_session)) {
		log (LOG_DEBUG, 0, "Missing session obj\n");
		return;
	}
	destaddr = pkt->rsvp_destaddr;
	if (debug_filter) {
		for (i = 0; i < debug_filter_num; i++) {
			if (debug_filters[i] == destaddr.s_addr)
				break;
		}
		if (i == debug_filter_num)
			return;
	}

	if (mapp->rsvp_integrity)
		fmt_object(mapp->rsvp_integrity, 0);
	log(LOG_DEBUG, 0, "  %-4s:", r_type[mapp->rsvp_msgtype]);
	fmt_object(mapp->rsvp_session, 0);

	switch (mapp->rsvp_msgtype) {

	    case (RSVP_PATH):
	    case (RSVP_PATH_TEAR):
		fmt_object(mapp->rsvp_timev, RSVP_PATH);
		fmt_object(mapp->rsvp_hop, RSVP_PATH);
		print_senders(pkt);
		break;

	    case (RSVP_RESV):
	    case (RSVP_RESV_TEAR):
		fmt_object(mapp->rsvp_timev, RSVP_RESV);
		fmt_object(mapp->rsvp_hop, RSVP_RESV);
		if ((scp = mapp->rsvp_scope_list))
			fmt_object(scp, RSVP_RESV);
		if (mapp->rsvp_confirm)
			fmt_object(mapp->rsvp_confirm, RSVP_RESV);

		print_flwds(pkt);
		log(LOG_DEBUG, 0, "\n");
		break;

	    case (RSVP_PATH_ERR):
		log(LOG_DEBUG, 0, "\n");	
		fmt_object(mapp->rsvp_errspec, RSVP_RESV_ERR);
		print_senders(pkt);
		break;

	    case (RSVP_RESV_ERR):
		log(LOG_DEBUG, 0, "\n");	
		fmt_object(mapp->rsvp_errspec, RSVP_RESV_ERR);
		print_flwds(pkt);
		break;

	    case (RSVP_CONFIRM):
		log(LOG_DEBUG, 0, "\n");
		fmt_object(mapp->rsvp_confirm, RSVP_CONFIRM);	
		fmt_object(mapp->rsvp_errspec, RSVP_RESV_ERR);
		print_flwds(pkt);
		break;
		
	default:
		log(LOG_DEBUG, 0, "unknown rsvp packet type: %d\n",
		    RSVP_TYPE_OF(pkt->pkt_data));
		break;
	}
	for (fop = mapp->rsvp_UnkObjList; fop; fop = fop->Fobj_next)
		fmt_object(&fop->Fobj_objhdr, 0);

/****
	log(LOG_DEBUG, 0,
	    "  --------------- end packet parse ---------------------------\n\n"
	    );
****/
	log(LOG_DEBUG, 0, "\n");

	if (old_order != BO_HOST) {
		hton_packet(pkt);
	}

}

void
print_senders(struct packet *pkt)
	{
	SenderDesc     *sdscp = SenderDesc_of(pkt);

	log(LOG_DEBUG, 0, "\t%s  %s\n", fmt_filtspec(sdscp->rsvp_stempl), 
					fmt_tspec(sdscp->rsvp_stspec));
	if (sdscp->rsvp_adspec)
		log(LOG_DEBUG, 0, "\t\t%s", fmt_adspec(sdscp->rsvp_adspec));
}

void
print_flwds(struct packet *pkt)
	{
	int		i;
	style_t		style = Style(pkt);
	FlowDesc	*flwdp = FlowDesc_of(pkt, 0);

	switch (style) {

	case STYLE_WF:
		log(LOG_DEBUG, 0, "   WF   %s\n", 
			fmt_flowspec(flwdp->rsvp_specp));
		break;

	case STYLE_FF:
		for (i = 0; i < pkt->rsvp_nflwd; i++) {
			flwdp = FlowDesc_of(pkt, i);
			log(LOG_DEBUG, 0, "    FF   %s    %s\n", 
				fmt_filtspec(flwdp->rsvp_filtp),
				fmt_flowspec(flwdp->rsvp_specp));
		}
		break;

	case STYLE_SE:
		log(LOG_DEBUG, 0, "    SE   %s   %s\n",
				fmt_filtspec(flwdp->rsvp_filtp),
				fmt_flowspec(flwdp->rsvp_specp));
		for (i = 1; i < pkt->rsvp_nflwd; i++) {
			flwdp = FlowDesc_of(pkt, i);
			log(LOG_DEBUG, 0, "         %s\n",
				fmt_filtspec(flwdp->rsvp_filtp));
		}
		break;

	default:
		log(LOG_DEBUG, 0, "   ??Style= %d\n", style);
		break;
  
	}
}

char *
rsvp_timestamp(void)
{
	struct timeval tv;
	struct tm	tmv;
	static char	buff[16];

	gettimeofday(&tv, NULL);
	memcpy(&tmv, localtime((time_t *) &tv.tv_sec), sizeof(struct tm));
	sprintf(buff, "%02d:%02d:%02d.%03d", tmv.tm_hour, tmv.tm_min,
			tmv.tm_sec, (int) tv.tv_usec/1000);
	return(buff);
}

/* log(): write to the log.
 *
 *	Write all messages of severity <= l_severity into log file; if
 *	not in daemon mode, write them onto console (stderr) as well.
 */
void
log(int severity, int syserr, const char *format, ...)
{
	char *a_time = rsvp_timestamp();
	va_list ap;

	assert(severity == LOG_ALWAYS || severity >= LOG_ERR);
	va_start(ap, format);

	if (severity <= l_debug) {
		if (severity < LOG_DEBUG)
	   		fprintf(stderr, "%s  ", a_time);
		vfprintf(stderr, format, ap);
	}

	if (severity < LOG_DEBUG)
		fprintf(logfp, "%s  ", a_time);
	vfprintf(logfp, format, ap);

	if (syserr == 0) {
		/* Do nothing for now */
	} else if (syserr < sys_nerr) {
		if (severity <= l_debug)
			fprintf(stderr, ": %s\n", sys_errlist[syserr]);
		fprintf(logfp, ": %s\n", sys_errlist[syserr]);
	} else {
		if (severity <= l_debug)
			fprintf(stderr, ": errno %d\n", syserr);
		fprintf(logfp, ": errno %d\n", syserr);
	}
	if (ftell(logfp) > MAX_LOG_SIZE)
		reset_log(0);
}

static char    *Log_Event_Types[] = 
                {"Rcv UDP", "Rcv Raw", "Rcv API",
		 "Snd UDP", "Snd Raw", "Snd U+R",
		 "API Reg", "API Rsv", "API Cls",
		 "API Upc",  "APIdbug", "APIstat",
		 "AddFlow", "ModFlow", "DelFlow", "AddFilt", "DelFilt",
		 "Ignore",  "Gen Err"
		};

/* log_event(): write event entry to log and to stderr if logging
 *	severity (l_debug) is LOG_DEBUG.
 */
void
log_event(int evtype, char *type, SESSION *destp, const char *format, ...)
	{
	va_list	ap;
	char	*a_time = rsvp_timestamp();
	char	a_sess[30];

	if (l_debug < LOG_DEBUG)  /* (Extra test: call has already tested */
		return;	  	  /* l_debug >= LOG_DEBUG, in IsDebug macro.*/

	va_start(ap, format);

	if (!destp)
		return;

	if (Log_Event_Cause == NULL) Log_Event_Cause = " ";
	sprintf(a_sess, "%s:%d[%d]",
			inet_ntoa(destp->sess_u.sess_ipv4.sess_destaddr),
			ntoh16(destp->sess_u.sess_ipv4.sess_destport),
			destp->sess_u.sess_ipv4.sess_protid);
	fprintf(stderr, "%12s|%s %-8.8s %-9.9s %s    ", 
		a_time, Log_Event_Cause, Log_Event_Types[evtype], type, a_sess);
	vfprintf(stderr, format, ap);
	fprintf(logfp,  "%12s|%s %-8.8s %-9.9s %-24s ", 
		a_time, Log_Event_Cause, Log_Event_Types[evtype], type, a_sess);
 
	vfprintf(logfp, format, ap);
	Log_Event_Cause = NULL;
	if (ftell(logfp) > MAX_LOG_SIZE)
		reset_log(0);		
}

/*
 *  Print out hex dump of specified range of storage.
 */
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
		fprintf(fp, "x%2.2lx: ", (unsigned long)(cp-p));
		for (wd = 0; wd<4; wd++)  {
			memcpy((char *) &temp, cp, sizeof(u_long) );
			if (len > 4) {
				fprintf(fp, " x%8.8lx", temp) ;
				cp += sizeof(long);
				len -= sizeof(long);
			}
			else {
				fprintf(fp, " x%*.*lx", 2*len, 2*len, temp);
				len = 0;
				break ;
			}
		}
		fprintf(fp, "\n") ;
	}
}

/* String copy routine that truncates if necessary to n-1 characters, 
 *   but always null-terminates.
 */
char *strtcpy(s1, s2, n)
char *s1, *s2;
int n;
    {
    strncpy(s1, s2, n);
    *(s1+n-1) = '\0';
    return(s1);
}

/*
 * Convert an IP address in u_long (network) format into a printable string.
 * Taken from mrouted code.
 */
char *inet_fmt(addr, s)
    u_int32_t addr;
    char *s;
{
    register u_char *a;

    a = (u_char *)&addr;
    sprintf(s, "%u.%u.%u.%u", a[0], a[1], a[2], a[3]);
    return (s);
}

/*
 *	Format bitmask into string s
 */
char *
bm_expand(bm,s)
u_long bm;
char *s;
{
    int i,j=0,first=1;
    
    s[j++] = '<';
    for (i=0; i<RSRR_MAX_VIFS; i++) {
	if (BIT_TST(bm,i)) {
	    if (!first)
		s[j++] = ',';
	    s[j++] = i+48;
	    first = 0;
	}
    }

    s[j++] = '>';
    s[j++] = '\0';
    return s;
}
