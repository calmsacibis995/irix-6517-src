/******************************************************************
 *
 *  Spider-X25 Interface Primitives
 *
 *  Copyright (C) 1991  Spider Systems Limited
 *
 *  X25_PROTO.H
 *
 *  Streams primitives include file 
 *  for X.25 Network Service Users
 *
 ******************************************************************/

/*
 *	 /projects/common/PBRAIN/SCCS/pbrainF/dev/src/include/sys/snet/0/s.x25_proto.h
 *	@(#)x25_proto.h	1.48
 *
 *	Last delta created	11:13:01 4/20/92
 *	This file extracted	14:53:25 11/13/92
 *
 */

/******************************************************************/
/* inna */
#ifndef X25_X25_PROTO_
#define X25_X25_PROTO_
/* ---- MESSAGE TYPE FIELDS FOR NETWORK SERVICE USER ---- */

#define		XL_DAT		0	/* Data type    */
#define		XL_CTL		1	/* Control type */

/* ---- INDICATIONS TO NETWORK SERVICE USER ---- */

#define		N_Data		0	/* Data           */
#define		N_EData		1	/* Expedited data */
#define		DTflow		2	/* data flow      */
#define		N_EAck		3	/* expedited ack  */
#define		N_CI		4	/* Connect        */
#define		N_CC		5	/* Connect conf   */
#define		N_DI		6	/* Disconnect     */
#define		N_RI		7	/* Reset          */
#define		N_RC		8	/* Reset conf     */
#define		N_Abort		9	/* Abort - DI but no resource */
#define 	N_DC		10	/* Disconnect Confirm         */

#define		N_Xlisten	11	/* Listen request */
#define		N_Xcanlis	12	/* Cancel listen  */

#define		N_DAck          13      /* data ack       */

#define		N_PVC_ATTACH	14	/* PVC attach msg */
#define		N_PVC_DETACH	15	/* PVC detach msg */


/* ----------------------------------------------------------------------
 *              Address formats for the NS user
 * ----------------------------------------------------------------------
 */

#define		LSAPMAXSIZE	9
#define		DTEMAXSIZE	(LSAPMAXSIZE * 2)-1
#define		NSAPMAXSIZE	20
#define		MAXAOCTETS	NSAPMAXSIZE
#define		MAXASEMIOCTETS	(MAXAOCTETS << 1)

struct lsapformat {
	unsigned char	lsap_len;
	unsigned char	lsap_add[LSAPMAXSIZE];
};

struct xaddrf {
	unsigned int		sn_id;
	unsigned char		aflags;
	struct lsapformat	DTE_MAC;
	unsigned char		nsap_len;
	unsigned char		NSAP[NSAPMAXSIZE];
};


/*
 *----------------------------------------------------------------------
 *	Flag settings for 'aflags' field.
 * ----------------------------------------------------------------------
 */

#define		NSAP_ADDR	0
#define		EXT_ADDR	1
#define		PVC_LCI		2

/* ----------------------------------------------------------------------
 *  Declaration of the 'qos' and 'extraformat' used in the user interface.
 * ----------------------------------------------------------------------
 */
#define MAX_CUG_LEN	2
#define MAX_RPOA_LEN 	8
#define MAX_NUI_LEN	64
#define MAX_FAC_LEN	32

#define MAX_PROT        32
#define PRT_SRC         1
#define PRT_DST         2
#define PRT_GLB         3

#define MAX_PRTY_LEN    6

/*  Charging Information Sizes  - Set up for four different tariffs.  */

#define		MAX_TARIFFS	4
#define		MAX_MU_LEN	16
#define		MAX_SC_LEN	(MAX_TARIFFS * 8)
#define		MAX_CD_LEN	(MAX_TARIFFS * 4)

#define CUG	1
#define BCUG	2

/* Possible values for reqackservice */

#define RC_CONF_DTE	1
#define RC_CONF_APP	2


