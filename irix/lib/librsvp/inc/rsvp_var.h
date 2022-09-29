
/*
 * @(#) $Id: rsvp_var.h,v 1.10 1998/11/25 08:43:36 eddiem Exp $
 */
/****************************************************************************

            RSVPD -- ReSerVation Protocol Daemon

                USC Information Sciences Institute
                Marina del Rey, California

		Original Version: Shai Herzog, Nov. 1993.
		Current Version: Steven Berson & Bob Braden, May 1996.

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


#ifndef __rsvp_var_h__
#define __rsvp_var_h__

/*	Times
 */
#define MIN_TIMER_PERIOD  200	/* the minimal time interval		*/
#define MAX_TIMER_Q       1000	/* the maximal timer queue len		*/
#define LOCAL_REPAIR_W	  2000	/* W = Timeout for path local repair	*/
#define REFRESH_DEFAULT   30000 /* Default refresh time in protocol	*/
#define API_REFRESH_T	  30000 /* Refresh time for pseudo-msgs from API*/
#define REFRESH_SLEW_MAX  0.30  /* SlewMax = Max increase rate for R	*/
#define K_FACTOR 	  3	/* K = Timeout factor			*/
#define Kb_FACTOR	  2	/* Kb = Blockade state timeout factor	*/
#define REFRESH_FAST_T	  1000	/* Fast refresh interval		*/
#define REFRESH_FAST_N	  3	/* No. of fast refresh packets to send	*/
#define DUMP_DS_TIMO      30000	/* msec between state dumps to log	*/

/*	TTLs
 */
#define RSVP_TTL_MAX	  63	/* Max TTL for mcasting Path, etc msgs	*/
#define RSVP_TTL_ENCAP	  1	/* TTL for UDP encapsulation		*/

/*	Limits
 */
#define SESS_HASH_SIZE	  293	/* Entries in session hash table	*/
#define MAX_SESSIONS      200	/* max # API sessions			*/
#define API_TABLE_SIZE 	  (2*MAX_SESSIONS) /* Double-size hash table	*/
#define KEY_TABLE_SIZE  40	/* Max # key association table entries  */
	/* Handle multiple interfaces, multiple keys, and 2 directions	*/
#define MAX_PKT		  4000	/* temporary maximum packet length	*/
#if defined (freebsd)
#define MAX_SOCK_CTRL     50    /* IP_RECVIF support, control structure */
                                /* for received phys. interface number  */
#endif
#define IPUDP_HDR_AREA_SIZE 32  /* Room to build IP and UDP headers	*/
#define MAX_FLWDS	  100	/* Max # flow descriptors allowed in map*/
#define MAX_LOG_SIZE	  400000
#define MIN_INTGR_SEQ_NO   3	/* Always-accept threshold for INTEGRITY*/
				/* sequence number.			*/

/*  Types
 */
typedef u_long  bitmap;


/***************************************************************
 *		RSVP state: Data Structures
 *
 ***************************************************************/

/*	FiltSpecStar (FILTER_SPEC*): variable-length list of
 *		(filter spec, info) pairs.  In RSB's the info is
 *		the time-to-die; in TCSB's it is the Fhandle.
 *
 *		WF: 1 entries with NULL pointer.
 *		FF: 1 entry
 *		SE: n entries
 */
typedef struct filtstar {
	u_int16_t	 fst_size;	/* Max number of filter specs */
	u_int16_t	 fst_count;	/* Number of filter specs */
	struct fst_pair  {
		FILTER_SPEC  *fst_filtp;	/* Ptr to FILTER SPEC	*/
		u_int32_t     fst_info;		/* Info			*/
		}	fst_p[1];		/* Var-len list of pairs*/
}    FiltSpecStar;

#define	fst_filtp0		fst_p[0].fst_filtp
#define fst_Filtp(i)		fst_p[(i)].fst_filtp
#define fst_Filt_TTD(i)		fst_p[(i)].fst_info

