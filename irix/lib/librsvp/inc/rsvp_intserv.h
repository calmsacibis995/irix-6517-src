/*
 * @(#) $Id: rsvp_intserv.h,v 1.2 1998/11/25 08:43:36 eddiem Exp $
 */

/**************************************************************************
 *         rapi_intserv.h
 *
 *	Defines formats of integrated services data structures.
 *	Matches: draft-ietf-intserv-rsvp-use-01.txt
 *
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

#ifndef __rsvpintserv_h__
#define __rsvpintserv_h__

#include "rsvp_types.h"
#include <sys/socket.h>
#ifndef NET_IF_H
#define NET_IF_H
#include <net/if.h>
#endif


__BEGIN_DECLS
typedef float	float32_t;
#define wordsof(x)	(((x)+3)/4)	/* Number of 32-bit words */

/**************************************************************************
 *
 *	Int-Serv Data Structures
 *
 **************************************************************************/


/*
 *	Service numbers
 */
#define	GENERAL_INFO		1
#define	GUARANTEED_SERV		2
#define	PREDICTIVE_SERV		3
#define	CONTROLLED_DELAY_SERV	4
#define	CONTROLLED_LOAD_SERV	5

/*
 *	Well-known parameter IDs
 */
enum  int_serv_wkp {
	IS_WKP_HOP_CNT =	4,
	IS_WKP_PATH_BW = 	6,
	IS_WKP_MIN_LATENCY =	8,
	IS_WKP_COMPOSED_MTU =	10,
	IS_WKP_TB_TSPEC = 	127	/* Token-bucket TSPEC parm */
};

/*
 *	Int-serv Main header
 */
typedef struct {
	u_char		ismh_version;	/* Version */
#define INTSERV_VERS_MASK 	0xf0
#define INTSERV_VERSION0	0
#define Intserv_Version(x)	(((x)&INTSERV_VERS_MASK)>>4)
#define Intserv_Version_OK(x) (((x)->ismh_version&INTSERV_VERS_MASK)== \
					INTSERV_VERSION0)

	u_char		ismh_unused;
	u_int16_t	ismh_len32b;	/* # 32-bit words excluding this hdr */
}  IS_main_hdr_t;

/* Convert ishm_length to equivalent RSVP object size, for checking */
#define Intserv_Obj_size(x) (((IS_main_hdr_t *)(x))->ismh_len32b * 4 + \
			sizeof(IS_main_hdr_t) + sizeof(Object_header))

/*
 *	Int-serv Service Element Header
 */
typedef struct {
	u_char		issh_service;	/* Service number	*/
	u_char		issh_flags;	/* Flag byte		*/
#define ISSH_BREAK_BIT	0x80		/* Flag: Break bit	*/

	u_int16_t	issh_len32b;	/* #32-bit words excluding this hdr */
}  IS_serv_hdr_t;

#define Issh_len32b(p)  ((p)->issh_len32b) /* (Not necessary now) 	*/

/*
 *	Int-serv Parameter Element Header
 */
typedef struct {
	u_char		isph_parm_num;	/* Parameter number */
	u_char		isph_flags;	/* Flags */
#define ISPH_FLG_INV	0x80		/* Flag: Invalid */

	u_int16_t	isph_len32b;	/* #32-bit words excluding this hdr */
}  IS_parm_hdr_t;

#define Set_Main_Hdr(p, len)	{(p)->ismh_version = INTSERV_VERSION0; \
				(p)->ismh_unused = 0; \
				(p)->ismh_len32b = wordsof(len); }

#define Set_Serv_Hdr(p, s, len) {(p)->issh_service = (s); \
				(p)->issh_flags = 0; \
				(p)->issh_len32b = wordsof(len); }

#define Set_Parm_Hdr(p, id, len) {(p)->isph_parm_num = (id); \
				(p)->isph_flags = 0; \
				(p)->isph_len32b = wordsof(len); }

#define Set_Break_Bit(p)  	((IS_serv_hdr_t *)p)->issh_flags|=ISSH_BREAK_BIT

#define Next_Main_Hdr(p)   (IS_main_hdr_t *)((u_int32_t *)(p)+1+(p)->ismh_len32b)
#define Next_Serv_Hdr(p)   (IS_serv_hdr_t *)((u_int32_t *)(p)+1+(p)->issh_len32b)
#define Next_Parm_Hdr(p)   (IS_parm_hdr_t *)((u_int32_t *)(p)+1+(p)->isph_len32b)

/**************************************************************************
 *
 *	Generic Tspec format
 *
 **************************************************************************/

#define TB_MIN_RATE	1
#define TB_MAX_RATE	40e12
#define TB_MIN_DEPTH	1
#define TB_MAX_DEPTH	250e9

