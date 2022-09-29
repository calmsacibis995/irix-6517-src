
/*
 * @(#) $Id: rsvp_netio.c,v 1.15 1998/11/25 08:43:14 eddiem Exp $
 */

/************************ rsvp_netio.c  ******************************
 *                                                                   *
 *                   Network I/O routines                            *
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
#include <sys/stat.h>
#include <fcntl.h>
#ifdef _MIB
#include "rsvp_mib.h"
#endif

/* Sockets and addresses
 */
extern u_int16_t	encap_port, encap_portp;  /* Pu, Pu' */
extern struct in_addr	encap_router;
extern struct sockaddr_in encap_sin;

/* External declarations
 */
void	ntoh_object(Object_header *);
extern struct rsrr_vif vif_list[];	/* this is already defined - mwang */

int	accept_path(int in_vif, struct packet *pkt);
int	accept_resv(int in_if, struct packet *);
int	accept_path_err(int in_if, struct packet *);
int	accept_resv_err(int in_if, struct packet *);
int	accept_path_tear(int in_vif, struct packet *);
int	accept_resv_tear(int in_if, struct packet *);
int	accept_resv_conf(int in_if, struct packet *);
void	rsvp_path_err(int in_if, int e_code, int e_value, struct packet *);
void	rsvp_resv_err(int e_code, int e_value, int flags,
                                        FiltSpecStar *, struct packet *);
int	do_sendmsg(int, struct sockaddr_in *, struct in_addr *, int,
					u_char, u_char *, int);
int	unicast_route(u_long);
void	set_integrity(struct packet *, int, struct in_addr *);
void	fin_integrity(struct packet *);
Fobject *copy_obj2Fobj(Object_header *);
void	FQins(Fobject *, Fobject **);
void	FQkill(Fobject **);
char	*fmt_hostname(struct in_addr *);
#define iptoname(p)	fmt_hostname(p)
int	map_if(struct in_addr *);

/* forward declarations
 */
int	rsvp_pkt_process(struct packet *, struct sockaddr_in *, int);
int	rsvp_pkt_process_1(struct packet *, struct sockaddr_in *, int);
int	check_version_sum(struct packet *pkt);
u_int16_t  in_cksum(u_char *, int);
void	build_base(struct packet *);
void	build_obj2pkt(Object_header *, struct packet *);

int	send_pkt_out_if(int, RSVP_HOP *, struct packet *);
int	send_pkt_out_vif(int, Session *, FILTER_SPEC *, struct packet *);
int	build_send_pkt(int, struct sockaddr_in *, struct in_addr *,
							struct packet *);
int	send_msgto(int, struct sockaddr_in *, struct in_addr *,struct packet *);
int	rsvp_map_packet(struct packet *);
int	check_integrity(struct packet *);

char *Type_name[] = {"", "PATH  ", "RESV  ", "PATH-ERR", "RESV-ERR",
			"PATH-TEAR", "RESV-TEAR", "RCONFIRM"};

#define UCAST_TTL	63


/*
 *  send_pkt_out_if(): Send (unicast) RSVP packet to specified previous
 *		hop via specified physical interface (or -1).
 *
 *		[The outgoing interface is not strictly required to send
 *		unicast messages, but do need it to choose encryption key.]
 */
int
send_pkt_out_if(
	int		 outif,
	RSVP_HOP	*hopp,
	struct packet	*pkt)
	{
	struct sockaddr_in sin;
	struct in_addr	*srcaddrp = NULL;
	int		 mode = 0;

	assert(hopp->hop4_addr.s_addr != INADDR_ANY);
 
	Set_Sockaddr_in( &sin, hopp->hop4_addr.s_addr, 0);
	pkt->pkt_ttl = UCAST_TTL;
	if (outif >= 0)
		srcaddrp = &if_vec[outif].if_orig;

	mode = build_send_pkt(outif, &sin, srcaddrp, pkt);

	if (IsDebug(DEBUG_ALL)) {
		char	tmp[32];

		if (outif >= 0)
			sprintf(tmp, "%d=>%s > %.16s/%d\n",
					outif, vif_name(outif),
					iptoname(&sin.sin_addr), pkt->pkt_ttl);
		else
			sprintf(tmp, "> %.16s/%d\n",
					iptoname(&sin.sin_addr), pkt->pkt_ttl);
		log_event(LOGEV_Send_UDP+mode-1,
			Type_name[pkt->pkt_map->rsvp_msgtype],
			pkt->pkt_map->rsvp_session, tmp);
		if (IsDebug(DEBUG_IO))
	    		print_rsvp(pkt);
	}
	return(0);
}



