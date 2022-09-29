
/*
 * @(#) $Id: rsvp_path.c,v 1.10 1998/11/25 08:43:14 eddiem Exp $
 */

/************************ rsvp_path.c  *******************************
 *                                                                   *
 *  Routines to receive, process, and send Path and PathTear         *
 *		messages.    					     *
 *                                                                   *
 *                                                                   *
 *********************************************************************/

/****************************************************************************

            RSVPD -- ReSerVation Protocol Daemon

                USC Information Sciences Institute
                Marina del Rey, California

		Original Version: Shai Herzog, Nov. 1993.
		Current Version:  Steven Berson & Bob Braden, May 1996.

  Copyright (c) 1996 by the University of Southern California
  All rights reserved.

  Permission to use, copy, modify, and distribute this software and its
  documentation in source and binary forms for any purpose and without
  fee is hereby granted, provided that both the above copyright notice
  and this permission notice appear in all copies. and that any
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

/* external declarations
 */
Session		*make_session(SESSION *);
Session		*locate_session(SESSION *);
Session		*locate_session_p(SESSION *);
int		start_UDP_encap();
void		api_PATH_EVENT_upcall();
int		send_pkt_out_vif(int, Session *, FILTER_SPEC *, struct packet *);
void		move_object();
int		Compare_Tspecs(SENDER_TSPEC *, SENDER_TSPEC *);
Object_header 	*copy_object(Object_header *);
struct packet 	*new_packet_area(packet_area *);
ADSPEC		*TC_Advertise(int, ADSPEC *, int);
int		match_filt2star(FILTER_SPEC *, FiltSpecStar *);
int		match_filter(FILTER_SPEC *, FILTER_SPEC *);
int		unicast_route();
u_int32_t	Compute_TTD(TIME_VALUES *), Compute_R(TIME_VALUES *);
char		*bm_expand();
#ifndef HOST_ONLY
void		rsrr_send_rq();
#endif
void		delete_resv4PSB();
int		update_TC(Session *, RSB *);
void		PSB_update_TC(Session *, PSB *);
int		resv_refresh(Session *, int);
int		map_if(struct in_addr *);
void		clear_scope_union(Session *);
void		incr_key_assoc_seqnos();
Fobject		*FQrmv(Fobject **);
void		FQkill(Fobject **);
int		match_policy(POLICY_DATA *, POLICY_DATA *);

/* forward declarations
 */
int		accept_path(int, struct packet *);
int		accept_path_tear(int, struct packet *);
PSB		*locate_PSB(Session *, SENDER_TEMPLATE *, int, RSVP_HOP *);
static PSB	*make_PSB(Session *, RSVP_HOP *, SENDER_TEMPLATE *);
int		kill_PSB(Session *, PSB *);
PSB *		Find_fPSB(Session *, SENDER_TEMPLATE *);
void		finish_path(Session *, PSB *);
void		multicast_path_tear(Session *, struct packet *);
void		common_path_header(Session *, struct packet *);
int		Route_Query(Session *, PSB *, int *, bitmap *);
void		Route_Update(Session *, PSB *, int, bitmap);
int		check_dstport_conflict(struct packet *);
int		path_refresh_common(Session *, PSB *, struct packet *);
int		path_refresh_PSB(Session *, PSB *);
int		path_refresh_common(Session *, PSB *, struct packet *);
void		send_path_out_vif(int, Session *, SENDER_TEMPLATE *,
							struct packet *);
int		tear_PSB(Session *, PSB *, struct packet *, Fobject *);

RSVP_HOP	Prev_Hop_Obj;

/*
 * accept_path() This routine processes an incoming Path message,
 *	to build/refresh path state.
 *
 *	in_vif = incoming vif, or -1 if unknown (multicast UDP encap'd)
 */