/*	Fobject: Framed object, i.e., object preceded by queueing pointer.
 *
 */
typedef struct Fobject_stru {
	struct Fobject_stru	*Fobj_next;	/* Next in list	*/
	Object_header		 Fobj_objhdr;	/* Object header */
						/* followed by object body */
}   Fobject;


/*
 *	RSB:	Reservation state block.
 */
typedef struct resv {
	struct resv	*rs_next;
	RSVP_HOP	 rs_rsvp_nhop;	/* IP addr of next RSVP hop	*/
					/* If addr==0(API) => LIH= Iapp	*/
	FiltSpecStar	*rs_filtstar;	/* Ptr to var-len filt spec list*/
	u_char		 rs_OIf;	/* Outgoing Interface		*/
	u_char		 rs_flags;	/* 				*/
	u_short		 rs_rsrr_flags;	/* RSRR flags			*/

	style_t		 rs_style;	/* Style opt vector		*/
	FLOWSPEC	*rs_spec;	/* ptr to FLOWSPEC object	*/
	FLOWSPEC	*rs_fwd_spec;	/* FLOWSPEC to be forwarded|NULL*/
	SCOPE		*rs_scope;	/* WF scope for this resv	*/
	CONFIRM		*rs_confirm;	/* Confirmation			*/
	u_int32_t	 rs_ttd;	/* Time-to-die for this state	*/
	Fobject		*rs_UnkObjList;	/* List of Unknown objs to fwd	*/
	Fobject		*rs_Frpolicy;	/* List of POLICY DATA objects  */
#ifdef _MIB
	void		*mib_resvhandle;
	void		*mib_resvfwdhandle;
#endif
} RSB;

#define rs_nhop		rs_rsvp_nhop.hop_u.hop_ipv4.hop_ipaddr
#define rs_filter0	rs_filtstar->fst_p[0].fst_filtp
#define rs_fcount	rs_filtstar->fst_count
#define filt_ttd	fst_info
#define rs_Filtp(i)	rs_filtstar->fst_p[(i)].fst_filtp
#define rs_Filt_TTD(i)	rs_filtstar->fst_p[(i)].fst_info


/*
 *	TCSB: Traffic Control reservation state block.
 */
typedef struct tc_resv {
	struct tc_resv	*tcs_next;
	int		 tcs_OIf;	/* Outgoing (logical) interface */
	FiltSpecStar	*tcs_filtstar;	/* Ptr to Var-len filtspec list */
	FLOWSPEC	*tcs_spec;	/* Effective FLOWSPEC object	*/
	SENDER_TSPEC	*tcs_tspec;	/* Effective SENDER_TSPEC object*/
	u_long		 tcs_rhandle;	/* Resv handle from TC		*/
	u_char		 tcs_flags;
	u_char		 tcs_unused;
} TCSB;

#define Fhandle		fst_info


/*
 *	PSB:	 Path State Block, per-sender
 *
 *		PSBs are chained off d_PSB_list head in session block.
 *		They are chained in order by sender IP address within
 *		a given PHOP address.
 */