/*
 * send_pkt_out_vif(): Send Path, PathTear, or ResvErr msg with specific
 *		dest and src addresses to specified vif.
 *
 *	XXX???  If encap_router address is defined, unicast a unicast UDP
 *	XXX???	copy to that address; this is to allow hosts inside a
 *	XXX???	firewall to send a Path message to the first-hop router.
 */
int
send_pkt_out_vif(
	int		 vif,
	Session		*destp,
	FILTER_SPEC	*filtp,
	struct packet	*pkt)
	{
	struct sockaddr_in sin;
	struct in_addr	*srcaddrp = &filtp->filt_srcaddr;
	int		mode = 0;
	
	assert(vif < vif_num);	/* Not API */

	Set_Sockaddr_in(&sin, destp->d_addr.s_addr, destp->d_port);
	mode = build_send_pkt(vif, &sin, srcaddrp, pkt);

	if (encap_router.s_addr != INADDR_ANY) {
		/* If a -R cmd-line option was given to specify the address
		 * of an RSVP-capable router, unicast a UDP copy of pkt to
		 * address.  Note: this could be generalized to a list.
		 */
		pkt->pkt_flags = PKTFLG_USE_UDP;
		build_send_pkt(vif, &sin, srcaddrp, pkt);
	}
	if (mode < 0)
		return -1;
	if (IsDebug(DEBUG_ALL)) {
		char	tmp[32];

		sprintf(tmp, "%d=>%s > %.16s/%d\n", vif, vif_name(vif),
					iptoname(&sin.sin_addr), pkt->pkt_ttl);
		log_event(LOGEV_Send_UDP+mode-1,
			Type_name[pkt->pkt_map->rsvp_msgtype],
			pkt->pkt_map->rsvp_session, tmp);
		if (IsDebug(DEBUG_IO))
	    		print_rsvp(pkt);
	}
	return(0);
}


/*
 * build_send_pkt():  Build RSVP message from a given packet map, and send it.
 */
int
build_send_pkt(
	int vif,
	struct sockaddr_in *tosockaddr,
	struct in_addr	*srcp,
	struct packet	*opkt)
	{
	struct packet	lpkt, *pkt = &lpkt;
	char		buff[MAX_PKT+IPUDP_HDR_AREA_SIZE];
	packet_map	*mapp = opkt->pkt_map;
	FlowDesc 	*flwdp;
	SenderDesc	*sdscp;
	int		i;

	assert(opkt->pkt_order == BO_HOST);

	*pkt = *opkt;	/* Local copy of packet struct */
	pkt->pkt_data = (common_header *) (buff+IPUDP_HDR_AREA_SIZE);

	/*	If we need an INTEGRITY object, add it to the map and
	 *	initialize it.  Then build fixed part of packet from map.
	 *	Then, if there is INTEGRITY object, point map to object
	 *	in packet (whihc must come immediately after common hdr).
	 */
	set_integrity(pkt, vif, srcp);
	build_base(pkt);
	if (mapp->rsvp_integrity)
		pkt->pkt_map->rsvp_integrity = (INTEGRITY *)
							(1 + pkt->pkt_data);
	pkt->pkt_flags &= ~PKTFLG_Send_RA;

	Incr_ifstats(vif, rsvpstat_msgs_out[mapp->rsvp_msgtype]);

	switch (mapp->rsvp_msgtype) {

	    case RSVP_CONFIRM:
		/* Confirm: send Router Alert Option
		 */
		pkt->pkt_flags |= PKTFLG_Send_RA;

	    case RSVP_RESV:
	    case RSVP_RESV_ERR:
	    case RSVP_RESV_TEAR:

		switch (Style(pkt)) {

		    case STYLE_WF:
			flwdp = FlowDesc_of(pkt, 0);
			build_obj2pkt(Object_of(flwdp->rsvp_specp), pkt);
			break;

		    case STYLE_FF:
			for (i= 0; i < mapp->rsvp_nlist; i++) {
				int size;

				flwdp = FlowDesc_of(pkt, i);
				size = FlowDesc_size(flwdp);
				if (size + pkt->pkt_len > Max_rsvp_msg) {
				    /* This rudimentary semantic fragmentation 
				     * code is an obsolete remnant, which is
				     * left here as a place-holder.
				     */
				    if (send_msgto(vif, tosockaddr, srcp, pkt)<0)
						return(-1);
				    build_base(pkt);
				}
				build_obj2pkt(Object_of(flwdp->rsvp_specp),
								pkt);
				build_obj2pkt(Object_of(flwdp->rsvp_filtp), 
								pkt);
			}
			break;

		    case STYLE_SE:
			flwdp = FlowDesc_of(pkt, 0);
			build_obj2pkt(Object_of(flwdp->rsvp_specp), pkt);
			for (i= 0; i < mapp->rsvp_nlist; i++) {
				flwdp = FlowDesc_of(pkt, i);
				build_obj2pkt(Object_of(flwdp->rsvp_filtp), 
								pkt);
			}
			break;			
		}
		break;

	    case RSVP_PATH:
	    case RSVP_PATH_TEAR:
		/* Path or Path_Tear: send Router Alert Option
		 */
		pkt->pkt_flags |= PKTFLG_Send_RA;

	    case RSVP_PATH_ERR:
		sdscp = SenderDesc_of(pkt);
		build_obj2pkt(Object_of(sdscp->rsvp_stempl), pkt);
		build_obj2pkt(Object_of(sdscp->rsvp_stspec), pkt);
		build_obj2pkt(Object_of(sdscp->rsvp_adspec), pkt);
		break;

	    default:
		assert(0);
		break;		
	}

	return (send_msgto(vif, tosockaddr, srcp, pkt));
}


