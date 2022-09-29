
/*
 * @(#) $Id: rsvp_err.c,v 1.7 1998/11/25 08:43:14 eddiem Exp $
 */

/************************ rsvp_err.c  ********************************
 *                                                                   *
 *     Routines to receive, process, and send PathErr, ResvErr, and  *
 *		Resv_Confirm messages.  			     *
 *                                                                   *
 *********************************************************************/
/****************************************************************************

            RSVPD -- ReSerVation Protocol Daemon

                USC Information Sciences Institute
                Marina del Rey, California

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

extern int      errno;
static ERROR_SPEC Error_Spec;

/*	External dcl's
 */
Session		*locate_session(SESSION *);
int		resv_refresh(Session *, int);
void		api_PATH_ERR_upcall(struct packet *);
void		api_RESV_ERR_upcall(struct packet *);
int		demerge_flowspecs(FLOWSPEC *, FLOWSPEC *);
SCOPE		*new_scope_obj(int);
int		search_scope(SCOPE *, struct in_addr *);
int		Compare_Flowspecs(FLOWSPEC *, FLOWSPEC *);
int		map_if(struct in_addr *);
struct		packet *new_packet_area(packet_area *);
Object_header *	copy_object(Object_header *);
int		send_pkt_out_if(int, RSVP_HOP *, struct packet *);
int		match_filt2star(FILTER_SPEC *, FiltSpecStar *);
FiltSpecStar *	Get_FiltSpecStar(int);
extern RSVP_HOP	Next_Hop_Obj;

/* Forward dcl's
 */
int		accept_path_err(int, struct packet *);
int		accept_resv_err(int, struct packet *);
int		accept_resv_conf(int, struct packet *);
void		send_err_out(RSVP_HOP *, int, struct packet *);
void		rsvp_path_err(int in_if, int, int, struct packet *);
void		rsvp_resv_err(int, int, int, FiltSpecStar *, struct packet *);
void		rsvp_resv_err1(int, int, int, FiltSpecStar *, struct packet *);
void		rsvp_RSB_err(Session *, RSB *, int, int);
void		Filtstar2map(FiltSpecStar *, struct packet *);
PSB	*	resv_err_blockade(Session *destp, struct packet *);
bitmap	*	resv_err_scope(Session *, SCOPE *);
void		send_confirm(Session *, RSB *);
void		update_BSB(Session *, PSB *, FLOWSPEC *);



/*
 *   accept_path_err(): receives an RSVP PathErr msg and handles it.
 *		Can only come from network.
 */
int
accept_path_err(
	int in_if,
	struct packet *pkt)
	{
	Session		*destp;
	PSB		*sp;
	SenderDesc	*sdscp = SenderDesc_of(pkt);
	packet_map	*mapp = pkt->pkt_map;

	FORCE_HOST_ORDER(pkt);

	/*
	 * If there is no session state, just ignore PathErr message.
	 */
	destp = locate_session(pkt->rsvp_sess);
	if (!destp) {
		log_event(LOGEV_ignore, "PATH-ERR", mapp->rsvp_session,
							"No session");
		return(0);
	}
             
	/*
	 *   For sender template in error, look up path state.  If none,
	 *   ignore PathErr msg.
	 */
	sp = locate_PSB(destp, sdscp->rsvp_stempl, -1, NULL ); /* XXX ?? */
	if (!sp) {
		log_event(LOGEV_ignore, "PATH-ERR", mapp->rsvp_session,
							"No sender");
		return(0);
	}
	Incr_ifstats(sp->ps_in_if, rsvpstat_msgs_in[RSVP_PATH_ERR]);

	/*      Check that INTEGRITY was included if it is required.
	*/
	if ((if_vec[sp->ps_in_if].if_flags&IF_FLAG_Intgrty) && 
			pkt->pkt_map->rsvp_integrity == NULL)
		return PKT_ERR_INTEGRITY;

	/*   If phop address is zero (API), deliver path error to API,
	 *   otherwise forward PathErr message.
	 */
	if (sp->ps_phop.s_addr == INADDR_ANY) {
		api_PATH_ERR_upcall(pkt);
	} else {
		/* Ignore any NHOP object and remove any UDP encapsulation;
		 * otherwise, forward the packet without change.
		 */
		pkt->pkt_map->rsvp_hop = NULL;
		pkt->pkt_flags &= ~PKTFLG_USE_UDP;
		send_err_out(&sp->ps_rsvp_phop, sp->ps_in_if, pkt);
	}
	return(0);
}


