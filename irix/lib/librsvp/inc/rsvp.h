
/*
 * @(#) $Id: rsvp.h,v 1.7 1998/11/25 08:43:36 eddiem Exp $
 */

/***************************** rsvp.h ********************************
 *                                                                   *
 *          Define RSVP protocol  -- packet formats, well-known      *
 *          		values, error types, ...                     *
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

#ifndef __rsvp_h__
#define __rsvp_h__

#ifndef _RSVP_TYPES_H_
#include "rsvp_types.h"
#endif

#define RSVP_VERSION	1		/* The packet format version */

/*
 *  Well-known multicast groups and UDP port for UDP encapsulation.
 *		  (*unofficial*)
 */
#define RSVP_ENCAP_GROUP	"224.0.0.14"	/* RSVP_ENCAPSULATION	*/
#define RSVP_ENCAP_PORT		1698		/* Pu  (rsvp-encap1)	*/
#define RSVP_ENCAP_PORTP	1699		/* Pu' (rsvp-encap2)	*/
#define RSVP_DFLT_PROTID	IPPROTO_UDP

/*
 *	Common header of RSVP messages.
 */
typedef struct {
	u_char		rsvp_verflags;	/* version and common flags */
	u_char		rsvp_type;	/* message type (defined above) */
	u_int16_t       rsvp_cksum;	/* checksum             */
	u_char		rsvp_snd_TTL;	/* Send TTL		*/
	u_char		rsvp_unused;	/* Reserved octet	*/
	int16_t		rsvp_length;	/* Message length in bytes */
}   common_header;

/* RSVP message types */

#define RSVP_PATH	1
#define RSVP_RESV	2
#define RSVP_PATH_ERR	3
#define RSVP_RESV_ERR	4
#define RSVP_PATH_TEAR	5
#define RSVP_RESV_TEAR	6
#define RSVP_CONFIRM	7
#define RSVP_REPORT	8
#define RSVP_DREQ       9
#define RSVP_DREP       10
#define RSVP_MAX_MSGTYPE 10
	
/*
 *	Useful macros for common header
 */
#define RSVP_VERSION_OF(x)	(((x)->rsvp_verflags & 0xf0) >> 4)
#define RSVP_FLAGS_OF(x)	((x)->rsvp_verflags & 0x0f)
#define RSVP_TYPE_OF(x)		((x)->rsvp_type)
#define RSVP_MAKE_VERFLAGS(v, f) ((((v) & 0x0f) << 4) | ((f) & 0x0f))


/*
 *	Standard format of an object header
 */
typedef struct {
	int16_t		obj_length;	/* Length in bytes */
	u_char		obj_class;	/* Class (values defined below) */
	u_char		obj_ctype;	/* C-Type (values defined below) */
}    Object_header;

#define Obj_Length(x)	((Object_header *)x)->obj_length
#define Obj_CType(x)	((Object_header *)x)->obj_ctype
#define Obj_Class(x)	((Object_header *)x)->obj_class
#define Obj_data(x)	((Object_header *)(x)+1)

/*
 *	Define object classes: Class-Num values
 */
#define class_NULL		0
#define class_SESSION		1
#define class_SESSION_GROUP	2
#define class_RSVP_HOP		3
#define class_INTEGRITY		4
#define class_TIME_VALUES	5
#define class_ERROR_SPEC	6
#define class_SCOPE		7
#define class_STYLE		8
#define class_FLOWSPEC		9
#define class_FILTER_SPEC	10
#define class_SENDER_TEMPLATE	11
#define class_SENDER_TSPEC	12
#define class_ADSPEC		13
#define class_POLICY_DATA	14
#define class_CONFIRM		15
#define class_DIAGNOSTIC        30   /* Diagnostic header object */
#define class_ROUTE             31   /* Record route for diagnostic msgs */
#define class_DIAG_RESPONSE     32   /* Diagnostic response object */
#define class_MAX               32

/*
 *	Define high bits of Class_num for handling unknown class
 *		00, 01: Reject and send error
 *		10:  	Ignore, do not forward or send error
 *		11:	Ignore but forward.
 */
#define UNKN_CLASS_MASK		0xc0
	

/********************************************************************
 *
 *			Define object formats
 *
 ********************************************************************/

/*
 *	SESSION object class
 */
#define ctype_SESSION_ipv4	1
#define ctype_SESSION_ipv6	2
#define ctype_SESSION_ipv4GPI	3	/* IPSEC: Generalized Port Id */
#define ctype_SESSION_ipv6GPI	4	/* IPSEC: Generalized Port Id */

