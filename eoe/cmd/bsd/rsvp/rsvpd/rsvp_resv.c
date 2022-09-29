
/*
 * @(#) $Id: rsvp_resv.c,v 1.12 1998/11/25 08:43:14 eddiem Exp $
 */

/************************ rsvp_resv.c  *******************************
 *                                                                   *
 *      Routines to receive, process, and send Resv and ResvTear     *
 *		messages.                                            *
 *                                                                   *
 *                                                                   *
 *********************************************************************/

/****************************************************************************

            RSVPD -- ReSerVation Protocol Daemon

                USC Information Sciences Institute
                Marina del Rey, California

            Original Version: Shai Herzog, Nov. 1993.
	    Current Version:  Steven Berson & Bob Braden, may 1996.

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
#ifdef _MIB
#include "rsvp_mib.h"
#endif

/*  Define some global objects
 *
 */
RSVP_HOP	Next_Hop_Obj;
STYLE		Style_Obj;
CONFIRM		Confirm_Obj;
FILTER_SPEC	Wildcard_FilterSpec_Obj;
#define		WILDCARD_FILT &Wildcard_FilterSpec_Obj

/*	The following globals are used in merging reservations across
 *	different PSB's for the same PHOP.
 */
static FLOWSPEC	*max_specp;
static RSB	*confRSBp;
static Fobject	*UnkObjL_perPHOP;

/* External declarations */
Session 	*locate_session(SESSION *);
Session 	*locate_session_p(SESSION *);
struct packet	*new_packet_area(packet_area *);
int		match_filter(FILTER_SPEC *, FILTER_SPEC *);
int		Compare_Flowspecs(FLOWSPEC *, FLOWSPEC *);
FLOWSPEC	*LUB_of_Flowspecs(FLOWSPEC *, FLOWSPEC *);
FLOWSPEC	*GLB_of_Flowspecs(FLOWSPEC *, FLOWSPEC *);
int 		addTspec2sum(SENDER_TSPEC *, SENDER_TSPEC *);
int		Compare_Tspecs(SENDER_TSPEC *, SENDER_TSPEC *);
u_int32_t	Compute_R(TIME_VALUES *), Compute_TTD(TIME_VALUES *);
int		match_scope(SCOPE *, SCOPE *);
int		form_scope_union(Session *);
void		scope_catf(SCOPE **, FILTER_SPEC *);
int		api_RESV_EVENT_RSB(Session *, RSB *, int);
void	 	rsvp_resv_err(int, int, int, FiltSpecStar *, struct packet *);
void		rsvp_RSB_err(Session *, RSB *, int, int);
Object_header	*copy_object(Object_header *);
void		api_RESV_EVENT_upcall(Session *, RSB *, TCSB*, int);
int		send_pkt_out_if(int, RSVP_HOP *, struct packet *);
void		send_confirm(Session *, RSB *);
void		clear_scope_union(Session *);
char		*fmt_flowspec(FLOWSPEC *);
char		*fmt_tspec(SENDER_TSPEC *);
char		*cnv_flags(char *, u_char);
int		unicast_route(u_long);
void		FQkill(Fobject **);
void		merge_UnkObjL2(Fobject **, Fobject **, int);
int		match_policy(POLICY_DATA *, POLICY_DATA *);

/* Forward declarations */
int		accept_resv(int, struct packet *);
int		accept_resv_tear(int, struct packet *);
int		flow_reservation(Session *, struct packet *, int,
			FLOWSPEC *, FiltSpecStar *);
int		tear_reserv(Session *, RSVP_HOP *, FiltSpecStar *,style_t);
void		resv_tear_PSB(Session *, PSB *, style_t, FILTER_SPEC *,
							struct packet *);
void		delete_resv4PSB(Session *, PSB *);
static void	send_resv_out(PSB *, struct packet *);
void		common_resv_header(struct packet *, Session *);
void		common_resv_tear_header(struct packet *, Session *);
RSB	*	make_RSB(Session *, int count);
int		enlarge_RSB_filtstar(RSB *);
RSB	*	locate_RSB(Session *, RSVP_HOP *, FiltSpecStar *, style_t);
RSB	*	RSB_match_path(Session *, PSB *);
TCSB	*	make_TCSB(Session *, int, int);
TCSB	*	trade_TCSB(Session *, TCSB *, int);
TCSB	*	locate_TCSB(Session *, int, FiltSpecStar *, style_t);
int		kill_RSB(Session *, RSB *);
void		kill_newRSB(Session *, RSB *);
int		kill_RSB1(Session *, RSB *, int);
void		kill_TCSB(Session *, TCSB *);
void		resv_update_ttd(RSB *);
int		resv_refresh(Session *, int);
int		resv_refresh_TimeOut(Session *);
int		resv_refresh_PSB(Session *, PSB *, struct packet *, int);
int		update_TC(Session *, RSB *);
void		PSB_update_TC(Session *, PSB *);
void		log_K(int, Session *, TCSB *);
FiltSpecStar *	Get_FiltSpecStar(int);
int		find_fstar(FILTER_SPEC *, FiltSpecStar *);
int		match_filt2star(FILTER_SPEC *, FiltSpecStar *);
int		match_fstar2star(FiltSpecStar *, FiltSpecStar *);
int		set_need_scope(Session *);
int		Styles_are_compat(style_t, style_t);
int		union_filtstar(FiltSpecStar *, FiltSpecStar **);
int		IsRoutePSB2nhop(Session *, PSB *, RSVP_HOP *);
int		is_scope_needed(Session *, PSB *);
void		process_dummy_rtear(struct packet *);
void		coalesce_filtstar(FiltSpecStar *);
int		sameas_last_spec(FLOWSPEC **, FLOWSPEC *);

#define SizeofFiltSpecStar(n) (sizeof(FiltSpecStar)+(n-1)*sizeof(struct fst_pair))


/*
 * accept_resv():  Process incoming Resv message, which is in host
 *	byte order.  It may be real packet from remote node or internal
 *	packet from local API.
 *
 *	pkt struct includes:
 *		pkt_data -> packet buffer
 *		pkt_map -> map of objects in packet
 *		pkt_ttl = TTL with which it arrived (if not UDP)
 *		pkt_flags & PKTFLG_USE_UDP: was it encapsulated?
 *
 *	This routine does the common checking and then calls
 *	flow_reservation() for each distinct flow descriptor packed
 *	into the message.
 *	
 */
int
accept_resv(
	int		 vif,	    /* Phyint on which Resv msg arrived */
	struct packet	*pkt)
	{
	int		 out_vif;
	Session    	*destp;
	int		 i, rc;
	int		 need_refresh = 0;
	style_t		 style;
	FiltSpecStar	 filtss, *filtssp;
	FLOWSPEC	*specp;
	RSB		*rp;
	
	assert(pkt->pkt_order == BO_HOST);
	style = Style(pkt);

	/* 	Determine Outgoing Interface OI.
	 *
	 * 	The logical outgoing interface OI is taken from the LIH in
	 *	the NHOP object (non-RSVP cloud may cause packet to appear on 
	 *	another interface).
	 */
	out_vif = (pkt->rsvp_nhop.s_addr == INADDR_ANY)? vif_num:
							pkt->rsvp_LIH;
	Incr_ifstats(out_vif, rsvpstat_msgs_in[RSVP_RESV]);
#ifdef _MIB
	pkt->pkt_in_if = vif;
#endif

	/*	Check that INTEGRITY was included if it is required.
	 */
	if ((vif_flags(out_vif)&IF_FLAG_Intgrty) && 
			pkt->pkt_map->rsvp_integrity == NULL)
		return PKT_ERR_INTEGRITY;
		

	/*	Look for path state.  If it does not exist, send
	 * 	"No path info" ResvErr msg.  If there is confusion about
	 *	zero DstPorts, send "Conflicting Dest ports" error.
	 */
	destp = locate_session_p(pkt->rsvp_sess);
	if (!destp) {
		rsvp_resv_err(RSVP_Err_NO_PATH, 0, 0,
					(FiltSpecStar *) -1 /*all*/, pkt);
		return(-1);
	}
	else if ((int)destp == -1) {
		rsvp_resv_err(RSVP_Err_BAD_DSTPORT, 0, 0,
					(FiltSpecStar *) -1 /*all*/, pkt);
		return(-1);
	}

	/*
	 *   Check for incompatible styles, and if found, reject message
	 *   with a "Conflicting style" error.  (Should never happen for
	 *   resv from local API, since old resv should be torn down.)
	 */
	for (rp = destp->d_RSB_list; rp != NULL; rp = rp->rs_next) {
		if (!Styles_are_compat(rp->rs_style, style)) {
			assert(rp->rs_OIf < vif_num);
			rsvp_resv_err(RSVP_Err_BAD_STYLE, 0, 0,
					(FiltSpecStar *) -1 /*all*/, pkt);
			return(-1);
		}
	}

	/*	Process the flow descriptor list to make reservations,
	 *	depending upon the style.  Filtss is a filter spec list.
	 */
	switch (style) {
	    case STYLE_WF:
		if (!pkt->rsvp_scope)
			Incr_ifstats(out_vif, rsvpstat_no_inscope);
		filtss.fst_count = 1;
		filtss.fst_Filtp(0) = WILDCARD_FILT;
		filtss.fst_Filt_TTD(0) = 0;
		specp = spec_of(FlowDesc_of(pkt, 0));
		rc = flow_reservation(destp, pkt, out_vif, specp, &filtss);	
		break;

	    case STYLE_FF:
		/*	Style FF: execute independently for each flow
		 *	descriptor in the message.
		 */
		rc = -1;
		filtss.fst_count = 1;
		for (i = 0; i < pkt->rsvp_nflwd; i++) {
			int trc;

			filtss.fst_Filtp(0) = filter_of(FlowDesc_of(pkt, i));
			filtss.fst_Filt_TTD(0) = 0;
			specp = spec_of(FlowDesc_of(pkt, i));
			trc = flow_reservation(destp, pkt, out_vif, specp, 
								&filtss);
			rc = MAX(trc, rc);
		}
		break;

	    case STYLE_SE:
		
		/* Obtain storage and construct FILTER_SPEC*, i.e., 
		 *	list of FILTER_SPECs, dynamically.
		 */
		filtssp = Get_FiltSpecStar(pkt->rsvp_nflwd);
		if (!filtssp)
	 		{
			Log_Mem_Full("Resv1");
			return(-1);
		}
		filtssp->fst_count = pkt->rsvp_nflwd;
		for (i=0; i < pkt->rsvp_nflwd; i++)
			filtssp->fst_Filtp(i) = filter_of(FlowDesc_of(pkt, i));
		specp = spec_of(FlowDesc_of(pkt, 0));
		rc = flow_reservation(destp, pkt, out_vif, specp, filtssp);
		free(filtssp);			
		break;

	    default:
		rsvp_resv_err(RSVP_Err_UNKNOWN_STYLE, 0, 0,
					(FiltSpecStar *) -1 /*all*/, pkt);
		break;
	}
	if (rc < 0)
		return(-1);
	else if (rc > 0)
		need_refresh = 1;

	/*
	 *	If any resv succeeded and refresh timer has not
	 *	been started, start it now.
	 */
	if (destp->d_RSB_list && (Compute_R(&destp->d_timevalr) == 0))
		add_to_timer((char *) destp, TIMEV_RESV, destp->d_Rtimor);

	/*	If the refresh_needed flag is now on, execute
	 *	resv_refresh to send immediate refresh through every
	 *	interface noted in d_r_incifs.
	 */
	dump_ds(need_refresh);
	if (need_refresh)
		resv_refresh(destp, 0);

	return(0);
}