struct extraformat {
	unsigned char	  fastselreq;
	unsigned char	  restrictresponse, reversecharges;
	unsigned char	  pwoptions;
	unsigned char	  locpacket, rempacket;
	unsigned char	  locwsize , remwsize;
	int		  nsdulimit;
	unsigned char	  nui_len;
	unsigned char	  nui_field[MAX_NUI_LEN];
	unsigned char     rpoa_len;
	unsigned char	  rpoa_field[MAX_RPOA_LEN];
	unsigned char     cug_type;
	unsigned char 	  cug_field[MAX_CUG_LEN];
	unsigned char	  reqcharging;
	unsigned char	  chg_cd_len;
	unsigned char	  chg_cd_field[MAX_CD_LEN];
	unsigned char	  chg_sc_len;
	unsigned char	  chg_sc_field[MAX_SC_LEN];
	unsigned char	  chg_mu_len;
	unsigned char	  chg_mu_field[MAX_MU_LEN];
	unsigned char	  called_add_mod;
	unsigned char	  call_redirect;
	struct lsapformat called;
	unsigned char	  call_deflect;
	unsigned char 	  x_fac_len;
	unsigned char	  cg_fac_len;
	unsigned char	  cd_fac_len;
	unsigned char	  fac_field[MAX_FAC_LEN];
};

struct qosformat {
	unsigned char	reqtclass;
	unsigned char	locthroughput, remthroughput;
	unsigned char	reqminthruput;
	unsigned char	locminthru, remminthru;
	unsigned char	reqtransitdelay;
	unsigned short	transitdelay;
	unsigned char	reqmaxtransitdelay;
	unsigned short	acceptable;
        unsigned char   reqpriority;
        unsigned char   reqprtygain;
        unsigned char   reqprtykeep;
        unsigned char   prtydata;
        unsigned char   prtygain;
        unsigned char   prtykeep;
        unsigned char   reqlowprtydata;
        unsigned char   reqlowprtygain;
        unsigned char   reqlowprtykeep;
        unsigned char   lowprtydata;
        unsigned char   lowprtygain;
        unsigned char   lowprtykeep;
        unsigned char   protection_type;
        unsigned char   prot_len;
        unsigned char   lowprot_len;
        unsigned char   protection[MAX_PROT];
        unsigned char   lowprotection[MAX_PROT];
	unsigned char	reqexpedited;
	unsigned char	reqackservice;

	struct extraformat xtras;
};


/* Packet & Window size negotiation values	*/

#define	DEF_X25_PKT	7		/* default X.25 packet size (2^n)   */
#define	DEF_X25_WIN	2		/* default X.25 window size	    */

/* pwoptions bit fields - true if set	*/

#define	NEGOT_PKT	0x01		/* packet size is negotiable	    */
#define	NEGOT_WIN	0x02		/* window size is negotiable	    */
#define	ASSERT_HWM	0x04		/* assert concatenation into NSDU   */
					/* up to high-water mark given	    */

/* ----------------------------------------------------------------------
 *                 Interface Message Structures
 * ----------------------------------------------------------------------
 */
  
typedef struct xhdrf {
	unsigned char	xl_type;
	unsigned char	xl_command;
} S_X25_HDR;

struct xdataf {
	unsigned char	xl_type;        /* Always XL_DAT */
	unsigned char 	xl_command;     /* Always N_Data */
	unsigned char 	More,           /* Set when more data is required to */
                                   	/* complete the nsdu */
			setDbit,	/* Set when data carries X.25 D-bit */
              		setQbit;	/* Set when data carries X.25 Q-bit */
};

struct xdatacf {
	unsigned char	xl_type;	/* Always XL_DAT */
	unsigned char	xl_command;	/* Always N_DAck */
};

struct xedataf {
	unsigned char	xl_type;	/* Always XL_DAT */
	unsigned char	xl_command;	/* Always N_EData */
};

struct xedatacf {
	unsigned char	xl_type;	/* Always XL_DAT */
	unsigned char	xl_command;	/* Always N_EAck */
};

