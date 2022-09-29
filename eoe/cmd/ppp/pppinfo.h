/* Find info about PPP daemons
 */

#ifndef _PPPINFO_H
#define _PPPINFO_H

#ident "$Revision: 1.7 $"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stream.h>

#include <sys/if_ppp.h>
#include "pputil.h"


struct ppp_status {
    __int32_t	ps_version;
#    define	PS_VERSION 10
    time_t	ps_initial_date;
    time_t	ps_cur_date;		/* current time of day */
    struct in_addr ps_lochost;
    struct in_addr ps_remhost;
    char	ps_rem_sysname[SYSNAME_LEN+1];
    pid_t	ps_pid;
    pid_t	ps_add_pid;
    __int32_t	ps_debug;
    __int32_t	ps_maxdevs;
    __int32_t	ps_outdevs;
    __int32_t	ps_mindevs;
    __int32_t	ps_numdevs;
    time_t	ps_idle_time;		/* seconds */
    time_t	ps_busy_time;
    time_t	ps_beepok_time;
    time_t	ps_add_time;		/* do not add lines until then */
    time_t	ps_add_backoff;
    time_t	ps_atime;		/* check to add or delete then */
    char	ps_idle;
    __uint32_t	ps_beep_st;
    float	ps_cur_ibps;
    float	ps_cur_obps;
    float	ps_avail_bps;
    float	ps_prev_avail_bps;
    float	ps_link_ibps;
    float	ps_link_obps;
    struct acct ps_acct;
    struct ps_dev {
	char	    callee;		/* ppp->dv.callmode == CALLEE */
	time_t	    ps_active;		/* ppp->dv.active */
	float	    bps;		/* ppp->bps */
	float	    lcp_echo_rtt;	/* ppp->lcp.echo_rtt */
	__int32_t    lcp_neg_mtru;	/* mp_ppp->lcp.neg_mtru */
	__int32_t    lcp_neg_mrru;	/* mp_ppp->lcp.neg_mrru */
	__int32_t    lcp_neg_mtu;	/* ppp->lcp.neg_mtu */
	__int32_t    lcp_neg_mru;	/* ppp->lcp.neg_mru */
	__int32_t    bad_ccp;		/* ppp->ccp.cnts.bad_ccp */
	__int32_t    ccp_rreq_sent;	/* ppp->ccp.cnts.rreq_sent */
	__int32_t    ccp_rreq_rcvd;	/* ppp->ccp.cnts.rreq_rcvd */
	__int32_t    ccp_rack_rcvd;	/* ppp->ccp.cnts.rack_rcvd */
	__int32_t    ident_len;		/* ppp->lcp.ident_rcvd_len */
	struct ppp_ident ident;		/* ppp->lcp.ident_rcvd */
    } ps_devs[MAX_PPP_LINKS];
    struct ppp_info ps_pi;
};

extern int pppinfo_get(int, struct ppp_status *);
extern int pppinfo_next_unit(int, int, struct ppp_status *);

#endif