typedef struct {
	struct in_addr	sess_destaddr;	/* DestAddress			*/

	u_char		sess_protid;	/* Protocol Id			*/
	u_char          sess_flags;
#define SESSFLG_E_Police 	0x01	/* E_Police: Entry policing flag*/

	u_int16_t	sess_destport;	/* DestPort			*/
}    Session_IPv4;

/*	GPI versions have virtual dest port instead of dest port; this
 *	changes the interpretation but not the format, so we do not
 *	define new structs for GPI.
 */

typedef struct {
	Object_header	sess_header;
	union {
		Session_IPv4	sess_ipv4;
/*      	Session_IPv6	sess_ipv6;		*/
	}		sess_u;
}    SESSION;

#define sess4_addr	sess_u.sess_ipv4.sess_destaddr
#define sess4_port	sess_u.sess_ipv4.sess_destport
#define sess4_prot	sess_u.sess_ipv4.sess_protid
#define sess4_flgs	sess_u.sess_ipv4.sess_flags

/*
 *	SESSION_GROUP object class [TBD]
 */
typedef struct {
	int sg_fill;	/* filler for certain compilers */
}    SESSION_GROUP;


/*
 *	RSVP_HOP object class
 */
#define ctype_RSVP_HOP_ipv4	1
#define ctype_RSVP_HOP_ipv6	2

typedef struct {
	struct in_addr	hop_ipaddr;	/* Next/Previous Hop Address */
	u_int32_t	hop_LIH;	/* Logical Interface Handle */
}    Rsvp_Hop_IPv4;

typedef struct {
	Object_header	hop_header;
	union {
		Rsvp_Hop_IPv4	hop_ipv4;
/*      	Rsvp_hop_IPv6	hop_ipv6; */
	}		hop_u;
}    RSVP_HOP;

#define hop4_LIH	hop_u.hop_ipv4.hop_LIH
#define hop4_addr	hop_u.hop_ipv4.hop_ipaddr

/*
 *	TIME_VALUES class
 */
#define ctype_TIME_VALUES	1

typedef struct {
	Object_header	timev_header; 
	u_int32_t	timev_R;	/* R = Refresh Period in ms. */
}   TIME_VALUES;

/*
 *	STYLE object class
 */
typedef u_int32_t	style_t;
#define ctype_STYLE	1

typedef struct {
	Object_header	style_header;
	style_t		style_word;
	}    STYLE;


/*	Define values for option vector
 */
#define Opt_Share_mask  0x00000018	/* 2 bits: Sharing control	*/
#define Opt_Distinct	0x00000008	/* Distinct reservations	*/
#define Opt_Shared	0x00000010	/* Shared reservations		*/

#define Opt_SndSel_mask	0x00000007	/* 3 bits: Sender selection	*/
#define Opt_Wildcard	0x00000001	/* Wildcard scope		*/
#define Opt_Explicit	0x00000002	/* Explicit scope		*/

#define Style_is_Wildcard(p) (((p)&Opt_SndSel_mask) == Opt_Wildcard)
#define Style_is_Shared(p) (((p)&Opt_Share_mask) == Opt_Shared)

/*	Define style values
 */
#define STYLE_WF	Opt_Shared + Opt_Wildcard
#define STYLE_FF	Opt_Distinct + Opt_Explicit
#define STYLE_SE	Opt_Shared + Opt_Explicit

/*	Define some historical style symbols
 */
#define WILDCARD	STYLE_WF
#define FIXED		STYLE_FF

/*
 *	FILTER SPEC object class
 */
#define ctype_FILTER_SPEC_ipv4	  1	/* IPv4 FILTER_SPEC		*/
#define ctype_FILTER_SPEC_ipv6	  2	/* IP6 FILTER_SPEC		*/
#define ctype_FILTER_SPEC_ipv6FL  3	/* IP6 Flow-label FILTER_SPEC	*/
#define ctype_FILTER_SPEC_ipv4GPI 4	/* IPv4/GPI FILTER_SPEC		*/
#define ctype_FILTER_SPEC_ipv6GPI 5	/* IPv6/GPI FILTER_SPEC 	*/

typedef struct {
	struct in_addr	filt_ipaddr;	/* IPv4 SrcAddress		*/
	u_int16_t	filt_unused;
	u_int16_t	filt_port;	/* SrcPort			*/
}    Filter_Spec_IPv4;