typedef struct sender {
	struct sender	*ps_next;	/* next PSB for same session	*/
	SENDER_TEMPLATE *ps_templ;	/* Sender template		*/
	SENDER_TSPEC	*ps_tspec;	/* Sender Tspec			*/
	RSVP_HOP	 ps_rsvp_phop; 	/* Previous hop			*/
	ADSPEC		*ps_adspec;	/* OPWA advertising spec	*/
	u_char		 ps_ip_ttl;	/* IP TTL from Path msg		*/
	u_char		 ps_unused1;
	u_char           ps_originvif;	/* Origin vif if sender is API	*/
	char		 ps_in_if;	/* IncInterface from routing	*/
	bitmap           ps_outif_list;	/* bitmap => OutInterface_list	*/

	u_char		 ps_flags;
#define PSBF_NonRSVP    0x01		/* Non-RSVP hop experienced	*/
#define PSBF_E_Police	0x04    	/* Entry policing for sender	*/
#define PSBF_LocalOnly	0x08		/* Can only match local Resv	*/
#define PSBF_UDP	0x10		/* This sender needs UDP encaps.*/
#define PSBF_Prefr_need 0x20		/* (Path) Refresh Needed	*/
#define PSBF_Rrefr_need 0x40		/* (Resv) Refresh Needed	*/
#define PSBF_InScope	0x80		/* Used to form union of scopes */

	u_char		 ps_unused2;
	u_short          ps_rsrr_flags;	/* RSRR flags			*/
#define PSBF_RSRR_NOTIFY 0x01		/* RSRR Notification flag	*/

	u_int32_t	 ps_ttd;	/* Time-to-die for the state	*/
	Fobject		*ps_UnkObjList;	/* List: unknown objects to fwd	*/
	Fobject		*ps_Frpolicy;	/* POLICY_DATA object ptr	*/
	struct sender	*ps_1stphop;	/* Ptr to first sender for PHOP	*/
	FLOWSPEC	*ps_resv_spec;	/* Last Resv refresh flowspec to*/
					/* this sender/phop		*/
		/* For wildcard resv, ps_resv_spec is  defined only when 
		 * ps==ps_1stphop
		 */
	/*
	 *	Following fields are effectively a BSB
	 *		(Blockade State Block)
	 */
	FLOWSPEC	*ps_BSB_Qb;	/* Blockade flowspec		*/
	int		 ps_BSB_Tb;	/* Blockade timer = # R cycles	*/	
#ifdef _MIB
	void		*mib_senderhandle; /* opaque MIB handle for this sender*/
#endif
} PSB;

#define ps_phop	ps_rsvp_phop.hop4_addr
#define ps_LIH	ps_rsvp_phop.hop4_LIH
#define IsAPI(psbp) (psbp->ps_phop.s_addr == INADDR_ANY)


/*
 *	Session:	Per-session internal data structure
 */
typedef struct Dest {
	struct Dest	*d_next;	/* next dest			*/
	SESSION		*d_session;	/* SESSION => DestAddr, DstPort	*/
	u_char		 d_flags;	/* flags for the dest		*/
#define SESSF_HaveScope	0x01		/* Scope union has been computed*/

	struct d_refr_vals {
		TIME_VALUES	d_timeval; /* Time values for sending */
		int		d_fast_n;  /* # fast refreshes left to send */
		/* Can add other values here for controlling adaptation of R */
		}	   d_refr_p;	/* For path state */
	struct d_refr_vals d_refr_r;	/* For resv state */
	bitmap		 d_r_incifs;	/* Incoming ifaces for Resv refresh*/
	PSB		*d_fPSB;	/* Ptr to forwarding PSB, if any */
	PSB		*d_PSB_list;	/* List of senders (path state) */
	RSB		*d_RSB_list;	/* List of RSB's -- requests    */
	TCSB	       **d_TCSB_listv;	/* Vector of per-OI TCSB lists 	*/
#ifdef _MIB
	void		*mib_sessionhandle; /* opaque MIB handle for this session */
#endif
} Session;

#define d_addr	 d_session->sess4_addr
#define d_port	 d_session->sess4_port
#define d_protid d_session->sess4_prot
#define d_timevalp d_refr_p.d_timeval
#define d_timevalr d_refr_r.d_timeval
#define d_Rtimop d_timevalp.timev_R
#define d_Rtimor d_timevalr.timev_R

	
/*
 *	Define map vector
 *
 *	Used to construct a map of objects in a packet.  When a packet
 *	is received, a commmon routine parses it (and converts its
 *	byte order if needed), and builds a map vector with
 *	pointers to the objects; unused pointers are NULL.  The
 *	parse depends upon the message type.
 *
 *	To send a packet, rsvpd first builds a map pointing to the
 *	required objects.  Then a common routine builds the packet
 *	(converting the byte order if needed) from the map.
 */