/*
 *	send_msgto(): Given an RSVP message, complete its common header,
 *	convert it to network byte order, compute a checksum, and send it.
 */
int
send_msgto(
	int vif,
	struct sockaddr_in *tosockaddr,
	struct in_addr	*srcaddrp,
	struct packet	*pkt)
	{
	common_header	*hdrp = pkt->pkt_data;

	assert(pkt->pkt_order == BO_HOST);

	/*
	 *	Complete common header and convert to network byte order
	 */
	memset((char *)hdrp, 0, sizeof(common_header));
	hdrp->rsvp_verflags = RSVP_MAKE_VERFLAGS(RSVP_VERSION, 0);
	hdrp->rsvp_type = pkt->pkt_map->rsvp_msgtype;
	hdrp->rsvp_length = pkt->pkt_len;

	hton_packet(pkt);

	assert(pkt->pkt_order == BO_NET);

	/*
	 *	Set ttl in common header and, either checksum packet
	 *	or, if there is INTEGRITY object, compute digest.
	 */
#ifdef __sgi
	/*
	 * Modify the rsvp_snd_TTL field so that down stream routers will
	 * know that this node does not support traffic control.  If there
	 * is an ADSPEC in the packet, we should set that also.
	 */
	if (if_vec[vif].if_flags & IF_FLAG_PassBreak) 
	     hdrp->rsvp_snd_TTL = pkt->pkt_ttl + 1;
	else
#endif
	hdrp->rsvp_snd_TTL = pkt->pkt_ttl;
	hdrp->rsvp_cksum = 0;
	if (pkt->pkt_map->rsvp_integrity)
		fin_integrity(pkt);
	else
		hdrp->rsvp_cksum =
			in_cksum((u_int8_t *)pkt->pkt_data, pkt->pkt_len);

	return do_sendmsg(vif, tosockaddr, srcaddrp, pkt->pkt_flags,
			pkt->pkt_ttl, (u_char *) pkt->pkt_data, pkt->pkt_len);
}



/*	build_base(): Start building new packet from given map.
 *
 *		o Initialize pkt len to size of common header; this field
 *		  will be used for bookkeeping as the variable part of the
 *		  packet is built.
 *		o Copy objects into packet from fixed part of map.
 *		o Copy unknown objects into packet
 *		  (Note that we have lost any order information...)
 *		o Set byte order to HOST.
 */
void
build_base(struct packet *pkt)
	{
	Object_header **mappp, *pktp;
	Fobject		*fobjp;

	pkt->pkt_len = sizeof(common_header);
	pktp = (Object_header *) ((common_header *) pkt->pkt_data + 1);

	mappp = (Object_header **) &pkt->pkt_map->rsvp_integrity;
	while (mappp <= (Object_header **) &pkt->pkt_map->rsvp_style) {
		if (*mappp)
			build_obj2pkt(*mappp, pkt);
		mappp++;

	}
	for (fobjp = pkt->pkt_map->rsvp_UnkObjList; fobjp;
						fobjp = fobjp->Fobj_next) {
		if (fobjp)
			build_obj2pkt(&fobjp->Fobj_objhdr, pkt);
	}
	pkt->pkt_order = BO_HOST;
}