typedef struct {
	struct in_addr	filt_ipaddr;	/* IPv4 SrcAddress		*/
	u_int32_t	filt_gpi;	/* Generalized Port Id		*/
}    Filter_Spec_IPv4GPI;

typedef struct {
	Object_header	filt_header;
	union {
		Filter_Spec_IPv4	filt_ipv4;
		Filter_Spec_IPv4GPI	filt_ipv4gpi;
/*		Filter_Spec_IPv6	filt_ipv6; 	*/
/*		Filter_Spec_IPv6FL	filt_ipv6fl; 	*/
/*		Filter_Spec_IPv6GPI	filt_ipv6gpi; 	*/
	}		filt_u;
}    FILTER_SPEC;

#define filt_srcaddr	filt_u.filt_ipv4.filt_ipaddr
#define filt_srcport	filt_u.filt_ipv4.filt_port

/*
 *	SENDER_TEMPLATE class objects
 */
#define ctype_SENDER_TEMPLATE_ipv4	1   /* IPv4 SENDER_TEMPLATE	*/
#define ctype_SENDER_TEMPLATE_ipv6	2   /* IPv6 SENDER_TEMPLATE	*/
#define ctype_SENDER_TEMPLATE_ipv6FL	3   /* IPv6 Flow-label SNDR_TEMPL */
#define ctype_SENDER_TEMPLATE_ipv4GPI	4   /* IPv4/GPI SENDER_TEMPLATE */
#define ctype_SENDER_TEMPLATE_ipv6GPI	5   /* IPv6/GPI SENDER_TEMPLATE */

typedef FILTER_SPEC  SENDER_TEMPLATE;	/* Identical to FILTER_SPEC */


/*
 *	ERROR_SPEC object class
 */
#define ctype_ERROR_SPEC_ipv4	1
#define ctype_ERROR_SPEC_ipv6	2

typedef struct {
	struct in_addr	errs_errnode;	/* Error Node Address		*/
	u_char		errs_flags;	/* Flags:			*/
#define ERROR_SPECF_InPlace	0x01	/*   Left resv in place		*/
#define ERROR_SPECF_NotGuilty	0x02	/*   This rcvr not guilty	*/

	u_char		errs_code;	/* Error Code (def'd below) */
	u_int16_t	errs_value;	/* Error Value		*/
#define ERR_FORWARD_OK  0x8000		/* Flag: OK to forward state */
#define Error_Usage(x)	(((x)>>12)&3)
#define ERR_Usage_globl 0x00		/* Globally-defined sub-code */
#define ERR_Usage_local 0x10		/* Locally-defined sub-code */
#define ERR_Usage_serv	0x11		/* Service-defined sub-code */
#define ERR_global_mask 0x0fff		/* Sub-code bits in Error Val */

}    Error_Spec_IPv4;


typedef struct {
	Object_header	errs_header;
	union {
		Error_Spec_IPv4	errs_ipv4;
/*		Error_Spec_IP6	errs_ipv6;	*/
	}		errs_u;
}    ERROR_SPEC;

#define errspec4_enode	errs_u.errs_ipv4.errs_errnode
#define errspec4_code	errs_u.errs_ipv4.errs_code
#define errspec4_value	errs_u.errs_ipv4.errs_value
#define errspec4_flags	errs_u.errs_ipv4.errs_flags

/*
 *	SCOPE object class
 */
#define ctype_SCOPE_list_ipv4		1
#define ctype_SCOPE_list_ipv6		2

typedef struct {
	struct in_addr	scopl_ipaddr[1];  /* var-len list of IP sender addrs */
}    Scope_list_ipv4;

typedef struct {
	Object_header	scopl_header;
	union {
		Scope_list_ipv4	scopl_ipv4;
/*     		Scope_List_ipv6	scopl_ipv6; */
	}		scope_u;
}    SCOPE;

#define scope4_addr	scope_u.scopl_ipv4.scopl_ipaddr
#define Scope_Cnt(scp)  ((Obj_Length(scp)-sizeof(Object_header))/sizeof(struct in_addr))
#define Scope_Len(cnt)  (cnt*sizeof(struct in_addr)+sizeof(Object_header))

/*
 *	INTEGRITY object class
 *
 */
#define ctype_INTEGRITY_MD5_ipv4	1
#define ctype_INTEGRITY_MD5_ipv6	2

#define MD5_LENG	16		/* Length of MD5 digest		*/

