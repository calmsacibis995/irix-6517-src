
/*
 * @(#) $Id: rsvp_api.c,v 1.12 1998/11/25 08:43:14 eddiem Exp $
 */

/************************ rsvp_api.c  ********************************
 *                                                                   *
 *  Routines to handle application program interface of rsvpd        *
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
#include "rsvp_api.h"
#include "rapi_lib.h"
#include <sys/uio.h> 

/* external declarations */
int		refresh_api();
int		rsvp_pkt_process(struct packet *, struct sockaddr_in *, int);
void		rsvp_resv_err(int, int, int, FiltSpecStar *, struct packet *);
void		rsvp_path_err(int, int, int, struct packet *);
u_long		Styleid_to_Optvec[];
int		writev();
extern char	*style_names[];
void		rapi_fmt_flowspec(rapi_flowspec_t *, char *, int);
void		rapi_fmt_tspec(rapi_tspec_t *, char *, int);
void		rapi_fmt_adspec(rapi_adspec_t *, char *, int);
void		rapi_fmt_filtspec(rapi_filter_t *, char *, int);
char *		iptoname(struct in_addr *);
int		map_if(struct in_addr *);
int		New_Adspec(ADSPEC *);
Session *	locate_session(SESSION *);
char	*	cnv_flags(char *, u_char);

/* forward declarations */
static void	send_to_api(int, rsvp_resp *, int);
static void	print_api_request(rsvp_req *, int, int);
void		api_PATH_EVENT_upcall(Session *, int);
void		api_RESV_EVENT_upcall(Session *, RSB *, TCSB*, int);
void		api_PATH_ERR_upcall(struct packet *);
void		api_RESV_ERR_upcall(struct packet *);
static		void upcall_to_appl(rsvp_resp *, int, FILTER_SPEC *);
int		api_prepare_path(rsvp_req *, int, struct packet *);
int		api_prepare_resv(rsvp_req *, struct packet *);
int		api_new_packet(struct packet *, int, int);
int		api_tear_oldresv(struct packet *, struct packet *);
int		api_status(rsvp_req *);
int		api_dest_status(Session *, rsvp_req *);
int		api_get_flows(Object_header *, rsvp_req *, struct packet *);
int		Move_spec_api2d(rapi_flowspec_t *, FLOWSPEC *);
char *		Move_spec_d2api(FLOWSPEC *, rapi_flowspec_t *);
int		Move_tspec_api2d(rapi_tspec_t *, SENDER_TSPEC *);
char *		Move_tspec_d2api(SENDER_TSPEC *, rapi_tspec_t *);
int		Move_adspec_api2d(rapi_adspec_t *, ADSPEC *);
char *		Move_adspec_d2api(ADSPEC *, rapi_adspec_t *);
int		Move_filter_api2d(rapi_filter_t *, FILTER_SPEC *);
char *		Move_filter_d2api(FILTER_SPEC *, rapi_filter_t *);
Session	*	locate_api_session(rsvp_req *);
void		log_api_event(int, char *, struct sockaddr_in *, int,
				const char *, int, int);
void		api_tear_path(api_rec *);
void		api_tear_resv(api_rec *);
void		api_free_packet(struct packet *);
void		api_ERR_upcall_common(rsvp_resp *, struct packet *);
int		path_set_laddr(rapi_filter_t *);
 

/*	Macro used in api_prepare_xxxx to add an object to API packet and
 *	to the map of the packet.  The map address is 'var'; the object
 *	has class (typedef) 'cls' and ctype (value) 'ctype'.
 */
#define New_Object(var, cls, ctype)  \
	Init_Object(objp, cls, ctype); \
	var = (cls *) objp; \
	objp = Next_Object(objp);

#define New_VObject(var, cls, ctype, len)  \
	Init_Var_Obj(objp, cls, ctype,len); \
	var = (cls *) objp; \
	objp = Next_Object(objp);

/*
 *	Array to translate style ID into option vector
 */
u_long	Styleid_to_Optvec [] =
	{/* 0 unused */ 0,
	 /* 1 WF */     STYLE_WF,
	 /* 2 FF */	STYLE_FF,
	 /* 3 SE */	STYLE_SE
	};

/*
 * process_api_req(): Process request from API client, received over Unix
 *	socket.  Return -1 if API session should be closed.
 *	sid = my local handle.
 */