/*	built_obj2pkt(): Move object *objp into packet buffer at offset
 *		determined from pkt_len field in packet structure, and
 *		advance pkt_len field.
 */
void
build_obj2pkt(Object_header *objp, struct packet *pkt)
	{
	Object_header *pktp;

	if (objp && (Object_Size(objp) > 0) ) {
		pktp = (Object_header *) ((char *) pkt->pkt_data +
							pkt->pkt_len);
		move_object(objp, pktp);
		pkt->pkt_len += pktp->obj_length;
	}
}


/*
 * rsvp_pkt_process(): Primary RSVP protocol processing routine for input.
 *			Called with either a new packet received from the
 *			network, or a stored packet from the API.
 */
int
rsvp_pkt_process(
	struct packet *pkt,		/* packet struct. Flags => UDP|raw */
	struct	sockaddr_in *fromp,	/* Addr(src IP addr) or NULL (API) */
	int inp_if)			/* Incoming interface (-1 => mcast) */
	{
	int		rerrno = PKT_OK;
	packet_map	*mapp = pkt->pkt_map;
	int		Not_from_API =  (fromp)?1:0;
	int		rc;

#ifdef _MIB
	if (mib_enabled)
	        rsvpd_heard_from(inp_if, fromp,
				 (pkt->pkt_flags & PKTFLG_USE_UDP));
#endif

	/*	If this packet was received from the network (ie if it is
	 *	in NET byte order), do basic input processing.  Otherwise,
	 *	it came from API and the map is already built.
	 *
	 *	Verify version number and RSVP checksum, and discard
	 *	message if any mismatch is found.
	 *
	 *	Parse packet and construct map, converting objects to host
	 *	byte order.  Note that rsvp_map_packet() call may do mallocs
	 *	for policy or unknown class objects.
	 */
	if (pkt->pkt_order == BO_NET) {
		rerrno = check_version_sum(pkt);
		if (rerrno != PKT_OK) {
#ifdef _MIB
		        if (mib_enabled)
			       rsvpd_inc_rsvpBadPackets();
#endif
			return(rerrno);
	        }

		rerrno = rsvp_map_packet(pkt);
	}

	/*	Check INTEGRITY object, if any.
	 */
	if (mapp->rsvp_integrity) {
		if ((rc = check_integrity(pkt)) < 0)
			goto Free_exit;
	}

	if (IsDebug(DEBUG_ALL)) {
		char temp[32];
		int  evtype;

		if (Not_from_API) {
			if (inp_if < 0)
				sprintf(temp, "< %.16s",
						iptoname(&fromp->sin_addr));
			else
				sprintf(temp, "%s<=%d < %.16s",
						vif_name(inp_if), inp_if,
						iptoname(&fromp->sin_addr));
			evtype = (pkt->pkt_flags&PKTFLG_USE_UDP)? 
					LOGEV_Recv_UDP:LOGEV_Recv_Raw;
		}
		else {
			sprintf(temp, "<API ttl=");
			evtype = LOGEV_Recv_API;
		}
		if (mapp->rsvp_session)
			log_event(evtype,
				Type_name[mapp->rsvp_msgtype],
				mapp->rsvp_session, "%s/%d\n",
					 temp, pkt->pkt_ttl);
		if (IsDebug(DEBUG_IO)) {
			print_rsvp(pkt);
		}
	}

	if (rerrno > 0) {
		/*	Packet contains object with unknown class,
		 *	and high-order bits of class number indicate
		 *	that packet should be rejected and error
		 *	message sent.
		 */
		if (!(mapp->rsvp_hop)||!(mapp->rsvp_session)) {
			/* No HOP or no SESSION... can't send err message. */
#ifdef _MIB
		        if (mib_enabled)
			       rsvpd_inc_rsvpBadPackets();
#endif
			return(rerrno);
		}

		switch (mapp->rsvp_msgtype) {

		    case RSVP_PATH:
			rsvp_path_err(inp_if, Get_Errcode(rerrno),
					Get_Errval(rerrno), pkt);
			break;

		    case RSVP_RESV:
			rsvp_resv_err(Get_Errcode(rerrno), Get_Errval(rerrno), 
					0, (FiltSpecStar *) -1, pkt);
			break;

		    default:
			/*	Spec says don't send error for Teardown msg */
			/*	Spec says don't send error for error msg */
			break;
		}
	}
	if (rerrno == PKT_OK)
		rerrno = rsvp_pkt_process_1(pkt, fromp, inp_if);

Free_exit:
	/*
	 *	Free any malloc'd unknown or policy objects that are left
	 */
	FQkill(&pkt->pkt_map->rsvp_UnkObjList);
	FQkill(&pkt->pkt_map->rsvp_dpolicy);
	return(rerrno);
}
	