typedef struct {
	u_int32_t	intgr_keyid;  	/* Key Id			*/
	struct in_addr	intgr_sender;	/* Sending System Address	*/
	u_int32_t	intgr_seqno;	/* Sequence #: avoid replay	*/
	u_char	intgr_digest[MD5_LENG];	/* MD5 Digest			*/
}  Integrity_MD5_IPv4;

typedef struct {
        Object_header	intgr_header;
	union {
		Integrity_MD5_IPv4	intgr_MD5_ipv4;
/*		Integrity_MD5_IPv6	intgr_MD5_ipv6;	*/
	}   intgr_u;
 }    INTEGRITY;

#define intgr4_keyid	intgr_u.intgr_MD5_ipv4.intgr_keyid
#define intgr4_sender	intgr_u.intgr_MD5_ipv4.intgr_sender
#define intgr4_seqno	intgr_u.intgr_MD5_ipv4.intgr_seqno
#define intgr4_digest	intgr_u.intgr_MD5_ipv4.intgr_digest

/*
 *	CONFIRM object class
 */
#define ctype_CONFIRM_ipv4	1
#define ctype_CONFIRM_ipv6	2

typedef struct {
	struct in_addr	recv_ipaddr;	/* Receiver requesting confirm'n */
}    Rsvp_confirm_IPv4;

typedef struct {
	Object_header	conf_header;
	union {
		Rsvp_confirm_IPv4	conf_ipv4;
/*      	Rsvp_confirm_IPv6	conf_ipv6; */
	}		conf_u;
}    CONFIRM;

#define conf4_addr	conf_u.conf_ipv4.recv_ipaddr

/*
 *	FLOWSPEC class object
 *
 *		Opaque to RSVP -- Contents defined in rapi_lib.h
 */
#define ctype_FLOWSPEC_Intserv0  2	/* The int-serv flowspec (v.0)*/
#include "rapi_lib.h"

typedef struct {
	Object_header	flow_header;
	IS_specbody_t   flow_body;	/* Defined in rapi_lib.h */
}    FLOWSPEC;


/*
 *	SENDER_TSPEC class object
 *
 *		Opaque to RSVP -- Contents defined in rapi_lib.h
 */
#define ctype_SENDER_TSPEC	2

typedef struct {
	Object_header	stspec_header;
	IS_tspbody_t	stspec_body;	/* Defined in rapi_lib.h */
}    SENDER_TSPEC;

/*
 *	ADSPEC class object
 *
 *		Opaque to RSVP -- Contents defined in rapi_lib.h
 */
#define ctype_ADSPEC_INTSERV	2
typedef struct {
	Object_header	adspec_header;
	IS_adsbody_t	adspec_body;	/* Defined in rapi_lib.h */
}    ADSPEC;


/*
 *	POLICY_DATA object class
 *
 *		Opaque RSVP -- Contents will be defined elsewhere
 */
#define ctype_POLICY_DATA	1

typedef struct {
	Object_header	policy_d_header;
}    POLICY_DATA;

/* Objects for diagnostics support 
 * refer internet draft specification 
 */

/*
 *	ROUTE Object class (Object used in Diagnostic messages)
 */
#define	ctype_ROUTE_ipv4	1
#define ctype_ROUTE_ip6		2

typedef struct {
	struct in_addr route_addr_list[1]; /* Var len address list */
} Route_IPv4;

typedef struct {
	Object_header	route_header;
	u_int16_t	reserved;
	u_char		reserved2;
	u_char		R_pointer;
	union {
		Route_IPv4 route_ipv4;
	/*	Route_IP6 route_ip6;	*/	
	} route_u;
} ROUTE;

#define route(x, i)	(x)->route_u.route_ipv4.route_addr_list[i]

/*
 *	DIAGNOSTIC RESPONSE (DREP) Object Class
 */
#define ctype_DIAG_RESPONSE_ipv4	1
#define ctype_DIAG_RESPONSE_ip6		2

typedef struct {
	u_int32_t	resp_arrival_time;	/* DREQ arrival time	*/
	struct in_addr	resp_inc_addr;		/* addr of incoming-iface */
	struct in_addr  resp_outg_addr;		/* addr of outgoing iface */
	struct in_addr  resp_prev_addr;		/* addr of previous hop	*/
						/* RSVP router		*/
	style_t		resp_resv_style;        /* reservation style	*/
	u_char		resp_D_TTL;		/* rsvp_send_ttl - ip_ttl */
						/* for this hop		*/
	u_char		resp_R_error;		/* M + error mesg + K value */
	u_int16_t	resp_timer;		/* local refresh timer value */
	SENDER_TSPEC	resp_tspecp;		/* tspec of diagnosed sender */
	FILTER_SPEC	resp_filter;		/* filter spec,		*/
	FLOWSPEC	resp_flow;		/* Flowspec, reservations */
} Response_IPv4;