/*
 * flow_reservation(): Process a given flow descriptor within a Resv
 *	msg, by finding or creating an RSB.  But if an error is found,
 *	create no RSB, send ResvErr message, and return.
 *
 *	Returns: -1 for error
 *	          1 OK, refresh needed
 *		  0 OK, no refresh
 */
int
flow_reservation(
	Session		*destp,		/* Session			*/
	Resv_pkt	*pkt,		/* Packet itself		*/
	int		 out_vif,	/* True Outgoing interface	*/
	FLOWSPEC	*specp,		/* Flowspec			*/
	FiltSpecStar	*filtssp)	/* Ptr to FILTER_SPEC*		*/
	{
	RSB		*rp;
	PSB		*sp;
	FLOWSPEC	*rs_oldspec = NULL;
	FILTER_SPEC	*filtp;
	int		 NeworMod = 0;
	int		 new_RSB = 0;
	int		 sender_cnt, i, rc;
	u_int32_t	 time2die;

	style_t		 style = Style(pkt);
		
	/*	Check the path state, as follows.
	 *
	 *	1. Locate the set of PSBs (senders) that route to OI and
	 *	   whose SENDER_TEMPLATES match *filtssp.
	 */
	sender_cnt = 0;
	for (sp = destp->d_PSB_list ; sp != NULL; sp = sp->ps_next) {

		if (!IsRoutePSB2nhop(destp, sp, pkt->pkt_map->rsvp_hop))
			continue;
		i = find_fstar(sp->ps_templ, filtssp);
		if (i<0)
			continue;
		/*
		 *	Count these PSBs, and add OI to a bit vector of
		 *	incoming interfaces for immediate refresh.
		 */
		sender_cnt++;
		BIT_SET(destp->d_r_incifs, sp->ps_in_if);
		filtssp->fst_Filt_TTD(i) = 1;
	}
	if (sender_cnt == 0) {
		/*	If this set is empty, build and send an error message
		 *	specifying "No Sender Information", and continue with
		 *	the next flow descriptor in the Resv message.
		 *
		 * XXX We are using permissive interpretation for SE: OK
		 *	if ANY of its FILTER_SPECs matches a sender.
		 */
 		rsvp_resv_err(RSVP_Err_NO_SENDER, 0, 0, filtssp, pkt);
		return(-1);
	}
	/*	If the style is SE and if some FILTER_SPEC included in
	 *	in Filtss matches no PSB, its TTD is still zero; coalesce
	 *	to delete all such FILTER_SPECs from Filtss.
	 */
	if (sender_cnt < filtssp->fst_count)
		coalesce_filtstar(filtssp);
	
	/*
	 *   o  Find or create a reservation state block (RSB) for
	 *	(SESSION, NHOP).  If the style is distinct, Filtss is also
	 *	used in the selection.  Call this the "active RSB".
	 *	
	 *   o  Start or restart the cleanup timer on the active RSB, or,
	 *	in the case of SE style, on each FILTER_SPEC of the RSB
	 *	that also appears in Filtss.
	 */
	time2die = Compute_TTD(pkt->pkt_map->rsvp_timev);
	rp = locate_RSB(destp, pkt->pkt_map->rsvp_hop, filtssp, style);
	if (!rp) {
		rp = make_RSB(destp, filtssp->fst_count);
		if (!rp) {
			Log_Mem_Full("Resv3");
			return(-1);
		}
		new_RSB = 1;

		/*	Active RSB is new:
		 *	1. Set NHOP, OI, and style from message.
		 *	2. Copy FILTER_SPEC* into the RSB.
		 *	3. Copy the FLOWSPEC and any SCOPE object into RSB.
		 *	4. Move pointer to unknown object list into RSB.
		 *	5. Set NeworMod flag on.
	 	 */
		rp->rs_nhop = pkt->rsvp_nhop;
		rp->rs_rsvp_nhop.hop4_LIH = pkt->rsvp_LIH;
		rp->rs_OIf = out_vif;
		rp->rs_style = Style(pkt);
		for (i = 0; i < filtssp->fst_count; i++) {
			filtp = copy_filter(filtssp->fst_Filtp(i));
			if (!filtp && (filtssp->fst_Filtp(i))) {
				kill_newRSB(destp, rp);
				Log_Mem_Full("Resv5");
				return(-1);
			}
			rp->rs_Filtp(i) = filtp;
			rp->rs_Filt_TTD(i) = time2die;
			rp->rs_fcount++;
		}
		rp->rs_spec = copy_spec(specp);
		if (!rp->rs_spec) {
			kill_newRSB(destp, rp);
			Log_Mem_Full("Resv4");
			return(-1);
		}
		if (pkt->pkt_map->rsvp_scope_list) {
			rp->rs_scope= copy_scope(pkt->pkt_map->rsvp_scope_list);
			if (!rp->rs_scope) {
				kill_newRSB(destp, rp);
				Log_Mem_Full("Resv6");
				return(-1);
			}
		}
		clear_scope_union(destp); /* Set to recompute scope union */
		NeworMod = 1;
#ifdef _MIB
		if (mib_enabled) {
		        rp->mib_resvhandle = mglue_new_resv(destp, rp, pkt);
			rsvpd_mod_rsvpSessionReceivers(destp->mib_sessionhandle, 1);
			if (!IsAPI(destp->d_PSB_list)) {
			     rp->mib_resvfwdhandle = mglue_new_resvfwd(destp, rp, pkt);
			     rsvpd_mod_rsvpSessionRequests(destp->mib_sessionhandle, 1);
			}
		}
#endif
	}
	/*	If the active RSB is not new, check whether Filtss from the
	 *	message contains FILTER_SPECs that are not in the RSB; if
	 *	so, add the new FILTER_SPECs and turn on the NeworMod flag.
	 */
	if (!new_RSB) {
		for (i= 0; i < filtssp->fst_count; i++) {
			FILTER_SPEC *nu_filtp = filtssp->fst_Filtp(i);
			int	j;

			j = find_fstar(nu_filtp, rp->rs_filtstar);
			if (j < 0) {
				j = find_fstar(NULL, rp->rs_filtstar);
				if (j < 0) {
					if (!enlarge_RSB_filtstar(rp)) {
						Log_Mem_Full("Resv7");
						return(-1);
					}
					j = find_fstar(NULL, rp->rs_filtstar);
				}
				rp->rs_Filtp(j) = copy_filter(nu_filtp);
				NeworMod = 1;
			}
			rp->rs_Filt_TTD(j) = time2die;				
		}
	}

	/*	Free any old list of unknown objects, and save any new list.
	 */
	FQkill(&rp->rs_UnkObjList);
	rp->rs_UnkObjList = pkt->pkt_map->rsvp_UnkObjList;
	pkt->pkt_map->rsvp_UnkObjList = NULL;

	resv_update_ttd(rp);

	/*
	 *	If the message contained a RESV_CONFIRM object, copy it
	 *	into the RSB and turn on the NeworMod flag.
	 */
	if (pkt->pkt_map->rsvp_confirm) {
		rp->rs_confirm = (CONFIRM *) copy_object(
				(Object_header *) pkt->pkt_map->rsvp_confirm);
		NeworMod = 1;
	}

	/*
	 *	If active RSB is not new, check whether STYLE, FLOWSPEC,
	 *	or SCOPE objects have changed; if so, copy changed
	 *	object into RSB and turn on the NeworMod flag.
	 */
	if (!new_RSB) {
		if (rp->rs_style != style) {
			rp->rs_style = style;
			NeworMod = 1;
		}		
		if (Compare_Flowspecs(specp, rp->rs_spec) != SPECS_EQL) {
			/* If flowspec has changed, keep old one in case
			 *	Admission Control fails...
			 */
        		rs_oldspec = rp->rs_spec;
       			rp->rs_spec = copy_spec(specp);
			NeworMod = 1;
		}
		if ((pkt->pkt_map->rsvp_scope_list) &&
		    !match_scope(rp->rs_scope, pkt->pkt_map->rsvp_scope_list)){
			clear_scope_union(destp); /* Recompute scope union */
			if (rp->rs_scope)
				free(rp->rs_scope);
			rp->rs_scope = 
				copy_scope(pkt->pkt_map->rsvp_scope_list);
			NeworMod = 1;
		}
	}
	/*
	 *	If NeworMod flag is off, continue with next flow
	 *	descriptor.
	 */
	if (!NeworMod)
		return(0);

	/*	Call Update Traffic Control routine to make/change/delete
	 *	kernel reservation based upon current resv and path state.
	 *	Returns: -1 if Admission Control failed.
	 *	          1 if changed kernel state
	 *		  0 otherwise.
	 */
	rc = update_TC(destp, rp);
	if (rc < 0) {
		int	flags = (!new_RSB)? ERROR_SPECF_InPlace : 0;

		/*
		 *	Admission Control failed; send error msg using
		 *	side-effect variable rsvp_errno.
		 *	If RSB was old, restore old reservation;
		 *	otherwise, delete new RSB.
		 */
		rsvp_resv_err(Get_Errcode(rsvp_errno), Get_Errval(rsvp_errno), 
						flags, filtssp, pkt);
		if (new_RSB)
			kill_newRSB(destp, rp);
		else {
			if (rp->rs_confirm)
				free(rp->rs_confirm);
			rp->rs_confirm = NULL;
			free((char *) rp->rs_spec);
			rp->rs_spec = rs_oldspec;
		}
		return(-1);
	}
	else if (rp->rs_confirm)
		rc = 1;
	if (rs_oldspec) {
		free((char *) rs_oldspec);
#ifdef _MIB
		if (mib_enabled)
		        rsvpd_update_rsvpResvTspec(rp->mib_resvhandle,
						   rp->rs_spec);
#endif
	}
	return(rc);
}



/*
 *  accept_resv_tear(): Process an incoming ResvTear message
 */