struct xrstf {
	unsigned char	xl_type;	/* Always XL_CTL */
	unsigned char	xl_command;	/* Always N_RI */
	unsigned char	originator,	/* Originator and Reason mapped from */
	             	reason,		/* X.25 cause/diag in indications */
        		cause,		/* X.25 cause byte (ignored in reqs) */
			diag;		/* X.25 diagnostic byte */
};

struct xrscf {
	unsigned char	xl_type;	/* Always XL_CTL */
	unsigned char	xl_command;	/* Always N_RC */
};

struct xdiscf {
	unsigned char	xl_type;	/* Always XL_CTL */
	unsigned char	xl_command;	/* Always N_DI */
	unsigned char	originator,	/* Originator and Reason mapped from */
			reason,		/* X.25 cause/diag in indications */
			cause,		/* X.25 cause byte (ignored in reqs) */
			diag;		/* X.25 diagnostic byte */
	int		conn_id;	/* Connection identifier (reject) */
	unsigned char	indicated_qos;	/* When set facilities indicated  */ 
	struct xaddrf   responder;      /* CONS responder address */
	struct xaddrf   deflected;      /* Address to deflect to */
	struct qosformat qos;		/* Facilities and CONS qos: if    */
					/* indicated_qos is set           */
};

struct xdcnff {
	unsigned char	 xl_type;	/* Always XL_CTL */
	unsigned char	 xl_command;	/* Always N_DC */
	unsigned char	 indicated_qos;	/* When set facilities indicated  */
	struct qosformat rqos;		/* Facilities and CONS qos: if    */
					/* indicated_qos is set           */
};

struct xabortf {
	unsigned char	xl_type;	/* Always XL_CTL */
	unsigned char	xl_command;	/* Always N_Abort */
};

struct xcallf {
	unsigned char	xl_type;	/* Always XL_CTL */
	unsigned char	xl_command;	/* Always N_CI */
	int		conn_id;	/* A connection id quoted on accept. */
	unsigned char	CONS_call;
	unsigned char	negotiate_qos;  /* When set, negotiate facils etc. */
					/* else use defaults */
	struct xaddrf	calledaddr;	/* The called and */
	struct xaddrf	callingaddr;	/* calling addresses */
	struct qosformat	qos;	/* Facilities and CONS qos: if */
					/* negotiate_qos is set */
};

struct xccnff {
	unsigned char	xl_type;	/* Always XL_CTL */
	unsigned char	xl_command;	/* Always N_CC */
	int		conn_id;	/* The connection id quoted on the */
					/* associated indication. */
	unsigned char	CONS_call;
	unsigned char	negotiate_qos;	/* When set, negotiate facils etc. */
					/* else use indicated values */
	struct xaddrf	responder;	/* Responding address */
	struct qosformat	rqos;	/* Facilities and CONS qos: if */
					/* negotiate_qos is set */
};


struct pvcattf {
	unsigned char     xl_type;      /* Always XL_CTL */
	unsigned char     xl_command;   /* Always N_PVC_ATTACH */
	unsigned short    lci;          /* Logical channel */
	unsigned int	  sn_id;        /* Subnetwork identifier */
	/* 0 for next 3 parameters implies use of default */
	unsigned char     reqackservice;
	unsigned char     reqnsdulimit;
	int               nsdulimit;   
	int               result_code;     /* Non-zero - error */
};

struct pvcdetf {
	unsigned char     xl_type;      /* Always XL_CTL */
	unsigned char     xl_command;   /* Always N_PVC_DETACH */
	int               reason_code;  /* Reports why */
};

/* ----------------------------------------------------------------------
 *   Numbers for the X.25 multiplexor code.  They include 
 *   identifiers for different sub-network types etc.
 * ----------------------------------------------------------------------
 */

/* ------ VALUES FOR LISTEN FLAG BYTES ------ */

#define		X25_DTE		1
#define		X25_NSAP	2
#define		X25_DONTCARE	1
#define		X25_STARTSWITH	2
#define		X25_IDENTITY	3