/*	First define elements of lists within map vector
 */
typedef struct {	/* For Path messages */
	SENDER_TEMPLATE	*rsvp_stempl;
	SENDER_TSPEC	*rsvp_stspec;
	ADSPEC		*rsvp_adspec;
}    SenderDesc;

typedef struct {	/* For Resv messages */
	FLOWSPEC	*rsvp_specp;		/* Flowspec */
	FILTER_SPEC	*rsvp_filtp;		/* Filter spec */
}    FlowDesc;

/*
 *	Now define map vector itself.  This vector is ordered to match
 *	the order of objects according to the Functional Spec document.
 */
#define MAX_LIST	100			/* For in-stack allocation */
typedef struct {
	INTEGRITY	*rsvp_integrity;
	SESSION		*rsvp_session;
	SESSION_GROUP	*rsvp_sess_grp;		/* (OBSOLETE) */
	RSVP_HOP	*rsvp_hop;
	TIME_VALUES	*rsvp_timev;
	ERROR_SPEC	*rsvp_errspec;
	Fobject		*rsvp_dpolicy;		/* Policy data */
	CONFIRM		*rsvp_confirm;
	SCOPE		*rsvp_scope_list;
	STYLE		*rsvp_style;
	int		 rsvp_msgtype;		/* in your face... */
	int		 rsvp_flags;
	Fobject		*rsvp_UnkObjList;	/* list of unknown objects */
	/*
	 *    Variable-length list, with rsvp_nlist entries
	 */
	int		 rsvp_nlist;	/* Count of entries in rsvp_list[] */
	union	{
	    SenderDesc Sender_list;	/* For Path-like messages */
	    FlowDesc   Resv_list;	/* For Resv-like messages */
	} 		 rsvp_list[1];
}  packet_map;

#define Map_Length(n) (sizeof(packet_map) + (n-1)*sizeof(FlowDesc))

/*
 *	Structure for handling, parsing, and building an RSVP message.
 *
 *		For sending, area of pkt_offset bytes is reserved at
 *		beginning of buffer for building IP and/or UDP header.
 *
 */
struct packet {
        common_header	*pkt_data;	/* -> RSVP msg in buffer	*/
        int		 pkt_len;	/* Size of packet/buffer	*/
	packet_map	*pkt_map;	/* -> packet map		*/
	u_short		 pkt_offset;	/* pkt_data - start_of_buff	*/
	u_char		 pkt_flags;	/* Flags			*/
#define PKTFLG_USE_UDP	0x01	/* Send as UDP-encap'd			*/
#define PKTFLG_RAW_UDP  0x02	/* Send UDP-encap as raw packet		*/
#define PKTFLG_SET_SRC	0x04	/* Build our own IP hdr with src	*/
#define PKTFLG_Send_RA	0x08	/* Send Router Alert option		*/

	u_char		 pkt_ttl;	/* IP TTL */
#ifdef _MIB
	char		pkt_in_if;	/* incoming interface		*/
#endif
        enum byteorder { BO_NET, BO_HOST } pkt_order;  /* XXX use flags */
};

#define FORCE_HOST_ORDER(packet) assert((packet)->pkt_order == BO_HOST)
#define FORCE_NET_ORDER(packet)  assert((packet)->pkt_order == BO_NET)

/*
 *	Define packet buffer and packet area
 */
typedef struct {
	common_header	rsvp_common_hdr;
	char            rsvp_objects[MAX_PKT];
}   rsvp_g;


typedef struct {
	struct packet	pa_packet_struct;	/* packet structure */	
	packet_map	pa_packet_map;		/* base packet map */
	FlowDesc	pa_packet_fd[MAX_FLWDS-1];
	rsvp_g		pa_packet_buff;		/* packet buffer */
}   packet_area;