int
process_api_req(int sid, rsvp_req *req, int len)
	{
	api_rec		*recp = &api_table[sid];
	common_header	*hdrp;
	struct packet	new_pkt;
	int		rc;
	Session		*destp;
	
	if (IsDebug(DEBUG_ALL)) {
		print_api_request(req, len, sid);
		if (l_debug >= LOG_HEXD && len > 0)
			hexf(stderr, (char *) req, len);
	}

	if (len != 0 && req->rq_version != VERSION_API) {
		log(LOG_ERR, 0, "API: bad version %d\n", req->rq_version);
		return (-1);  /* Close it */
	}

	if (len == 0 || req->rq_type == API_CLOSE) {
		/*
		 * Close: Session close requested, or client terminated.
		 * If there is stored Path (Resv) message, turn it into a
		 * PathTear (ResvTear) message and procsss it as input.
		 */
		api_tear_path(recp);
		api_tear_resv(recp);
		return(-1);	/* Caller will delete session & timer */
	}

	switch (req->rq_type) {

	case API2_REGISTER:
		/*	REGISTER command handles 3 different functions:
		 *	(1) Session registration (nflwd=0 & no Path state)
		 *	(2) New Sender defn (nflwd=1)
		 *	(3) Teardown sender (nflwd=0, path state exists)
		 */
		recp->api_dest = req->rq_dest;
		recp->api_protid =
			(req->rq_protid)?req->rq_protid: RSVP_DFLT_PROTID;
		/*
		 *	Save flags, but remove obsolete direction flags.
		 *	Generate direction information locally.
		 */
		recp->api_flags = req->rq_flags & ~(API_DIR_SEND|API_DIR_RECV);

		if (req->rq_nflwd == 0) {
			recp->api_flags &= ~API_DIR_SEND;
			/*
			 *	No sender descriptor => New registration
			 *				or teardown request
			 */
			hdrp = recp->api_p_packet.pkt_data;
			if (hdrp == NULL||hdrp->rsvp_type != RSVP_PATH) {
				/*
				 *  New session registration.  If there
				 *  is already path state, trigger path
				 *  events.
				 */
				destp = locate_api_session(req);
				if (destp)
					api_PATH_EVENT_upcall(destp, 0);
				
				return(0);  /* None in place, just ignore */
			}
			
			/*
			 * Turn path state into PathTear message, send it,
			 * and then free API packet buffer & map.  If there
			 * is no resv state either, stop refresh timer.
			 */
			api_tear_path(recp);
			if (recp->api_r_packet.pkt_data == NULL)
				del_from_timer((char *)
					(unsigned long) sid, TIMEV_API);
			return(0);
		}
		/* else, we are registering a sender.  Set up virtual Path
		 * message.
		 */
		recp->api_flags |= API_DIR_SEND;
		hdrp = recp->api_p_packet.pkt_data;
		if (hdrp == NULL) {
			/* 	Allocate space for a packet buffer.  (Use max
			 *	message size; could compute actual size)  XXX
			 */
			if (api_new_packet(&recp->api_p_packet, 1, MAX_PKT) < 0)
				return(-1);
		}
		rc = api_prepare_path(req, len, &recp->api_p_packet);
		if (rc == 0) {
			/* There was a client error.  Delete packet.
			 */
			api_free_packet(&recp->api_p_packet);	
			return(0);
		} else if (rc < 0) {
			/* There was internal error.  Return -1 => caller
			 *	will close the API socket.
			 */
			return(-1);
		}
		recp->api_p_packet.pkt_ttl =
				recp->api_p_packet.pkt_data->rsvp_snd_TTL;
		/*
		 * We finished updating API state, now handle the request
		 * itself
		 */
		refresh_api(sid);
		break;

	case API2_RESV:
		/*	Reservation request.
		 */
		hdrp = recp->api_r_packet.pkt_data;
		if (req->rq_nflwd == 0) {
			recp->api_flags &= ~API_DIR_RECV;
			/*
			 *	No flow descriptors => teardown request
			 */
			if (hdrp == NULL||hdrp->rsvp_type != RSVP_RESV)
				return(0);  /* None in place, just ignore */
			
			/*	Send ResvTear and free Resv pkt buff and
			 *	map.  If there is no path state either, stop
			 *	refresh timer.
			 */
			api_tear_resv(recp);
			if (recp->api_p_packet.pkt_data == NULL)
				del_from_timer((char *)
					(unsigned long) sid, TIMEV_API);
			return(0);
		}
		/*	Initialize local packet struct with new data
		 *	buffer and map, then call api_prepare_resv() to
		 *	move request data into it.
		 */
		recp->api_flags |= API_DIR_RECV;
		if (api_new_packet(&new_pkt, req->rq_nflwd, Max_rsvp_msg) < 0)
			return(-1);
		rc = api_prepare_resv(req, &new_pkt);
		if (rc == 0) {
			/* There was a client error.  Delete packet.
			 */
			api_free_packet(&recp->api_r_packet);
			return(0);
		} else if (rc < 0) {
			/* There was internal error.  Return -1 => caller
			 *	will close the API socket.
			 */
			return(-1);
		}

		/*	If already have Resv, this is a modification;
		 *	tear down state that is being removed, and then
		 *	free old data buffer and map.
		 */
		if (hdrp) {
			if (api_tear_oldresv(&recp->api_r_packet, &new_pkt)<0)
				return(-1);
		}
		/*	Move pointers to data and map into API record
		 */
		recp->api_r_packet.pkt_data = new_pkt.pkt_data;	
		recp->api_r_packet.pkt_map = new_pkt.pkt_map;
		recp->api_r_packet.pkt_len = new_pkt.pkt_len;
		recp->api_r_packet.pkt_order = BO_HOST;
		recp->api_r_packet.pkt_flags = 0;
		recp->api_r_packet.pkt_ttl = 0;
		/*
		 * We finished updating API state; now handle the request
		 * itself
		 */
		refresh_api(sid);
		/*
		 *	Delete any CONFIRM object, which is single-shot.
		 */
		recp->api_r_packet.pkt_map->rsvp_confirm = NULL;		
		break;

	default:
		log(LOG_ERR, 0, "API: Bad rq_type=%d\n", req->rq_type);
		return (-1);
	}

	add_to_timer((char *) sid, TIMEV_API, API_REFRESH_T);
	
	dump_ds(0);
	return (0);
}


/* 	Allocate new packet buffer and new map for API.
 *	(Use max message size; could compute actual size)  XXX
 */
int
api_new_packet(struct packet *pkt, int nflwd, int data_len)
	{
	common_header	*hdrp;
	packet_map	*mapp;
	int		 map_len = Map_Length(nflwd);

	if ((hdrp = (common_header *) malloc(data_len))==NULL ||
	    (mapp = (packet_map *) malloc(map_len))==NULL) {
		Log_Mem_Full("API send/resv req");
		return (-1);
	}
	pkt->pkt_offset = 0;
	pkt->pkt_map = mapp;
	pkt->pkt_data = hdrp;
	pkt->pkt_len = 0;
	pkt->pkt_flags = pkt->pkt_ttl = 0;
	pkt->pkt_order = BO_HOST;
	memset((char *)mapp, 0, sizeof(packet_map));
	return(0);
}

void
api_tear_path(api_rec *recp)
	{
	common_header	*hdrp = recp->api_p_packet.pkt_data;

	if (!hdrp)
		return;
	hdrp->rsvp_type = RSVP_PATH_TEAR;
	recp->api_p_packet.pkt_map->rsvp_msgtype = RSVP_PATH_TEAR;
	rsvp_pkt_process(&recp->api_p_packet, NULL, vif_num);
	api_free_packet(&recp->api_p_packet);
}

void
api_tear_resv(api_rec *recp)
	{
	common_header	*hdrp = recp->api_r_packet.pkt_data;
		
	if (!hdrp)
		return;
	hdrp->rsvp_type = RSVP_RESV_TEAR;
	recp->api_r_packet.pkt_map->rsvp_msgtype = RSVP_RESV_TEAR;
	/*
	 * Note that the RSVP spec is designed to allow us to make a valid
	 * ResvTear msg from a Resv msg, by just changing the message type.
	 * We could explicitly delete the flowspec from the ResvTear, but
	 * it's better to exercise the logic that ignores the flowspec.
	 */
	rsvp_pkt_process(&recp->api_r_packet, NULL, vif_num);
	api_free_packet(&recp->api_r_packet);
}

void
api_free_packet(struct packet *pkt)
	{
	if (pkt->pkt_data)
		free(pkt->pkt_data);
	if (pkt->pkt_map)
		free(pkt->pkt_map);
	pkt->pkt_data = NULL;
	pkt->pkt_map = NULL;
	pkt->pkt_len = 0;
}