/*
 *	Generic Tspec Parameters
 */
typedef struct {
	float32_t	TB_Tspec_r;		/* Token bucket rate (B/sec) */
	float32_t	TB_Tspec_b;		/* Token bucket depth (B) */
	float32_t	TB_Tspec_p;		/* Peak data rate (B/sec) */
	u_int32_t	TB_Tspec_m;		/* Min Policed Unit (B) */
	u_int32_t	TB_Tspec_M;		/* Max pkt size (B)	*/
}   TB_Tsp_parms_t;

/*
 *	Generic Tspec
 */
typedef struct {
	IS_serv_hdr_t	gen_Tspec_serv_hdr;	/* (GENERAL_INFO, length) */
	IS_parm_hdr_t	gen_Tspec_parm_hdr;	/* (IS_WKP_TB_TSPEC,) */
	TB_Tsp_parms_t	gen_Tspec_parms;
}    gen_Tspec_t;

#define gtspec_r	gen_Tspec_parms.TB_Tspec_r
#define gtspec_b	gen_Tspec_parms.TB_Tspec_b
#define gtspec_m	gen_Tspec_parms.TB_Tspec_m
#define gtspec_M	gen_Tspec_parms.TB_Tspec_M
#define gtspec_p	gen_Tspec_parms.TB_Tspec_p
#define gtspec_parmno	gen_Tspec_parm_hdr.isph_parm_num
#define gtspec_flags	gen_Tspec_parm_hdr.isph_flags

#define gtspec_len	(sizeof(gen_Tspec_t) - sizeof(IS_serv_hdr_t))

/**************************************************************************
 *
 *	Formats for Controlled-Load service
 *
 **************************************************************************/

/*
 *	Controlled-Load Flowspec
 */
typedef struct {
	IS_serv_hdr_t	CL_spec_serv_hdr;    /* (CONTROLLED_LOAD_SERV,0,len)*/
	IS_parm_hdr_t	CL_spec_parm_hdr;    /* (IS_WKP_TB_TSPEC,)	*/
	TB_Tsp_parms_t	CL_spec_parms;	     /* 			*/
}  CL_flowspec_t;

#define CLspec_r	CL_spec_parms.TB_Tspec_r
#define CLspec_b	CL_spec_parms.TB_Tspec_b
#define CLspec_p	CL_spec_parms.TB_Tspec_p
#define CLspec_m	CL_spec_parms.TB_Tspec_m
#define CLspec_M	CL_spec_parms.TB_Tspec_M
#define CLspec_parmno	CL_spec_parm_hdr.isph_parm_num
#define CLspec_flags	CL_spec_parm_hdr.isph_flags
#define CLspec_len32b	CL_spec_parm_hdr.isph_len32b

#define CLspec_len	(sizeof(CL_flowspec_t) - sizeof(IS_serv_hdr_t))

/**************************************************************************
 *
 *	Formats for Guaranteed service
 *
 **************************************************************************/

/*	Service-specific Parameter IDs
 */
enum	{
	IS_GUAR_RSPEC =		130,

	GUAR_ADSPARM_C	=	131,
	GUAR_ADSPARM_D  =	132,
	GUAR_ADSPARM_Ctot =	133,
	GUAR_ADSPARM_Dtot =	134,
	GUAR_ADSPARM_Csum =	135,
	GUAR_ADSPARM_Dsum =	136
};

/*
 *	Guaranteed Rspec parameters
 */
typedef struct {
	float32_t	Guar_R;			/*  Guaranteed Rate: B/s */
	u_int32_t	Guar_S;			/*  Slack term: microsecs*/
}    guar_Rspec_t;

/*
 *	Guaranteed Flowspec
 */
typedef struct {
	IS_serv_hdr_t	Guar_serv_hdr;		/* (GUARANTEED, 0, length) */

	IS_parm_hdr_t	Guar_Tspec_hdr;		/* (IS_WKP_TB_TSPEC,)      */
	TB_Tsp_parms_t	Guar_Tspec_parms;	/* GENERIC Tspec parms     */

	IS_parm_hdr_t	Guar_Rspec_hdr;		/* (IS_GUAR_RSPEC,)        */
	guar_Rspec_t	Guar_Rspec;		/* Guaranteed rate (B/sec) */
}   Guar_flowspec_t;