/*
 *	Pointers for finding things in RSVP packets,
 *		 relative to struct packet *
 */
#define rsvp_R		pkt_map->rsvp_timev->timev_R
#define rsvp_sess	pkt_map->rsvp_session
#define rsvp_destaddr	pkt_map->rsvp_session->sess4_addr
#define rsvp_dstport	pkt_map->rsvp_session->sess4_port
#define rsvp_protid	pkt_map->rsvp_session->sess4_prot
#define rsvp_sflags	pkt_map->rsvp_session->sess4_flgs
#define rsvp_phop	pkt_map->rsvp_hop->hop4_addr
#define rsvp_nhop	pkt_map->rsvp_hop->hop4_addr
#define rsvp_LIH	pkt_map->rsvp_hop->hop4_LIH
#define rsvp_nflwd	pkt_map->rsvp_nlist
#define rsvp_errvalue	pkt_map->rsvp_errspec->errspec4_value
#define rsvp_errcode	pkt_map->rsvp_errspec->errspec4_code
#define rsvp_errnode	pkt_map->rsvp_errspec->errspec4_enode
#define rsvp_errflags	pkt_map->rsvp_errspec->errspec4_flags
#define rsvp_scope	pkt_map->rsvp_scope_list
#define rsvp_confrcvr	pkt_map->rsvp_confirm->conf_u.conf_ipv4.recv_ipaddr

#define Resv_pkt	struct packet	/* Resv message */
#define Path_pkt	struct packet	/* Path message */	
#define RTear_pkt	struct packet	/* ResvTear message */
#define PTear_pkt	struct packet	/* PathTear message */
#define RErr_pkt	struct packet	/* ResvErr message */
#define PErr_pkt	struct packet	/* PathErr message */
#define RConf_pkt	struct packet	/* ResvConf message */


#define SenderDesc_of(x)	&(x)->pkt_map->rsvp_list[0].Sender_list
#define SenderDesc_size(sdscp)	Object_Size(sdscp->rsvp_stempl) + \
				Object_Size(sdscp->rsvp_stspec) + \
				Object_Size(sdscp->rsvp_adspec)
#define STempl_of(sdscp)	((FILTER_SPEC *)((sdscp)->rsvp_stempl))
#define FlowDesc_of(x, i)	&(x)->pkt_map->rsvp_list[i].Resv_list
#define FlowDesc_size(flwdp)	Object_Size(flwdp->rsvp_specp) +\
				Object_Size(flwdp->rsvp_filtp)

#define rflows(x)		FlowDesc_of(x, 0)
#define filter_of(flwdp)	(flwdp)->rsvp_filtp
					
#define spec_of(flwdp)		(flwdp)->rsvp_specp

#define Style_of(x)		(x)->pkt_map->rsvp_style
#define Style(x)		(Style_of(x))->style_word

/* 
 *	Per-API-client-session internal datastructure
 */
typedef struct {
	struct packet	api_p_packet;	/* Packet struct for Path msg */
	struct packet 	api_r_packet;	/* Packet struct for Resv msg */
	int             api_fd;		/* file desc for Unix socket */
	int		api_pid;	/* Client process id */
	int		api_a_sid;	/* Client session id */
	u_char		api_flags;	/* Send/receive flags */
	u_char		api_protid;	/* Protocol Id	*/
	struct sockaddr_in api_dest;	/* Destination addr, port */
}  api_rec;


/*
 *	Per-phyint internal data structure
 */
