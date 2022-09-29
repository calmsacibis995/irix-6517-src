/******************************************************************************

	    COPYRIGHT (c) 1988, 1994 by Globetrotter Software Inc.
	This software has been provided pursuant to a License Agreement
	containing restrictions on its use.  This software contains
	valuable trade secrets and proprietary information of 
	Globetrotter Software Inc and is protected by law.  It may 
	not be copied or distributed in any form or medium, disclosed 
	to third parties, reverse engineered or used in any manner not 
	provided for in said License Agreement except with the prior 
	written authorization from Globetrotter Software Inc.

 *****************************************************************************/

#ifndef _FLEXLM_H_
#define _FLEXLM_H_

#define LONG int

/*
 *	Macros to make c++ and ansi-c happy
 */

#if defined(c_plusplus) || defined(__cplusplus)
#define  lm_extern  extern "C"
#define  lm_noargs  void
#define  lm_args(args) args
#else	/* ! c++ */
#define  lm_extern  extern
#if defined(__STDC__) || defined (__ANSI_C__) || defined(ANSI) || defined(PC)
#define  lm_noargs  void
#define  lm_args(args) args
#else	/* ! stdc || ansi */
#define  lm_noargs  /* void */
#define  lm_args(args) ()
#endif	/* stdc || ansi */
#endif	/* c++ */


#ifndef FAR
#define FAR 
#endif /* FAR */

typedef unsigned LONG LM_A_VAL_TYPE;
typedef void  FAR * LM_VOID_PTR;
typedef void  FAR * FAR * LM_VOID_PTR_PTR;
typedef int   FAR * LM_INT_PTR;
typedef short FAR * LM_SHORT_PTR;
typedef LONG  FAR * LM_LONG_PTR;
typedef unsigned LONG  FAR * LM_U_LONG_PTR;
typedef char  FAR * LM_CHAR_PTR;
typedef char  FAR * FAR * LM_CHAR_PTR_PTR;
typedef unsigned char  FAR * LM_U_CHAR_PTR;


/*
 *	FLEXlm version
 */

#define FLEXLM_VERSION 4
#define FLEXLM_REVISION 1
#define FLEXLM_PATCH ""


#define LM_BADDATE       -11    /* Bad date in license file */
#define LM_LONGGONE      -10    /* Software Expired */
#define LM_TOOEARLY      -31    /* Start date for feature not reached */


#define LM_DUP_NONE 0x4000	/* Don't allow any duplicates */
#define LM_DUP_SITE   0		/* Nothing to match => everything matches */
#define LM_DUP_USER   1		/* Allow dup if user matches */
#define LM_DUP_HOST   2		/* Allow dup if host matches */
#define LM_DUP_DISP   4		/* Allow dup if display matches */
#define LM_DUP_VENDOR 8		/* Allow dup if vendor-defined matches */
#define LM_COUNT_DUP_STRING "16384"	/* For ls_vendor.c: LM_DUP_NONE */
#define LM_NO_COUNT_DUP_STRING "3"	/* For ls_vendor.c: _USER | _HOST */


#define LM_RESERVED_FEATURE 	"FEATURE"

/*	
 *	Flags for 'flag' field of lc_cryptstr()
 */
#define LM_MAXPATHLEN	512		/* Maximum file path length */
#define MAX_FEATURE_LEN 30		/* Longest featurename string */
#define DATE_LEN	11		/* dd-mmm-yyyy */
#define MAX_CONFIG_LINE	2048		/* Max length of a license file line */
#define	MAX_SERVER_NAME	32		/* Maximum FLEXlm length of hostname */
#define	MAX_HOSTNAME	64		/* Maximum length of a hostname */
#define	MAX_DISPLAY_NAME 32		/* Maximum length of a display name */
#define MAX_USER_NAME 20		/* Maximum length of a user name */
#define MAX_VENDOR_CHECKOUT_DATA 32	/* Maximum length of vendor-defined */
					/*		checkout data       */