int
accept_resv_tear(
	int		in_if,	/* Alleged outgoing iface (IGNORED)  */
	struct packet	*pkt)
	{
	Session		*destp;
	PSB		*sp;
	RSB		*rp;
	FiltSpecStar	filtss, *filtssp = NULL;  /* FILTER_SPEC* in packet */
	packet_area	data;
	struct packet	*outpkt;
	int		out_vif;
	style_t		style = Style(pkt);
	int		Refresh_Needed;
	int		m_size = (char *) &pkt->pkt_map->rsvp_list -
					(char *) pkt->pkt_map;
	int		i, j, nmatch;
	RSVP_HOP	*nhopp;

	/*	The logical outgoing interface OI is taken from the LIH in
	 *	the NHOP object.
	 */
	out_vif = (pkt->rsvp_nhop.s_addr == INADDR_ANY)? vif_num:
							pkt->rsvp_LIH;
	Incr_ifstats(out_vif, rsvpstat_msgs_in[RSVP_RESV_TEAR]);

	/*	Check that INTEGRITY was included if it is required.
	 */
	if ((vif_flags(out_vif)&IF_FLAG_Intgrty) && 
			pkt->pkt_map->rsvp_integrity == NULL)
		return PKT_ERR_INTEGRITY;

	destp = locate_session(pkt->rsvp_sess);
	if (!destp)
		return(0);

	/*	Check that styles match.  Send BAD_STYLE error if not,
	 *	and ignore ResvTear message [THIS CASE NOT IN SPEC]
	 */
 	if ((rp = destp->d_RSB_list) && (rp->rs_style != style)) {
		rsvp_resv_err(RSVP_Err_BAD_STYLE, 0, 0,
					(FiltSpecStar *) -1 /*all*/, pkt);
		return(0);
	}
			
	/*	Process the flow descriptor list in the ResvTear message to
	 *	tear down local reservation state in style-dependent manner.
	 */
	nhopp = pkt->pkt_map->rsvp_hop;
	switch (style) {
	    case STYLE_WF:
		filtss.fst_count = 1;
		filtss.fst_filtp0 = WILDCARD_FILT;
		Refresh_Needed = 
			tear_reserv(destp, nhopp, &filtss, style);		
		break;

	    case STYLE_FF:
		filtss.fst_count = 1;
		for (i = 0; i < pkt->rsvp_nflwd; i++) {
			filtss.fst_filtp0 = filter_of(FlowDesc_of(pkt, i));
			Refresh_Needed |=
				tear_reserv(destp, nhopp, &filtss, style);
		}
		break;

	    case STYLE_SE:
		
		/* Obtain storage and construct FILTER_SPEC*, i.e., 
		 *	list of FILTER_SPECs, dynamically.
		 */
		filtssp = Get_FiltSpecStar(pkt->rsvp_nflwd);
		if (!filtssp)
	 		{
			Log_Mem_Full("Resv6");
			return(-1);
		}
		filtssp->fst_count = pkt->rsvp_nflwd;
		for (i=0; i < pkt->rsvp_nflwd; i++) {
			filtssp->fst_Filtp(i) = filter_of(FlowDesc_of(pkt, i));
		}
		Refresh_Needed = tear_reserv(destp, nhopp, filtssp, style);	
		free(filtssp);
		break;

	    default:
		rsvp_resv_err(RSVP_Err_BAD_STYLE, 0, 0, 
					(FiltSpecStar *) -1 /*all*/, pkt);
		return(-1);
	}

	/*	Create ResvTear msg to be forwarded.  Copy into map everything
	 *	up to flow descriptor list.  Make a separate copy of the
	 *	HOP object, since we will change it when we send Tear msg(s).
	 */
	outpkt = new_packet_area(&data);
	memcpy((char*) outpkt->pkt_map, (char *) pkt->pkt_map, m_size);
	outpkt->pkt_map->rsvp_hop = (RSVP_HOP *)
					copy_object((Object_header *)nhopp);
	outpkt->pkt_map->rsvp_UnkObjList = pkt->pkt_map->rsvp_UnkObjList;
	outpkt->rsvp_scope = NULL;
	outpkt->rsvp_nflwd = 0;

	/*
	 *  1.  Select each PSB whose OutInterface_list includes the
	 *	outgoing interface OI (more strictly, that routes to NHOP).
	 *	For distinct style, SENDER_TEMPLATE must also match a
	 *	filter spec in the received ResvTear.
	 */
	nmatch = 0;
	for (sp = destp->d_PSB_list; sp != NULL; sp = sp->ps_next) {
		if (sp->ps_phop.s_addr == INADDR_ANY)
			continue;
		if (!IsRoutePSB2nhop(destp, sp, nhopp))
			continue;

		/* 2.   Select a flow descriptor Fj in the ResvTear message
		 *	whose FILTER_SPEC matches the SENDER_TEMPLATE in the
		 *	PSB.  If no match is found, return to select next PSB.
		 */
		for (j = 0; j < pkt->rsvp_nflwd; j++) {
			if (Style_is_Wildcard(style) ||
		  	    match_filter(sp->ps_templ,
					filter_of(FlowDesc_of(pkt, j))))
			break;
		}
		if (j < pkt->rsvp_nflwd) {
			nmatch++;

			/*	Do PSB-specific processing, building flow
			 *	descriptor list in outpkt.
			 */
			resv_tear_PSB(destp, sp, Style(pkt),
				 filter_of(FlowDesc_of(pkt,j)), outpkt);
		}

		/* 3.   If the next PSB is for a different PHOP or the last
		 *	PSB has been processed, forward any ResvTear message
		 *	that has been built.  Senders are ordered by phop 
		 *	address.
		 */
		if (sp->ps_next == NULL ||
		     sp->ps_next->ps_phop.s_addr != sp->ps_phop.s_addr) {
			if (outpkt->rsvp_nflwd)
				send_resv_out(sp, outpkt);
			outpkt->rsvp_nflwd = 0;
		}
	}

	/*	If any PSB's were found in the preceding step, and if the
	 *	Resv_Refresh_Needed flag is now on, execute the RESV REFRESH
	 *	sequence.  resv_tear_PSB has set bits for incoming
	 *	interfaces to be refreshed.
	 */
	if (nmatch > 0 && (Refresh_Needed))
		resv_refresh(destp, 0);	

	free(outpkt->pkt_map->rsvp_hop);
	dump_ds(1);	/* Log state change */
	return(0);
}

/*	Do PSB-specific processing to create Resv_Tear message: build flow
 *	descriptor list in outpkt.
 */
void
resv_tear_PSB(
	Session 	*destp,
	PSB		*sp,
	style_t		 style,
	FILTER_SPEC	*filtp,
	struct packet	*outpkt)
	{
	FlowDesc	*outflwdp;
	PSB		*BSBp, *tsp;
	
	/*	Delete last FLOWSPEC sent to this PSB, and blockade state.
	 */
	tsp = (Style_is_Shared(style))? sp->ps_1stphop: sp;
	if (tsp->ps_resv_spec) {
		free(tsp->ps_resv_spec);
		tsp->ps_resv_spec = NULL;
	}
	BSBp = tsp;
	if (BSBp->ps_BSB_Qb) {
		free(BSBp->ps_BSB_Qb);
		BSBp->ps_BSB_Qb = NULL;
		BSBp->ps_BSB_Tb = 0;
	}
	/* -    Search for an RSB (for any outgoing interface) to which the
	 *	PSB routes and whose FILTER_SPEC matches the PSB.
	 *
	 * -	If an RSB is found, add the PHOP of the PSB to the
	 *	Refresh_PHOP_list.
	 */
	if (RSB_match_path(destp, sp)) {
		BIT_SET(destp->d_r_incifs, sp->ps_in_if);
		return;
	}

	/* -	If no such RSB is found then add Fj to the new
	 *	ResvTear message being built, in a manner
	 *	appropriate to the style.  We omit flowspec, as
	 *	the protocol spec allows.
	 */
	outflwdp = FlowDesc_of(outpkt, outpkt->rsvp_nflwd);
	outflwdp->rsvp_specp = NULL;
	switch (style) {

	case STYLE_WF:
		/* For wildcard style, send only one ResvTear to
		 *	each previous hop.
		 */
		if (outpkt->rsvp_nflwd>0)
			return;
		outflwdp->rsvp_filtp = NULL;
		break;

	case STYLE_FF:
	case STYLE_SE:
		outflwdp->rsvp_filtp = filtp;
		break;
	}
	outpkt->rsvp_nflwd++;

}


/*
 *	Resv refresh timeout has occurred.  Refresh all Resv state for
 *	given session.
 */		
int
resv_refresh_TimeOut(Session *destp)
	{
	PSB *sp;

	/*	Clear last_FLOWSPEC pointers in all PSBs, and then
	 *	call common routine resv_refresh().
	 */
	for (sp = destp->d_PSB_list; sp != NULL; sp = sp->ps_next) {
		if (sp->ps_resv_spec)
			free(sp->ps_resv_spec);
		sp->ps_resv_spec = NULL;
	}
	return resv_refresh(destp, 0);
}
	
	
/*
 * resv_refresh():  Sends refresh Resv refresh message(s).
 *
 *	Spec says resv refreshes are to be sent to specific PHOPs, or
 *	to all PHOPs.  This implementation cuts a corner, by sending
 *	resv refreshes to specific incoming interfaces, selected by 
 *	bits in d_r_incifs; no bits => all.
 *
 *	Parameter IsResvErr is 1 if entered from processing ResvErr.
 *
 *	Returns: 0 if timer should be rescheduled, else -1.
 */
int
resv_refresh(Session *destp, int IsResvErr)
	{
	struct packet	*pkt;
	packet_area	 data;
	PSB		*sp, *lastsp;
	RSB		*rp;
	style_t		 style;

	/* Time out any expired state for this Session, and if it's
	 *	all gone, return -1 => cancel refresh timer.
	 */
	cleanup_resv_state(destp);
	if (destp->d_RSB_list == NULL)
		return(-1);

	/*	Create an output message containing INTEGRITY,
	 *	SESSION, RSVP_HOP, and TIME_VALUES objects.
	 */
	pkt = new_packet_area(&data);
	common_resv_header(pkt, destp);

	/*
	 *	Determine style for these reservations from the first
	 *	RSB for the session, and move the STYLE object into
	 *	the proto-message.
	 */
	rp = destp->d_RSB_list;
	if (!rp) return -1;
	style = rp->rs_style;
	pkt->pkt_map->rsvp_style = &Style_Obj;
	Obj_CType(&Style_Obj) = ctype_STYLE;
	Style(pkt) = style;

	/*	If style is wildcard, form union of SCOPE lists from RSB's,
	 *	with local senders deleted.  Call this scope_union.
	 */
	if (Style_is_Wildcard(style))
		form_scope_union(destp);

	/*	Initialize globals.  Then select each sender PSB for desired
	 *	incinf and call resv_refresh_PSB() to add its contribution
	 *	to merged (& packed) Resv refresh message.  For last PSB
	 *	for same PHOP, resv_refresh_PSB sends resulting Resv.
	 */
	pkt->rsvp_nflwd = 0;
	max_specp = NULL;
	confRSBp = NULL;
	UnkObjL_perPHOP = NULL;
	for (sp = destp->d_PSB_list; sp != NULL; sp = sp->ps_next) {
		/*
		 * Ignore senders whose incoming interfaces are not needed now.
		 */
		if ((destp->d_r_incifs)&&
				!BIT_TST(destp->d_r_incifs, sp->ps_in_if))
			continue;

		resv_refresh_PSB(destp, lastsp = sp, pkt, IsResvErr);
	}
	pkt->rsvp_nflwd = 0;
	BIT_ZERO(destp->d_r_incifs);
	if (confRSBp) {
		free(confRSBp->rs_confirm);
		confRSBp->rs_confirm = NULL;
	}
	return(0);
}