typedef struct {
	char         if_name[IFNAMSIZ];	/* Interface name		*/
	u_char          if_up;		/* Boolean: Traffic Control up	*/
	u_char		if_flags;
#define IF_FLAG_IFF_UP	0x01		/* Iface up (Must match IFF_UP)	*/
#define IF_FLAG_UseUDP	0x10		/* Interface uses UDP encaps	*/
#define IF_FLAG_Police  0x20		/* Police all data rcvd on iface*/
#define IF_FLAG_Intgrty	0x40		/* INTEGRITY required		*/
#define IF_FLAG_Disable	0x80		/* Disable RSVP			*/
#define IF_FLAG_PassNormal 0x02		/* Bypass TC, process RSVP msg. normally */
#define IF_FLAG_PassBreak 0x04          /* Bypass TC, but set break bit */
#define IF_FLAG_Pass (IF_FLAG_PassNormal | IF_FLAG_PassBreak)
	u_char		if_udpttl;	/* TTL for sending UDP encaps	*/
	struct in_addr  if_orig;	/* Interface IP address		*/
	u_int32_t	if_Rdefault;	/* Default refresh rate R to snd*/
#ifdef _MIB
	int		if_index;	/* index for some MIB tables	*/
	uint		if_numflows;	/* through this if		*/
	int		if_numbufs;	/* num bufs needed to handle max burst */
#endif
/*
 *	Followinq values are used by TC interface module to update general
 *	path characteristics.  This is not strictly RSVP...
 */
	float32_t	if_path_bw;	/* Path bandwidth		*/
	u_int32_t	if_min_latency;	/* Min latency			*/
	u_int32_t	if_path_mtu;	/* XXX temporary		*/

#ifdef STATS
	RSVPstat	if_stats;	/* Statistics table		*/
#endif
}   if_rec;

#define vif_flags(v)	if_vec[vif_toif[(v)]].if_flags
#define vif_name(v)	((v<if_num)?if_vec[v].if_name:(v<vif_num)?"vif":"API")
#ifdef STATS
#define Incr_ifstats(i, val)	if_vec[i].if_stats.##val++;
#else
#define Incr_ifstats(i, val)
#endif


/*
 *	Timer event type codes
 */
#define TIMEV_PATH		1	/* Generate Path refresh */
#define TIMEV_RESV		2	/* Generate Resv refresh */
#define TIMEV_API		3	/* Generate API refresh */
#define TIMEV_LocalRepair	4	/* Path local repair delay */
#ifdef RTAP
#define TIMEV_RTAPsleep		5	/* RTAP sleep command	*/
#define TIMEV_RTAPdata		6	/* RTAP send-data time  */
#endif

/*
 *	INTEGRITY Key Association table entry
 */
typedef	struct {
	u_int32_t	kas_keyid;	/* Key id			*/
	u_int32_t	kas_seqno;	/* Last seq # received		*/
	struct in_addr	kas_sender;	/* Sender address or 0		*/
	char		kas_if;		/* Sending interface | -1	*/
	char		kas_algno;	/* Algorithm number		*/
#define INTGR_MD5	1

	char		kas_unused;
	char		kas_keylen;	/* Key length (0=> empty entry) */
	u_char		kas_key[MD5_LENG];
}  KEY_ASSOC;

/*
 *	Define logging levels (These are taken from syslog.h; repeated here
 *	for convenience).
 */
#define LOG_CRIT	2	/* critical conditions */
#define LOG_ERR		3	/* error conditions */
#define LOG_WARNING	4	/* warning conditions */
#define LOG_NOTICE	5	/* normal but signification condition */
#define LOG_INFO	6	/* informational */
#define LOG_DEBUG	7	/* debug-level messages */
#define LOG_HEXD	8	/* include hex dumps of interface events */

/*	Debug Mask bits
 */
#define DEBUG_IO		0x01	/* Log packets sent & received    */
#define DEBUG_DS_DUMP		0x02	/* Dump data structures           */
#define DEBUG_EVENTS		0x04	/* Log API, kernel reservation events */
#define DEBUG_ROUTE		0x08	/* Log routes                     */
#define DEBUG_RSRR		0x08	/* Log multicast routes           */
#define DEBUG_MCAST_STATE	0x10	/* Multicast RSVP state (to dmap) */
#define DEBUG_TIMERS		0x20	/* Timer events */
#define DEBUG_ALL		0xff