struct xlistenf {
	unsigned char	xl_type;	/* Always XL_CTL */
	unsigned char	xl_command;	/* Always N_Xlisten */
	int		lmax;		/* Maximum number of CI's at a time */
	int		l_result;	/* Result flag */
};

struct xcanlisf {
	unsigned char	xl_type;	/* Always XL_CTL */
	unsigned char	xl_command;	/* Always N_Xcanlis */
	int		c_result;	/* Result flag */
};

/* ----------------------------------------------------------------------
 *                Diagnostic Code Defines
 * ----------------------------------------------------------------------
 */

/* Internal definitions for N_ provider and user - zero is undefined */

#define		NS_USER			1
#define		NS_PROVIDER		2	/* See DIS 8878 Annex A  */

/*            Network Service Reason Codes

   Definitions for NS originator = NS provider */

#define		NS_GENERIC		0XE0	/* General         (224) */
#define		NS_DTRANSIENT		0XE1	/* disc - transient      */
#define		NS_DPERMANENT		0XE2	/* disc - permanent      */
#define		NS_TUNSPECIFIED		0XE3	/* rej  - transient      */
#define		NS_PUNSPECIFIED		0XE4	/* rej  - permanent      */
#define		NS_QOSNATRANSIENT	0XE5	/* rej  - transient      */
#define		NS_QOSNAPERMANENT	0XE6	/* rej  - permanent      */
#define		NS_NSAPTUNREACHABLE	0XE7	/* rej  - transient      */
#define		NS_NSAPPUNREACHABLE	0XE8	/* rej  - permanent(232) */
#define		NS_NSAPUNKNOWN		0XEB	/* rej  - permanent(235) */

/* Definitions for NS originator = NS user     */

#define		NU_GENERIC		0XF0	/* General        (240)  */
#define		NU_DNORMAL		0XF1	/* disc - normal         */
#define		NU_DABNORMAL		0XF2	/* disc - abnormal       */
#define		NU_DINCOMPUSERDATA	0XF3	/* disc - incomp         */
#define		NU_TRANSIENT		0XF4	/* rej  - transient      */
#define		NU_PERMANENT		0XF5	/* rej  - permanent      */
#define		NU_QOSNATRANSIENT	0XF6	/* rej  - transient      */
#define		NU_QOSNAPERMANENT	0XF7	/* rej  - permanent      */
#define		NU_INCOMPUSERDATA	0XF8	/* rej  - cudf           */
#define		NU_BADPROTID		0XF9	/* rej  - prot id (249)  */

/* NS reasons for reset - first Originator =  NS provider */

#define		NS_RUNSPECIFIED		0XE9	/* unspecified    (233)  */
#define		NS_RCONGESTION		0XEA	/* congestion     (234)  */

/* Originator = NS user */

#define		NU_RESYNC		0XFA	/* user resync    (250)  */

/* ------------------------------------------------------------------------
 * PVC related error, result and indication codes for attach, detach.
 * ------------------------------------------------------------------------ */

#define	PVC_SUCCESS		0	/* Zero - OK */
#define	PVC_NOSUCHSUBNET	1	/* Subnetwork not configured */
#define	PVC_CFGERROR		2	/* LCI not in range, no PVCs */
#define	PVC_NODBELEMENTS	3	/* No db available */
#define	PVC_PARERROR		4	/* Error in req. pars */
#define PVC_BUSY		6	/* PVC in non-attach state */
#define PVC_CONGESTION		7	/* Resources low */
#define	PVC_WRONGSTATE		8	/* State wrong for fn */
#define	PVC_NOPERMISSION	9	/* Later enhancement? */

#define PVC_LINKDOWN		10	/* The link has gone down */
#define	PVC_RMTERROR		11	/* No response from remote */
#define	PVC_USRERROR		12	/* User i/f error detected */
#define PVC_INTERROR		13	/* Internal error - report */
#define	PVC_NOATTACH		14	/* Not attached yet (detach) */
#define	PVC_WAIT		15	/* Wait code - not to user */
#endif