/*
 *	resv_refresh_PSB():  Process one PSB to generate Resv refresh
 *		message for its PHOP.  If this is last PSB for PHOP,
 *		send the Resv message.
 */
int
resv_refresh_PSB(
	Session		*destp,
	PSB		*sp,
	struct packet	*pkt,
	int		IsResvErr)
	{
	RSB		*rp;
	PSB		*BSBp;
	FlowDesc	*flwdp;
	FLOWSPEC	*fwd_specp;
	style_t		 style = Style(pkt);
	int		 B_Merge = 0;
	int		 i, n_match, cmp;
	packet_map	*mapp = pkt->pkt_map;
	int		 Need_Scope = 0;
	Fobject		*UnkObjectL;

	n_match = 0;
	UnkObjectL = NULL;
	/*
	 *	1. Select all RSBs whose Filter_spec_lists match the
	 *	   SENDER_TEMPLATE object and whose OI appears in the
	 *	   OutInterface_list of the PSB
	 *	   (i.e., to which the given PSB will route).
	 */
	for (rp = destp->d_RSB_list; rp != NULL; rp = rp->rs_next) {
		if (!IsRoutePSB2nhop(destp, sp, &rp->rs_rsvp_nhop))
			continue;
		if (!match_filt2star(sp->ps_templ, rp->rs_filtstar))
			continue;

		/* 	If sender in PSB is API, then:
		 *	(a) If RSB has a CONFIRM object, then create and send
		 *	    a ResvConf message containing the object, delete it.
		 *	(b) Continue with next PSB.
		 */
		if (IsAPI(sp)) {
			if (rp->rs_confirm) {
				send_confirm(destp, rp);
				rp->rs_confirm = NULL;
			}
			continue;
		}

		/*
		 * 2. (If the B_Merge flag is off then) ignore a blockaded
		 *	RSB, as follows.
		 *
		 *      Select BSBs that match this RSB.  If any of
		 *	these BSBs has a Qb that is not strictly larger
		 *	than TC_Flowspec, then this RSB is blockaded.
		 *	Continue processing with the next RSB.
		 */
		fwd_specp = (rp->rs_fwd_spec)? rp->rs_fwd_spec: rp->rs_spec;
		BSBp = (Style_is_Wildcard(style))? sp->ps_1stphop: sp;

		if (BSBp->ps_BSB_Qb && LT(BSBp->ps_BSB_Tb, time_now)) {
			/* Blockade state has timed out; delete it.
			 */
			free(BSBp->ps_BSB_Qb);
			BSBp->ps_BSB_Qb = NULL;
			BSBp->ps_BSB_Tb = 0;
		}
		if ((BSBp->ps_BSB_Qb) &&
		     Compare_Flowspecs(BSBp->ps_BSB_Qb, fwd_specp)!=SPEC1_GTR) {
			Incr_ifstats(BSBp->ps_in_if, rsvpstat_blockade_ev);
			continue;
		}

		n_match++;
		/*
		 *	3. Merge the flowspecs from this set of RSBs.
		 *	   Maintain RSB seen so far that has largest flowspec
		 *	   in confRSBp, for sending a confirm message.
		 */
		cmp = Compare_Flowspecs(fwd_specp, max_specp);
		assert(cmp != SPECS_INCOMPAT);
		if (cmp == SPEC1_GTR) {
			max_specp = fwd_specp;

			if (confRSBp)
			    send_confirm(destp, confRSBp);
			confRSBp = NULL;
			if (rp->rs_confirm)
				confRSBp = rp;
		}
		else {
			if (cmp == SPECS_USELUB)
				max_specp =
				      LUB_of_Flowspecs(rp->rs_spec, max_specp);
			if (rp->rs_confirm)
				send_confirm(destp, rp);
		}
		/*
		 *	Also merge the lists of unknown objects, if any.
		 */
#define MAKE_COPY 1
		merge_UnkObjL2(&rp->rs_UnkObjList, &UnkObjectL, MAKE_COPY);
	}
	if (IsAPI(sp))
		return(0);

	/*	However, if steps 1 and 2 result in finding that all
	 *	RSBs matching this PSB are blockaded, then:
	 *	-  If this Resv REFRESH sequence was invoked from
	 *	   accept_resv_err(), then return to the latter.
	 *	-  Otherwise, turn on the B_Merge flag and restart at
	 *	   step 1 (actually, second copy of code):
	 */
	if (n_match == 0) {
		if (IsResvErr)
			return(0);

		B_Merge = 1;		/* (isn't really used) */
		max_specp = NULL;
		for (rp = destp->d_RSB_list; rp != NULL; rp = rp->rs_next) {
			if (!IsRoutePSB2nhop(destp, sp, &rp->rs_rsvp_nhop))
				continue;
			if (!match_filt2star(sp->ps_templ, rp->rs_filtstar))
				continue;
			/*
			 *	3. Marge the flowspecs from this set of
			 *	   RSBs, as follows.
			 *
			 *	   Compute the GLB over the Flowspec objects
			 *	   of this set of RSBs.
			 *	   While computing the GLB, delete any
			 *	   RESV_CONFIRM objects.
			 */
			n_match++;
			max_specp = GLB_of_Flowspecs(rp->rs_spec, max_specp);
			assert(max_specp); /* These flowspecs must be OK */

			if (rp->rs_confirm) {
				free(rp->rs_confirm);
				rp->rs_confirm = NULL;
			}
			confRSBp = NULL;
		}
	}

	/*
	 *	All matching RSB's have been processed.  If there
	 *	were some, do style-dependent processing:
	 *	    o Distinct style (FF): Pack flow descriptor into pkt.
	 *	    o Shared style (SE,WF): continue to merge.
	 */
	if (n_match) {
		flwdp = FlowDesc_of(pkt, pkt->rsvp_nflwd);
		assert(max_specp);
		switch (style) {

		case STYLE_WF:
			flwdp->rsvp_specp = max_specp;
			flwdp->rsvp_filtp = NULL;
			pkt->rsvp_nflwd = 1;
			/*
			 *    If this sender has scope bit on, add this
			 *    sender host to the outgoing SCOPE list.
			 */
			if (sp->ps_flags & PSBF_InScope)
				scope_catf(&mapp->rsvp_scope_list, sp->ps_templ);
			break;

		case STYLE_FF:
			if (!confRSBp &&
			    sameas_last_spec(&sp->ps_resv_spec, max_specp)) {
				max_specp = NULL;
				FQkill(&UnkObjectL);
				break;
			}
			flwdp->rsvp_specp = max_specp;
			max_specp = NULL;
			/*  To merge filter specs, simply use sender template,
			 */
			flwdp->rsvp_filtp = sp->ps_templ;
			pkt->rsvp_nflwd++; /* Test nflwd for overflow XXX */
			break;

		case STYLE_SE:
			(FlowDesc_of(pkt, 0))->rsvp_specp = max_specp;
			flwdp->rsvp_filtp = sp->ps_templ;
			pkt->rsvp_nflwd++;  /* Test nflwd for overflow XXX */
			break;
		}
	}

	merge_UnkObjL2(&UnkObjectL, &UnkObjL_perPHOP, !MAKE_COPY);

	/*
	 *  Senders are ordered by phop address.  Return if next PSB has
	 *  the same PHOP, else finish up message and send it.
	 */
	if (sp->ps_next && sp->ps_next->ps_phop.s_addr == sp->ps_phop.s_addr) {
		return(0);
	}


	/*	If there are no flow descriptors, or if new flowspec is same
	 *	as last sent for this PSB and there is no confirmation
	 *	request, return without sending.
	 */
	if (pkt->rsvp_nflwd == 0 || (
	    Style_is_Shared(style) && !confRSBp
	    && sameas_last_spec(&sp->ps_1stphop->ps_resv_spec, max_specp))) {
		FQkill(&UnkObjL_perPHOP);
		pkt->rsvp_nflwd = 0;
		return(0);
	}
	pkt->pkt_map->rsvp_UnkObjList = UnkObjL_perPHOP;
	UnkObjL_perPHOP = NULL;

	/*
	 *	If a RESV_CONFIRM object was saved earlier, put a ptr
	 *	to it in the new Resv message.
	 */
	if (confRSBp)
		mapp->rsvp_confirm = confRSBp->rs_confirm;

	/*	If the style is wildcard, decide whether a SCOPE object
	 *	must be sent; if not, delete any SCOPE object.
	 */
	if (Style_is_Wildcard(style)) {
		Need_Scope = is_scope_needed(destp, sp);
		if (!Need_Scope) {
			free(mapp->rsvp_scope_list);
			mapp->rsvp_scope_list = NULL;
		}
	}
	/*
	 *	Send Resv message (unless there is an empty scope list)
 	 *	Must first make copy of filter spec objects and set their
	 *	class (because they were SENDER_TEMPLATEs).
	 */
	if (!Need_Scope || mapp->rsvp_scope_list) {
		for (i= 0; i < pkt->rsvp_nflwd; i++) {
			flwdp = FlowDesc_of(pkt, i);
			if (flwdp->rsvp_filtp) {
				flwdp->rsvp_filtp= copy_filter(flwdp->rsvp_filtp);
				Obj_Class(flwdp->rsvp_filtp)= class_FILTER_SPEC;
			}
		}
		send_resv_out(sp, pkt);
	} else
		/* Was not sent because scope list was empty */
		Incr_ifstats(sp->ps_in_if, rsvpstat_no_outscope);

	/*
	 *	Free storage and re-initialize pkt for next PHOP
	 */
	for (i= 0; i < pkt->rsvp_nflwd; i++) {
		flwdp = FlowDesc_of(pkt, i);
		if (flwdp->rsvp_filtp)
			free(flwdp->rsvp_filtp);
		flwdp->rsvp_specp = NULL;
	}
	pkt->rsvp_nflwd = 0;
	if (mapp->rsvp_scope_list) {
		free(mapp->rsvp_scope_list);
		mapp->rsvp_scope_list = NULL;
	}
	max_specp = NULL;
	FQkill(&pkt->pkt_map->rsvp_UnkObjList);
	return(0);
}

/*	Compare new resv refresh flowspec for particular sender/phop
 *	with the last one sent.  If they are equal, return 1, else
 *	save new one and return 0.
 */
int
sameas_last_spec(FLOWSPEC **last_specpp, FLOWSPEC *newspecp)
	{
	if (*last_specpp
	      && Compare_Flowspecs(*last_specpp, newspecp) == SPECS_EQL)
		return(1);
	if (*last_specpp)
		free(*last_specpp);
	*last_specpp = copy_spec(newspecp);
	return(0);
}


#define MAX_RTEAR_PACK 2 /* XXX  (For testing!) ***/

/*
 * cleanup_resv_state():  For given session, kill all expired reservations.
 *		For consistency, we kill a reservation by constructing and
 *		processing a dummy RESV_TEAR message.
 */