#define MAX_PLATFORM_NAME 12		/* e.g., "sun4_u4" */
#define MAX_DAEMON_NAME 10		/* Max length of DAEMON string */
#define MAX_VENDOR_NAME MAX_DAEMON_NAME	/* Synomym for MAX_DAEMON_NAME */
#define MAX_SERVERS	5		/* Maximum number of servers */
#define MAX_USER_DEFINED 64		/* Max size of vendor-defined string */
#define MAX_VER_LEN 10			/* Maximum length of a version string */
#define MAX_LONG_LEN 10			/* Length of a long after sprintf */
#define MAX_SHORT_LEN 5			/* Length of a short after sprintf */
#define MAX_INET 16			/* Maximum length of INET addr string */
#define MAX_BINDATE_YEAR 2027		/* Binary date has 7-bit year */

/*
 *	License file location
 */


#define LM_DEFAULT_ENV_SPEC "LM_LICENSE_FILE"	/* How a user can specify */


/*
 *	V1/V2/V3 compatibility macros
 */



/*
 *	Communications constants
 */
#define MASTER_WAIT 20                  /* # seconds to wait for a connection */

/*
 *	Structure types
 */

#define VENDORCODE_BIT64	1	/* 64-bit code */
#define VENDORCODE_BIT64_CODED	2	/* 64-bit code with feature data */
#define VENDORCODE_3	3		/* VENDORCODE2 with version data */
#define VENDORCODE_4	4		/* VENDORCODE3 with new vendor keys */

/*
 *	Host identification data structure
 */
#define MAX_HOSTID_LEN (MAX_SERVER_NAME + 9)
					/* hostname + strlen("HOSTNAME=") */

typedef struct hostid {			/* Host ID data */
			short override;	/* Hostid checking override type */
#define NO_EXTENDED 1			/* Turn off extended hostid */
#define DEMO_SOFTWARE 2			/* DEMO software, no hostid */
			short type;	/* Type of HOST ID */
#define	NOHOSTID 0
#define HOSTID_LONG 1			/* Longword hostid, eg, SUN */
#define HOSTID_ETHER 2			/* Ethernet address, eg, VAX */
#define HOSTID_ANY 3			/* Any hostid */
#define HOSTID_REP_NORMAL 0
#define HOSTID_REP_DECIMAL 1


			short representation; /* Normal or other */
			union {
				LONG data;
#define ETHER_LEN 6			/* Length of an ethernet address */
				unsigned char e[ETHER_LEN];
				char user[MAX_USER_NAME+1];
				char display[MAX_DISPLAY_NAME+1];
				char host[MAX_SERVER_NAME+1];
				char vendor[MAX_HOSTID_LEN+1];
				char string[MAX_HOSTID_LEN+1];
				short internet[4];
			      } id;

#define hostid_value id.data
#define hostid_eth id.e
#define hostid_user id.user
#define hostid_display id.display
#define hostid_hostname id.host
#define hostid_string id.string
#define hostid_internet id.internet
		      } HOSTID, FAR *HOSTID_PTR;


#define MAX_CRYPT_LEN 20	/* use 8 bytes of encrypted return string to
				   produce a 16 char HEX representation  + 4 */

/*
 *	Vendor encryption seed
 */

typedef struct vendorcode {
			    short type;	    /* Type of structure */
			    LONG data[2];   /* 64-bit code */
			  } VENDORCODE1;

typedef struct vendorcode2 {
			    short type;	   /* Type of structure */
			    LONG data[2];  /* 64-bit code */
			    LONG keys[3];  
					   
					   
			  } VENDORCODE2;

typedef struct vendorcode3 {
			    short type;	   /* Type of structure */
			    LONG data[2];  /* 64-bit code */
			    LONG keys[3];  
					   
					   
			    short flexlm_version;
			    short flexlm_revision;
			  } VENDORCODE3;

typedef struct vendorcode4 {
			    short type;	   /* Type of structure */
			    unsigned LONG data[2]; /* 64-bit code */
			    unsigned LONG keys[4]; 
					   	   
					   	   
					   	   
						   
			    short flexlm_version;
			    short flexlm_revision;
			  } VENDORCODE4,  FAR *VENDORCODE_PTR;