int
accept_path(int in_vif, struct packet *pkt)
	{
	u_int32_t	 ttd;
	Session   	*destp;
	PSB       	*psbp;
	bitmap		 new_routes;
	int		 rr_vif;
	SenderDesc   	*sdp = SenderDesc_of(pkt);
	FILTER_SPEC	*filtp;
	Fobject		*fop;
#ifdef _MIB
	int		new_sender=0;
#endif

	/*
	 * Find/build a Session block
	 *
	 *	    If (a PSB is found whose) session matches the
     	 *	    DestAddress and Protocol Id fields of the received
     	 *	    SESSION object, but the DstPorts differ and one is
     	 *	    zero, then build and send a "Conflicting Dst Port"
    	 *	    PathErr message, drop the Path message, and return.
	 */
	destp = locate_session_p(pkt->rsvp_sess);
	if ((int)destp == -1) {
		rsvp_path_err(in_vif, RSVP_Err_BAD_DSTPORT, 0, pkt);
		return(-1);
	}	
	else if (destp == NULL) {
		destp = make_session(pkt->rsvp_sess);
		if (!destp)
			return(-1);
	}
        /*	Search for a path state block (PSB) whose (session,
	 *	sender_template) pair matches the corresponding objects
	 *	in the message.  For a multicast session, also match
	 *	IncInterface; for a unicast session, match PHOP address.
 	 */
	psbp = (pkt->rsvp_phop.s_addr == INADDR_ANY)?
	           locate_PSB(destp, sdp->rsvp_stempl, -1, NULL) :
		(IN_MULTICAST(ntoh32(destp->d_addr.s_addr)))?
	           locate_PSB(destp, sdp->rsvp_stempl, in_vif, NULL) :
		   locate_PSB(destp, sdp->rsvp_stempl, -1, 
						pkt->pkt_map->rsvp_hop);
	if (psbp == NULL) {
		/*
		 *	If there is no matching PSB, create a new PSB.
		 */
		psbp = make_PSB(destp, pkt->pkt_map->rsvp_hop, 
							sdp->rsvp_stempl);
		if (!psbp)
			return(-1);
#ifdef _MIB
		new_sender = 1;
#endif
		/*
		 *	Copy contents of the SENDER_TEMPLATE, SENDER_TSPEC,
		 *	and PHOP (IP address and LIH) objects into
		 *	the PSB.
		 */
		psbp->ps_templ = copy_filter(sdp->rsvp_stempl);
		psbp->ps_tspec = copy_tspec(sdp->rsvp_stspec);
		Move_Object(pkt->pkt_map->rsvp_hop, &psbp->ps_rsvp_phop);
		psbp->ps_Frpolicy = pkt->pkt_map->rsvp_dpolicy;
		pkt->pkt_map->rsvp_dpolicy = NULL;

		/*	If the sender is from the local API, set
		 *	OutInterface_List to the single interface whose
		 *	address matches the sender address, and make
		 *	IncInterface undefined.  Otherwise, record iface
		 *	on which it arrived, and for multicast, turn on
		 *	Local_Only flag.
		 */
		if (pkt->rsvp_phop.s_addr == INADDR_ANY) {
			psbp->ps_in_if = vif_num;
			psbp->ps_originvif = (u_char) pkt->rsvp_LIH;
		} else {
			psbp->ps_in_if = in_vif;
			if (IN_MULTICAST(ntoh32(destp->d_addr.s_addr))) {
				psbp->ps_flags |= PSBF_LocalOnly;
			}
		}
		/*	If this is the first PSB for the session, start a
		 *	refresh timer.
		 */
		if (Compute_R(&destp->d_timevalp) == 0)
			add_to_timer((char *) destp, TIMEV_PATH, destp->d_Rtimop);

		/*	Since new sender is added, must recompute scope union.
		 */
		clear_scope_union(destp);

		/*	Turn on the Path_Refresh_Needed flag.
		 */
		psbp->ps_flags |= PSBF_Prefr_need;
	}
	else {
		/*	Otherwise (there is a matching PSB):
		 *
		 *	If a PSB is found with a matching sender host but the
     		 *	SrcPorts differ and one of the SrcPorts is zero, then
     		 *	build and send an "Conflicting Sender Port" PathErr 
		 *	message, drop the Path message, and return.
		 */
		filtp = STempl_of(sdp);
		if (!match_filter((FILTER_SPEC *)filtp, psbp->ps_templ)){
			rsvp_path_err(in_vif, RSVP_Err_BAD_SNDPORT, 0, pkt);
			return(-1);
		}
		/*	If the PHOP IP address or the LIH differs between the 
		 *	message and the PSB, copy the new value into the PSB
		 *	abd turn on the Resv_Refresh_Needed flag.
                 *	If the sender Tspec differs, copy the new value into
		 *	the PSB and turn on the Path_Refresh_Needed flag.
		 */
		if (!IsAPI(psbp) && (
			     pkt->rsvp_phop.s_addr != psbp->ps_phop.s_addr ||
		   	     pkt->rsvp_LIH != psbp->ps_LIH) ) {
			Move_Object(pkt->pkt_map->rsvp_hop, &psbp->ps_rsvp_phop);
			psbp->ps_flags |= PSBF_Rrefr_need;
#ifdef _MIB
			if (mib_enabled)
			       rsvpd_update_rsvpSenderPhop(psbp->mib_senderhandle,
							   &(psbp->ps_rsvp_phop));
#endif
		}					
		if (Compare_Tspecs(sdp->rsvp_stspec, psbp->ps_tspec) 
								!= SPECS_EQL) {
			free((char *) psbp->ps_tspec);
			psbp->ps_tspec = copy_tspec(sdp->rsvp_stspec);
			psbp->ps_flags |= PSBF_Prefr_need;
#ifdef _MIB
			if (mib_enabled)
			       rsvpd_update_rsvpSenderTspec(psbp->mib_senderhandle,
							    psbp->ps_tspec);
#endif
		}
	}

	/*	Update the current PSB, as follows.
	 *
	 *	-  Start or restart the cleanup timer for the PSB.
	 *
	 *	-  Copy current Adspec into the PSB.
	 *
	 *	-  Copy E_Police flag from SESSION object into PSB.
	 *
	 *	-  Store the received TTL into the PSB.
	 *	   If the received TTL differs from Send_TTL in the RSVP
	 *	   common header, set the Non_RSVP flag on in the PSB.
	 *
	 *	-  Free any old unknown objects and save any new ones.
	 *
	 */
	ttd = Compute_TTD(pkt->pkt_map->rsvp_timev);
	if (LT(psbp->ps_ttd, ttd))  /* Update sender time-to-die */
			psbp->ps_ttd = ttd;

        if ((sdp->rsvp_adspec)) {
                if (psbp->ps_adspec)
			free((char *) psbp->ps_adspec);
                psbp->ps_adspec = copy_adspec(sdp->rsvp_adspec);
        }
	if (pkt->rsvp_sflags & SESSFLG_E_Police)
		psbp->ps_flags |= PSBF_E_Police;

	psbp->ps_ip_ttl = pkt->pkt_ttl; /* Set/update IP TTL */
	if (pkt->pkt_ttl != pkt->pkt_data->rsvp_snd_TTL)
		psbp->ps_flags |= PSBF_NonRSVP;

	FQkill(&psbp->ps_UnkObjList);
	psbp->ps_UnkObjList = pkt->pkt_map->rsvp_UnkObjList;
	pkt->pkt_map->rsvp_UnkObjList = NULL;

	/*	If policy objects have changed (or order has chnaged),
	 *	free the old ones, save the new ones, and pass them
	 *	to policy control.
	 */
	for (fop= pkt->pkt_map->rsvp_dpolicy; fop; fop = fop->Fobj_next){
		if (!match_policy((POLICY_DATA *) &fop->Fobj_objhdr,
				 (POLICY_DATA *) 
					&psbp->ps_Frpolicy->Fobj_objhdr)){
			FQkill(&psbp->ps_Frpolicy);
			psbp->ps_Frpolicy = pkt->pkt_map->rsvp_dpolicy;
			pkt->pkt_map->rsvp_dpolicy = NULL;
			/* Call Policy Control */
			break;
		}
	}

	/*
	 *	If packet arrived with UDP encapsulation, set flag bit.
	 */
	if (pkt->pkt_flags&PKTFLG_USE_UDP)
		psbp->ps_flags |= PSBF_UDP;

#ifdef _MIB
	if (mib_enabled && new_sender) {
		psbp->mib_senderhandle = mglue_new_sender(destp, psbp, pkt);
		rsvpd_mod_rsvpSessionSenders(destp->mib_sessionhandle, 1);
	}
#endif

	/*	Perform/Initiate route computation.  Maybe it is
	 *	asynchronous...  If not, update state now, using
	 *	incoming interface and outgoing link info, then
	 *	finish Path processing by doing any needed refreshes.
	 */
	rr_vif = in_vif;
	if (!Route_Query(destp, psbp, &rr_vif, &new_routes)) {
		Route_Update(destp, psbp, rr_vif, new_routes);
		finish_path(destp, psbp);
	}

	return(0);
}