int
cleanup_resv_state(Session *destp)
	{
	packet_area	 data;
	struct packet	*pkt = NULL;
	packet_map	*mapp;
	RSB		*rpn;
	int		 i;

	rpn = destp->d_RSB_list;	/* Next RSB */
	while (rpn) {
		RSB *rp = rpn;

		rpn = rpn->rs_next;
		if (!LT(rp->rs_ttd, time_now))
			continue;
		Incr_ifstats(rp->rs_OIf, rsvpstat_resv_timeout);

		for (i= 0; i < rp->rs_fcount; i++) {
			if (LT(rp->rs_Filt_TTD(i), time_now)) {
			    FlowDesc *flwdp;

			    if (pkt == NULL) {
				pkt = new_packet_area(&data);
				common_resv_tear_header(pkt, destp);
				Style(pkt) = rp->rs_style;
				pkt->rsvp_nhop = rp->rs_rsvp_nhop.hop4_addr;
				pkt->rsvp_LIH = rp->rs_rsvp_nhop.hop4_LIH;
			    }

			    /*	If packet is full, process
			     *	the dummy packet and start again.
			     */
			    mapp = pkt->pkt_map;
			    if (mapp->rsvp_nlist >= MAX_RTEAR_PACK) {
				process_dummy_rtear(pkt);
				mapp->rsvp_nlist = 0;
			    }
			    /*	Add new filter to dummy packet
			     */
			    flwdp = FlowDesc_of(pkt, mapp->rsvp_nlist);
		    	    flwdp->rsvp_filtp = copy_filter(rp->rs_Filtp(i));
			    flwdp->rsvp_specp = NULL;
			    mapp->rsvp_nlist++;
			}
		}
		if (Style_is_Shared(rp->rs_style) || rpn == NULL ||
		    rpn->rs_nhop.s_addr != rp->rs_nhop.s_addr) {
			if (pkt && pkt->rsvp_nflwd) { 
				process_dummy_rtear(pkt);
				pkt->rsvp_nflwd = 0;	
			}		
		}					
	}
	return(0);
}

void
process_dummy_rtear(struct packet *pkt)
	{
	int i;

	if (!pkt)
		return;

	accept_resv_tear(-1, pkt);
	for (i= 0; i < pkt->rsvp_nflwd; i++) {
		if (spec_of(FlowDesc_of(pkt, i)))
			free(spec_of(FlowDesc_of(pkt, i)));
		if (filter_of(FlowDesc_of(pkt, i)))
			free(filter_of(FlowDesc_of(pkt, i)));
	}
}


/*
 *	tear_reserv(): Delete reservation for specific FiltSpecStar.
 *		Return Refresh_Needed = 1 if TC state changes.
 *		This is called from accept_resv_tear().
 */
int
tear_reserv(Session *destp, RSVP_HOP *nhopp, FiltSpecStar *filtssp,
								style_t style)
	{
	RSB	*rp;
	int	 i, j;

	/*	Find an RSB matching (SESSION, NHOP).  If the style is
	 *	Distinct, Filtss is also used in the selection.  Call this
	 *	the active RSB.
	 */
	rp = locate_RSB(destp, nhopp, filtssp, style);
	if (!rp)
		return(0);

	/*	If style is shared, delete any FILTER\_SPECs in active RSB
	 *	that match some FILTER\_SPEC in Filtss.
	 */
		/*  XXX Could construct some clever algorithm to avoid the
		 *	n*2 search in the most likely case: both lists are
		 *	in the same order.
		 */
	if (Style_is_Shared(style)) {
		for (i = 0; i < filtssp->fst_count; i++) {
			for (j= 0; j < rp->rs_fcount; j++) {
			    if (match_filter(filtssp->fst_Filtp(i), 
						rp->rs_Filtp(j))){
				free(rp->rs_Filtp(j));
				rp->rs_Filt_TTD(j) = 0;
		  	  }
			}
		}
		/*	Squeeze out and free marked filters
		 */
		coalesce_filtstar(rp->rs_filtstar);
	}
	/*
	 *	If filters all gone, delete the active RSB (and update TC)
	*/
	if (!Style_is_Shared(style) || rp->rs_fcount == 0)
		return (kill_RSB(destp, rp));
	/*
	 *	else simply update traffic control filters and return 
	 *	Refresh_Needed = 0.
	 */
	update_TC(destp, rp);
	return(0);
}


/*
 *  delete_resv4PSB():  Delete reservations corresponding to given PSB.
 *			Called from kill_PSB().
 */
void
delete_resv4PSB(Session *destp, PSB *psbp)
	{
	RSB	*rp, *rpn;
	int	j;

	/*
	 *	Find each RSB that matches this PSB
	 */
	for (rp = destp->d_RSB_list; rp != NULL; rp = rpn) {
		style_t	style = rp->rs_style;

		rpn = rp->rs_next;
		if (!IsRoutePSB2nhop(destp, psbp, &rp->rs_rsvp_nhop))
			continue;
		if (!match_filt2star(psbp->ps_templ, rp->rs_filtstar))
			continue;			
		
		if (Style_is_Wildcard(style)) {
			PSB	*sp;
			/*
			 *  Wildcard (WF) style => Search for other matching
			 *	PSB.  If one found, continue with next RSB.
			 */
 			for (sp = destp->d_PSB_list; sp; sp = sp->ps_next) {
				if (sp == psbp)
					continue;
				if (!IsRoutePSB2nhop(destp, sp,
							&rp->rs_rsvp_nhop))
					continue;
				if (match_filt2star(sp->ps_templ, 
							rp->rs_filtstar))
					break;
			}
			if (sp)
				continue;
		}
		/*	This RSB has gotta go!
		 */
		if (Style_is_Shared(style)) {
		    /*
		     *	Delete filter spec matching sender from RSB filter list.
		     */
		    for (j= 0; j < rp->rs_fcount; j++) {
		    	if (match_filter(psbp->ps_templ, rp->rs_Filtp(j))){
				free(rp->rs_Filtp(j));
				rp->rs_Filt_TTD(j) = 0;
				coalesce_filtstar(rp->rs_filtstar);
				break;
			}
		     }
		}
		/*	If RSB now has no filters or style is distinct (FF),
		 *	delete RSB (and update TC). Otherwise, only update TC.
		 */
		if (!Style_is_Shared(style) || rp->rs_fcount == 0)
			kill_RSB(destp, rp);
		else
			update_TC(destp, rp);
	}
}


/*
 *  update_TC():
 *	This routine may be invoked when path state or reservation state
 *	changes, to (re-)calculate and adjust the local traffic control
 *	state in accordance with the current reservation and path state. 
 *	The parameter *rp is the 'active' RSB.
 *
 *	Deletes TCSB and returns -1 if it fails in any way, otherwise,
 *	returns Boolean flag Resv_Refresh_Needed.  If the flag is true,
 *	update_TC also notifies any matching local applications with a
 *	RESV_EVENT upcall.
 */