#define Gspec_r		Guar_Tspec_parms.TB_Tspec_r
#define Gspec_b		Guar_Tspec_parms.TB_Tspec_b
#define Gspec_p		Guar_Tspec_parms.TB_Tspec_p
#define Gspec_m		Guar_Tspec_parms.TB_Tspec_m
#define Gspec_M		Guar_Tspec_parms.TB_Tspec_M
#define Gspec_R		Guar_Rspec.Guar_R
#define Gspec_S		Guar_Rspec.Guar_S
#define Gspec_T_parmno	Guar_Tspec_hdr.isph_parm_num
#define Gspec_T_flags	Guar_Tspec_hdr.isph_flags
#define Gspec_R_parmno	Guar_Rspec_hdr.isph_parm_num
#define Gspec_R_flags	Guar_Rspec_hdr.isph_flags

#define Gspec_len	(sizeof(Guar_flowspec_t) - sizeof(IS_serv_hdr_t))	

/*
 *	Guaranteed Adspec parameters -- fixed part
 */
typedef struct {
	IS_serv_hdr_t	Gads_serv_hdr;	/* (GUARANTEED, x, len) */
	IS_parm_hdr_t	Gads_Ctot_hdr;	/* (GUAR_ADSPARM_Ctot,) */
	u_int32_t	Gads_Ctot;
	IS_parm_hdr_t	Gads_Dtot_hdr;	/* (GUAR_ADSPARM_Dtot,) */
	u_int32_t	Gads_Dtot;
	IS_parm_hdr_t	Gads_Csum_hdr;	/* (GUAR_ADSPARM_Csum,) */
	u_int32_t	Gads_Csum;
	IS_parm_hdr_t	Gads_Dsum_hdr;	/* (GUAR_ADSPARM_Dsum,) */
	u_int32_t	Gads_Dsum;
	/*
	 *	May be followed by override general param values
	 */
}   Gads_parms_t;


/**************************************************************************
 *
 *	Basic Adspec Pieces
 *
 **************************************************************************/

/*
 *	General Path Characterization Parameters
 */
typedef struct {
	IS_serv_hdr_t	gen_parm_hdr;		/* (GENERAL_INFO, len)   */
	IS_parm_hdr_t	gen_parm_hopcnt_hdr;	/* (IS_WKP_HOP_CNT,)    */
	u_int32_t	gen_parm_hopcnt;
	IS_parm_hdr_t	gen_parm_pathbw_hdr;	/* (IS_WKP_PATH_BW,)    */
	float32_t	gen_parm_path_bw;
	IS_parm_hdr_t	gen_parm_minlat_hdr;	/* (IS_WKP_MIN_LATENCY,) */
	u_int32_t	gen_parm_min_latency;
	IS_parm_hdr_t	gen_parm_compmtu_hdr;	/* (IS_WKP_COMPOSED_MTU,) */
	u_int32_t	gen_parm_composed_MTU;
}    genparm_parms_t;


/*
 *	Minimal Adspec per-service fragment: an empty service header.
 */
typedef struct {
	IS_serv_hdr_t	mads_hdr;		/* (<service>, 1, len=0) */
}   Min_adspec_t;


/**************************************************************************/

/*
 *	Contents of int-serv flowspec
 */
typedef struct {
	IS_main_hdr_t	spec_mh;
	union {
		CL_flowspec_t    CL_spec;	/* Controlled-Load service */
		Guar_flowspec_t  G_spec;	/* Guaranteed service	*/
	}   		spec_u;
}   IS_specbody_t;

#define ISmh_len32b	spec_mh.ismh_len32b
#define ISmh_version	spec_mh.ismh_version
#define ISmh_unused	spec_mh.ismh_unused

/*
 *	Contents of int-serv Tspec
 */
typedef struct {
	IS_main_hdr_t	st_mh;
        union {
		/* There could be service-dependent Tspecs, but there aren't.
		 */
        	gen_Tspec_t	gen_stspec;	/* Generic Tspec	*/
	}		tspec_u;
}   IS_tspbody_t;

#define IStmh_len32b	st_mh.ismh_len32b
#define IStmh_version	st_mh.ismh_version
#define IStmh_unused	st_mh.ismh_unused

/*
 *	Contents of (minimal) int-serv Adspec
 */
typedef struct {
	IS_main_hdr_t	adspec_mh;	/* Main header			   */
	genparm_parms_t adspec_genparms;/* General char parm fragment	   */
	/*
	 *	Followed by variable-length fragments for some or all
	 *	services.  These can be minimal length fragments.
	 */
}   IS_adsbody_t;

#define MAX_ADSPEC_LEN  sizeof(Object_header) + sizeof(IS_adsbody_t ) +  \
			sizeof(Gads_parms_t) + sizeof(Min_adspec_t) +  \
			2*sizeof(genparm_parms_t)
#define DFLT_ADSPEC_LEN sizeof(Object_header) + sizeof(IS_adsbody_t) + \
			2*sizeof(Min_adspec_t)



__END_DECLS

#endif				/* __rsvpintserv_h_ */