/*	Finish processing Path message after completion of perhaps
 *	asynchronous route query.
 */
void
finish_path(Session *destp, PSB *psbp)
	{
	dump_ds(psbp->ps_flags & PSBF_Prefr_need);
	
	/*	If the path state is new or modified and this Path message
	 *	came from a network interface, make a Path Event upcall
	 *	for each local application for this session.
 	 */
	if (psbp->ps_flags&PSBF_Prefr_need) {
 		if (!IsAPI(psbp))
			api_PATH_EVENT_upcall(destp, 0);

	/*	If needed, generate immediate Path refresh for this PSB.
	 *	Then update traffic control to match new path state.
	 */
		psbp->ps_flags &= ~PSBF_Prefr_need;
		path_refresh_PSB(destp, psbp);
		PSB_update_TC(destp, psbp);
	}

	/*	If needed, generate immediate Resv refreshes for this
	 *	PSB.  Send Resv refresh towards all senders using same
	 *	incoming interface (perhaps multiple PHOPs).
	 */
	if (psbp->ps_flags & PSBF_Rrefr_need) {
		psbp->ps_flags &= ~PSBF_Rrefr_need;
		BIT_SET(destp->d_r_incifs, psbp->ps_in_if);
		resv_refresh(destp, 0);
	}
}