/*
 *  accept_resv_err(): Receives an RSVP ResvErr and handles it.
 *
 *		Can only come from network.
 */
int
accept_resv_err(
	int in_if,		/* (real) incoming interface */
	struct packet *pkt)
	{
	packet_map	*mapp = pkt->pkt_map;
	packet_area	 data;		/* To build output packet */
	struct packet	*outpkt;
	Session		*destp;
	bitmap		*bitm_vecp = NULL; /* Ptr to vector of routes */
	RSB		*rp;
	PSB		*BSBp;
	int		 N, rc;
	style_t		 style = Style(pkt);

	FORCE_HOST_ORDER(pkt);

	/*      Check that INTEGRITY was included if it is required.
	*/
	if (in_if >= 0 && (if_vec[in_if].if_flags&IF_FLAG_Intgrty) && 
			pkt->pkt_map->rsvp_integrity == NULL)
		return PKT_ERR_INTEGRITY;

	/* 	If there is no path state for SESSION, drop the ResvErr
	 *	message and return.
	 */
	destp = locate_session(pkt->rsvp_sess);
	if (!destp) {
		log_event(LOGEV_ignore, "RESV-ERR", mapp->rsvp_session,
							"No sess");
		return(0);
	}

	if (!Style_is_Shared(style) && pkt->rsvp_nflwd > 1)
		return PKT_ERR_NUMFLOW;
		/* XXX Could force nflwd = 1, free others */

	/* XXX Need to increment stats for RESV_ERR... but may not know in_if
	 */
	if (in_if >= 0)
		Incr_ifstats(in_if, rsvpstat_msgs_in[RSVP_RESV_ERR]);

	/*	If the Error Code = 01 (Admission Control failure),
	 *	do special processing of blockade state.
	 */
	if (pkt->rsvp_errcode == RSVP_Err_ADMISSION) {
		BSBp = resv_err_blockade(destp, pkt);
		if (BSBp) {
			if (in_if >= 0)
				BIT_SET(destp->d_r_incifs, in_if);
			else
				destp->d_r_incifs = 0;
			resv_refresh(destp, 1);
			destp->d_r_incifs = 0;
		}
	}

	/*
	 *	If scope is wildcard and there is a SCOPE object;
	 *	compute vector of routing bitmaps parallel to SCOPE.
	 */
	if (Style_is_Wildcard(style) && (pkt->rsvp_scope)) {
		bitm_vecp = resv_err_scope(destp, pkt->rsvp_scope);
		if (bitm_vecp == NULL) {
			Log_Mem_Full("ResvErr");
			return(0);
		}
	}

	/* Build output packet struct and copy appropriate entries from
	 * input map.
	 */
	outpkt = new_packet_area(&data);
	outpkt->pkt_map->rsvp_session = mapp->rsvp_session;
	outpkt->pkt_map->rsvp_hop = &Next_Hop_Obj;
	outpkt->pkt_map->rsvp_style = mapp->rsvp_style;
	outpkt->pkt_map->rsvp_flags = mapp->rsvp_flags;
	outpkt->pkt_map->rsvp_msgtype = RSVP_RESV_ERR;
	outpkt->pkt_map->rsvp_errspec = mapp->rsvp_errspec;
	outpkt->pkt_map->rsvp_UnkObjList = mapp->rsvp_UnkObjList;
	spec_of(rflows(outpkt)) = spec_of(rflows(pkt));

	/*
	 *	Execute the following for each RSB for this session
	 *	whose OI difffers from in_if and whose Filter_spec_list
	 *	has at least one filter spec in common with the
	 *	FILTER_SPEC* in the ResvErr message.
	 *
	 *	1. Copy all matching filter specs into output ResvErr packet.
	 */
	for (rp = destp->d_RSB_list; rp ; rp = rp->rs_next) {

		if (vif_toif[rp->rs_OIf] == in_if)
			continue;
		
		if (Style_is_Wildcard(style))
			outpkt->rsvp_nflwd = 1;
		else {
			int  i, j = 0;

			for (i= 0; i< pkt->rsvp_nflwd; i++) {
			    FILTER_SPEC *filtp = filter_of(FlowDesc_of(pkt, i));

			    if (match_filt2star(filtp, rp->rs_filtstar)) {
				if (j)
					spec_of(FlowDesc_of(outpkt, j)) = NULL;
				filter_of(FlowDesc_of(outpkt, j++)) = filtp;
			    }
			}
			if (j == 0)
				continue;	/* No matches */
			outpkt->rsvp_nflwd = j;
		}

		/*
		 *  2.	If Error Code = 01 and the InPlace flag is 1 and one
		 *	or more of the BSBs found/created above has a Qb that
		 *	is strictly greater than Flowspec in the RSB, then
		 *	continue with the next matching RSB, if any.
		 */
		if (pkt->rsvp_errcode == RSVP_Err_ADMISSION && (BSBp) &&
		   (pkt->rsvp_errflags & ERROR_SPECF_InPlace)) {
			rc = Compare_Flowspecs(BSBp->ps_BSB_Qb, rp->rs_spec);
			if (rc == SPEC1_GTR)
				continue;
		}

		/*
		 *  3.  If NHOP in the RSB is the local API, then:
		 *	-- If the FLOWSPEC in the ResvErr message is
		 *	   strictly greater than the RSB FLOWSPEC, then
		 *	   turn on the NotGuilty flag in the ERROR_SPEC.
		 *	-- Deliver an error upcall to application and
		 *	   continue with the next RSB.
		 */
		if (rp->rs_nhop.s_addr == INADDR_ANY) {
			if (Compare_Flowspecs( spec_of(rflows(pkt)), 
						rp->rs_spec) == SPEC1_GTR)
				outpkt->rsvp_errflags |= ERROR_SPECF_NotGuilty;

			api_RESV_ERR_upcall(outpkt);
			continue;
		}
		if (bitm_vecp) {
			/*
			 *	Forward wildcard-scope ResvErr
			 */
			int i, nscope = Scope_Cnt(pkt->rsvp_scope);
			struct in_addr *adp;
				
			outpkt->rsvp_scope = new_scope_obj(nscope);
			adp = outpkt->rsvp_scope->scope4_addr;
				
			for (i = 0; i < nscope; i++) {
				if (BIT_TST(bitm_vecp[i], rp->rs_OIf))
					*adp++= pkt->rsvp_scope->scope4_addr[i];
			}
			N =  (char *)adp - (char *)outpkt->rsvp_scope;
			/*
			 *	If scope list is empty, continue with the
			 *	next RSB.
			 */
			if (N == 0)
				continue;
			Obj_Length(outpkt->rsvp_scope) = N;
			free(outpkt->rsvp_scope);
		}
		/*	Send to the NHOP address specified by the RSB.
		 *	Set PHOP object for outgoing interface.
		 *	Include scope list if the style is wildcard.
		 */
		outpkt->rsvp_phop.s_addr = IF_toip(vif_toif[rp->rs_OIf]);
		outpkt->rsvp_LIH = rp->rs_OIf;
		send_err_out(&rp->rs_rsvp_nhop, rp->rs_OIf, outpkt);
	}
	if (bitm_vecp)
		free(bitm_vecp);
	return(0);
}