/*	Tear down API state in o_pkt that does not appear in n_pkt.
 */
int
api_tear_oldresv(struct packet *o_pkt, struct packet *n_pkt)
	{
	int		oi, oj, nk;
	style_t		style = Style(o_pkt);

	o_pkt->pkt_map->rsvp_msgtype = RSVP_RESV_TEAR;
	if (style != Style(n_pkt)) {
		/*	Styles differ.  Just tear down old Resv.
		 */
		rsvp_pkt_process(o_pkt, NULL, vif_num);
		api_free_packet(o_pkt);
		return(0);
	}

	/*	Rebuild old packet map to include only those filter
	 *	specs that don't match new packet map.  If any filter
	 *	specs remain, send old packet as ResvTear.
	 */
	oj = 0;
	for (oi = 0; oi < o_pkt->rsvp_nflwd; oi++) {
		FILTER_SPEC *o_filtp = filter_of(FlowDesc_of(o_pkt, oi));

		for (nk = 0; nk < n_pkt->rsvp_nflwd; nk++) {
			if (match_filter(o_filtp,
					filter_of(FlowDesc_of(n_pkt, nk))))
				break;
		}
		if (nk == n_pkt->rsvp_nflwd) {
			/* No match, filter should be torn down.
			 */
			if (!Style_is_Shared(style))
				spec_of(FlowDesc_of(o_pkt, oj)) =
					spec_of(FlowDesc_of(o_pkt, oi));
			filter_of(FlowDesc_of(o_pkt, oj++)) =
					filter_of(FlowDesc_of(o_pkt, oi));
		}
	}
	if (oj) {
		o_pkt->rsvp_nflwd = oj;
		rsvp_pkt_process(o_pkt, NULL, vif_num);
	}
	api_free_packet(o_pkt);
	return(0);
}


/*
 * api_prepare_resv(): Accepts an API request for a resv. and turns it
 *		into a standard RSVP Resv packet... as if it arrived
 *		from another router (except in host byte order).
 *		Returns -1 if error, else 0.
 */
int
api_prepare_resv(rsvp_req *req, struct packet *pkt)
	{
	Object_header	*objp;
	packet_map	*mapp = pkt->pkt_map;
	int		 styleID;
	
	memset((char *)pkt->pkt_data, 0, sizeof(common_header));
	mapp->rsvp_msgtype = pkt->pkt_data->rsvp_type = RSVP_RESV;

	objp = (Object_header *)(pkt->pkt_data + 1);
	New_Object(mapp->rsvp_session, SESSION, ctype_SESSION_ipv4);
	pkt->rsvp_destaddr = req->rq_dest.sin_addr;
	pkt->rsvp_dstport = req->rq_dest.sin_port;
	pkt->rsvp_protid = (req->rq_protid)?req->rq_protid: RSVP_DFLT_PROTID;

	/*	XXX Should check dstport != 0 if protid = 6 or 17.
	 */
	
	New_Object(mapp->rsvp_hop, RSVP_HOP, ctype_RSVP_HOP_ipv4);
	pkt->rsvp_nhop.s_addr = INADDR_ANY;	/* (signals: from API) */

	New_Object(mapp->rsvp_timev, TIME_VALUES, ctype_TIME_VALUES);
	pkt->rsvp_R = API_REFRESH_T;

	/*	If requested by application, send CONFIRM object.
	 */
	if (req->rq_flags & RAPI_REQ_CONFIRM) {
		New_Object(mapp->rsvp_confirm, CONFIRM, ctype_CONFIRM_ipv4);
		pkt->rsvp_confrcvr.s_addr = Get_local_addr;
	}

	/*	Style:  Map RAPI style into option vector
	 */
	styleID = req->rq_style;
	New_Object(mapp->rsvp_style, STYLE, ctype_STYLE);
	if (styleID > RAPI_RSTYLE_MAX) {
		log(LOG_ERR, 0, "API: bad styleid\n");
		return(-1);
	}
        mapp->rsvp_style->style_word = 	Styleid_to_Optvec[styleID];

	/*	Reservation local address: apply default and then
	 *	map into interface number, which is passed in LIH.
	 */
	if (req->rq_host.sin_addr.s_addr == INADDR_ANY)
		req->rq_host.sin_addr.s_addr = Get_local_addr;
	/* This assumes if's are subset of vif's XXX */
	pkt->rsvp_LIH = map_if(&req->rq_host.sin_addr);
	
	pkt->rsvp_nflwd = req->rq_nflwd;
	api_get_flows(objp, req, pkt);	/* XXX test for error */

	/*	Finish up common header.
	 *	Note: no checksum for API packet (per byte-order field)
	 */
	pkt->pkt_data->rsvp_cksum = 0;
	pkt->pkt_data->rsvp_verflags = RSVP_MAKE_VERFLAGS(RSVP_VERSION, 0);
	pkt->pkt_data->rsvp_length = pkt->pkt_len = 
				(char *)objp - (char *) pkt->pkt_data;
	/*
	 *	If client sent bad local addr, send error upcall and return 0.
	 */
	if ((int)pkt->rsvp_LIH < 0) {
		rsvp_resv_err(RSVP_Err_API_ERROR, RAPI_ERR_BADSEND, 0, 
				(FiltSpecStar *) -1, pkt);
		return(0);
	}
	/*
	 *	If client sent DstPort in the SESSION object of zero
	 *	but SrcPort in a FILTER_SPEC is non-zero, send 
	 *	"Conflicting Src Port" error upcall and return 0.
	 */
	if (pkt->rsvp_dstport == 0 && filter_of(FlowDesc_of(pkt,0))) {
	    int i;
	    for (i =0; i < pkt->rsvp_nflwd ; i++)
		if (filter_of(FlowDesc_of(pkt,i))->filt_srcport != 0) {
			rsvp_resv_err(RSVP_Err_API_ERROR, RAPI_ERR_BADSPORT, 0, 
				(FiltSpecStar *) -1, pkt);
			return(0);
		}
	}
	return(1);   /* OK */
}

/*	Parse flow descriptor list of reservation request from API, and
 *	build map entries.
 */