/*   Route_Query(): Initiate/perform route computation for sender.
 *		Return 1 if asynchronous, else 0.
 *
 *   If destination is multicast and mrouted is running (and raw I/O?? XXX)
 *   then send RSRR message to mrouted requesting route for (S,G) and
 *   return 1.  For unicast routing, do synchronous lookup, set output
 *   bitmap *bm, and return 0.  For the API in a (possibly multihomed)
 *   host, match S against our interfaces to generate *bm, and return 0.
 */
int
Route_Query(Session *destp, PSB *psbp, int *in_vifp, bitmap *bmp)
	{
	int i;

	*bmp = 0;

	if (!IN_MULTICAST(ntoh32(destp->d_addr.s_addr))) {
		/*
		 *	Unicast case.
		 */
		if (map_if(&destp->d_addr) >= 0) {
			/* Unicast Path message destined to one of my
			 *	interfaces.	That's it...
			 */
			return(0);
		}
		if (!NoUnicast) {
			i = unicast_route(destp->d_addr.s_addr);
			if (i == -1)  /* (In case asynchronous) */
				return(1);
			if (if_vec[i].if_orig.s_addr == destp->d_addr.s_addr) {
				/* This interface IS the destination! */
				return(0);
			}
			BIT_SET(*bmp, i);
			return(0);
		}
	}
	else  if (!NoMroute) {
		/*
		 *	Multicast case: If there is an mrouted, send RSRR
		 *	route query and return 1 (asynch) unless route 
		 *	notification is enabled for this PSB and PSB state
		 *	has not changed.
		 */
#ifdef HOST_ONLY
		assert(0);
#else
		if ((!BIT_TST(psbp->ps_rsrr_flags, PSBF_RSRR_NOTIFY) ||
			psbp->ps_flags & PSBF_Prefr_need)) {

			rsrr_send_rq(destp, psbp);
			return(1);	/* Is asynchronous */
		}
		else {
			*bmp = psbp->ps_outif_list;
			*in_vifp = psbp->ps_in_if;
			return(0);
		}
#endif /* HOST_ONLY */
	}
	else if (IsAPI(psbp)) {
		/* If sender is local API, s_originvif is outgoing interface;
		 * set this as the only route.
		 */
		BIT_SET(*bmp, psbp->ps_originvif);
		*in_vifp = psbp->ps_in_if;
		return(0);
	}
    	/*   Else... assume we are a host with one interface.
	 */
	/* Peserve the actual interface if we know it; else assume interface 0.
	 */
	if (*in_vifp < 0)
		*in_vifp = 0;
	BIT_ZERO(*bmp);
	return(0);
}


/*	Route_Update(): Update state after route computation.
 *		May be called synchronously (called from
 *		path_refresh_common()) or asyncronously (called
 *		from rsrr_update()).
 */