/*
 *	The current default VENDORCODE
 */

#define VENDORCODE VENDORCODE4

#define LM_CODE(name, x, y, k1, k2, k3, k4, k5)  static VENDORCODE name = \
						{ VENDORCODE_4, \
						  { (x)^(k5), (y)^(k5) }, \
						  { (k1), (k2), (k3), (k4) }, \
						  FLEXLM_VERSION, \
						  FLEXLM_REVISION }

#define LM_CODE_GLOBAL(name, x, y, k1, k2, k3, k4, k5)  VENDORCODE name = \
						{ VENDORCODE_4, \
						  { (x)^(k5), (y)^(k5) }, \
						  { (k1), (k2), (k3), (k4) }, \
						  FLEXLM_VERSION, \
						  FLEXLM_REVISION }


/*
 *	Server data from the license file FEATURE file
 */
typedef struct lm_server {		/* License servers */
			    char name[MAX_HOSTNAME+1];	/* Hostname */
			    struct hostid id;		/* hostid */
			    struct lm_server FAR *next;	/* NULL =none */
			    int commtype; 		/* TCP/UDP/FIFO/FILE */
			    int port;			/* TCP/UDP port */
			    LM_CHAR_PTR filename;		/* FILE base path */
#if 0
				/* Fields below are only used in servers */
			    LM_SOCKET fd1;  	/* File descriptor for output */
			    LM_SOCKET fd2;  	/* File descriptor for input */
#endif
			    int state;		/* State of connection on fd1 */
			    int us;	     /* "host we are running on" flag */
			    LONG exptime; 	/* When this connection attempt
								times out */
#ifdef SUPPORT_IPX
			    SOCKADDR_IPX spx_addr;	/* SPX socket(port) */
#endif 
			  } LM_SERVER, FAR *LM_SERVER_PTR;

/*
 *	Communication end point description
 */
typedef struct comm_endpoint {
	    int transport; 		/* TCP/UDP/FIFO/FILE/SPX */
	    union {
		    int			port;		/* TCP/UDP */
#ifdef SUPPORT_IPX
		    SOCKADDR_IPX	spx_addr;	/* SPX */
#endif
		    /* RPC may come in here later */
	    } transport_addr;
	} COMM_ENDPOINT, FAR *COMM_ENDPOINT_PTR;

/*
 *	Feature data from the license file FEATURE file
 */