#define LOG_ALWAYS	0	/* special value for log severity level */

/* 	Defaults for logging controls
 */
#ifdef ISI_TEST
#define  DEFAULT_DEBUG_MASK \
		DEBUG_EVENTS | DEBUG_MCAST_STATE | DEBUG_DS_DUMP | DEBUG_IO	
#define  DEFAULT_LOGGING_LEVEL	LOG_DEBUG
#else
#define  DEFAULT_DEBUG_MASK	0	
#define  DEFAULT_LOGGING_LEVEL	LOG_INFO
#endif /* ISI_TEST */

/*	Debugging events
 */
enum	debug_events {	/* Must match Log_Event_Types[] in rsvp_debug.c */
		LOGEV_Recv_UDP, LOGEV_Recv_Raw, LOGEV_Recv_API,
		LOGEV_Send_UDP, LOGEV_Send_Raw, LOGEV_Send_RandU,
		LOGEV_API_regi, LOGEV_API_resv, LOGEV_API_close,
		LOGEV_API_upcall, LOGEV_API_debug, LOGEV_API_stat,
		LOGEV_TC_addflow, LOGEV_TC_modflow, LOGEV_TC_delflow, 
		LOGEV_TC_addfilt, LOGEV_TC_delfilt,
		LOGEV_ignore,	LOGEV_iserr
	};


/*
 *	Packet parse internal error codes
 */
#define PKT_OK		   0
#define PKT_ERR_LENGTH	  -1	/* Len in hdr != real packet length	*/
#define PKT_ERR_VERSION   -2	/* Unknown version			*/
#define PKT_ERR_CKSUM	  -3	/* Bad checksum				*/
#define PKT_ERR_INTEGRITY -4	/* INTEGRITY bad or missing		*/
#define PKT_ERR_NOINTASS  -5	/* INTEGRITY association unknown	*/
#define PKT_ERR_MSGTYPE   -6	/* Unknown message type			*/
#define PKT_ERR_BADOBJ    -7	/* Object should not appear in msg type */
#define PKT_ERR_MISSOBJ   -8	/* Missing required object 		*/
#define PKT_ERR_OBJLEN	  -9	/* Illegal object length		*/
#define PKT_ERR_UNKCLASS -10	/* Unknown object class			*/
#define PKT_ERR_ORDER    -11	/* Violation of required object order 	*/
#define PKT_ERR_NOSESS	 -12	/* Missing SESSION object 		*/
#define PKT_ERR_NOSTYLE  -13    /* Missing STYLE 			*/
#define PKT_ERR_BADSTYL  -14	/* Unknown style id 			*/
#define PKT_ERR_NOFILT   -14	/* Missing FILTER_SPEC			*/
#define PKT_ERR_NUMFLOW  -16	/* Wrong # flow descriptors 		*/
#define PKT_ERR_BADLIH   -17	/* Logical Interface Handle out of range*/
#define PKT_ERR_BADSPORT -18	/* SrcPort ! = 0 but DstPort == 0	*/
#define PKT_ERR_HOPZERO	 -19	/* N/PHOP IP addr is zero		*/
#define PKT_ERR_TTLZERO	 -20	/* Path message had TTL = 0		*/
#define PKT_ERR_REPLAY	 -21	/* INTEGRITY seq# replayed		*/


/*
 *	XXX should be in rsvp_mac.h
 */

#define IsPath(x) ((x)->rsvp_msgtype == RSVP_PATH||\
		(x)->rsvp_msgtype == RSVP_PATH_TEAR ||\
		(x)->rsvp_msgtype == RSVP_PATH_ERR)

#define IsResv(x) (!IsPath(x))	/* May not always be true.. */

#endif	/* __rsvp_var_h__ */