int
api_get_flows(Object_header *objp, rsvp_req *req, struct packet *pkt) {
	int	i;
	API_Flowspec	*api_specp;
	API_FilterSpec	*api_filtp;
	FlowDesc	*flwdp;

	api_filtp = (API_FilterSpec *) After_APIObj(req->rq_policy);

	for (i = 0; i < pkt->rsvp_nflwd; i++) {
		flwdp = FlowDesc_of(pkt, i);

		api_specp = (API_Flowspec *) After_APIObj(api_filtp);
		switch (api_specp->form) {

		    case RAPI_FLOWSTYPE_Intserv:
			/* Check Intserv version */
			if ((api_specp->specbody_IS.spec_mh.ismh_version&
				INTSERV_VERS_MASK) != INTSERV_VERSION0)
				return -1; 			/* XXX */
			New_VObject(flwdp->rsvp_specp, FLOWSPEC,
				ctype_FLOWSPEC_Intserv0,
				size_api2d(api_specp->len));
			Move_spec_api2d(api_specp, flwdp->rsvp_specp);
			break;

		    case RAPI_EMPTY_OTYPE:
			flwdp->rsvp_specp = NULL;
			break;

		    default:
			return -1;	/* XXX Reason code */
		}

		if (api_filtp->form != RAPI_EMPTY_OTYPE) {
			New_Object(flwdp->rsvp_filtp, FILTER_SPEC,
						ctype_FILTER_SPEC_ipv4);
			Move_filter_api2d(api_filtp, flwdp->rsvp_filtp);
		}
		else
			flwdp->rsvp_filtp = NULL;
		api_filtp = (API_FilterSpec *) After_APIObj(api_specp);
	}
	return(0);
}

/*
 * api_prepare_path(): Accepts an API request for a path and turns
 *	it into a standard RSVP Path packet... as if it arrived
 * 	from another router (except in host byte order).
 */
int
api_prepare_path(
	rsvp_req	*req,
	int		 len,
	struct packet	*pkt)
	{
	SenderDesc	*sdscp;
	Object_header	*objp;
	packet_map	*mapp = pkt->pkt_map;
	API_FilterSpec	*api_filtp;
	API_TSpec 	*api_tspecp;
	API_Adspec	*api_adspecp;

	memset((char *)pkt->pkt_data, 0, sizeof(common_header));
	mapp->rsvp_msgtype = pkt->pkt_data->rsvp_type = RSVP_PATH;

	objp = (Object_header *)(pkt->pkt_data + 1);
	New_Object(mapp->rsvp_session, SESSION,
		(req->rq_flags & RAPI_GPI_SESSION)? ctype_SESSION_ipv4GPI:
						   ctype_SESSION_ipv4);
	pkt->rsvp_destaddr = req->rq_dest.sin_addr;
	pkt->rsvp_dstport = req->rq_dest.sin_port;
	pkt->rsvp_protid = (req->rq_protid)?req->rq_protid: RSVP_DFLT_PROTID;
	pkt->rsvp_sflags = SESSFLG_E_Police;

	New_Object(mapp->rsvp_hop, RSVP_HOP, ctype_RSVP_HOP_ipv4);
	pkt->rsvp_phop.s_addr = INADDR_ANY;	/* => from API */
	/* LIH set below */

	New_Object(mapp->rsvp_timev, TIME_VALUES, ctype_TIME_VALUES);
	pkt->rsvp_R = API_REFRESH_T;

	/*
	 * Push flags from map into packet itself, and set version
	 */
	pkt->pkt_data->rsvp_verflags = RSVP_MAKE_VERFLAGS(RSVP_VERSION,
						mapp->rsvp_flags);
	/*
	 * Only one flowspec (Tspec) per request.
	 *	(Should be enforced by the client library routine).
	 */
	if (req->rq_nflwd != 1) {
		log(LOG_ERR, 0, "API: sender nflwd > 1\n");
		return(-1);
	}

	pkt->rsvp_nflwd = 1;
	sdscp = SenderDesc_of(pkt);
	api_filtp = (API_FilterSpec *) After_APIObj(req->rq_policy);
	api_tspecp = (API_TSpec *) After_APIObj(api_filtp);
	api_adspecp = (API_Adspec *)  After_APIObj(api_tspecp);

	if ((char *)api_adspecp == (char *)req + len) {
		/* No Adspec included in request
		 */
		api_adspecp = NULL;
	}
	else if (After_APIObj(api_adspecp) != (char *)req + len) {
		log(LOG_ERR, 0, "API: Req len err\n");
		return(-1);
	}

	/*	If local address is INADDR_ANY, set default interface.
	 *	Map sender local address into interface number and pass
	 *	it in LIH.  But if *not* our interface, set LIH = -1
	 */
	pkt->rsvp_LIH = path_set_laddr(api_filtp);

	/*
	 *	If client sent bad local addr, send error upcall and return 0.
	 */
	if ((int)pkt->rsvp_LIH < 0) {
		rsvp_path_err(-1, RSVP_Err_API_ERROR, RAPI_ERR_BADSEND, pkt);
		return(0);
	}
	New_Object(sdscp->rsvp_stempl, SENDER_TEMPLATE, 0);
	Move_filter_api2d(api_filtp, sdscp->rsvp_stempl);
	New_VObject(sdscp->rsvp_stspec, SENDER_TSPEC, ctype_SENDER_TSPEC,
					size_api2d(api_tspecp->len));
	Move_tspec_api2d(api_tspecp, sdscp->rsvp_stspec);

	/*	If sender gave us initial adspec, put it into Path message.
	 *	Otherwise, call Traffic Control interface to create minimal
	 *	adspec, and put that into message.
	 */
	if (api_adspecp) {
		New_VObject(sdscp->rsvp_adspec, ADSPEC, ctype_ADSPEC_INTSERV,
					size_api2d(api_adspecp->len));
		Move_adspec_api2d(api_adspecp, sdscp->rsvp_adspec);
	}
	else {
		New_VObject(sdscp->rsvp_adspec, ADSPEC, ctype_ADSPEC_INTSERV,
                                       		(DFLT_ADSPEC_LEN));
		New_Adspec(sdscp->rsvp_adspec);
	}						

	/*	Finish up common header.
	 *	Note: no checksum for API packet (per byte order field)
	 */
	pkt->pkt_data->rsvp_cksum = 0;
	pkt->pkt_data->rsvp_length = pkt->pkt_len = 
				(char *)objp - (char *) pkt->pkt_data;
	pkt->pkt_data->rsvp_snd_TTL = (req->rq_ttl)? req->rq_ttl+1:
							RSVP_TTL_MAX;

	/* If local address was bad, generate an error
	 *	upcall now and free the packet.
	 */
	if ((int)pkt->rsvp_LIH < 0) {
		rsvp_path_err(-1, RSVP_Err_API_ERROR, RAPI_ERR_BADRECV, pkt);
		return(0);
	}
	/*
	 *	If the DstPort in the SESSION object was zero
	 *	but SrcPort in SENDER_TEMPLATE was non-zero, 
	 *	upcall with "Conflicting Src Port" error and free packet.
	 */
	if (pkt->rsvp_dstport == 0 &&
	    STempl_of(SenderDesc_of(pkt))->filt_srcport != 0) {
		rsvp_path_err(-1, RSVP_Err_API_ERROR, RAPI_ERR_BADSPORT, pkt);
		return(0);
	}
	return(1);
}