void
Route_Update(Session *destp, PSB *psbp, int in_vif, bitmap new_routes)
	{
	extern char s[];
	PSB	*fPSB = Find_fPSB(destp, psbp->ps_templ);

	if (!IsAPI(psbp))
		BIT_SET(new_routes, vif_num);

	/*  Count Path messages.  Note: Delayed until here
	 *  because may not have known incoming interface until
	 *  asynchronous return from RSRR told us.
	 */
	if (in_vif >= 0)
		Incr_ifstats(in_vif, rsvpstat_msgs_in[RSVP_PATH]);

	if (IsAPI(psbp)) {
		/* If inc iface was unknown, assume routing result
		 */
		if (psbp->ps_in_if < 0)
			psbp->ps_in_if = in_vif;
		fPSB = psbp;
	}
	else if (in_vif >= 0) {  /* Multicast routing */

		/* If inc iface was unknown, assume routing result
		 */
		if (psbp->ps_in_if < 0)
			psbp->ps_in_if = in_vif;

		/* If this Path msg arrived on an interface different from
		 * what routing says but *psbp was the forwarding PSB, then
		 * route has changed; make *psbp local-only.
 		 */
		if (psbp->ps_in_if != in_vif && fPSB == psbp) {
			psbp->ps_flags |= PSBF_LocalOnly;
			psbp->ps_outif_list = 0;
			fPSB = locate_PSB(destp, psbp->ps_templ, in_vif, NULL);
		}
		/* Else if Path msg arrived on correct routing interface but
		 * this PSB is not forwarding PSB (it was local-only), swap
		 * local-only designation with forwarding PSB.
		 */
		else if (psbp->ps_in_if == in_vif && fPSB != psbp) {
			if (fPSB) {
				psbp->ps_outif_list = fPSB->ps_outif_list;
				fPSB->ps_outif_list = 0;
				fPSB->ps_flags |= PSBF_LocalOnly;
			}
			fPSB = psbp;
			psbp->ps_flags &= ~PSBF_LocalOnly; 
		}

		/* We know incoming interface and sender is not from API; if
		 * pkt arrived with UDP encaps, mark incoming interface UDP.
		 */
		if (psbp->ps_flags & PSBF_UDP) {
			start_UDP_encap(in_vif);
			if_vec[vif_toif[in_vif]].if_flags |= IF_FLAG_UseUDP;
		}
	}
	else {
		/*	Unicast routing or no routing (end system)
		 */
		fPSB = psbp;
	}

	/*	If outgoing links have changed for existing PSB, set timer
	 *	to initiate local repair (=> path refresh).
	 */
	if (fPSB && fPSB->ps_outif_list!= new_routes) {
		if (fPSB->ps_outif_list){
			if (IsDebug(DEBUG_RSRR)) {
	    			log(LOG_DEBUG, 0,
					"Outgoing vifs changed from %s\n",
					bm_expand(fPSB->ps_outif_list, s));
			}
			add_to_timer((char *) destp,
					TIMEV_LocalRepair, LOCAL_REPAIR_W);
		}
		fPSB->ps_outif_list = new_routes;
#ifdef _MIB
		if (mib_enabled)
		       rsvpd_update_rsvpSenderOutInterfaceEntry(fPSB->mib_senderhandle,
								new_routes);
#endif
	}
}



/*
 *  accept_path_tear(): Processes an incoming PathTear message.
 */
int
accept_path_tear(int in_vif, struct packet *pkt)
	{
	Session		*destp;
	PSB		*psbp;
	packet_area	data;		/* Packet area */
	struct packet	*new_pkt;
	SenderDesc	*sdp = SenderDesc_of(pkt);

	destp = locate_session(pkt->rsvp_sess);
	if (destp == NULL) {
		log_event(LOGEV_ignore, "PATH-TEAR",
			pkt->pkt_map->rsvp_session, "// No dest\n");
		return(0);
	}
	/*	Locate PSB for same:
	 *	  [Multicast dest:] (session, sender, PHOP, in_if)
	 *	  [Unicast dest:]   (session, sender, PHOP)
	 */
	psbp = (IN_MULTICAST(ntoh32(destp->d_addr.s_addr)))?
	      locate_PSB(destp, sdp->rsvp_stempl, in_vif,pkt->pkt_map->rsvp_hop):
	      locate_PSB(destp, sdp->rsvp_stempl, -1, pkt->pkt_map->rsvp_hop);
	if (!psbp)
		return(0);
	if (pkt->pkt_ttl == 0) {
		Incr_ifstats(psbp->ps_in_if, rsvpstat_path_ttl0_in);
		return(0);
	}
	Incr_ifstats(psbp->ps_in_if, rsvpstat_msgs_in[RSVP_PATH_TEAR]);

	new_pkt = new_packet_area(&data);
	tear_PSB(destp, psbp, new_pkt, pkt->pkt_map->rsvp_UnkObjList);
	return 0;
}


/*
 * path_refresh(): This routine is called upon refresh timeout, to
 *	send Path refresh for all PSBs.
 */
int
path_refresh(Session *destp)
	{
	packet_area	data;		/* Packet area */
	struct packet	*pkt;
	PSB		*psbp;

	/* 	Time out expired path state.  If it's all gone, return -1.
	 */
	if (cleanup_path_state(destp) < 0)
		return(-1);

#ifdef SECURITY
	/*	Increment sequence numbers for all INTEGRITY
	 *	associations.
	 */
	incr_key_assoc_seqnos();
#endif
	
	/*	Build packet struct, map vector, and packet buffer, and
	 *	set up common part of Path msg in map.
	 */
	pkt = new_packet_area(&data);
	pkt->pkt_map->rsvp_msgtype = RSVP_PATH;
	common_path_header(destp, pkt);

	/*
	 * For PSB, send refresh.
	 */
	for (psbp= destp->d_PSB_list; psbp; psbp = psbp->ps_next) {
		path_refresh_common(destp, psbp, pkt);
	}

	dump_ds(0);	/* record state but not too often... */
	return (0);
}