/*
 *	accept_resv_conf():  Received reservation confirmation message.
 *		Forward and/or do confirmation upcall to applications
 */
int
accept_resv_conf(int in_if, struct packet *pkt)
	{
	RSVP_HOP	dummy_Hop_Obj;

	FORCE_HOST_ORDER(pkt);
	
	Init_Object(&dummy_Hop_Obj, RSVP_HOP, ctype_RSVP_HOP_ipv4);
	dummy_Hop_Obj.hop4_addr = pkt->rsvp_confrcvr;

	/*
	 *	If the (unicast) IP address found in the RESV_CONFIRM
	 *	object in the ResvConf message matches an interface of the
	 *	node, a confirmation upcall is made to the matching
	 *	application.
	 */
	if (map_if(&pkt->rsvp_confrcvr) > -1)
		api_RESV_ERR_upcall(pkt);

	/*	Otherwise, forward the ResvConf message to the IP address
	 *	in its RESV_CONFIRM object.
	 */
	else
		send_pkt_out_if(-1, &dummy_Hop_Obj, pkt);
	return(0);
}

void
send_err_out(RSVP_HOP *hopp, int oif, struct packet *pkt)
	{
	send_pkt_out_if(oif, hopp, pkt);
}

/*	Given ResvErr msg containing SCOPE object, use path state to 
 *	built vector of routing bitmaps paralleling SCOPE list.
 */