/*	Process rapi_status request.
 */
int
api_status(rsvp_req *req)
	{
	Session *destp;
	SESSION  Session_obj;
	api_rec	*apip;
	int	 i;

        if (req->rq_dest.sin_addr.s_addr != INADDR_ANY) {
		/* Specific destination  --
         	 * Find the dest (session) record
		 */
		destp = locate_api_session(req);
        	if (destp == NULL)
			return(-1);
		api_dest_status(destp, req);
		return(0);
	}
	/*	Else status of all destinations for this user process
	 */
	for (i=0, apip= api_table; i < API_TABLE_SIZE; i++, apip++) {
		if (apip->api_fd == 0)
			break;
		if (apip->api_pid != req->rq_pid)
			break;
		
		Session_obj.sess4_addr.s_addr = apip->api_dest.sin_addr.s_addr;
		Session_obj.sess4_port = apip->api_dest.sin_port;
		Session_obj.sess4_prot = req->rq_protid;
		destp = locate_api_session(req);
        	if (destp)
			api_dest_status(destp, req);
	}
	return(0);
}

int
api_dest_status(Session *dest, rsvp_req *req)
	{
#ifdef ISI_TEST
	if (req->rq_flags & RAPI_STAT_PATH) {
		api_PATH_EVENT_upcall(dest, 1);
	}
	if (req->rq_flags & RAPI_STAT_RESV) {
	/*** Need to call api_RESV_EVENT_upcall for all RSBs & TCSBs
		api_RESV_EVENT_upcall(dest, 1);
	 ***/
	}
#endif /* ISI_TEST */
	return(0);
}


/*
 *  api_PATH_EVENT_upcall():  Notify local API clients of Path info.
 *	Also, if there is an active reservation request from API,
 *	refresh it immediately.  If is_stat is non-zero, use STAT
 *	type in response msg.
 */
void
api_PATH_EVENT_upcall(Session *destp, int is_stat)
	{
	char		*resp_buf;
	rsvp_resp	*resp;
	API_TSpec	*tspecp;
	API_FilterSpec	*filtp;
	API_Adspec	*adsp;
	PSB		*sp;
	int		 sid, resp_len;

	/*	Make first pass over PSBs to compute length of required
	 *	response message, and then malloc a buffer of that length.
	 */
	resp_len = sizeof(rsvp_resp);
	for (sp = destp->d_PSB_list; sp != NULL; sp = sp->ps_next) {
		if (!is_stat && sp->ps_phop.s_addr == INADDR_ANY)
			continue;
		resp_len += sizeof(rapi_filter_t) + 
		      	 		size_d2api(Object_Size(sp->ps_tspec));
		if (sp->ps_adspec)
			resp_len += size_d2api(Object_Size(sp->ps_adspec));
	}
	if (!(resp_buf = malloc(resp_len))) {
		Log_Mem_Full("API Resp");
		return;
	}
	memset(resp = (rsvp_resp *)resp_buf, 0, resp_len);
	resp->resp_type = (is_stat)? RAPI_PATH_STATUS: RAPI_PATH_EVENT;
	Set_Sockaddr_in(&resp->resp_dest, destp->d_addr.s_addr, destp->d_port);
	resp->resp_protid = destp->d_protid;

	/*
	 *	Second pass over PSBs to insert sender tspecs and templates
	 */
	filtp = (API_FilterSpec *) resp->resp_flows;
	for (sp = destp->d_PSB_list; sp != NULL; sp = sp->ps_next) {
		if (!is_stat && sp->ps_phop.s_addr == INADDR_ANY)
			continue;
                tspecp = (API_TSpec *) Move_filter_d2api(sp->ps_templ, filtp);
                		/* Move TSpec or empty object */
		filtp = (API_FilterSpec *)
				       Move_tspec_d2api(sp->ps_tspec, tspecp);
		resp->resp_nflwd++;
	}
	/*
	 *	Third pass over PSBs to insert adspec list
	 */
	adsp = (API_Adspec *) filtp;
	for (sp = destp->d_PSB_list; sp != NULL; sp = sp->ps_next) {
		if (!is_stat && sp->ps_phop.s_addr == INADDR_ANY)
			continue;
		adsp = (API_Adspec *) Move_adspec_d2api(sp->ps_adspec, adsp);
	}

	upcall_to_appl(resp, (char *) adsp - resp_buf, NULL);

	/*
	 *	If notifying of PATH state and there was a
          *	pending Resv from API, retry Resv immediately.
	 */
        for(sid = 0; sid < API_TABLE_SIZE; sid++) {
                api_rec *recp = &api_table[sid];

		if (resp->resp_nflwd && recp->api_r_packet.pkt_data)
			rsvp_pkt_process(&recp->api_r_packet, NULL, vif_num);
	}
	free(resp_buf);
}


/*
 * api_PATH_ERR_upcall: Pass Path Error message across API to all
 *			application(s) that match it.
 */
void
api_PATH_ERR_upcall(struct packet *pkt)
	{
	char		resp_buf[MAX_PKT];
	rsvp_resp	*resp = (rsvp_resp *) resp_buf;
	API_FilterSpec	*api_filtp = (API_FilterSpec *) &resp->resp_flows;
	API_TSpec	*api_tspecp;
	SenderDesc	*sdscp = SenderDesc_of(pkt);

	FORCE_HOST_ORDER(pkt);
	memset(resp, 0, MAX_PKT);	/* Excessive... */
	resp->resp_type = RAPI_PATH_ERROR;
	api_ERR_upcall_common(resp, pkt);

	assert(pkt->rsvp_nflwd <= 1);
	if (pkt->rsvp_nflwd < 1)
		return;
	resp->resp_nflwd = 1;
 
	api_tspecp = (API_TSpec *)
			Move_filter_d2api(sdscp->rsvp_stempl, api_filtp);
	api_filtp = (API_FilterSpec *)
			Move_tspec_d2api(sdscp->rsvp_stspec, api_tspecp);
	
	upcall_to_appl(resp, (char *)api_filtp - resp_buf, sdscp->rsvp_stempl);
}