typedef struct config {			/* Feature data line */
/*
 *			First, the required fields
 */
			short type;			/* Type */
	
#define CONFIG_FEATURE 0				/*  FEATURE line */


#define CONFIG_UNKNOWN 9999				/*  Unknown line */

			char feature[MAX_FEATURE_LEN+1]; /* Ascii name */
			char version[MAX_VER_LEN+1];/* Feat's version */
			char daemon[MAX_DAEMON_NAME+1];	/* DAEMON to serve */
			char date[DATE_LEN+1];		/* Expiration date */
			int users;			/* Licensed # users */
			char code[MAX_CRYPT_LEN+1];	/* encryption code */
#define CONFIG_PORT_HOST_PLUS_CODE "PORT_AT_HOST_PLUS   " /* +port@host marker*/
			LM_SERVER_PTR server;		/* License server(s) */
			int lf;				/* License file index */
/*
 *			Optional stuff below here ...
 */

			LM_CHAR_PTR lc_vendor_def;	    /* Vendor-defined string */
			struct hostid id;		/* Licensed host */
			char  fromversion[MAX_VER_LEN + 1];
							/* Upgrade from ver. */

						
			int lc_got_options;		/* Bitmap of options,
							   for int-type opts */

#define LM_LICENSE_LINGER_PRESENT       0x1
#define LM_LICENSE_DUP_PRESENT          0x2
#define LM_LICENSE_WQUEUE_PRESENT       0x4
#define LM_LICENSE_WTERMS_PRESENT       0x8
#define LM_LICENSE_WLOSS_PRESENT       0x10
#define LM_LICENSE_OVERDRAFT_PRESENT   0x20
#define LM_LICENSE_CKSUM_PRESENT       0x40
#define LM_LICENSE_PKG_OPTION_PRESENT  0x80

/*
 *		NOTE: lc_linger, lc_dup_group, lc_prereq, lc_sublic, and
 *		      lc_dist_constraint are for future use by Globetrotter
 *		      Software, Inc.  DO NOT USE these fields.
 */
			int lc_linger;		/* Linger to override client */
			int lc_dup_group;	/* dup_group -override client */
			int lc_overdraft;	/* # of overdraft licenses */
			int lc_cksum;		/* Line checksum */
			int lc_pkg_options;	/* OPTIONS= */
#define LM_LICENSE_PKG_OPT_SUITE 0x1	/* PACKAGE is a SUITE */
			LM_CHAR_PTR lc_vendor_info;/* (Unencrypted) vendor info */
			LM_CHAR_PTR lc_dist_info;	/* (Unencrypted) dist. info */
			LM_CHAR_PTR lc_user_info;	/* (Unencrypted) enduser info */
			LM_CHAR_PTR lc_asset_info;	/* (Unencrypted) asset info */
			LM_CHAR_PTR lc_issuer;	/* Who issued the license */
			LM_CHAR_PTR lc_notice;	/* Intellectual prop.notice */
			LM_CHAR_PTR lc_prereq;	/* Prerequesite products */
			LM_CHAR_PTR lc_sublic;	/* Sub-licensed products */
			LM_CHAR_PTR lc_dist_constraint; /* extra distributor 
								constraints */

/*
 *			Internal GSI use only (DO NOT USE)
 */
			LM_CHAR_PTR lc_w_binary;
			LM_CHAR_PTR lc_w_argv0;
			int lc_w_queue;
			int lc_w_termsig;
			int lc_w_loss;
/*
 *		Package info
 */
			unsigned char package_mask;	
#define LM_LICENSE_PKG_ENABLE 0x1	/* Enabling FEATURE for package */
					/* If PKG_SUITE is NOT set, this
					   should never be checked out
					   or listed by lmstat */
#define LM_LICENSE_PKG_SUITE 0x2	/* This enabling FEATURE is for a 
					   SUITE (implies ENABLE is set) */
#define LM_LICENSE_PKG_COMPONENT 0x4	/* a component from a PACKAGE */

			struct config FAR *components;   /* If a PACKAGE */
			struct config FAR *parent_feat;  /* If a component --
							    assoc. FEATURE 
							    line */
			struct config FAR *parent_pkg;	/* If a component, 
							   points to associated
							   pkg */
/*
 *			Links
 */
			struct config FAR *next;	/* Ptr to next one */
			struct config FAR *last;	/* Ptr to previous */
		      } CONFIG, FAR *CONFIG_PTR, FAR * FAR *CONFIG_PTR_PTR;

/*
 *	License data descriptor strings
 */
#define LM_LICENSE_VENDOR_STRING "VENDOR_STRING"
#define LM_LICENSE_HOSTID_STRING "HOSTID"
#define LM_LICENSE_COMPONENT_STRING "COMPONENTS"
#define LM_LICENSE_COMPONENT_SEP ":"
#define LM_LICENSE_OPTIONS_STRING "OPTIONS"
#define LM_LICENSE_VENDOR_INFO "vendor_info"
#define LM_LICENSE_DIST_INFO "dist_info"
#define LM_LICENSE_USER_INFO "user_info"
#define LM_LICENSE_ASSET_INFO "asset_info"
#define LM_LICENSE_CKSUM "ck"
#define LM_LICENSE_ISSUER "ISSUER"
#define LM_LICENSE_NOTICE "NOTICE"
#define LM_LICENSE_OVERDRAFT_STRING "OVERDRAFT"
#define LM_LICENSE_SUITE_STRING "SUITE"


/*
 *	For future use - DO NOT USE
 */