/*	Inner routine for rsvp_pkt_process()
 */
int
rsvp_pkt_process_1(
	struct packet *pkt,		/* packet struct. Flags => UDP|raw */
	struct	sockaddr_in *fromp,	/* Addr(src IP addr) or NULL (API) */
	int inp_if)			/* Incoming interface (-1 => mcast) */
	{
	packet_map	*mapp = pkt->pkt_map;
	int		Not_from_API =  (fromp)?1:0;
	int		i, rc;

	/*	If there is a hop address and it's not from API, hop address
	 *	must be non-zero.
	 */
	if ((Not_from_API) && (pkt->pkt_map->rsvp_hop) && 
				pkt->rsvp_phop.s_addr == INADDR_ANY)
		return PKT_ERR_HOPZERO;
	/*
	 *	For each msg type, do format checking and various other
	 *	checks, then call appropriate handling routine.
	 */
	rc = 0;
	switch (mapp->rsvp_msgtype) {

		case (RSVP_PATH):
			/* If number of sender descriptors is zero, 
			 * ignore the message.  This case is a (deliberate) 
			 * wierdness of the Version 1 RSVP spec.
 			 *	XXX Should we log or count these anomalies ?
			 */
			if (pkt->rsvp_nflwd == 0)
				return PKT_OK;  /* Wierdness */
			if (!(mapp->rsvp_hop)||!(mapp->rsvp_timev))
				return PKT_ERR_MISSOBJ;
			if (!(mapp->rsvp_list[0].Sender_list.rsvp_stempl) ||
			    !(mapp->rsvp_list[0].Sender_list.rsvp_stspec) )
				return PKT_ERR_MISSOBJ;
			if (pkt->pkt_ttl == 0)
				return PKT_ERR_TTLZERO;
			/*
			 *	If the DstPort in the SESSION object is zero
			 *	but SrcPort in SENDER_TEMPLATE is non-zero, 
			 *	return "Conflicting Src Port" error.
			 */
			if (pkt->rsvp_dstport == 0 &&
			    STempl_of(SenderDesc_of(pkt))->filt_srcport != 0)
				return PKT_ERR_BADSPORT;

	/*	Multihomed host without mrouted: cannot tell the incoming
	 *	interface on which a multicast Path message really arrived
	 *	under SunOS (or probably any system derived from 4.3 BSD).
	 *	It would assume default (if# 0); then when send Resv msg,
	 *	may attach wrong NHOP address.  First try mapping destination
	 *	address into one of our interfaces.  If that fails, use kludge:
	 *	do unicast route lookup and use resulting interface addr.
	 *	THIS ASSUMES UNICAST ROUTING IN THE OTHER DIRECTION MATCHES
	 *	MULTICAST ROUTING, and that vif's are a subset of if's.
	 */
			if (inp_if < 0 && NoMroute && if_num > 1)  {
			    inp_if = map_if(&pkt->rsvp_destaddr);
			    if (inp_if < 0) {
				inp_if = unicast_route(pkt->rsvp_phop.s_addr);
				if (inp_if < 0) {
					log(LOG_WARNING, 0, "No inc iface\n");
					return(PKT_OK);  /* XXX Not OK */
				}
			    }
			}

			rc = accept_path(inp_if, pkt);
			break;

		case (RSVP_RESV):
			/* If number of flow descriptors is zero, 
			 * ignore the message.  This case is a (deliberate) 
			 * wierdness of the Version 1 RSVP spec.
 			 *	XXX Should we log or count these anomalies ?
			 */
			if (pkt->rsvp_nflwd == 0)
				return PKT_OK;  /* Wierdness */
#ifdef DEBUG
			/* Nhop address should be normally be identical
			 * to IP src addr, but non-RSVP clouds can do
			 * funny things...
	 		 */
			if ((Not_from_API) &&
			    fromp->sin_addr.s_addr != pkt->rsvp_nhop.s_addr)
				log(LOG_WARNING, 0,
					"Nhop != IP src in Resv\n");
#endif /* DEBUG */

			if (!(mapp->rsvp_style) ||
			    !(mapp->rsvp_hop)||!(mapp->rsvp_timev))
				return PKT_ERR_MISSOBJ;
			/*
			 *	If the DstPort in the SESSION object is zero
			 *	but SrcPort in a FILTER_SPEC is non-zero, 
			 *	return "Conflicting Src Port" error.
			 */
			if (pkt->rsvp_dstport == 0){
				for (i =0; i < pkt->rsvp_nflwd ; i++) {
					FILTER_SPEC *filtp =
						filter_of(FlowDesc_of(pkt,i));

					if (filtp && filtp->filt_srcport != 0)
						return PKT_ERR_BADSPORT;
				}
			}
			/*
			 *	Check reasonableness of the LIH
			 */
			if (pkt->rsvp_nhop.s_addr != INADDR_ANY &&	
			    pkt->rsvp_LIH > vif_num)
				return PKT_ERR_BADLIH;

			rc = accept_resv(inp_if, pkt);
			break;

		case (RSVP_PATH_TEAR):
			if (pkt->rsvp_nflwd == 0)
				return PKT_OK;  /* Wierdness */
			if (!(mapp->rsvp_hop))
				return PKT_ERR_MISSOBJ;
			mapp->rsvp_timev = NULL;  /* ignore TIMEV */
			rc = accept_path_tear(inp_if, pkt);
			break;

		case (RSVP_PATH_ERR):
			if (pkt->rsvp_nflwd == 0)
				return PKT_OK;  /* Wierdness */
			if (!(mapp->rsvp_errspec) )
				return PKT_ERR_MISSOBJ;
			mapp->rsvp_timev = NULL;  /* ignore TIMEV */

			rc = accept_path_err(inp_if, pkt);
			break;

		case (RSVP_RESV_TEAR):
			/* Note that a ResvTear can legitimately have no
			 * flow descriptor, for WF style.
			 */
			if (!(mapp->rsvp_style) ||
			    !(mapp->rsvp_hop) )
				return PKT_ERR_MISSOBJ;
			/*
			 *	Check reasonableness of the LIH
			 */
			if (pkt->rsvp_nhop.s_addr != INADDR_ANY &&	
			    pkt->rsvp_LIH > vif_num)
				return PKT_ERR_BADLIH;
			mapp->rsvp_timev = NULL;  /* ignore TIMEV */

 			rc = accept_resv_tear(inp_if, pkt);
			break;

		case (RSVP_RESV_ERR):
			if (pkt->rsvp_nflwd == 0)
				return PKT_OK;  /* Wierdness */
			if (!(mapp->rsvp_style) ||
			    !(mapp->rsvp_errspec) ||
			    !(mapp->rsvp_hop) )
				return PKT_ERR_MISSOBJ;
			mapp->rsvp_timev = NULL;  /* ignore TIMEV */

			rc = accept_resv_err(inp_if, pkt);
			break;

		case (RSVP_CONFIRM):
			if (!(mapp->rsvp_style) ||
			    !(mapp->rsvp_errspec) ||
			    !(mapp->rsvp_confirm))
				return PKT_ERR_MISSOBJ;
			
			mapp->rsvp_timev = NULL;  /* ignore TIMEV */

			rc = accept_resv_conf(inp_if, pkt);
			break;

		default:
			return PKT_ERR_MSGTYPE;
	}

	if (rc < -1)
		return rc;
	return PKT_OK;
}