/*
 *  api_RESV_EVENT_upcall()
 *
 *	Make RESV_EVENT upcall to application for specific RSB and TCSB.
 *	If reservation has been torn down, *TCSB will be NULL.
 *	If is_stat is non-zero, use STATUS type in response msg.
 */
void
api_RESV_EVENT_upcall(Session *destp, RSB *rp, TCSB *kp, int is_stat)
	{
	char		 resp_buf[MAX_PKT];
	rsvp_resp	*resp = (rsvp_resp *) resp_buf;
	API_Flowspec	*api_specp;
	API_FilterSpec	*api_filtp;
	FILTER_SPEC	*filtp;
	int		 i;

	memset(resp, 0, MAX_PKT);
	resp->resp_type = (is_stat)? RAPI_RESV_STATUS: RAPI_RESV_EVENT;

	Set_Sockaddr_in(&resp->resp_dest, destp->d_addr.s_addr, destp->d_port);
	resp->resp_protid = destp->d_protid;

	/*	Map style into external style id
	 */
	switch (rp->rs_style) {
		case STYLE_WF:
			resp->resp_style = RAPI_RSTYLE_WILDCARD;
			break;
		case STYLE_FF:
			resp->resp_style = RAPI_RSTYLE_FIXED;	
			break;
		case STYLE_SE:
			resp->resp_style = RAPI_RSTYLE_SE;
			break;
		default:
			assert(0);
	}

	api_filtp = (API_FilterSpec *) resp->resp_flows;
	if (!kp) {
		resp->resp_nflwd = 0;
		upcall_to_appl(resp, (char *) api_filtp - resp_buf, NULL);
		return;
	}
        /* 	After rq_policy in API response comes pair:
         *              Style=WF: <NUL,Q>
         *              Style=FF: <F,Q>
         *              Style=SE: <F,Q>
	 *	Note that only one flowspec is passed per upcall; one FF
	 *	style Resv may produce multiple upcalls.  Furthermore, only
	 *	a matching filter spec is passed for SE.
         */
	for (i = 0; i < kp->tcs_filtstar->fst_count; i++) {
		if (resp->resp_style == RAPI_RSTYLE_WILDCARD)
			filtp = NULL;
		else
			filtp = kp->tcs_filtstar->fst_Filtp(i);

        	api_specp = (API_Flowspec *) Move_filter_d2api(filtp, api_filtp);
		api_filtp = (API_FilterSpec *) Move_spec_d2api(
							kp->tcs_spec, api_specp);
		resp->resp_nflwd = 1;
		upcall_to_appl(resp, (char *) api_filtp - resp_buf, filtp);
	}
}


/*	api_RESV_ERR_upcall:  Send reservation error or confirmation to
 *		all applications matching session.
 */
void
api_RESV_ERR_upcall(struct packet *pkt)
	{
	char            resp_buf[MAX_PKT];
	rsvp_resp      *resp = (rsvp_resp *) resp_buf;
	int             i;
	API_FilterSpec	*api_filtp = (API_FilterSpec *) &resp->resp_flows;
	API_Flowspec 	*api_specp;

	FORCE_HOST_ORDER(pkt);
	memset(resp, 0, MAX_PKT);	/* Excessive... */
	resp->resp_type = (pkt->rsvp_errcode == RSVP_Err_NONE)?
					RAPI_RESV_CONFIRM:
					RAPI_RESV_ERROR;

	api_ERR_upcall_common(resp, pkt);

	/*	Map style into external style id
	 */
	switch (Style(pkt)) {
		case STYLE_WF:
			resp->resp_style = RAPI_RSTYLE_WILDCARD;
			break;
		case STYLE_FF:
			resp->resp_style = RAPI_RSTYLE_FIXED;	
			break;
		case STYLE_SE:
			resp->resp_style = RAPI_RSTYLE_SE;
			break;
		default:
			assert(0);
	}	
	resp->resp_nflwd = pkt->rsvp_nflwd;
	for (i = 0; i < pkt->rsvp_nflwd; i++) {
		FlowDesc *flwdp = FlowDesc_of(pkt, i);

		api_specp = (API_Flowspec *)                 	
			Move_filter_d2api(flwdp->rsvp_filtp, api_filtp);
                api_filtp = (API_FilterSpec *)
			Move_spec_d2api(flwdp->rsvp_specp, api_specp);
	}
	upcall_to_appl(resp, (char *) api_filtp - resp_buf, NULL);
}


void
api_ERR_upcall_common(rsvp_resp *resp, struct packet *pkt)
	{
	Set_Sockaddr_in(&resp->resp_dest, pkt->rsvp_destaddr.s_addr, 
						pkt->rsvp_dstport);
	resp->resp_protid = pkt->rsvp_protid;

	resp->resp_errcode = pkt->rsvp_errcode;
	resp->resp_errval =  pkt->rsvp_errvalue;
	resp->resp_errflags = pkt->rsvp_errflags;
	/* We use sockaddr_in rather than in_addr in resp to carry
	 *	IPv4/IPv6 typing
	 */
	resp->resp_errnode.sin_addr = pkt->rsvp_errnode;
	resp->resp_errnode.sin_family = AF_INET;
}


/*	Vector of direction flags for each event type.
 */
static int  DIR_per_EVENT[] = {0,
		0,		/* 1- Path event: OK even if dir not set */
		API_DIR_SEND,	/* 2- Resv event */
		API_DIR_SEND,	/* 3- Path error event */
		API_DIR_RECV,	/* 4- Resv error event */
		API_DIR_RECV,	/* 5- Confirmation event */
		0, 0, 0,
		0,		/* 9- Path status	*/
		API_DIR_SEND	/* 10- Resv status	*/
	};

/*	Given response message, pass it it all API sessions with matching
 *	session and direction, and for FILTER_SPEC *filtp matching sender
 *	if filtp is not NULL.
 *	
 */
static void
upcall_to_appl(rsvp_resp *resp, int len, FILTER_SPEC *filtp)
	{
	int	sid;

	resp->resp_version = VERSION_API;
	for(sid = 0; sid < API_TABLE_SIZE; sid++) {
		api_rec	*recp = &api_table[sid];

		if (recp->api_fd == 0)
			continue;
		if (DIR_per_EVENT[resp->resp_type] &&
		   (recp->api_flags & DIR_per_EVENT[resp->resp_type]) == 0)
			continue;

		if (filtp) {
			if (!recp->api_p_packet.pkt_data)
				continue;
			else if (!match_filter(filtp,
			      (SenderDesc_of(&recp->api_p_packet))->rsvp_stempl))
				continue;
		}
		if (resp->resp_dest.sin_addr.s_addr == 
				recp->api_dest.sin_addr.s_addr &&
		    resp->resp_dest.sin_port == recp->api_dest.sin_port &&
		    resp->resp_protid == recp->api_protid)
		  	send_to_api(sid, resp, len);
	}
}