/*
 * path_refresh_PSB: This routine is called when path state is created
 *	or modified, to do an immediate refresh for specified PSB.
 */
int
path_refresh_PSB(Session *destp, PSB *psbp)
	{
	packet_area	data;		/* Packet area */
	struct packet	*pkt;

	/*	Build packet struct, map vector, and packet buffer, and
	 *	set up common part of Path msg in map.
	 */
	pkt = new_packet_area(&data);
	pkt->pkt_map->rsvp_msgtype = RSVP_PATH;
	common_path_header(destp, pkt);

	path_refresh_common(destp, psbp, pkt);
	dump_ds(0);	/* record state but not too often... */
	return (0);
}


/*
 *	This routine sends a path refresh for a particular sender,
 *	i.e., a PSB.  This routine may be entered by either the
 *	expiration of the path refresh timer or directly as the result
 *	of the (Path_)Refresh_Needed flag being turned on during the
 *	processing of a received Path message.
 */
int
path_refresh_common(Session *destp, PSB *psbp, struct packet *pkt)
	{
	SenderDesc	*sdscp = SenderDesc_of(pkt);
	int		vif;

	/*	Compute the IP TTL for the Path message as one less than
	 *	the TTL value from the sender.  However, if the result is
	 *	zero, return without sending the Path message.
	 */
	if ((pkt->pkt_ttl = psbp->ps_ip_ttl - 1) <= 0) {
		Incr_ifstats(psbp->ps_in_if, rsvpstat_path_ttl0_out);
		return(0);
	}	

	 /*	Create a sender descriptor containing the SENDER_TEMPLATE,
	  *	SENDER_TSPEC, and POLICY_DATA objects, if present in the
	  *	PSB, and pack it into the Path message being built.
	  */
	sdscp->rsvp_stempl = psbp->ps_templ;
	sdscp->rsvp_stspec = psbp->ps_tspec;
	/*
	 *	Copy into Path msg map a pointer to list of unknown objects
	 *	to be forwarded.
	 */
	pkt->pkt_map->rsvp_UnkObjList = psbp->ps_UnkObjList;
	
	/*	Send a copy of the Path message to each interface OI in
	 *	OutInterface_list.  Before sending each copy:
	 *	1. Pass any ADSPEC and SENDER_TSPEC objects present in the PSB
	 *	   to the traffic control call TC_Advertise.  Insert the
	 *	   modified ADSPEC object that is returned into the Path
	 *	   message.
	 *	2. If the PSB has the E_Police flag on but interface OI is
	 *	   incapable of policing, turn on the E_Police flag in the
	 *	   message.
	 *	3. Insert into the PHOP object the interface address and
	 *	   the LIH for the interface.
	 */
	sdscp->rsvp_adspec = NULL;
	for (vif= 0; vif < vif_num; vif++) {
		if (!BIT_TST(psbp->ps_outif_list, vif))
			continue;
#ifdef SCHEDULE
		if (psbp->ps_adspec)
			sdscp->rsvp_adspec =
				TC_Advertise(vif, psbp->ps_adspec,
					(int) psbp->ps_flags&PSBF_NonRSVP);
#endif /* SCHEDULE */

		if ((psbp->ps_flags&PSBF_E_Police)&&
						if_vec[vif_toif[vif]].if_up==0)
			pkt->rsvp_sflags |= SESSFLG_E_Police;

#ifdef __sgi
		if ((if_vec[vif].if_flags & IF_FLAG_Disable) == 0) {
		     send_path_out_vif(vif, destp, STempl_of(sdscp), pkt);
		}
#endif /* __sgi */

		if (sdscp->rsvp_adspec) {
			free(sdscp->rsvp_adspec);
			sdscp->rsvp_adspec = NULL;
		}
	}
	return(0);
}


/*
 * locate_PSB() returns the PSB matching a (SESSION, SENDER_TEMPLATE) and
 *		possibly in_vif and/or PHOP address.  In_vif < 0 and NULL
 *		PHOP address are wildcards.
 *
 */