typedef struct {
	Object_header	resp_header;
	union {
	Response_IPv4	resp_ipv4;
/*	Response_IP6	resp_ip6; */
	} resp_u;
} DIAG_RESPONSE;

#define resp_rstyle		resp_u.resp_ipv4.resp_resv_style
#define resp_rflow		resp_u.resp_ipv4.resp_flow
#define resp_filtp		resp_u.resp_ipv4.resp_filter
#define resp_tspec		resp_u.resp_ipv4.resp_tspecp
#define resp_in_addr	        resp_u.resp_ipv4.resp_inc_addr
#define resp_out_addr	        resp_u.resp_ipv4.resp_outg_addr
#define resp_pre_addr	        resp_u.resp_ipv4.resp_prev_addr
#define resp_Rerror		resp_u.resp_ipv4.resp_R_error
#define resp_timeval            resp_u.resp_ipv4.resp_timer
#define resp_arrtime	        resp_u.resp_ipv4.resp_arrival_time
#define resp_DTTL		resp_u.resp_ipv4.resp_D_TTL
#define DIAG_RESPONSE_MFLAG(x)	(((x)->resp_Rerror & 0x80) >> 7)
#define DIAG_RESPONSE_RERROR(x)	(((x)->resp_Rerror & 0x70) >> 4)
#define DIAG_RESPONSE_K(x)	((x)->resp_Rerror & 0x0F)
#define DRESP_BASIC_SIZE        sizeof(DIAG_RESPONSE) - sizeof(FLOWSPEC) - \
				sizeof(FILTER_SPEC) - sizeof(SENDER_TSPEC)

/*
 *	DIAGNOSTIC object class -- diagnostic request header 
 */
#define ctype_DIAGNOSTIC_ipv4	1
#define ctype_DIAGNOSTIC_ip6	2

typedef struct {
	u_char		diag_max_hops;	   /* max number of RSVP hops	*/
	u_char		diag_hop_count;	   /* # of RSVP hops 		*/
	u_char		diag_mttl;	   /* multicast TTL		*/
	u_char		diag_reply_mode;   /* the last bit of this field 
					    * indicates how the reply
                                            * should be returned	*/
	u_int16_t	diag_path_MTU;
	u_int16_t	diag_frag_offset;
	u_int32_t	diag_msg_ID;	   /* Unique message ID		*/
	FILTER_SPEC	diag_source_filtp; /* FILTER SPEC, sender host	*/
	struct in_addr 	diag_lasthop_addr; /* IP address of last hop	*/
	struct in_addr 	diag_resp_addr;    /* IP address to send DREP	*/
	} Diagnostic_IPv4;

typedef struct {
	Object_header diag_header;
	union {
		Diagnostic_IPv4	diag_ipv4;
/*		Diagnostic_IP6  diag_ip6;	*/
		} diag_u;
	} DIAGNOSTIC;

#define diag_replymode	        diag_u.diag_ipv4.diag_reply_mode
#define diag_saddr		diag_u.diag_ipv4.diag_source_filtp.filt_srcaddr
#define diag_sfiltp		diag_u.diag_ipv4.diag_source_filtp
#define diag_sport		diag_u.diag_ipv4.diag_source_filtp.filt_srcport
#define diag_msgID		diag_u.diag_ipv4.diag_msg_ID
#define diag_raddr		diag_u.diag_ipv4.diag_resp_addr
#define diag_laddr		diag_u.diag_ipv4.diag_lasthop_addr
#define diag_maxhops		diag_u.diag_ipv4.diag_max_hops
#define diag_ttl		diag_u.diag_ipv4.diag_mttl
#define diag_hopcount		diag_u.diag_ipv4.diag_hop_count
#define diag_pMTU		diag_u.diag_ipv4.diag_path_MTU
#define diag_frag_off		diag_u.diag_ipv4.diag_frag_offset
#define DIAG_MFBIT(x)  		((x)->diag_replymode & 0x01 )
#define DIAG_HBIT(x)  		(((x)->diag_replymode & 0x02) >> 1 )
	
/* End of object definitions for diagnostics */	