int
update_TC(Session *destp, RSB *rp)
	{
	TCSB		*kp;
	RSB		*trp;
	PSB		*sp;
	FLOWSPEC	*TC_specp, *Fwd_specp;
	FiltSpecStar	*TC_FiltSp, *filtssp;
	SENDER_TSPEC	 Path_Te, *Path_Tep = NULL;
	int		 Resv_Refresh_Needed = 0, Update_TCSB = 0;
	int		 cmp, i, j, sender_cnt;
	int		 TC_flags = 0;
	u_long		 rc = TC_OK;
	int		 realflow=0;
	
	/*	Consider the set of RSB's matching SESSION, and OI from the
	 *	active RSB.  If the style is distinct, the Filterspec_list
	 *	in the RSB must also be matched.  Merge them to compute:
	 *	  o TC_Flowspec, the effective TC flowspec to be installed
	 *	  o TC_FiltStar, the union of FILTER_SPEC*'s in the set.
	 */
	TC_specp = NULL;
	TC_FiltSp = NULL;
	Fwd_specp = NULL;
	for (trp = destp->d_RSB_list; trp != NULL; trp = trp->rs_next) {
		if (trp->rs_OIf != rp->rs_OIf)
			continue;
		if (!Style_is_Shared(rp->rs_style) &&
			   !match_fstar2star(trp->rs_filtstar, rp->rs_filtstar))
			continue;

		cmp = Compare_Flowspecs(trp->rs_spec, TC_specp);
		switch (cmp) {
		    case SPEC1_GTR:
			TC_specp = trp->rs_spec;
			break;
		    case SPECS_USELUB:
			TC_specp = LUB_of_Flowspecs(trp->rs_spec, TC_specp);
			break;
		    case SPECS_INCOMPAT:
			rsvp_errno = Set_Errno( RSVP_Err_TC_ERROR, 
						RSVP_Erv_Conflict_Serv);
			return(-1);
		    default:
			break;
		}
		union_filtstar(trp->rs_filtstar, &TC_FiltSp);
	}

	/*	Scan all RSB's matching SESSION (and Filtss, if distinct
	 *	style) for all OI different from active RSB.
	 *	Set the TC_B_Police flag on if TC_Flowsped is smaller
	 *	than, or incomparable to, any FLOWSPEC in those RSBs.
	 */
	for (trp = destp->d_RSB_list; trp != NULL; trp = trp->rs_next) {
		if (trp->rs_OIf == rp->rs_OIf)
			continue;
		if (!Style_is_Shared(rp->rs_style) &&
			    !match_fstar2star(trp->rs_filtstar, rp->rs_filtstar))
			continue;
		rc = Compare_Flowspecs(trp->rs_spec, TC_specp);
		if (rc == SPEC1_GTR || rc == SPECS_INCOMPAT)
			TC_flags |= TCF_B_POLICE;
	}

	/*  	Locate the set of PSBs (senders) whose SENDER_TEMPLATEs
	 *	match Filter_spec_list in the active RSB and whose
	 *	OutInterface_list includes OI.
	 *
	 *	o Set TC_E_Police flag on if any of these PSBs have
	 *	    their E-Police flag on.  Set TC_M_Police flag on if it
	 *	    is a shared style and there is more than one PSB in
	 *	    the set.
	 *	o Compute Path_Te as the sum of the SENDER_TSPEC objects
	 *	    in this set of PSBs.
	 */
	Init_Object(&Path_Te, SENDER_TSPEC, ctype_SENDER_TSPEC);
	Path_Tep = &Path_Te;
	sender_cnt = 0;

	for (sp = destp->d_PSB_list ; sp != NULL; sp = sp->ps_next) {
		if (!IsRoutePSB2nhop(destp, sp, &rp->rs_rsvp_nhop))
			continue;

		if (sp->ps_flags & PSBF_E_Police ||
	 	   (if_vec[vif_toif[(int)sp->ps_in_if]].if_flags & 
								IF_FLAG_Police))
			TC_flags |= TCF_E_POLICE;

		if (Style_is_Wildcard(rp->rs_style) ||
		    match_filt2star(sp->ps_templ, rp->rs_filtstar)) {
			addTspec2sum(sp->ps_tspec, Path_Tep);
			sender_cnt++;
		}
	}
	if (sender_cnt > 1)
		TC_flags |= TCF_M_POLICE;

	/*   Search for a TCSB matching SESSION and OI; for a
	 *	distinct style (FF), it must also match Filter_spec_list.
	 *	If none is found, create and initialize a new TCSB.
	 */
 	kp = locate_TCSB(destp, rp->rs_OIf, rp->rs_filtstar, rp->rs_style);
	if (!kp) {
		if (!TC_FiltSp)
			return 1;
		kp = make_TCSB(destp, rp->rs_OIf, TC_FiltSp->fst_count);
		if (!kp)
			goto Mem_error;
 		Resv_Refresh_Needed = 1;
 		Update_TCSB = 1;
#ifdef __sgi
		if (if_vec[kp->tcs_OIf].if_flags & IF_FLAG_Disable) {
		     /*
		      * If the interface is disabled, return error if there is
		      * a reservation request.
		      */
		     rc = TC_ERROR;
		     /* printf("resv flow for disabled interface %d\n", kp->tcs_OIf); */
		} else if (if_vec[kp->tcs_OIf].if_flags & IF_FLAG_Pass) {
		     /*
		      * Traffic control on this interface has been bypassed,
		      * just return OK.
		      */
		     rc = 1;
		     /* printf("resv flow for bypass interface %d\n", kp->tcs_OIf); */
		} else {
		     /* normal case */
		     rc = TC_AddFlowspec(kp->tcs_OIf, TC_specp, Path_Tep, TC_flags,
					 &Fwd_specp);
		     /* printf("resv flow for normal interface %d\n", kp->tcs_OIf); */
		}
		kp->tcs_rhandle = rc;
#else
#ifdef SCHEDULE
		rc = TC_AddFlowspec(kp->tcs_OIf, TC_specp, Path_Tep, TC_flags,
							&Fwd_specp);
		kp->tcs_rhandle = rc;
#endif
#endif /* __sgi */

		if (IsDebug(DEBUG_ALL)) {
			char out[80];

			log_K(LOGEV_TC_addflow, destp, kp);
			if (IsDebug(DEBUG_EVENTS)) {
					strncpy(out, fmt_tspec(Path_Tep), 80);
					log(LOG_DEBUG, 0,
						"    flowspec= %s  Tspec=%s\n",
					fmt_flowspec(TC_specp), out);
			}
		}
		if (rc == TC_ERROR) {	/* Failed. Free TCSB. */
			kill_TCSB(destp, kp);
			if (TC_FiltSp)
				free(TC_FiltSp);
			return(-1);
		}
		/* Will set filter specs below in common code
		 */
	}

	/*
	 *	If there are no matching RSBs now, simply kill the reservation
	 *	and make upcall to matching applications with nflwd = 0.
	 */
	else if (TC_specp == NULL) {
		if (IsDebug(DEBUG_ALL)) {
			log_K(LOGEV_TC_delflow, destp, kp);
			if (IsDebug(DEBUG_EVENTS))
				log(LOG_DEBUG, 0,
					"    flowspec= %s\n",
					fmt_flowspec(kp->tcs_spec));
		}
#ifdef __sgi
		if ((if_vec[kp->tcs_OIf].if_flags & (IF_FLAG_Pass|IF_FLAG_Disable)) == 0)
		        TC_DelFlowspec(kp->tcs_OIf, kp->tcs_rhandle);
#else
#ifdef SCHEDULE
		TC_DelFlowspec(kp->tcs_OIf, kp->tcs_rhandle);
#endif
#endif /* __sgi */

#ifdef _MIB
		if (mib_enabled && 
		    ((if_vec[kp->tcs_OIf].if_flags & IF_FLAG_Disable) == 0))
		        rsvpd_lostFlow(kp->tcs_rhandle,
				       destp->mib_sessionhandle,
				       rp->mib_resvhandle,
				       rp->mib_resvfwdhandle);
#endif

		kill_TCSB(destp, kp);
		if (TC_FiltSp)
			free(TC_FiltSp);
		api_RESV_EVENT_upcall(destp, rp, NULL, 0);
		return(1);
	}
	/*	Else if there are no matching PSBs now, we must be in the
	 *	midst of removing RSBs matching a deleted PSB, but there are
	 *	still RSBs merged into this TCSB.  Just return.
	 */
	else if (sender_cnt == 0)
		return(0);
	/*
	 *	If TC_Flowspec, Path_Te, or police flags have changed,
	 *	modify reservation.
	 */
	else if (Compare_Flowspecs(TC_specp, kp->tcs_spec) != SPECS_EQL ||
	           Compare_Tspecs(Path_Tep, kp->tcs_tspec) != SPECS_EQL) {

 		Resv_Refresh_Needed = 1;
 		Update_TCSB = 1;
ModFlow: /* I know it's ugly... don't set flag if only flags differ */
#ifdef __sgi
		if (if_vec[kp->tcs_OIf].if_flags & IF_FLAG_Disable) 
		        rc = TC_ERROR;
		else if (if_vec[kp->tcs_OIf].if_flags & IF_FLAG_Pass)
		        rc = TC_OK;
		else
		        rc = TC_ModFlowspec(kp->tcs_OIf, kp->tcs_rhandle, TC_specp,
					    Path_Tep, TC_flags, &Fwd_specp);
#else
#ifdef SCHEDULE
                rc = TC_ModFlowspec(kp->tcs_OIf, kp->tcs_rhandle, TC_specp,
				Path_Tep, TC_flags, &Fwd_specp);
#endif
#endif /* __sgi */

		if (IsDebug(DEBUG_ALL)) {
			log_K(LOGEV_TC_modflow, destp, kp);
			if (IsDebug(DEBUG_EVENTS)) {
				char out[80];

				strncpy(out, fmt_tspec(kp->tcs_tspec), 80);
				log(LOG_DEBUG, 0,
					"    Old: flowspec=%s  Tspec=%s\n",
					fmt_flowspec(kp->tcs_spec), out);
				strncpy(out, fmt_tspec(Path_Tep), 80);
				log(LOG_DEBUG, 0,
					"    New: flowspec=%s  Tspec=%s\n",
					fmt_flowspec(TC_specp), out);
			}
		}
		if (rc == TC_ERROR) {  /* Failed. Leave existing TCSB*/
			return(-1);
		}
	}
	else if (TC_flags != kp->tcs_flags)
		goto ModFlow;
		/* Change of flags is purely local, does not trigger refresh
		 */

	/*
	 *	If AddFlow or ModFlow returned a new flowspec, attach it to
	 *	all matching RSBs (same OI, filter spec).
	 */
	if (Fwd_specp) {
		for (trp = destp->d_RSB_list; trp != NULL; trp = trp->rs_next) {
			if (trp->rs_OIf != rp->rs_OIf)
				continue;
			if (!Style_is_Shared(rp->rs_style) &&
			   !match_fstar2star(trp->rs_filtstar, rp->rs_filtstar))
				continue;
			if (trp->rs_fwd_spec)
				free(trp->rs_fwd_spec);
			trp->rs_fwd_spec = copy_spec(Fwd_specp);
		}
		free(Fwd_specp);
	}
	/*
	 *	Make FILTER_SPEC* in TCSB agree with TC_Filtstar, calling
	 *	traffic control as necessary.
	 *	1. Delete any filter specs in TCSB that are not in TC_Filtstar.
	 */

	filtssp = kp->tcs_filtstar;
	for (i= 0; i < filtssp->fst_count; i++) {
		if (filtssp->fst_Filtp(i)&&
			find_fstar(filtssp->fst_Filtp(i), TC_FiltSp) < 0) {
#ifdef __sgi
		        if (if_vec[kp->tcs_OIf].if_flags & (IF_FLAG_Pass|IF_FLAG_Disable)) 
			        rc = TC_OK;
			else
			        rc = TC_DelFilter(kp->tcs_OIf, filtssp->fst_p[i].Fhandle);
#else
#ifdef SCHEDULE
			rc = TC_DelFilter(kp->tcs_OIf, filtssp->fst_p[i].Fhandle);
#endif
#endif /* __sgi */
			if (IsDebug(DEBUG_ALL)) {
				log_K(LOGEV_TC_delfilt, destp, kp);
				if (IsDebug(DEBUG_EVENTS))
					log(LOG_DEBUG, 0, "    Fhandle= %d\n",
						filtssp->fst_p[i].Fhandle);
			}
			if (rc == TC_ERROR) {	/* Failed. Free TCSB. */
				kill_TCSB(destp, kp);
				if (TC_FiltSp)
					free(TC_FiltSp);
				return(-1);
			}
			filtssp->fst_p[i].Fhandle = 0;
			free(filtssp->fst_Filtp(i));
			filtssp->fst_Filtp(i) = NULL;
			Resv_Refresh_Needed = 1;
		}
	}
	/*
	 *	2. Add to TCSB any filter specs in TC_Filtstar but not in TCSB.
	 */
	for (j= 0; j < TC_FiltSp->fst_count; j++) {
		FILTER_SPEC *filtp = TC_FiltSp->fst_Filtp(j);

		if (find_fstar(filtp, filtssp)<0) {
			i = find_fstar(NULL, filtssp);
			if (i < 0) {
				/* Damn... FiltSpecStar area needs to grow! */
				kp = trade_TCSB(destp, kp,
					kp->tcs_filtstar->fst_size+16);
				if (!kp)
					goto Mem_error;
				i = find_fstar(NULL, kp->tcs_filtstar);
				assert(i >= 0);
				filtssp = kp->tcs_filtstar;
			}
#ifdef __sgi
			if (if_vec[kp->tcs_OIf].if_flags & IF_FLAG_Disable) {
			     rc = TC_ERROR;
			} else if (if_vec[kp->tcs_OIf].if_flags & IF_FLAG_Pass) {
			     /*
			      * Traffic control on this interface has been bypassed,
			      * just return OK.
			      */
			     rc = 1;
			} else {
			     /* normal case */
			     rc = TC_AddFilter(kp->tcs_OIf,
					       kp->tcs_rhandle, destp, filtp);
			     realflow = 1;
			}
#else
#ifdef SCHEDULE
			rc = TC_AddFilter(kp->tcs_OIf,
						kp->tcs_rhandle, destp, filtp);
#endif
#endif /* __sgi */

#ifdef _MIB
			/*
			 * Send notification of the new flow.  If the system
			 * administrator needs to approve all new flows, move
			 * this call above the TC_AddFlowspec call.
			 */
			if (mib_enabled && realflow && (rc != TC_ERROR))
			        rsvpd_newFlow(kp->tcs_rhandle,
					      destp->mib_sessionhandle,
					      rp->mib_resvhandle,
					      rp->mib_resvfwdhandle);
#endif /* _MIB */

			if (IsDebug(DEBUG_ALL)) {
				log_K(LOGEV_TC_addfilt, destp, kp);
				if (IsDebug(DEBUG_EVENTS))
					log(LOG_DEBUG, 0,
						"    Filter= %s  Fhandle=%d\n",
						fmt_filtspec(filtp), rc);
			}

			if (rc == TC_ERROR) {	/* Failed. Free TCSB. */
				kill_TCSB(destp, kp);
				if (TC_FiltSp)
					free(TC_FiltSp);
				return(-1);
			}
			assert(filtssp->fst_count <= filtssp->fst_size);
			filtssp->fst_p[i].Fhandle = rc;
			filtssp->fst_Filtp(i) = copy_filter(filtp);
			Resv_Refresh_Needed = 1;
		}
	}
	if (Update_TCSB) {
		/*
		 *	Since TC calls succeeded, update TCSB now.
		 */
		if (kp->tcs_spec)
			free(kp->tcs_spec);
		kp->tcs_spec = copy_spec(TC_specp);
		if (kp->tcs_tspec)
			free(kp->tcs_tspec);
		kp->tcs_tspec = copy_tspec(Path_Tep);
		if (!kp->tcs_spec || !kp->tcs_tspec)
			goto Mem_error;		/* out of memory */
	}
	kp->tcs_flags = TC_flags;
	
	/*   If the Resv_Refresh_Needed flag is on and RSB is not from
	 *   API, make a RESV_EVENT upcall to all matching applications.
	 */
	if ((Resv_Refresh_Needed) && rp->rs_nhop.s_addr != INADDR_ANY)
		api_RESV_EVENT_upcall(destp, rp, kp, 0);

	if (TC_FiltSp)
		free(TC_FiltSp);
	return(Resv_Refresh_Needed);

Mem_error:
	rsvp_errno = Set_Errno( RSVP_Err_RSVP_SYS_ERROR, RSVP_Erv_MEMORY);
	if (TC_FiltSp)
		free(TC_FiltSp);
	return(-1);
}