#define LM_LICENSE_LINGER_STRING "LINGER"
#define LM_LICENSE_DUP_STRING "DUP_GROUP"
#define LM_LICENSE_PREREQ "PREREQ"
#define LM_LICENSE_SUBLIC "SUBLIC"
#define LM_LICENSE_DIST_CONSTRAINT "DIST_CONSTRAINT"
/*
 *	Wrapper - GSI use only
 */
#define LM_LICENSE_WBINARY_STRING "w_binary"
#define LM_LICENSE_WARGV0_STRING "w_argv"
#define LM_LICENSE_WQUEUE_STRING "w_queue"
#define LM_LICENSE_WTERMS_STRING "w_term_signal"
#define LM_LICENSE_WLOSS_STRING "W_LIC_LOSS"


/*
 *	License file pointer returned by l_open_file()
 */



typedef struct lm_hostid_redirect {
				HOSTID from;
				HOSTID to;
				struct lm_hostid_redirect FAR *next;
				} LM_HOSTID_REDIRECT,
				  FAR *LM_HOSTID_REDIRECT_PTR;
	
typedef unsigned LONG LM_TIMER;
/*
 *	MSGQUEUE -- used by l_rcvmsg
 */


/*
 *	Handles returned by FLEXlm
 */


typedef struct lm_handle {
			   int type;		/* Type of struct */
			 } LM_HANDLE,	/* Handle returned by certain calls */
		           FAR *LM_HANDLE_PTR,
			   FAR * FAR *LM_HANDLE_PTR_PTR;

/*
 *	User data returned from the license server
 */

typedef int LM_LICENSE_HANDLE;
#define API_ENTRY




/*	
 *
 *	Module:	lm_code.h v3.20.1.1
 *	Last changed:  07 Apr 1995
 *	Description: 	Encryption codes to be used in a VENDORCODE macro 
 *			for FLEXlm daemons, create_license, lm_init(),
 *			and lm_checkout() call - modify these values 
 *			for your own use.  (The VENDOR_KEYx values
 *			are assigned by Globetrotter Software).
 *
 *	example LM_CODE() macro:
 *
 *		LM_CODE(var_name, ENCRYPTION_CODE_1, ENCRYPTION_CODE_2,
 *					VENDOR_KEY1, VENDOR_KEY2, 
 *					VENDOR_KEY3, VENDOR_KEY4);
 *
 */

/*
 *	ENCRYPTION_CODE_1 and 2
 *	VENDOR's private encryption seed
 *		These are 0x87654321 and 0x12345678 by default.
 *		Each vendor must ensure that you replace these with
 *		numbers which are unique to your company, and keep these
 *		numbers secret.  Only someone with access to these
 *		numbers can generate license files that will work with
 *		your application.
 *		MAKE SURE the numbers are not left to the defaults.
 */

#define ENCRYPTION_CODE_1 0x5fb36a92 
#define ENCRYPTION_CODE_2 0xa375cd18

/*
 *	FLEXlm vendor keys
 */


#define VENDOR_KEY1     0xbd873cdf
#define VENDOR_KEY2     0x0701d11d
#define VENDOR_KEY3     0x410a2b7a
#define VENDOR_KEY4     0x6b2493f0
#define VENDOR_KEY5     0xbd84ee7f


/*
 *	FLEXlm vendor name
 */

#define VENDOR_NAME     "sgifd"

/* prototypes */
char *  l_extract_licstring(char*, char*);
int l_valid_version(char *);
int l_parse_feature_line_(char *, char *, CONFIG *);
int l_compare_version_(char *, char *);
int local_verify_conf_( CONFIG *, char *,VENDORCODE *, LONG *, LONG *);
char * l_crypt_private_(CONFIG *, char *, VENDORCODE *);
char * l_extract_date(char *);
void l_get_id_( HOSTID *, char *, char *);
int l_exp(char *, LONG *);
int l_start(char *, LONG *);
unsigned LONG l_svk( char *, VENDORCODE *);
int l_good_bin_date(char *);
int check_licence(char *, char *, LONG *, LONG *);
int fill_date(char *dstr, int *date, char *month, int *year);
#endif /* _FLEXLM_H_ */
