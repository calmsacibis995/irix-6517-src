/*
 * @(#) $Id: rapi_lib.h,v 1.12 1998/11/25 08:43:36 eddiem Exp $
 */

/**************************************************************************
 *	 rapi_lib.h -- include file for RSVP API (RAPI)
 *
 *	Definitions of RSVP API (RAPI) library calls.
 *		Matches: draft-ietf-intserv-rsvp-use-01.txt
 *
 *	Also includes rsvp_intserv.h, which contains formats of integrated
 *	services data structures across application program interface.
 *
 **************************************************************************/
/****************************************************************************

            RSVPD -- ReSerVation Protocol Daemon

                USC Information Sciences Institute
                Marina del Rey, California

		Current version by: Bob Braden, August 1996

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

#ifndef __rapilib_h__
#define __rapilib_h__

/* allow C++ apps to use these files also - mwang */
#ifdef __cplusplus
extern "C" {
#endif

/* these files are in the rsvp directory for SGI - mwang */
#include "rsvp/rsvp_types.h"
#include "rsvp/rsvp_intserv.h"
#include <sys/socket.h>
#ifndef NET_IF_H
#define NET_IF_H
#include <net/if.h>
#endif


__BEGIN_DECLS


/**************************************************************************
 **************************************************************************
 **
 **      RAPI (RSVP API) defns for flowspec, Tspec, Adspec begin here...
 **
 **
 **************************************************************************
 **************************************************************************/

/*
 *      Flowspec/Tspec Service types
 */
enum    qos_service_type {
#ifdef OBSOLETE_API
	/* Following are used only for Legacy format			*/
	QOS_TSPEC =		0,	/* Traffic specification	*/
	QOS_CNTR_DELAY =	1,      /* Controlled delay service	*/
	QOS_PREDICTIVE =	2,      /* Predictive service		*/
	QOS_GUARANTEED =	3,      /* Guaranteed service		*/

	/* Following are used only for Extended-Legacy format		*/
	QOS_GUARANTEEDX =	4,	/* (real) Guaranteed service 	*/
	QOS_CNTR_LOAD =		5,	/* Controlled-Load service	*/
	QOS_TSPECX =		6	/* Generic Tspec		*/
#else
	QOS_TSPEC = 		0,	/* Generic Tspec		*/
	QOS_CNTR_LOAD =		1,	/* Controlled-Load service	*/
	QOS_GUARANTEED =	2	/* Guaranteed Service		*/

/* Backwards compatibility */
#define QOS_TSPECX	QOS_TSPEC
#define QOS_GUARANTEEDX	QOS_GUARANTEED

#endif /* OBSOLETE_API */


};

#ifdef OBSOLETE_API   
/*
 *      Legacy Flowspec format
 *
 *	This format was used in experimental and prototype RSVP
 *	implementations (e.g., the 1995 Interop Demo).  It is included
 *	here for backwards compatibility, but will eventually vanish.
 *
 *	To ease the transition, we also define an extended legacy format
 *	to handle current Guaranteed and Controlled-Load specifications.
 */
typedef struct {
        u_int32_t       spec_type;		/* qos_service_type */
        struct {
                u_int32_t       spec_Tspec_b;   /* Token bucket depth (bits) */
                u_int32_t       spec_Tspec_r;   /* Token bucket avg rate (bps)*/
        }               spec_Tspec;
        u_int32_t       spec_d;                 /* Target Delay (ms) */
        u_int32_t       spec_sl;                /* Service Level */
}   qos_flowspec_t;

#define spec_r  spec_Tspec.spec_Tspec_r
#define spec_b  spec_Tspec.spec_Tspec_b
#endif /* OBSOLETE_API */

typedef  struct {
	float32_t       spec_Tspec_b;	/* Token bucket depth (B) */
	float32_t       spec_Tspec_r;	/* Token bucket avg rate (Bps)*/
	float32_t	spec_Tspec_p;	/* Peak data rate (Bps)	*/
	u_int32_t	spec_Tspec_m;	/* Min policed unit (B)	*/
	u_int32_t	spec_Tspec_M;	/* Max pkt size (B)	*/
}    qos_Tspec_body;
            
/*
 *      Simplified-format Flowspec
 *
 *	This structure contains the union of the parameters for
 *	Controlled-Load and Guaranteed service models.
 */
typedef struct {
        u_int32_t       spec_type;		/* qos_service_type */
        qos_Tspec_body  xspec_Tspec;		/* Tspec		*/
        float32_t       xspec_R;                /* Rate (Bps)		*/
        u_int32_t       xspec_S;                /* Slack term (microsecs)*/
}   qos_flowspecx_t;

#define xspec_r  xspec_Tspec.spec_Tspec_r
#define xspec_b  xspec_Tspec.spec_Tspec_b
#define xspec_p  xspec_Tspec.spec_Tspec_p
#define xspec_m  xspec_Tspec.spec_Tspec_m
#define xspec_M  xspec_Tspec.spec_Tspec_M
        
/*
 *      Simplified-format Tspec.
 *
 *	This structure contains the generic Tspec parameters.
 */
typedef struct {
        u_int32_t       spec_type;		/* qos_service_type */
        qos_Tspec_body	xtspec_Tspec;		/* Tspec	*/
}   qos_tspecx_t;

#define xtspec_r  xtspec_Tspec.spec_Tspec_r
#define xtspec_b  xtspec_Tspec.spec_Tspec_b
#define xtspec_p  xtspec_Tspec.spec_Tspec_p
#define xtspec_m  xtspec_Tspec.spec_Tspec_m
#define xtspec_M  xtspec_Tspec.spec_Tspec_M
        
/*
 *	Simplified-format Adspec.
 *
 *	This structure is the union of all adspec parameters for
 *	Controlled-Load and Guaranteed service models.
 */
#define XASPEC_FLG_BRK	0x01	/* Break flag */
#define XASPEC_FLG_INV	0x02	/* Invalid flag: Some parameter for */
				/* this service has its Invalid flag on */

typedef struct {
	/*
	 *	General path characterization parameters
	 */
	u_char		xaspec_flags;  /* See flags above */
	u_int16_t	xaspec_hopcnt;
	float32_t	xaspec_path_bw;
	u_int32_t	xaspec_min_latency;
	u_int32_t	xaspec_composed_MTU;

	/*      Controlled-Load service Adspec parameters
	 */
	u_char	  	xClaspec_flags;		/* See flags above	 */
	u_char	 	xClaspec_override;	/* 1 => override all gen */
						/*    parameters	 */
	u_int16_t       xClaspec_hopcnt;
	float32_t       xClaspec_path_bw;
	u_int32_t       xClaspec_min_latency;
	u_int32_t       xClaspec_composed_MTU;

	/*	Guaranteed service Adspec parameters
	 */
	u_char		xGaspec_flags;		/* See flags above */
	u_int32_t	xGaspec_Ctot;
	u_int32_t	xGaspec_Dtot;
	u_int32_t	xGaspec_Csum;
	u_int32_t	xGaspec_Dsum;
	u_char	  	xGaspec_override;       /* 1 => override all gen */
						/*     parameters	 */
	u_int16_t       xGaspec_hopcnt;
	float32_t       xGaspec_path_bw;
	u_int32_t       xGaspec_min_latency;
	u_int32_t       xGaspec_composed_MTU;
}  qos_adspecx_t;


/**************************************************************************
 *
 *	RAPI call formats and definitions
 *
 **************************************************************************/

#define RAPI_VERSION	501	/* RAPI Version 5.01 */
#define MAX_RAPI_SESS	100	/* max #flows the daemon api can handle */

/*
 *	Generic RAPI object header
 */
typedef struct {
	int	     		len;	/* Actual length in bytes */
	int			form;	/* Format (see rapi_format) */

	/* Followed by type-specific contents */
}  rapi_hdr_t;

#define RAPIObj_Size(p) (((rapi_hdr_t *)(p))->len)
#define RAPIObj_data(p) ((rapi_hdr_t *)(p)+1)
#define After_RAPIObj(p) ((char *)(p) + RAPIObj_Size(p))

/*	Define RAPI flowspec/Tspec formats
 */
typedef enum {
	RAPI_EMPTY_OTYPE =	0,	/* Format = 0 => empty object */

#ifdef OBSOLETE_API
	RAPI_FLOWSTYPE_CSZ  = 1,	/* qos: Legacy format */
#endif /* OBSOLETE_RAPI */
	RAPI_FLOWSTYPE_Intserv = 2,	/* Int-Serv format flowspec	*/
	RAPI_FLOWSTYPE_Simplified = 3,	/* Simplified format flowspec	*/

	RAPI_TSPECTYPE_Intserv = 17,	/* Int-Serv format (sndr)Tspec	*/
	RAPI_TSPECTYPE_Simplified = 18,	/* Simplified format (sndr)Tspec*/

	RAPI_ADSTYPE_Intserv = 33,	/* Int-Serv format Adspec	*/
	RAPI_ADSTYPE_Simplified =34,	/* SImplified format Adspec	*/

#ifdef OBSOLETE_API
	RAPI_FILTERFORM_1 = 257,	/* Complex: sockaddr + (mask, value) */
#endif
	RAPI_FILTERFORM_BASE = 258,	/* Simple V4: Only sockaddr */
	RAPI_FILTERFORM_GPI = 259,	/* V4 GPI filter format */
	RAPI_FILTERFORM_BASE6 = 260,	/* Simple V6: Only sockaddr */
	RAPI_FILTERFORM_GPI6 = 261	/* V6 GPI filter format */
} rapi_format;

#ifdef OBSOLETE_API
/* Backwards compatibility */
#define RAPI_FLOWSTYPE_CSZX	RAPI_FLOWSTYPE_Simplified
#define RAPI_FILTERFORM_2	RAPI_FILTERFORM_BASE
#define	RAPI_FILTER_NONE	RAPI_EMPTY_OTYPE	/* Empty filter	*/
#endif

/*
 *	RAPI Flowspec descriptor
 */
typedef struct {
	int	    	len;	/* actual length in bytes */
	int		form;	/* flowspec format (see enum above) */
	union {
#ifdef OBSOLETE_API
		qos_flowspec_t	qos;	/* Legacy format */
#endif
		qos_flowspecx_t	qosx;	/* Simplified format flowspec */
		IS_specbody_t   IS;	/* Int-serv format flowspec */
	}		specbody_u;
}   rapi_flowspec_t;

#ifdef OBSOLETE_API
#define specbody_qos    specbody_u.qos
#endif
#define specbody_qosx   specbody_u.qosx
#define specbody_IS	specbody_u.IS

/*
 *	RAPI Tspec descriptor
 */
typedef struct {
	int	    	len;	/* actual length in bytes */
	int		form;	/* flowspec format (see enum above) */
	union {
		qos_tspecx_t	qosxt;	/* Simplified format Tspec */
		IS_tspbody_t	ISt;	/* Int-serv format Tspec */
	}		tspecbody_u;
}   rapi_tspec_t;
#define tspecbody_qosx	tspecbody_u.qosxt
#define tspecbody_IS	tspecbody_u.ISt
	
/*
 *	RAPI Adspec descriptor
 */
typedef struct {
	int		len;		/* actual length in bytes */
	int		form;		/* adspec format(see rapi_format)*/
	union {
		qos_adspecx_t	adsx;	/* Simplified format adspec */
		IS_adsbody_t	ISa;	/* Int-serv format adspec */
	}		adsbody_u;
}   rapi_adspec_t;

#define adspecbody_qosx adsbody_u.adsx
#define adspecbody_IS	adsbody_u.ISa


/**************************************************************************
 *
 *	Filter Spec formats
 *
 **************************************************************************/

/*	RAPI filter spec/sender template formats
 */
typedef rapi_format	rapi_filterform_t;

#ifdef OBSOLETE_API
#define	RAPI_MAXFILTER_T1	48
typedef struct {
	struct sockaddr_in sender;
	int		len;
	u_char	  v[RAPI_MAXFILTER_T1];
	u_char	  m[RAPI_MAXFILTER_T1];
} rapi_filter_t1_t;
#endif

typedef struct {
	struct sockaddr_in sender;
} rapi_filter_base_t;

#ifdef OBSOLETE_API
#define rapi_filter_t2_t	rapi_filter_base_t
#endif

typedef struct {
	struct in_addr	sender;
	u_int32_t	gpi;
} rapi_filter_gpi_t;

#ifdef USE_IPV6
typedef struct {
	struct sockaddr_in6 sender;
} rapi_filter_base6_t;

typedef struct {
	struct in6_addr sender;
	u_int32_t	gpi;
} rapi_filter_gpi6_t;
#endif /* USE_IPV6 */


/*
 *	Filter Spec descriptor
 */
typedef struct {
	int	    		len;	/* length of filter, in bytes */
	rapi_filterform_t	form;	/* format (rapi_filterform_t) */
	union {
#ifdef OBSOLETE_API
	    rapi_filter_t1_t	t1;
	    rapi_filter_t2_t	t2;
#endif
	    rapi_filter_base_t	base;
	    rapi_filter_gpi_t	gpi;
#ifdef USE_IPV6
	    rapi_filter_base_t	base6;
	    rapi_filter_gpi_t	gpi6;
#endif /* USE_IPV6 */
	}	   filt_u;		/* variable length */
} rapi_filter_t;

#ifdef OBSOLETE_API
#define rapi_filtaddr		filt_u.t2.sender.sin_addr
#endif
#define rapi_filtbase_addr	filt_u.base.sender.sin_addr
#define rapi_filtbase_port	filt_u.base.sender.sin_port
#define rapi_filtgpi_addr	filt_u.gpi.sender.sin_addr
#ifdef USE_IPV6
#define rapi_filtbase6_addr	filt_u.base.sender.sin6_addr
#define rapi_filtbase6_port	filt_u.base.sender.sin6_port
#define rapi_filtgpi6_addr	filt_u.gpi.sender.sin6_addr
#endif /* USE_IPV6 */


/**************************************************************************
 *
 *      Policy Data formats
 *
 **************************************************************************/

typedef struct {
	char    name[1];
}   rapi_policy_name_string_t;

/*
 *      Policy Formats
 */
typedef enum {
	RAPI_POLICY_NAME_STRING = 513    
} rapi_policy_form_t;


typedef enum {
   RAPI_POLICYF_INTEGRITY=0x1,  /* Integrity it mandatory for this object */
   RAPI_POLICYF_GLOBAL   =0x2,  /* global policy objects (snd/recv) */
   RAPI_POLICYF_RESP     =0x4
} rapi_policy_flags_t;

typedef struct {
	int			len;	   /* actual length in bytes */
	rapi_policy_form_t      form;	   /* policy data format */
	rapi_policy_flags_t     flags;  
	union {
	    rapi_policy_name_string_t   name_string;
	} pol_u;
} rapi_policy_t;

/******************************************************************/
/******************************************************************/

/*
 *	Reservation style ids
 */
typedef enum {
	RAPI_RSTYLE_WILDCARD = 1,	/* STYLE_ID_WF */
	RAPI_RSTYLE_FIXED = 2, 		/* STYLE_ID_FF */
	RAPI_RSTYLE_SE = 3		/* STYLE_ID_SE */
} rapi_styleid_t;
#define RAPI_RSTYLE_MAX  3

/*
 *	Reservation style extension
 */
/*	This structure is currently undefined; in the future, it will
 *	allow passing style-specific parameters for possible new styles.
 */
typedef void rapi_stylex_t;

	/* RAPI client handle */
typedef unsigned int rapi_sid_t;
#define NULL_SID	0	/* Error return from rapi_session */

	/* RAPI User callback function */

typedef enum {
	RAPI_PATH_EVENT = 1,
	RAPI_RESV_EVENT = 2,
	RAPI_PATH_ERROR = 3,
	RAPI_RESV_ERROR = 4,
	RAPI_RESV_CONFIRM = 5,
/* Following are private to ISI implementation: */
	RAPI_PATH_STATUS = 9,
	RAPI_RESV_STATUS = 10
} rapi_eventinfo_t;

typedef int  ((*rapi_event_rtn_t) (
	rapi_sid_t,		/* Which sid generated event	*/
	rapi_eventinfo_t,	/* Event type			*/

	int,			/* Style ID			*/
	int,			/* Error code			*/
	int,			/* Error value			*/
	struct sockaddr *,	/* Error node address		*/
	u_char,			/* Error flags			*/
#define RAPI_ERRF_InPlace  0x01	 /*   Left resv in place	*/
#define RAPI_ERRF_NotGuilty 0x02 /*   This rcvr not guilty	*/

	int,			/* Number of filter specs/sender*/
				/* 	templates in list	*/
	rapi_filter_t *,	/* Filter spec/sender templ list*/
	int,			/* Number of flowspecs/Tspecs   */
	rapi_flowspec_t *,	/* Flowspec/Tspec list		*/
	int,			/* Number of adspecs		*/
	rapi_adspec_t *,	/* Adspec list			*/
	void *			/* Client supplied arg		*/
));


/*
 *	RAPI External Functions
 */
rapi_sid_t rapi_session(
	struct sockaddr *	dest,		/* Dest host, port */
	int			protid,		/* Protocol Id */
	int			flags,		/* Session flags */
#define RAPI_USE_INTSERV        0X10    /* Use  Int-Serv fmt in upcalls */
#define RAPI_GPI_SESSION        0x08    /* Use GPI session format */

	rapi_event_rtn_t	event_rtn,	/* optional */
	void		*	event_rtn_arg,
	int		*	errnop		/* Place to store errno */
);

int rapi_sender(
	rapi_sid_t		sid,
	int			flags,
	struct sockaddr *	host,		 /* Sender [host, ] port */
	rapi_filter_t *		sender_template, /* optional */
	rapi_tspec_t  *		sender_tspec,
	rapi_adspec_t *		sender_adspec,	 /* optional */
	rapi_policy_t *		sender_policy,	 /* optional */
	int			ttl		 /* Data TTL (optional) */
);

int rapi_reserve(
	rapi_sid_t		sid,
	int			flags,
#define RAPI_REQ_CONFIRM	0x20	/* Request confirmation */

	struct sockaddr *	rhost,
	rapi_styleid_t		styleid,
	rapi_stylex_t *		style_extension,
	rapi_policy_t *		rcvr_policy,	/* optional */

	int			n_filter,
	rapi_filter_t *		filter_spec_list,
	int			n_flow,
	rapi_flowspec_t *	flow_spec_list
);


int 	rapi_release(rapi_sid_t sid);
int 	rapi_getfd(rapi_sid_t sid);
int 	rapi_dispatch(void);
int 	rapi_set_timo(int timo);
int	rapi_version();

/*	Formatting routines in rapi_fmt.c
 */
void	rapi_fmt_flowspec(rapi_flowspec_t *, char *, int);
void	rapi_fmt_tspec(rapi_tspec_t *, char *, int);
void	rapi_fmt_adspec(rapi_adspec_t *, char *, int);
void	rapi_fmt_filtspec(rapi_filter_t *, char *, int);

/*
 *	RAPI Error Codes
 */
#define RAPI_ERR_OK		0	/* No error			*/
#define RAPI_ERR_INVAL		1	/* Invalid parameter		*/
#define RAPI_ERR_MAXSESS	2	/* Too many sessions		*/
#define RAPI_ERR_BADSID		3	/* Sid out of legal range.	*/
#define RAPI_ERR_N_FFS		4	/* Wrong n_filter/n_flow for style*/
#define RAPI_ERR_BADSTYLE	5	/* Illegal reservation style	*/
#define RAPI_ERR_SYSCALL	6	/* Some sort of syscall err; see errno*/
#define RAPI_ERR_OVERFLOW	7	/* Parm list overflow		*/
#define RAPI_ERR_MEMFULL	8	/* Not enough memory		*/
#define RAPI_ERR_NORSVP		9	/* Daemon doesn't respond/exist.*/
#define RAPI_ERR_OBJTYPE	10	/* Object type error		*/
#define RAPI_ERR_OBJLEN		11	/* Object length error		*/
#define RAPI_ERR_NOTSPEC	12	/* No sender tspec in rapi_sendr*/
#define RAPI_ERR_INTSERV	13	/* Intserv format error		*/
/* 				14	 * UNUSED			*/

/*  The following occur only asynchronously, as the error value when
 *	error code = RSVP_Err_API_ERROR
 */
#define RAPI_ERR_BADSEND	14	/* Sender addr not my interface	*/
#define RAPI_ERR_BADRECV	15	/* Recv addr not my interface	*/
#define RAPI_ERR_BADSPORT	16	/* Sport !=0 but Dport == 0	*/
#define RAPI_ERR_GPISESS	17	/* Parms wrong for GPI_SESSION flag*/

#define RAPI_ERR_UNSUPPORTED	254
#define RAPI_ERR_UNKNOWN	255

#ifdef ISI_TEST

/*	rapi_status():  Trigger EVENT upcalls for all current state for
 *		specified session ID.  This is experimental.
 */
int
rapi_status(
	rapi_sid_t		sid,
	int			flags
#define RAPI_STAT_PATH		0x80	/* Path state status */
#define RAPI_STAT_RESV		0x40	/* Resv state status */
);
#endif /* ISI_TEST */

__END_DECLS

#ifdef __cplusplus
}
#endif

#endif				/* __rapilib_h_ */