/*	Update traffic control with respect to given PSB, which is new
 *	or changed.
 */
void
PSB_update_TC(Session *destp, PSB *psbp)
	{
	RSB		*rp;
	bitmap		out_vifs;
	int		rc;

	BIT_ZERO(out_vifs);
	for (rp = destp->d_RSB_list; rp != NULL; rp = rp->rs_next) {
		/*
		 *	Search for first RSB for given OI that PSB
		 *	routes to and whose Filter_spec_list includes
		 *	a FILTER_SPEC matching the SENDER_TEMPLATE.
		 *	Using this as the 'active RB', update traffic
		 *	control on that OI.  Continue until all RSBs
		 *	have been processed.
		 */
		if (BIT_TST(out_vifs, rp->rs_OIf))
			continue;
		if (!IsRoutePSB2nhop(destp, psbp, &rp->rs_rsvp_nhop))
			continue;
		if (!match_filt2star(psbp->ps_templ, rp->rs_filtstar))
			continue;
		/*
		 *	Update traffic control.  In unlikely event
		 *	that admission Control fails, send ResvErr msg
		 */
		rc = update_TC(destp, rp);
		if (rc < 0)
			rsvp_RSB_err(destp, rp, Get_Errcode(rsvp_errno), 
				      Get_Errval(rsvp_errno));
	}
}


/*
 * 	IsRoutePSB2nhop():	Return 1 if sender defined by PSB can
 *				route to specified next hop.
 */
int
IsRoutePSB2nhop(Session *destp, PSB *psbp, RSVP_HOP *hopp)
	{
	/*
	 *	If Resv came from network, it cannot match LocalOnly API.
	 */ 	
	if (hopp->hop_u.hop_ipv4.hop_ipaddr.s_addr != INADDR_ANY) {
		if  (psbp->ps_flags & PSBF_LocalOnly)
			return(0);
		else if (BIT_TST(psbp->ps_outif_list, hopp->hop4_LIH))
			return(1);
	}
	else  {
		/*	Resv came from API.  OK if:
		 *	* dest is unicast, or
		 *	* incoming interface matches recv interface, or
		 *	* PSB not LocalOnly and not from API and routes to OI.
		 */
		if (!IN_MULTICAST(ntoh32(destp->d_addr.s_addr)))
			return(1);
		else if (psbp->ps_in_if == hopp->hop4_LIH)
			return(1);
		else if (!(psbp->ps_flags & PSBF_LocalOnly) && !IsAPI(psbp) &&
			  BIT_TST(psbp->ps_outif_list, hopp->hop4_LIH))
			return(1);
	}
	return(0);
}


/*	Search for (any) RSB that matches the given PSB
 */
RSB *
RSB_match_path(Session *destp, PSB *psbp)
	{
	RSB		*rp;

	for (rp = destp->d_RSB_list; rp; rp = rp->rs_next) {
		if (!IsRoutePSB2nhop(destp, psbp, &rp->rs_rsvp_nhop))
			continue;
		if (match_filt2star(psbp->ps_templ, rp->rs_filtstar))
			break;
	}
	return(rp);
}


/*
 *  Return pointer to RSB for (SESSION, NHOP, [FILTER_SPEC *]),
 *  or NULL if none is found.
 */
RSB *
locate_RSB(Session *destp, RSVP_HOP *nhopp,
				FiltSpecStar *filtssp, style_t style)
	{
	RSB *rp;

	for (rp = destp->d_RSB_list; rp != NULL; rp = rp->rs_next) {
		if (nhopp->hop4_addr.s_addr != rp->rs_nhop.s_addr)
			continue;
		if (Style_is_Shared(style) ||
		    match_filter(filtssp->fst_filtp0, rp->rs_filter0))
			break;
	}
	return(rp);
}


TCSB *
locate_TCSB(
	Session		*destp,
	int		 out_if,
	FiltSpecStar	*filtssp,
	style_t		 style)
	{
	TCSB *kp;

	if (Style_is_Shared(style))
		return(destp->d_TCSB_listv[out_if]);

	for (kp = destp->d_TCSB_listv[out_if]; kp != NULL; kp = kp->tcs_next) {
		if (match_filter(filtssp->fst_filtp0,
				kp->tcs_filtstar->fst_filtp0))
			break;
	}
	return(kp);
}


RSB *
make_RSB(
	Session		*destp,
	int		 count)
	{
	int		 n;
	RSB 		*rp;

	n = SizeofFiltSpecStar(1) + sizeof(RSB);
	rp = (RSB *) malloc(n);
	if (!rp)
		return(NULL);
	memset((char *)rp, 0, n);

#ifndef LPM
	rp->rs_next = destp->d_RSB_list;
	destp->d_RSB_list = rp;
#endif
	/* Intially, a one-slot FiltSpecStar is allocated contiguous to
	 *	RSB.  But if count>1, enlarge it by malloc.
	 */
	rp->rs_filtstar = (FiltSpecStar *)((char *) rp + sizeof(RSB));
	rp->rs_filtstar->fst_size = 1;
	rp->rs_filtstar->fst_count = 0;
	while (rp->rs_filtstar->fst_size < count) {
		if (!enlarge_RSB_filtstar(rp)) {
			if (rp->rs_filtstar->fst_size > 1)
				free(rp->rs_filtstar);
			free(rp);
			return(NULL);
		}
	}
	return(rp);
}

/*	Enlarge filtspecstar of given RSB.  Malloc a new area.
 */
int
enlarge_RSB_filtstar(RSB *rp)
	{
	int		orig_size = rp->rs_filtstar->fst_size;
	int		new_size = orig_size+4;
	int		L = SizeofFiltSpecStar(new_size);
	FiltSpecStar	*fsp = malloc(L);

	if (!fsp)
		return(0);
	memset((char *)fsp, 0, L);
	memcpy((char *)fsp, (char *)rp->rs_filtstar,
				SizeofFiltSpecStar(orig_size));
	fsp->fst_size = new_size;
	if (orig_size > 1)
		free(rp->rs_filtstar);
	rp->rs_filtstar = fsp;
	return(1);
}
	

/*	Make TCSB control block with 'nhandles' empty filter
 *	spec slots.
 */
TCSB *
make_TCSB(Session *destp, int out_if, int nhandles)
	{
	TCSB		*kp;
	int		 size;

	/* Allocate TCSB contiguous FiltSpecStar area.  Clear all.
	 */
	size = SizeofFiltSpecStar(nhandles) + sizeof(TCSB);
	kp = (TCSB *) malloc(size);
	if (!kp) {
		Log_Mem_Full("TCresv1");
		return(NULL);
	}
	memset((char *)kp, 0, size);
	kp->tcs_filtstar = (FiltSpecStar *)((char *) kp + sizeof(TCSB));

	kp->tcs_next = destp->d_TCSB_listv[out_if];
	destp->d_TCSB_listv[out_if] = kp;

	kp->tcs_OIf = out_if;
	kp->tcs_filtstar->fst_count = 0;
	kp->tcs_filtstar->fst_size = nhandles;
	return(kp);
}

/*	Expand an existing TCSB control block with space for nhandle
 *	filter handles.
 */
TCSB *
trade_TCSB(Session *destp, TCSB *kp, int nhandles)
	{
	TCSB		*nkp;
	int		 n = kp->tcs_filtstar->fst_size;

	nkp = make_TCSB(destp, kp->tcs_OIf, nhandles);
	if (!nkp) {
		Log_Mem_Full("TCresv1");
		return(NULL);
	}
	memcpy(nkp, kp, sizeof(TCSB));
	nkp->tcs_filtstar = (FiltSpecStar *)((char *) nkp + sizeof(TCSB));
	memcpy( (char *) nkp->tcs_filtstar, (char *) kp->tcs_filtstar,
						SizeofFiltSpecStar(n));
	nkp->tcs_filtstar->fst_size = nhandles;
	free(kp);
	return(nkp);
}


/*  kill_RSB(): Delete specified reservation request element RSB and adjust
 *		TC reservation state accordingly.
 */
int
kill_RSB(Session *destp, RSB *rp)
{
#ifdef _MIB
        if (mib_enabled) {
	        rsvpd_mod_rsvpSessionReceivers(destp->mib_sessionhandle, -1);
		rsvpd_deactivate_rsvpResvEntry(rp->mib_resvhandle);

		rsvpd_mod_rsvpSessionRequests(destp->mib_sessionhandle, -1);
		rsvpd_deactivate_rsvpResvFwdEntry(rp->mib_resvfwdhandle);
        }
#endif
	return (kill_RSB1(destp, rp, 1));
}

/*  kill_newRSB(): Delete specified RSB that was in process of being
 *		created, but not yet reflected in TC reservation.
 */
void
kill_newRSB(Session *destp, RSB *rp)
	{
	kill_RSB1(destp, rp, 0);
}

/*  Common inner routine for kill_[new]RSB()
 *	Returns Refresh_Needed
 */