/*	Check version and checksum in incoming packet, converting
 *	common header to host byte order in the process.  Ignore
 *	zero checksum.
 */
int
check_version_sum(struct packet *pkt)
	{
	int	pkt_version = RSVP_VERSION_OF(pkt->pkt_data);

	assert(pkt->pkt_order == BO_NET);

	/*
	 * 	Check version, checksum (if non-zero), and length.
	 */
	if  (pkt_version != RSVP_VERSION) {
		log(LOG_WARNING, 0, "Bad pkt version #%d\n", pkt_version);
		return PKT_ERR_VERSION;
	}
	if ((pkt->pkt_data->rsvp_cksum) &&
		in_cksum((u_char *) pkt->pkt_data, pkt->pkt_len)) {
			log(LOG_WARNING, 0, "Checksum error\n");
			return PKT_ERR_CKSUM;
	}
	NTOH16(pkt->pkt_data->rsvp_length);
	if (pkt->pkt_data->rsvp_length != pkt->pkt_len) {
		log(LOG_WARNING, 0, "Pkt length wrong (%u)\n", pkt->pkt_len);
		return PKT_ERR_LENGTH;
	}
	return PKT_OK;
}


/*
 *	rsvp_map_packet(): Given input packet and map vector pointed to
 *		by packet structure, parse the packet and build a map of
 *		it, converting the packet to host byte order as we go.
 *		Returns PKT_OK (0) or negative PKT_ERR_.... or positive
 *		errno (if unknown object class).
 */