/*
 *  Write message 'resp' of length 'len' bytes across API to application
 *  with local session id 'sid'.
 *
 */
void
send_to_api(int sid, rsvp_resp *resp, int len)
	{
	struct iovec iov[2];
	api_rec *recp = &api_table[sid];

	resp->resp_a_sid = recp->api_a_sid;	/* Client's SID */

	if (IsDebug(DEBUG_ALL)) {
		static char  *Event_Names[] = { "",
			"Path Evt", "Resv Evt", "Perr Evt", "Rerr Evt",
			"RConfirm", "??6 Ev",   "??7 Ev",   "??8 Ev",
			"Pstat Ev", "Rstat Ev"};

		log_api_event(LOGEV_API_upcall, Event_Names[resp->resp_type],
				&resp->resp_dest, resp->resp_protid,
				"> API pid=%d Asid=%d\n",
				recp->api_pid, recp->api_a_sid);
	}
	if (l_debug >= LOG_HEXD && len > 0) {
		hexf(stderr, (char *) resp, len);
	}

	iov[0].iov_base = (char *) (&len); 
	iov[0].iov_len  = sizeof(int); 
	iov[1].iov_base = (char *) resp; 
	iov[1].iov_len  = len;   
      
	if (writev(api_table[sid].api_fd, iov, 2) == -1)  {
		log(LOG_ERR, errno, "NBIO write error\n"); 
	}
}


int
api_refresh_delay(int sid)
	{
	api_rec *recp = &api_table[sid];

	if (recp->api_p_packet.pkt_data)
		return ((&recp->api_p_packet)->rsvp_R);
	else if (recp->api_r_packet.pkt_data)
		return ((&recp->api_r_packet)->rsvp_R);
	else
		return (0);
}



static void
print_api_request(rsvp_req *req, int len, int sid)
	{
	api_rec		*recp = &api_table[sid];
	API_FilterSpec	*api_filtp;
	API_TSpec	*api_tspecp;
	API_Flowspec	*api_specp;
	char		 buff1[80], buff2[80];
	int		 i;
	char		*flgstr;

	if (len==0 || req->rq_type == API_CLOSE) {
		log_api_event(LOGEV_API_close, "",
			&recp->api_dest, recp->api_protid,
			"<API pid=%d Asid=%d\n", 
			recp->api_pid, recp->api_a_sid);
		return;
	}
		
	switch (req->rq_type) {

	    case API2_REGISTER:
		flgstr = cnv_flags("SRCIG???", req->rq_flags);
		/*  Flags:
		 *	S: Sender
		 *	R: Receiver
		 *	C: Confirmation request on rapi_reserve call
		 *	I: Intserv format for upcalls
		 *	G: GPI format
		 */
		log_api_event(LOGEV_API_regi, flgstr,
			&req->rq_dest, req->rq_protid,
			"<API pid=%d Asid=%d\n", 
			req->rq_pid,  req->rq_a_sid);
		if (IsDebug(DEBUG_EVENTS)&& req->rq_nflwd) {
			api_filtp = (API_FilterSpec *) 
						After_APIObj(req->rq_policy);
			api_tspecp = (API_TSpec *) After_APIObj(api_filtp);
                       	rapi_fmt_filtspec(api_filtp, buff1, 80);
			rapi_fmt_tspec(api_tspecp, buff2, 80);
             		log(LOG_DEBUG, 0, "   Register sender: %s  %s\n",
					buff1, buff2);
		}
		break;

	case API2_RESV:
		log_api_event(LOGEV_API_resv, style_names[req->rq_style],
			 &req->rq_dest, req->rq_protid,
			"<API pid=%d Asid=%d\n", 
			req->rq_pid, req->rq_a_sid);
		if (IsDebug(DEBUG_EVENTS)) {
			log(LOG_DEBUG, 0, "  Reserve  StyleID= %d\n",
				 req->rq_style);
			api_filtp = (API_FilterSpec *) 
						After_APIObj(req->rq_policy);
			for (i = 0; i < req->rq_nflwd; i++) {
				api_specp = (API_Flowspec *) 
						After_APIObj(api_filtp);
                      		rapi_fmt_filtspec(api_filtp, buff1, 80);
				buff2[0] = '\0';
				if (api_specp->form != RAPI_EMPTY_OTYPE)
				    rapi_fmt_flowspec(api_specp, buff2, 80);
				api_filtp = (API_FilterSpec *) 
						After_APIObj(api_specp);
             			log(LOG_DEBUG, 0, "    %s  %s\n", buff1, buff2);
			}
		}
		break;

	default:
		break;
	}
}

/*      Define a set of object reformatting routines:
 *
 *	    Move_XXXX_api2d: copy object of type XXXX, repackaging it
 *		from the [r]api framing of daemon's object framing.
 *
 *	    Move_XXXX_d2api: copy object of type XXXX, repackaging it
 *		from the daemon object framing to [r]api framing.
 */
int
Move_spec_api2d(rapi_flowspec_t * api_specp, FLOWSPEC * d_specp)
	{
	int size;

	/* XXX should validity-check type and length
	 */
	Obj_Length(d_specp) = size = size_api2d(api_specp->len);
	memcpy(Obj_data(d_specp), RAPIObj_data(api_specp),
						size - sizeof(Object_header));
	return(0);
}

char *
Move_spec_d2api(FLOWSPEC * d_specp, rapi_flowspec_t * api_specp)
	{
	int size;

	if (!d_specp) {
		api_specp->len = sizeof(rapi_hdr_t);
		api_specp->form = RAPI_EMPTY_OTYPE;  /* Empty entry */
		return(After_APIObj(api_specp));
	}
	api_specp->len = size = size_d2api(Obj_Length(d_specp));
	api_specp->form = RAPI_FLOWSTYPE_Intserv;
	memcpy(RAPIObj_data(api_specp), Obj_data(d_specp),
						size - sizeof(rapi_hdr_t));
	return(After_APIObj(api_specp));
}