PSB         *
locate_PSB(Session *destp, SENDER_TEMPLATE *filtp, int in_vif, RSVP_HOP *phop)
	{
	PSB         *sp;

	for (sp = destp->d_PSB_list; sp != NULL; sp = sp->ps_next) {
		if (match_filter(filtp, sp->ps_templ)) {
			if ( (in_vif < 0 || in_vif == sp->ps_in_if)
			  && (phop == NULL ||
				 phop->hop4_addr.s_addr == sp->ps_phop.s_addr))
					return sp;
		}
	}
	return (NULL);
}

/*
 * make_PSB(): Add a PSB data structure to a session
 */
PSB *
make_PSB(Session *dst, RSVP_HOP *phop, SENDER_TEMPLATE *stemp)
	{
	PSB	*psbp, **spp, *tsp;

	psbp = (PSB *) calloc(1, sizeof(PSB));
	if (!psbp) {
		Log_Mem_Full("New sender");
		return(NULL);
	}

	psbp->ps_ttd = time_now;
	psbp->ps_next = NULL;
	BIT_ZERO(psbp->ps_outif_list);
	psbp->ps_rsrr_flags = 0;

	/*
	 *  Insert PSB in list in ascending order by sender IP address
	 *	within phop.
	 */
	for (spp = &dst->d_PSB_list; ; spp = &((*spp)->ps_next)){
		if (!(*spp)
		      || phop->hop4_addr.s_addr < (*spp)->ps_phop.s_addr) {
			/* First sender for this PHOP: insert it here and
			 * set 1stphop field to point to itself.
			 */
			psbp->ps_next = *spp;
			*spp = psbp;
			psbp->ps_1stphop = psbp;
			return(psbp);
		}
		if (phop->hop4_addr.s_addr == (*spp)->ps_phop.s_addr)
			break;
	}
	/*
	 *	PHOP address already in list.
	 */
	if (stemp->filt_srcaddr.s_addr < (*spp)->ps_templ->filt_srcaddr.s_addr){
		/*
		 *  Special case: new sender is first (smallest) address
		 */
		psbp->ps_1stphop = psbp;	/* pt to itself */
		for (tsp = *spp; tsp ; tsp = tsp->ps_next) {
			if (phop->hop4_addr.s_addr != tsp->ps_phop.s_addr)
				break;
			tsp->ps_1stphop = psbp;
		}
	} else {
		psbp->ps_1stphop = *spp;
		assert((*spp)->ps_1stphop == *spp);
		for (spp = &((*spp)->ps_next); *spp ; spp = &((*spp)->ps_next)){
			if (phop->hop4_addr.s_addr < (*spp)->ps_phop.s_addr
			 || stemp->filt_srcaddr.s_addr < 
					(*spp)->ps_templ->filt_srcaddr.s_addr)
				break;
		}
	}
	psbp->ps_next = *spp;
	*spp = psbp;
	return (psbp);
}


/*
 * kill_PSB(): This routine frees a PSB data structure.
 *	Called from cleanup_path_state (when sender times out) or
 *	from accept_path_tear.
 *	It deletes any dependent reservation state, and if all path and
 *	reservation state is gone, it deletes the session and returns -1.
 */
int
kill_PSB(Session *destp, PSB *psbp) {
	PSB	**spp, *sp, *fsp;
	int	from_net;

	assert(destp);
	if (!psbp)
		return(0);
#ifdef _MIB
	if (mib_enabled) {
	        rsvpd_mod_rsvpSessionSenders(destp->mib_sessionhandle, -1);
		rsvpd_deactivate_rsvpSenderEntry(psbp->mib_senderhandle);
        }
#endif

	/*
	 * remove PSB from linked list
	 */
	for (spp = &destp->d_PSB_list; (*spp) != NULL;
				 		spp = &((*spp)->ps_next)) {
		if ((*spp)->ps_1stphop == (*spp))
			fsp = *spp;
		if ((*spp) == psbp)
			break;
	}
	if (*spp != NULL)
		*spp = psbp->ps_next;

	/*	If this was first PSB per phop, recompute 1stphop pointers.
	 */
	if (fsp == psbp) {
		for (sp = psbp->ps_next; sp; sp = sp->ps_next) {
			if (sp->ps_1stphop != psbp)
				break;
			sp->ps_1stphop = psbp->ps_next;
		}
	}
	delete_resv4PSB(destp, psbp);	/* Delete any dangling reservations */
	clear_scope_union(destp);	/* Set to recompute scope union */
	from_net = !IsAPI(psbp);

	free((char *) psbp->ps_templ);
	if (psbp->ps_tspec)
		free((char *) psbp->ps_tspec);
	if (psbp->ps_adspec)
		free((char *) psbp->ps_adspec);
	if (psbp->ps_BSB_Qb)
		free((char *) psbp->ps_BSB_Qb);
	if (psbp->ps_resv_spec)
		free(psbp->ps_resv_spec);
	FQkill(&psbp->ps_UnkObjList);
	free((char *) psbp);	/* delete the PSB block itself */

	/* Send Path Event upcall to all matching apps, except local state*/
	if (from_net)
		api_PATH_EVENT_upcall(destp, 0);

	if (!destp->d_PSB_list && !destp->d_RSB_list) {
		kill_session(destp);
		return(-1);
	}
	return (0);
}