int
rsvp_map_packet(struct packet *pkt)
	{
	common_header	*hdrp = pkt->pkt_data;
	packet_map	*mapp = pkt->pkt_map;
	char		*end_of_data = (char *) pkt->pkt_data + pkt->pkt_len;
	Object_header	*objp;
	style_t		 optvec = 0;
	FlowDesc	*flwdp = NULL;
	SenderDesc	*sdscp = SenderDesc_of(pkt);
	FLOWSPEC	*Last_specp = NULL;
	Fobject 	*Fobjp;

	/*
	 *	Clear base segment of packet map
	 */
	memset((char *) mapp, 0, sizeof(packet_map));
	mapp->rsvp_nlist = 0;
	mapp->rsvp_msgtype = RSVP_TYPE_OF(pkt->pkt_data);

	objp = (Object_header *) (hdrp + 1);
	while (objp <  (Object_header *)end_of_data) {

#if BYTE_ORDER == LITTLE_ENDIAN
	    ntoh_object(objp);
#endif
	    if ((Obj_Length(objp)&3) || Obj_Length(objp)<4 ||
			(char *)Next_Object(objp) > end_of_data){
		return PKT_ERR_OBJLEN;
	    }
	
	    switch (Obj_Class(objp)) {

		case class_NULL:
			/* Ignore it */
			break;

		case class_SESSION:
			mapp->rsvp_session = (SESSION *) objp;
  			break;

		case class_SESSION_GROUP:
			/*** Not implemented: will be ignored ***/
			mapp->rsvp_sess_grp = (SESSION_GROUP *) objp;
			break;

		case class_RSVP_HOP:
			mapp->rsvp_hop = (RSVP_HOP *) objp;
			break;

		case class_INTEGRITY:
			mapp->rsvp_integrity = (INTEGRITY *) objp;
			break;

		case class_TIME_VALUES:
			mapp->rsvp_timev = (TIME_VALUES *) objp;
			break;

		case class_ERROR_SPEC:
			mapp->rsvp_errspec = (ERROR_SPEC *) objp;
			break;

		case class_SCOPE:
			mapp->rsvp_scope_list = (SCOPE *) objp;
			break;				

		case class_STYLE:
			if (!IsResv(mapp))
				return PKT_ERR_BADOBJ;
			mapp->rsvp_style = (STYLE *) objp;
			optvec = Style(pkt);
			break;
			
		case class_FLOWSPEC:
			/* Each flowspec begins new flow descriptor */
			if (!IsResv(mapp))
				return PKT_ERR_BADOBJ;
			flwdp = &mapp->rsvp_list[mapp->rsvp_nlist].Resv_list;
			Last_specp = flwdp->rsvp_specp = (FLOWSPEC *)objp;
			flwdp->rsvp_filtp = NULL;
			break;

		case class_FILTER_SPEC:
			if (!IsResv(mapp))
				return PKT_ERR_BADOBJ;
			flwdp = &mapp->rsvp_list[mapp->rsvp_nlist++].Resv_list;
			flwdp->rsvp_filtp = (FILTER_SPEC *)objp;
			if (flwdp->rsvp_specp == NULL &&
			    mapp->rsvp_msgtype != RSVP_RESV_TEAR) {
				if (mapp->rsvp_nlist == 0)
					return PKT_ERR_ORDER;
  					/* First must be flowspec */
				if (optvec == STYLE_FF)
			   		flwdp->rsvp_specp = Last_specp;
					/* FF: Flowspec ellided -- use last */
			}
			flwdp = &mapp->rsvp_list[mapp->rsvp_nlist].Resv_list;
			flwdp->rsvp_specp = NULL;
			break;

		case class_SENDER_TEMPLATE:
			if (!IsPath(mapp))
				return PKT_ERR_BADOBJ;
			if (mapp->rsvp_nlist > 0)
				return PKT_ERR_BADOBJ;
			mapp->rsvp_nlist = 1;
			sdscp->rsvp_stempl = (SENDER_TEMPLATE *) objp;
			break;

		case class_SENDER_TSPEC:
			if (!IsPath(mapp))
				return PKT_ERR_BADOBJ;
			/* Could test for duplicate Sender_Tspec or Adspec...
			 * instead, just take the last one that is found.
			 */
			sdscp->rsvp_stspec = (SENDER_TSPEC *) objp;
			break;

		case class_ADSPEC:
			if (!IsPath(mapp))
				return PKT_ERR_BADOBJ;
			sdscp->rsvp_adspec = (ADSPEC *) objp;
			break;

		case class_POLICY_DATA:
			/*	Save policy objects in list of framed objects
			 */
			Fobjp = copy_obj2Fobj(objp);
			if (!Fobjp) {
				Log_Mem_Full("PD obj");
				break;
			}
			FQins(Fobjp, &mapp->rsvp_dpolicy);
			break;

		case class_CONFIRM:
			mapp->rsvp_confirm = (CONFIRM *) objp;
			break;

		default:
			/*	Unknown object class.  Look at high-order
			 *	bits to decide what to do.
			 */
			switch (Obj_Class(objp) & UNKN_CLASS_MASK) {

			    case 0x00:
			    case 0x40:
				/*	Reject message.  Return
				 *	code, val=(class,ctype)
				 */
				return (Set_Errno(RSVP_Err_UNKN_OBJ_CLASS,
				     ((Obj_Class(objp)<<8)|Obj_CType(objp))));

			    case 0x80:
				/*	Ignore object
				 */
				break;

			    case 0xc0:
				/*	Save and forward object.  Copy it into
				 *	a framed object and add to end of list.
				 */
				Fobjp = copy_obj2Fobj(objp);
				if (!Fobjp) {
					Log_Mem_Full("Unk Class obj");
					break;
				}
				FQins(Fobjp, &mapp->rsvp_UnkObjList);
				break;
			}
			log(LOG_WARNING, 0, "Unknown class %d ctype %d\n",
				Obj_Class(objp), Obj_CType(objp));
			break;
	    }
	    objp = Next_Object(objp);
	}
	if (objp != (Object_header *)end_of_data) {
		log(LOG_WARNING, 0, "Hdr len err: %d\n",
				(char *)objp - (char *) end_of_data);
                return PKT_ERR_LENGTH;
        }
        pkt->pkt_order = BO_HOST;

	if (!(mapp->rsvp_session))
		return PKT_ERR_NOSESS;

        /*
         *      Do style-dependent processing of Resv-type msg
         */
        if (IsResv(mapp)) {
                switch (optvec) {
                    case 0:
                        /* Error: missing style */
                        return PKT_ERR_NOSTYLE;

                    case STYLE_WF:
 			if (flwdp) { /* Found a flowspec => msg not empty */
				mapp->rsvp_nlist = 1;
				flwdp->rsvp_filtp = NULL;
			}
                        break;

		    case STYLE_SE:
                    case STYLE_FF:
                        break;

                    default:
			/*  Error message sent at higher level */
                        break;
                }
        }
        return PKT_OK;
}