#ifdef __MAIN__
/*
 *	Vector of maximum defined C-Type value per Class-Num
 */
int	CType_max[class_MAX+1] = {
		255,
		ctype_SESSION_ipv6GPI,
		0 /* XXX Sess group */,
		ctype_RSVP_HOP_ipv6,
		0 /* XXX Integrity */,
		ctype_TIME_VALUES,
		ctype_ERROR_SPEC_ipv6,
		ctype_SCOPE_list_ipv6,
		ctype_STYLE,
		ctype_FLOWSPEC_Intserv0,
		ctype_FILTER_SPEC_ipv6GPI,
		ctype_SENDER_TEMPLATE_ipv6GPI,
		ctype_SENDER_TSPEC,
		ctype_ADSPEC_INTSERV,
		0 /* XXX POLICY_DATA */,
		ctype_CONFIRM_ipv6,
		0			,	0,
		0			,	0,
		0			,	0,
		0			,	0,
		0			,	0,
		0			,	0,
		0			,	0,
		ctype_DIAGNOSTIC_ip6	,	255, /* ROUTE */
		ctype_DIAG_RESPONSE_ip6
};
#endif 	

/*
 *	Useful definitions
 */
#define FilterSpec	FILTER_SPEC
#define FlowSpec	FLOWSPEC
#define Authentication	POLICY_DATA
#define PolicyData	POLICY_DATA

/*	RSVP error codes
 */
#define RSVP_Err_NONE		0	/* No error (CONFIRM)		*/
#define RSVP_Erv_Nonev		0	/*    No-error Error Value	*/

#define RSVP_Err_ADMISSION	1	/* Admission Control failure	*/
	/* Globally-defined sub-codes: */
#define RSVP_Erv_Other		0	/* Unspecified cause		*/
#define RSVP_Erv_DelayBnd	1	/* Cannot meet delay bound req	*/
#define RSVP_Erv_Bandwidth	2	/* Insufficient bandwidth	*/
#define RSVP_Erv_MTU		3	/* MTU in flowspec too large	*/

#define RSVP_Err_POLICY		2	/* Policy control failure	*/
	/* Globally-defined sub-codes: TBD */

#define RSVP_Err_NO_PATH	3	/* No path state for Resv	*/
#define RSVP_Err_NO_SENDER	4	/* No sender info for Resv	*/
#define RSVP_Err_BAD_STYLE	5	/* Conflicting style		*/
#define RSVP_Err_UNKNOWN_STYLE	6	/* Unknown reservation style	*/
#define RSVP_Err_BAD_DSTPORT	7	/* Conflicting DstPort in Sess	*/
#define RSVP_Err_BAD_SNDPORT	8	/* Conflicting Sender Port	*/

#define RSVP_Err_PREEMPTED	12	/* Service Preempted		*/
#define RSVP_Err_UNKN_OBJ_CLASS	13	/* Unknown object Class-Num	*/
					/*   ErrVal = Class_num, CType	*/
#define RSVP_Err_UNKNOWN_CTYPE	14	/* Unknown object C-Type	*/
					/*   ErrVal = Class_num, CType	*/

#define RSVP_Err_API_ERROR	20	/* API client error		*/
					/*   ErrVal = API error code	*/
#define RSVP_Err_TC_ERROR	21	/* Traffic Control error	*/
#define RSVP_Erv_Conflict_Serv  01	/* Service Conflict		*/
#define RSVP_Erv_No_Serv	02	/* Unknown Service		*/
#define RSVP_Erv_Crazy_Flowspec	03	/* Unreasonable Flowspec	*/
#define RSVP_Erv_Crazy_Tspec	04	/* Unreasonable Tspec		*/

#define RSVP_Err_TC_SYS_ERROR	22	/* Traffic control system error	*/
					/* ErrVal = kernel error code	*/
#define RSVP_Err_RSVP_SYS_ERROR 23	/* RSVSystem error		*/
#define RSVP_Erv_MEMORY		1	/* Out of memory */
#define RSVP_Erv_API		2	/* API logic error */

/* Error values for diagnostics
 *
 */
#define RSVP_Erv_Diag_NOPATH	0x01    /* No PATH for diagnosed sender */
#define	RSVP_Erv_Diag_MTUBIG    0x02	/* DREQ packet size exceeded path MTU */
#define RSVP_Erv_Diag_ROUTEBIG	0x04    /* ROUTE object size causes DREP 
					 * length to exceed path MTU */  

#endif	/* __rsvp_h__ */
 