bitmap *
resv_err_scope(Session *destp, SCOPE *scope_inp)
	{
	PSB		*sp;
	bitmap		*bmp;
	int		 n, i;

	n = Scope_Cnt(scope_inp) * sizeof(bitmap);
	bmp = (bitmap *) malloc(n);
	if (bmp == NULL)
		return(NULL);
	memset((char *) bmp, 0, n); 
	
	for (sp = destp->d_PSB_list; sp != NULL; sp = sp->ps_next) {
		/* There may be more than one sender per sender IP address;
		 *	use only the last.
		 */
		if ((sp->ps_next) &&
				sp->ps_next->ps_templ->filt_srcaddr.s_addr ==
					sp->ps_templ->filt_srcaddr.s_addr)
			continue;
		/*
		 * If path sender matches s[i] in SCOPE list, record bitmap
		 *	in vector[i].
		 */
		i = search_scope(scope_inp, &sp->ps_templ->filt_srcaddr);
		if (i < 0)
			continue;
		bmp[i] = sp->ps_outif_list;
	}
	return(bmp);
}


/*
 *   Initiate a Path_Err message, resulting from packet *pp_in.  Send
 *   it point-to-point to previous hop.
 *
 */

void 
rsvp_path_err(
	int		in_if,
	int		e_code,
	int		e_value,
	struct packet	*pp_in)
	{
	packet_area	data;
	struct packet	*pkt;

	pkt = new_packet_area(&data);

	/* If Path message was UDP-encapsulated, send PathErr with UDP
	 * encapsulation.
	 */
	pkt->pkt_flags = PKTFLG_USE_UDP & pp_in->pkt_flags;

	/* Copy appropriate entries from fixed part of map, plus index-th
	 * sender descriptor. Also construct ERROR_SPEC object in new map.
	 */
	pkt->pkt_map->rsvp_session = pp_in->pkt_map->rsvp_session;
	pkt->pkt_map->rsvp_integrity = pp_in->pkt_map->rsvp_integrity;
	pkt->pkt_map->rsvp_flags = pp_in->pkt_map->rsvp_flags;
	pkt->pkt_map->rsvp_msgtype = RSVP_PATH_ERR;
	memcpy((char *) SenderDesc_of(pkt), 
			(char *) SenderDesc_of(pp_in), 
			sizeof(SenderDesc));
	pkt->rsvp_nflwd = 1;
	Init_Object(&Error_Spec, ERROR_SPEC, ctype_ERROR_SPEC_ipv4);
	pkt->pkt_map->rsvp_errspec = &Error_Spec;
	pkt->rsvp_errcode = e_code;
	pkt->rsvp_errvalue = e_value;
	pkt->rsvp_errnode.s_addr = Get_local_addr;

	log_event(LOGEV_iserr, "PATH", pkt->pkt_map->rsvp_session,
					"!c=%d val=%d\n", e_code, e_value);

	if (pp_in->rsvp_phop.s_addr == INADDR_ANY) {
		/*
		 *  Local reservation; return the error to the API
		 */
		api_PATH_ERR_upcall(pkt);
	} else {
		send_err_out(pp_in->pkt_map->rsvp_hop, in_if, pkt);
	}
}