int
Move_tspec_api2d(rapi_tspec_t * api_tspecp, SENDER_TSPEC * d_tspecp)
	{
		/* XXX should validity-check type and length */
	Move_spec_api2d((rapi_flowspec_t *)api_tspecp, (FLOWSPEC *)d_tspecp);
	return(0);
}

char *
Move_tspec_d2api(SENDER_TSPEC * d_tspecp, rapi_tspec_t * api_tspecp)
	{
	char *cp;

	cp = Move_spec_d2api((FLOWSPEC *) d_tspecp, 
					(rapi_flowspec_t *)api_tspecp);
	api_tspecp->form = RAPI_TSPECTYPE_Intserv;
	return cp;
}

int
Move_adspec_api2d(rapi_adspec_t * api_adspecp, ADSPEC * d_adspecp)
	{
		/* XXX should validity-check type and length */
	Move_spec_api2d((rapi_flowspec_t *)api_adspecp, (FLOWSPEC *)d_adspecp);
	return(0);
}

char *
Move_adspec_d2api(ADSPEC * d_adspecp, rapi_adspec_t * api_adspecp)
	{
	char *cp;

	cp = Move_spec_d2api((FLOWSPEC *) d_adspecp,
					(rapi_flowspec_t *) api_adspecp);
	api_adspecp->form = RAPI_ADSTYPE_Intserv;
	return cp;
}

int
Move_filter_api2d(rapi_filter_t * api_filtp, FILTER_SPEC * d_filtp)
	{
	switch (api_filtp->form) {

#ifdef OBSOLETE_API
	    case RAPI_FILTERFORM_1:
		/* We currently implement RAPI_FILTERFORM_1 as if it
		 * were RAPI_FILTERTYPE_2, i.e., using the sockaddr and
		 * ignoring the (mask,value) part.
		 */
#endif
	    case RAPI_FILTERFORM_BASE:
		Obj_CType(d_filtp) = ctype_SENDER_TEMPLATE_ipv4;
		d_filtp->filt_srcaddr.s_addr = 
				api_filtp->rapi_filtbase_addr.s_addr;
		d_filtp->filt_srcport = api_filtp->filt_u.base.sender.sin_port;
		break;

	    case RAPI_FILTERFORM_GPI:
		Obj_CType(d_filtp) = ctype_SENDER_TEMPLATE_ipv4GPI;
		d_filtp->filt_u.filt_ipv4gpi.filt_ipaddr.s_addr = 
					api_filtp->filt_u.gpi.sender.s_addr;
		d_filtp->filt_u.filt_ipv4gpi.filt_gpi = 		
					api_filtp->filt_u.gpi.gpi;
		break;
		
	    default:
		log(LOG_ERR, 0, "API: Bad filterspec type %d\n", 
							api_filtp->form);
		break;
	}
	return(0);
}

int
path_set_laddr(rapi_filter_t * api_filtp)
	{
	switch (api_filtp->form) {
#ifdef OBSOLETE_API
	    case RAPI_FILTERFORM_1:
		/* We currently implement RAPI_FILTERFORM_1 as if it
		 * were RAPI_FILTERTYPE_2, i.e., using the sockaddr and
		 * ignoring the (mask,value) part.
		 */
#endif
	    case RAPI_FILTERFORM_BASE:
		if (api_filtp->rapi_filtbase_addr.s_addr == INADDR_ANY)
			api_filtp->rapi_filtbase_addr.s_addr = Get_local_addr;
		return map_if(&api_filtp->rapi_filtbase_addr);

	    case RAPI_FILTERFORM_GPI:
		if (api_filtp->filt_u.gpi.sender.s_addr == INADDR_ANY)
			api_filtp->filt_u.gpi.sender.s_addr = Get_local_addr;
		return map_if(&api_filtp->filt_u.gpi.sender);
		
	    default:  /* Error noted later */
		break;
	}
	return(-1);
}

char *
Move_filter_d2api(FILTER_SPEC * d_filtp, rapi_filter_t * api_filtp)
	{
	if (!d_filtp) {
		api_filtp->len = sizeof(rapi_hdr_t);
		api_filtp->form = RAPI_EMPTY_OTYPE;  /* Empty entry */
		return (After_APIObj(api_filtp));
	}
	switch (Obj_CType(d_filtp)) {

	    case ctype_FILTER_SPEC_ipv4:
		api_filtp->form = RAPI_FILTERFORM_BASE;
		Set_Sockaddr_in( &api_filtp->filt_u.base.sender,
			d_filtp->filt_srcaddr.s_addr,
			d_filtp->filt_srcport);
		api_filtp->len = sizeof(rapi_hdr_t)+sizeof(rapi_filter_base_t);
		break;

	    case ctype_FILTER_SPEC_ipv4GPI:
		api_filtp->form = RAPI_FILTERFORM_GPI;
		api_filtp->filt_u.gpi.sender.s_addr =
			d_filtp->filt_u.filt_ipv4gpi.filt_ipaddr.s_addr;
		api_filtp->filt_u.gpi.gpi =
			d_filtp->filt_u.filt_ipv4gpi.filt_gpi;
		api_filtp->len = sizeof(rapi_hdr_t) + sizeof(rapi_filter_gpi_t);
		break;

	    default:
		/*	Something is wrong with filter spec.  Pass null (but
		 *	not empty) one.  Maybe someone will notice...
		 *	But really: should validity check filter spec before
		 *	reach here!
		 */
		api_filtp->form = RAPI_FILTERFORM_BASE;
		Set_Sockaddr_in( &api_filtp->filt_u.base.sender,INADDR_ANY, 0);
		api_filtp->len = sizeof(rapi_hdr_t)+sizeof(rapi_filter_base_t);
		break;
	}
	return (After_APIObj(api_filtp));
}

Session *
locate_api_session(rsvp_req *req)
	{
	SESSION Session_obj;

	Session_obj.sess4_addr.s_addr = req->rq_dest.sin_addr.s_addr;
	Session_obj.sess4_port = req->rq_dest.sin_port;
	Session_obj.sess4_prot = (req->rq_protid)?req->rq_protid:
							RSVP_DFLT_PROTID;
	return(locate_session(&Session_obj));
}

void
log_api_event(int evtype, char *type, struct sockaddr_in *adrp, int protid,
		const char *format, int pid, int asid)
	{
	SESSION Session_obj;

	Session_obj.sess4_addr.s_addr = adrp->sin_addr.s_addr;
	Session_obj.sess4_port = adrp->sin_port;
	Session_obj.sess4_prot = protid;
	log_event(evtype, type, &Session_obj, format, pid, asid);
}
	