/*
 * Compute TCP/UDP/IP checksum.
 * See p. 7 of RFC-1071.
 */
u_int16_t
in_cksum(u_int8_t *bp, int n)
{
        u_int16_t *sp = (u_int16_t *) bp;
        u_int32_t sum = 0;

        /* Sum half-words */
        while (n>1) {
                sum += *sp++;
                n -= 2;
        }
        /* Add left-over byte, if any */
        if (n>0)
                sum += * (char *) sp;
        /* Fold 32-bit sum to 16 bits */
        sum = (sum & 0xFFFF) + (sum >> 16);
        sum = ~(sum + (sum >> 16)) & 0xFFFF;
        if (sum == 0xffff)
                sum = 0;
        return (u_int16_t)sum;
}

#ifndef SECURITY
int
check_integrity(struct packet *pkt)
	{
	return(1);
}

/*
 *      set_integrity()
 *              Initialize INTEGRITY object, if one is required, for
 *              sending packet through specified vif.
 */
void
set_integrity(struct packet *pkt, int vif, struct in_addr *scrp)
	{
}

/*
 *      fin_integrity()
 *              Compute crypto digest over given packet and complete
 *              the INTEGRITY object.
 */
void
fin_integrity(struct packet *pkt)
	{
}
#endif /* SECURITY */