/*
 *  rsvp_resv_err():
 *	Initiate Resv_Err message for Resv message *err_pkt, filter spec
 *	*filtssp.  Send Resv_Err point-to-point to next hop, or
 *	if local reservation, deliver to API.
 */
void 
rsvp_resv_err(
	int		 e_code,
	int		 e_value,
	int		 e_flags,
	FiltSpecStar	*e_filtssp,
	Resv_pkt	*err_pkt)
	{
	int		 k;
	style_t		 style = Style(err_pkt);
	FiltSpecStar	 filtss, *filtssp;

	if ((int)e_filtssp != -1) {
		rsvp_resv_err1(e_code, e_value, e_flags, e_filtssp, err_pkt);
		return;
	}
	/*
	 *	-1 => For all flow descriptors in packet
	 */
	filtss.fst_count = 1;
	switch (style) {
	    case STYLE_WF:
	    case STYLE_FF:
		/*	Style FF: execute independently for each flow
		 *	descriptor in the message.
		 */
		for (k=0; k < err_pkt->rsvp_nflwd; k++) {
		    filtss.fst_Filtp(0) = filter_of(FlowDesc_of(err_pkt, k));
		    rsvp_resv_err1(e_code, e_value, e_flags, &filtss, err_pkt);
		}
		break;

	    case STYLE_SE:
		/* 	SE => one flow descriptor but multiple filter specs.
		 *	Obtain storage and construct FILTER_SPEC*, i.e., 
		 *	list of FILTER_SPECs, dynamically.
		 */
		filtssp = Get_FiltSpecStar(err_pkt->rsvp_nflwd);
		if (!filtssp)
	 		{
			Log_Mem_Full("ResvErr1");
			return;
		}
		filtssp->fst_count = err_pkt->rsvp_nflwd;
		for (k=0; k < err_pkt->rsvp_nflwd; k++)
			filtssp->fst_Filtp(k) = 
					filter_of(FlowDesc_of(err_pkt, k));
		rsvp_resv_err1(e_code, e_value, e_flags, filtssp, err_pkt);
		free(filtssp);			
		break;

	    default:
		break;
	}
}
	

/*
 *  rsvp_resv_err1():
 *	Inner routine of rsvp_resv_err()
 */