/*	Locate "forwarding" PSB for specified session, sender.  Forwarding
 *	PSB is the PSB that is *not* marked LocalyOnly.  Return addr of PSB
 *	or NULL if there is none.
 */
PSB *
Find_fPSB(Session *destp, SENDER_TEMPLATE *filtp)
	{
	PSB		*sp;

	for (sp = destp->d_PSB_list; sp != NULL; sp = sp->ps_next){
		if (match_filter(filtp, sp->ps_templ)) {
			if (!(sp->ps_flags&PSBF_LocalOnly))
				return(sp);
		}
	}
	return(NULL);
}


/*
 * cleanup_path_state(): For given session, kill all expired path state.
 *		Return 1 if something was killed, -1 if entire session
 *		was killed, else 0.
 */
int
cleanup_path_state(Session *destp)
	{
	PSB		*psbp, *psbpx;
	packet_area	data;		/* Packet area */
	struct packet	*pkt = NULL;
	int		rc, killed = 0;

	/*
	 *	For each expired PSB, forward PTear message to all
	 *	outgoing vifs and then kill PSB.
	 */
	for (psbp = destp->d_PSB_list; psbp; psbp = psbpx) {
		psbpx = psbp->ps_next;
		if (!LT(psbp->ps_ttd, time_now))
			continue;
		killed = 1;
		Incr_ifstats(psbp->ps_in_if, rsvpstat_path_timeout);
		if (!pkt)
			pkt = new_packet_area(&data);
		rc = tear_PSB(destp, psbp, pkt, NULL);
		if (rc < 0)
			return -1;
	}
	return(killed);
}

/*
 *	Delete a PSB: forward PTear message to all outgoing vifs and then kill 
 *	PSB.  Common code for accept_path_tear() and cleanup_path_state().
 *	In the first case, specify list of unknown objects to be forwarded.
 */
int
tear_PSB(Session *destp, PSB *psbp, struct packet *pkt, Fobject *fobjp)
	{
	SenderDesc 	*newsdp;
	int		vif;

	/*
	 *	Generate PathTear message
	 */
	pkt->pkt_map->rsvp_msgtype = RSVP_PATH_TEAR;
	common_path_header(destp, pkt);
	newsdp = SenderDesc_of(pkt);
	newsdp->rsvp_stempl = psbp->ps_templ;
	/* Omit Sender_Tspec and Adspec. */
	pkt->pkt_map->rsvp_UnkObjList = fobjp;

	/*
	 *	Send PathTear message to each outgoing vif.
	 */
	if (psbp->ps_ip_ttl > 0) {
		pkt->pkt_ttl = psbp->ps_ip_ttl - 1;
		for (vif = 0; vif < vif_num; vif++) {
			if (BIT_TST(psbp->ps_outif_list, vif))
#ifdef __sgi
			        if ((if_vec[vif].if_flags & IF_FLAG_Disable) == 0)
#endif
				send_path_out_vif(vif, destp,
					 STempl_of(newsdp), pkt);
		}
	}
	return (kill_PSB(destp, psbp));
}



void
common_path_header(Session *destp, struct packet *pkt)
	{
	packet_map *mapp = pkt->pkt_map;

	mapp->rsvp_session = destp->d_session;
	if(pkt->pkt_map->rsvp_msgtype != RSVP_PATH_TEAR) {
		mapp->rsvp_timev = &destp->d_timevalp;
	}
	mapp->rsvp_hop = &Prev_Hop_Obj;
	pkt->rsvp_nflwd = 1;	/* Exactly one sender */
}

void
send_path_out_vif(
	int		vif,
	Session		*destp,
	SENDER_TEMPLATE *sdscp,
	struct packet	*pkt)
	{
	assert(pkt->pkt_order == BO_HOST);

	/*
	 * Previous-Hop address = addr of output interface.
	 */
	pkt->rsvp_phop.s_addr = VIF_toip(vif);
	pkt->rsvp_LIH = vif;	/* Logical Interface Handle */
	send_pkt_out_vif(vif, destp, sdscp, pkt);
}