int
kill_RSB1(Session *destp, RSB *rp, int isold)
	{
	RSB 	**rpp;
	int	  i;
	int	  Refresh_Needed = 0;

	/*	Delete RSB from list, and then call update_TC() to
	 *	make TC reservation consistent with reduced set of RSBs
	 *	(unless RSB was new).
	 *	Note: Deleting a reservation request cannot increase the
	 * 	kernel flowspec.  In the unlikely event that this results
	 * 	in an Admission Control failure, we just ignore it; the
	 * 	original reservation will stay in place.
	 */
	for (rpp = &destp->d_RSB_list; (*rpp) != NULL && (*rpp) != rp; 
			rpp = &((*rpp)->rs_next));
	if (*rpp != NULL)
		*rpp = rp->rs_next;

	if (isold)
		Refresh_Needed = update_TC(destp, rp);

	/*	If this was last RSB for session, delete Resv refresh timer
	 */
	if (destp->d_RSB_list == NULL) {
		del_from_timer((char *) destp, TIMEV_RESV);
		destp->d_timevalr.timev_R = 0;
	}
	/*	Free everything in sight...
	 */
	free(rp->rs_spec);
	for (i=0; i < rp->rs_fcount; i++)
		if (rp->rs_Filtp(i))
			free(rp->rs_Filtp(i));
	if (rp->rs_filtstar->fst_size > 1)
		free(rp->rs_filtstar);
	if (rp->rs_fwd_spec)
		free(rp->rs_fwd_spec);
	if (rp->rs_scope) {
		free(rp->rs_scope);
		clear_scope_union(destp);  /* Set to recompute scope union */
	}
	if (rp->rs_confirm)
		free(rp->rs_confirm);
	FQkill(&rp->rs_UnkObjList);
	free((char *)rp);
	return(Refresh_Needed);
}


/*	Update RSB time-to-die from MIN of ttd's of filter specs.
 */
void
resv_update_ttd(RSB *rp)
	{
	int		j;
	u_int32_t	ttd = 99999999;
	
	for (j= 0; j < rp->rs_fcount; j++)
		ttd = MIN(ttd, rp->rs_Filt_TTD(j));
	rp->rs_ttd = ttd;
}


/*
 *	kill_TCSB(): Delete kernel reservation block
 */
void
kill_TCSB(Session *destp, TCSB *kp)
	{
	TCSB		**kpp;
	FiltSpecStar	 *filtssp;
	int		  i, OIf = kp->tcs_OIf;

	if (!kp)
		return;

	/* Unlink and delete kp
	 */
	for (kpp = &destp->d_TCSB_listv[OIf]; (*kpp) != NULL && (*kpp) != kp;
						 kpp = &((*kpp)->tcs_next));
	if (*kpp != NULL)
		*kpp = kp->tcs_next;
	free(kp->tcs_spec);
	if (kp->tcs_tspec)
		free(kp->tcs_tspec);
	filtssp = kp->tcs_filtstar;
	for (i=0; i < filtssp->fst_count; i++)
		if (filtssp->fst_Filtp(i))
			free(filtssp->fst_Filtp(i));
	free((char *)kp);
}

/*	Buy storage and initialize FILTER_SPEC* structure with n slots.
 */
FiltSpecStar *
Get_FiltSpecStar(int n)
	{
	FiltSpecStar	*filtssp;
	int		 len = SizeofFiltSpecStar(n);

	filtssp = (FiltSpecStar *) malloc(len);
	if (filtssp) {
		memset((char *)filtssp, 0, len);
		filtssp->fst_size = n;
	}
	return(filtssp);
}

/*	Return index in FILTER_SPEC* vector of filter matching *filtp,
 *	or -1.  But if filtp = NULL, look for empty slot.  If can't find
 *	one, try to expand; if that fails, return -1.
 */
int
find_fstar( FILTER_SPEC *filtp, FiltSpecStar *filtssp)
	{
	int i;

	if (!filtp) {
		for (i= 0; i < filtssp->fst_count; i++) {
			if (filtssp->fst_Filtp(i) == NULL)
				return(i);
		}
		/*
		 *	Empty slot not found.  Get next slot if available.
		 */
		if (filtssp->fst_count < filtssp->fst_size)
			return(filtssp->fst_count++);
		return(-1);
	}
	for (i= 0; i < filtssp->fst_count; i++) {
		if (match_filter(filtp, filtssp->fst_Filtp(i)))
			return(i);
	}	
	return(-1);
}


/*	Match two FILTER_SPEC*'s; matches if any pair matches.
 *
 */
int
match_fstar2star(FiltSpecStar *fssp1, FiltSpecStar *fssp2)
	{
	int	i, j;

	assert(fssp1->fst_count == fssp2->fst_count);

	if (fssp1->fst_count == 1 && fssp1->fst_filtp0 == WILDCARD_FILT)
		return 1;

	for (i= 0; i < fssp1->fst_count; i++)
		for (j = 0; j < fssp2->fst_count; j++)
			if (match_filter (fssp1->fst_Filtp(i),
					  fssp2->fst_p[j].fst_filtp))
				return(1);
	return(0);
}

/*	Return 1 if FILTER_SPEC matches a filter in FILTER_SPEC*, else 0.
 *
 */
int
match_filt2star(FILTER_SPEC *filtp, FiltSpecStar *fssp)
	{
	int	i;

	if (fssp->fst_count == 1 && fssp->fst_filtp0 == WILDCARD_FILT)
		return(1);	/* wildcard */

	for (i= 0; i < fssp->fst_count; i++) {
		if ((fssp->fst_Filtp(i)) &&
		    match_filter(filtp, fssp->fst_Filtp(i)))
			return(1);
	}
	return(0);
}

/**#define INIT_FST_COUNT 100 **/
#define INIT_FST_COUNT 1 /* for testing XXX */


/*	Add new FiltSpecStar to union.  Return -1 if error, else 0.
 */
int
union_filtstar(FiltSpecStar *newfsp, FiltSpecStar **fspp)
	{
	FiltSpecStar 	*fsp = *fspp, *nfsp;
	int		i, j;

	if (fsp == NULL) {
		/*	First time... malloc an area.
		 */
		fsp = Get_FiltSpecStar(INIT_FST_COUNT);
		if (!fsp)
			return(-1);
	}
	for (i= 0; i < newfsp->fst_count; i++) {
		FILTER_SPEC *filtp = newfsp->fst_Filtp(i);

		for (j = 0; j < fsp->fst_count; j++) {
			if (match_filter(filtp, fsp->fst_Filtp(j)))
				break;
		}
		if (j == fsp->fst_count) {
			/* Add new filter to union */
			if (fsp->fst_count >= fsp->fst_size) {
				nfsp = Get_FiltSpecStar(2*fsp->fst_count);
				if (!nfsp)
					return(-1);
				memcpy(nfsp, fsp, sizeof(FiltSpecStar) +
				    (fsp->fst_count-1)*sizeof(FILTER_SPEC *));
				nfsp->fst_size = 2*fsp->fst_count;
				free(fsp);
				fsp = nfsp;
			}
			fsp->fst_p[fsp->fst_count++].fst_filtp = filtp;
		}
	}
	*fspp = fsp;
	return(0);
}


/*	Coalesce (RSB) FILTER SPEC list, removing entries whose TTD is zero.
 */
void
coalesce_filtstar(FiltSpecStar *filtssp)
	{
	int	i, j;

	i = 0;
	for (j= 0; j < filtssp->fst_count; j++) {
		if (filtssp->fst_Filt_TTD(j)) {
			filtssp->fst_Filtp(i) = filtssp->fst_Filtp(j);
			filtssp->fst_Filt_TTD(i++) = filtssp->fst_Filt_TTD(j);
		}
	}
	filtssp->fst_count = i;
}

/*
 *	Decide whether a SCOPE object is needed in WF Resv refresh message
 *	to given PHOP P.
 *
 *	A SCOPE object is needed for phop P if:
 *	    there is an RSB R and a PHOP P' different from P, such that data
 *	    packets from P are routed to R and either R has a SCOPE list or
 *	    P' is different from the NHOP address of R.
 */
int
is_scope_needed(Session *destp, PSB *psbp)
	{
	RSB		*rp;
	PSB		*sp;
	int		 phop_differs, routes_to;

	/*	For each RSB R...
	 */
	for (rp = destp->d_RSB_list; rp != NULL; rp = rp->rs_next) {

		/*	If RSB has SCOPE list, return True.  Otherwise,
		 *	scan list of PSBs for those that route to R.
		 *	  If PSB has PHOP = P, turn on routes_to flag.
		 *	  If PSB has PHOP != P and PHOP != NHOP of R,
		 *		turn on phop_differs flag.
		 */
		if (rp->rs_scope)
			return(1);
		phop_differs = 0;
		routes_to = 0;
		for (sp = destp->d_PSB_list; sp != NULL; sp = sp->ps_next) {
			
			if (!IsRoutePSB2nhop(destp, psbp, &rp->rs_rsvp_nhop))
				continue;

			if (sp->ps_phop.s_addr == psbp->ps_phop.s_addr)
				routes_to = 1;
			else if (sp->ps_phop.s_addr != rp->rs_nhop.s_addr)
				phop_differs = 1;
		}

		/*	If routes_to and phop_differs flags are both on,
		 *	a SCOPE list is needed; return True.
		 */
		if ((routes_to) && (phop_differs))
			return(1);
	}
	return(0);
}
	

/*
 *  common_resv_header():  Fill in common header fields for Resv msg
 */
void
common_resv_header(struct packet *pkt, Session *destp)
	{
	packet_map	*mapp = pkt->pkt_map;

	mapp->rsvp_msgtype = RSVP_RESV;
	mapp->rsvp_session = destp->d_session;
	mapp->rsvp_timev = &destp->d_timevalr;
	mapp->rsvp_hop = &Next_Hop_Obj;
	mapp->rsvp_dpolicy = NULL;
	pkt->rsvp_nflwd = 0;
}


/*
 *  common_resv_tear_header():  Fill in common header fields for Resv_Tear
 */
void
common_resv_tear_header(struct packet *pkt, Session *destp)
	{
	packet_map	*mapp = pkt->pkt_map;

	mapp->rsvp_msgtype = RSVP_RESV_TEAR;
	mapp->rsvp_session = destp->d_session;
	mapp->rsvp_hop = &Next_Hop_Obj;
	mapp->rsvp_style = &Style_Obj;
	Obj_CType(&Style_Obj) = ctype_STYLE;
	mapp->rsvp_nlist = 0;
}

/*
 * send_resv_out(): Send out Resv message to specified address.
 */
static void
send_resv_out(
	PSB		*psbp,
	struct packet	*pkt)
	{
	int		outif = psbp->ps_in_if;

	FORCE_HOST_ORDER(pkt);
	pkt->rsvp_nhop.s_addr = IF_toip(vif_toif[outif]);
	pkt->rsvp_LIH = psbp->ps_LIH;

	send_pkt_out_if(vif_toif[(int)psbp->ps_in_if], &psbp->ps_rsvp_phop, pkt);
}

void
log_K(int evtype, Session *destp, TCSB *kp)
	{
	char *flgstr = cnv_flags("?????BME", kp->tcs_flags);

	log_event(evtype, if_vec[kp->tcs_OIf].if_name, destp->d_session,
			" Flg=%s =>handle=%d\n", flgstr, kp->tcs_rhandle);
}

int
Styles_are_compat(style_t st1, style_t st2) {
        if (st1 == st2)
                return 1;
        else
                return 0;
}