void 
rsvp_resv_err1(
	int		 e_code,
	int		 e_value,
	int		 e_flags,
	FiltSpecStar	*filtssp,
	Resv_pkt	*err_pkt)
	{
	packet_area	 data;		/* Packet buffer, map, etc */
	struct packet 	*pkt;
	packet_map	*mapp;
	int		OIf = err_pkt->rsvp_LIH;

	pkt = new_packet_area(&data);
	mapp = pkt->pkt_map;
	mapp->rsvp_msgtype = RSVP_RESV_ERR;

	mapp->rsvp_session = err_pkt->pkt_map->rsvp_session;
	/*** mapp->rsvp_spolicy = ?? */
	mapp->rsvp_hop = &Next_Hop_Obj;

	mapp->rsvp_style = err_pkt->pkt_map->rsvp_style;
	if (Style_is_Wildcard(Style(err_pkt)))
		pkt->rsvp_scope = err_pkt->rsvp_scope;

	Filtstar2map(filtssp, pkt);
	spec_of(FlowDesc_of(pkt, 0)) = spec_of(FlowDesc_of(err_pkt, 0));

	Init_Object(&Error_Spec, ERROR_SPEC, ctype_ERROR_SPEC_ipv4);
	mapp->rsvp_errspec = &Error_Spec;
	pkt->rsvp_errcode = e_code;
	pkt->rsvp_errvalue = e_value;
	pkt->rsvp_errflags = e_flags;
	pkt->rsvp_errnode.s_addr = Get_local_addr;

	log_event(LOGEV_iserr, "RESV", mapp->rsvp_session,
					"!c=%d val=%d\n", e_code, e_value);

	if (err_pkt->rsvp_nhop.s_addr == INADDR_ANY)
		api_RESV_ERR_upcall(pkt);
	else {
		pkt->rsvp_phop.s_addr = IF_toip(vif_toif[OIf]);
		pkt->rsvp_LIH = OIf;
		send_pkt_out_if(OIf, err_pkt->pkt_map->rsvp_hop, pkt);
	}
}

/*
 *  rsvp_RSB_err():
 *	Initiate Resv_Err message for given RSB.
 */
void 
rsvp_RSB_err(
	Session		*destp,
	RSB		*rp,
	int		 e_code,
	int		 e_value)
	{
	packet_area	 data;		/* Packet buffer, map, etc */
	struct packet 	*pkt;
	packet_map	*mapp;
	extern STYLE	 Style_Obj;

	if (!rp)
		return;
	pkt = new_packet_area(&data);
	mapp = pkt->pkt_map;
	mapp->rsvp_msgtype = RSVP_RESV_ERR;

	mapp->rsvp_session = destp->d_session;
	/*** mapp->rsvp_spolicy = ?? */
	mapp->rsvp_hop = &Next_Hop_Obj;

	mapp->rsvp_style = &Style_Obj;
	Obj_CType(&Style_Obj) = ctype_STYLE;
	Style(pkt) = rp->rs_style;
	pkt->rsvp_scope = rp->rs_scope;

	Filtstar2map(rp->rs_filtstar, pkt);
	spec_of(FlowDesc_of(pkt, 0)) = rp->rs_spec;

	Init_Object(&Error_Spec, ERROR_SPEC, ctype_ERROR_SPEC_ipv4);
	mapp->rsvp_errspec = &Error_Spec;
	pkt->rsvp_errcode = e_code;
	pkt->rsvp_errvalue = e_value;
	pkt->rsvp_errnode.s_addr = Get_local_addr;

	log_event(LOGEV_iserr, "RESV", mapp->rsvp_session,
					"!c=%d val=%d\n", e_code, e_value);

	if (rp->rs_nhop.s_addr == INADDR_ANY)
		api_RESV_ERR_upcall(pkt);
	else {
		pkt->rsvp_phop.s_addr = IF_toip(vif_toif[rp->rs_OIf]);
		pkt->rsvp_LIH = rp->rs_OIf;
		send_pkt_out_if(rp->rs_OIf, &rp->rs_rsvp_nhop, pkt);
	}
}


void
Filtstar2map(FiltSpecStar *filtssp, struct packet *pkt)
	{
	int i;

	pkt->rsvp_nflwd = filtssp->fst_count;
	for (i = 0; i < pkt->rsvp_nflwd; i++) {
		FlowDesc *flwdp = FlowDesc_of(pkt, i);

		flwdp->rsvp_filtp = filtssp->fst_p[i].fst_filtp;
		flwdp->rsvp_specp = NULL;
	}
}


/*	Special blockade processing when receive ResvErr message.
 *	1. Find/Create one (or more?) Blockade State Blocks (BSBs),
 *	   in style-dependent manner.
 *	2. For each BSB, set (or replace) its FLOWSPEC Qb with
 *	   FLOWSPEC from the message, and set/reset its timer Tb.
 */
PSB *
resv_err_blockade(Session *destp, struct packet *pkt)
	{
	PSB		*sp, *BSBp = NULL;
	FILTER_SPEC	*filtp;
	int		i;

	switch (Style(pkt)) {

	    case STYLE_WF:
		/*	For WF (wildcard) style, there will be one BSB
		 *	per (session, PHOP) pair.
		 */
		for (sp = destp->d_PSB_list; sp != NULL; sp = sp->ps_next) {
			if (sp->ps_phop.s_addr == pkt->rsvp_phop.s_addr) {
				break;
			}
		}
		if (sp)
			update_BSB(destp, BSBp = sp, 
						spec_of(FlowDesc_of(pkt,0)));
		break;

	    case STYLE_FF:
	    case STYLE_SE:
		/*	For FF style, there will be one BSB per (session,
		 *	filter_spec) pair.  Note that an FF style ResvErr
		 *	message carries only one flow descriptor.
		 *
		 *	For SE style, there will be one BSB per (session,
		 *	filter_spec), for each filter_spec in the flow
		 *	descriptor.
		 */
		for (i = 0; i < pkt->rsvp_nflwd; i++) {
			filtp = filter_of(FlowDesc_of(pkt,0));
			for (sp = destp->d_PSB_list; sp; sp = sp->ps_next) {
				if (match_filter(filtp, sp->ps_templ)) {
					break;
				}
			}
			if (sp) {
			    BSBp = sp;
			    update_BSB(destp, BSBp, spec_of(FlowDesc_of(pkt,0)));
			}
		}
		break;

	}
	return (BSBp);
}

void
update_BSB(Session *destp, PSB *BSBp, FLOWSPEC *specp)
	{
	if (BSBp) {
		BSBp->ps_BSB_Qb = copy_spec(specp);
		BSBp->ps_BSB_Tb = time_now + Kb_FACTOR * destp->d_Rtimor; 
	}
}

/*
 *	Send an RSVP ResvConf message, containing specified CONFIRM
 *	object (which is freed).
 */
void
send_confirm(
	Session 	*destp,
	RSB		*rp)
	{
	packet_area	 data;		/* Packet buffer, map, etc */
	struct packet 	*pkt;
	packet_map	*mapp;
	extern STYLE	 Style_Obj;

	if (!rp)
		return;
	pkt = new_packet_area(&data);
	mapp = pkt->pkt_map;
	mapp->rsvp_msgtype = RSVP_CONFIRM;

	mapp->rsvp_session = destp->d_session;
	mapp->rsvp_style = &Style_Obj;
	Obj_CType(&Style_Obj) = ctype_STYLE;
	Style(pkt) = rp->rs_style;

	mapp->rsvp_confirm = rp->rs_confirm;
	rp->rs_confirm = NULL;

	Filtstar2map(rp->rs_filtstar, pkt);
	spec_of(FlowDesc_of(pkt, 0)) = rp->rs_spec;

	Init_Object(&Error_Spec, ERROR_SPEC, ctype_ERROR_SPEC_ipv4);
	mapp->rsvp_errspec = &Error_Spec;
	pkt->rsvp_errcode = RSVP_Err_NONE;
	pkt->rsvp_errvalue = RSVP_Erv_Nonev;
	pkt->rsvp_errnode.s_addr = Get_local_addr;

	log_event(LOGEV_iserr, "CONF", mapp->rsvp_session,
					"!c=%d val=%d\n", 0, 0);

	if (rp->rs_nhop.s_addr == INADDR_ANY)
		api_RESV_ERR_upcall(pkt);
	else
		send_pkt_out_if(rp->rs_OIf, &rp->rs_rsvp_nhop, pkt);
	free(mapp->rsvp_confirm);
}
