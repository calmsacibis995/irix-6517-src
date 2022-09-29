/*
 *  @(#) $Id: rsvp_specs.c,v 1.11 1998/11/25 08:43:14 eddiem Exp $
 */

/************************ rsvp_specs.c ***************************
 *                                                               *
 *  Routines that know service-dependent formats -- flowspecs,   *
 *  Adspecs, and Tspecs.  These routines merge, convert byte     *
 *  order, and format these objects.                             *
 *                                                               *
 *****************************************************************/

/*****************************************************************************/
/*                                                                           */
/*          RSVPD -- ReSerVation Protocol Daemon                             */
/*                                                                           */
/*              USC Information Sciences Institute                           */
/*              Marina del Rey, California                                   */
/*                                                                           */
/*		Original Version: Shai Herzog, Nov. 1993.                    */
/*		Current Version:  Steven Berson & Bob Braden, May 1996.      */
/*                                                                           */
/*      Copyright (c) 1996 by the University of Southern California          */
/*      All rights reserved.                                                 */
/*                                                                           */
/*	Permission to use, copy, modify, and distribute this software and    */
/*	its documentation in source and binary forms for any purpose and     */
/*	without fee is hereby granted, provided that both the above          */
/*	copyright notice and this permission notice appear in all copies,    */
/*	and that any documentation, advertising materials, and other         */
/*	materials related to such distribution and use acknowledge that      */
/*	the software was developed in part by the University of Southern     */
/*	California, Information Sciences Institute.  The name of the         */
/*	University may not be used to endorse or promote products derived    */
/*	from this software without specific prior written permission.        */
/*                                                                           */
/*                                                                           */
/*	THE UNIVERSITY OF SOUTHERN CALIFORNIA makes no representations about */
/*	the suitability of this software for any purpose.  THIS SOFTWARE IS  */
/*	PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,      */
/*	INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF             */
/*	MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.                */
/*                                                                           */
/*****************************************************************************/

#include <stdio.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <assert.h>
#include <string.h>

#include "rsvp_daemon.h"

#ifdef STANDARD_C_LIBRARY
#  include <stdlib.h>
#  include <stddef.h>           /* for offsetof */
#else
#  include <malloc.h>
#  include <memory.h>
#endif

#ifdef RSVP_TCPDUMP
#define _MACHINE_ENDIAN_H_ 1
#endif

char *fmt_flowspec1();

#ifdef RSVP_TCPDUMP
extern char *getname();
#define ipaddr_string(p) getname(p)	/* tcpdump print routine */
#else
#define iptoname(p)	 fmt_hostname(p)
#define ipaddr_string(p) iptoname(p)	/* local routine */
char	*fmt_hostname(struct in_addr *);
#endif

/*	Endian conversions for Integrated Services headers
 *
 */
#define NTOH_IS_Main_Hdr(p) NTOH16((p)->ismh_len32b)
#define NTOH_IS_Serv_Hdr(p) NTOH16((p)->issh_len32b)
#define NTOH_IS_Parm_Hdr(p) NTOH16((p)->isph_len32b)

#define HTON_IS_Main_Hdr(p) HTON16((p)->ismh_len32b)
#define HTON_IS_Parm_Hdr(p) HTON16((p)->isph_len32b)
#define HTON_IS_Serv_Hdr(p) HTON16((p)->issh_len32b)

/*
 *	External declarations
 */
char *  rapi_fmt_specbody3(IS_specbody_t *);
char *	rapi_fmt_tspecbody3(IS_tspbody_t *);
char *	rapi_fmt_adspecbody3(IS_adsbody_t *);

/*
 *	Forward declarations
 */
int	Compare_Flowspecs(FLOWSPEC *, FLOWSPEC *);
FLOWSPEC *LUB_of_Flowspecs(FLOWSPEC *, FLOWSPEC *);
FLOWSPEC *GLB_of_Flowspecs(FLOWSPEC *, FLOWSPEC *);
int	Compare_Tspecs(SENDER_TSPEC *, SENDER_TSPEC *);
int	addTspec2sum(SENDER_TSPEC *, SENDER_TSPEC *);
int	newTspec_sum(SENDER_TSPEC *, SENDER_TSPEC *);
int	Compare_Adspecs(ADSPEC *, ADSPEC *);
int	New_Adspec(ADSPEC *);
char *	fmt_flowspec(FLOWSPEC *);
char *	fmt_tspec(SENDER_TSPEC *);
char *	fmt_adspec(ADSPEC *);
void	hton_flowspec(FLOWSPEC *);
void	ntoh_flowspec(FLOWSPEC *);
void	hton_tspec(SENDER_TSPEC *);
void	ntoh_tspec(SENDER_TSPEC *);
void	hton_adspec(ADSPEC *);
void	ntoh_adspec(ADSPEC *);
void	hton_genparm(IS_parm_hdr_t *);
void	ntoh_genparm(IS_parm_hdr_t *);


/*
 * Compare_Flowspec(): Compare two flowspecs and return one of:
 *	SPEC1_GTR:	First is bigger
 *	SPECS_EQL:	Two are equal (identical)
 *	SPEC2_GTR:	Second is bigger
 *	SPECS_USELUB:	An LUB different from both can be computed
 *	SPECS_INCOMPAT: They are incompatible.
 *	SPEC1_BAD:	First contains error
 *	SPEC2_BAD:	Second contains error
 *
 */
int
Compare_Flowspecs(FLOWSPEC *s1, FLOWSPEC *s2)
	{
	IS_serv_hdr_t	*sp1, *sp2;

	if (s1 == NULL) {
		if (s2 == NULL)
			return SPECS_EQL;
		else
			return SPEC1_LSS;
	}
	if (s2 == NULL)
		return SPEC1_GTR;

	if (Obj_CType(s1) != ctype_FLOWSPEC_Intserv0)
		return SPEC1_BAD;
	if (Obj_CType(s2) != ctype_FLOWSPEC_Intserv0)
		return SPEC2_BAD;

	sp1 = (IS_serv_hdr_t *) &s1->flow_body.spec_u;
	sp2 = (IS_serv_hdr_t *) &s2->flow_body.spec_u;

	/* For now, regard two different services as incompatible.
	 */
	if (sp1->issh_service != sp2->issh_service)
		return SPECS_INCOMPAT;

	switch (sp1->issh_service) {

	    case GUARANTEED_SERV:
		{ Guar_flowspec_t *o1 = (Guar_flowspec_t *) sp1;
		  Guar_flowspec_t *o2 = (Guar_flowspec_t *) sp2;

		if (o1->Gspec_r == o2->Gspec_r
		 && o1->Gspec_b == o2->Gspec_b
		 && o1->Gspec_p == o2->Gspec_p
		 && o1->Gspec_m == o2->Gspec_m
		 && o1->Gspec_M == o2->Gspec_M
		 && o1->Gspec_R == o2->Gspec_R
		 && o1->Gspec_S == o2->Gspec_S)
			return SPECS_EQL;

		if (o1->Gspec_r >= o2->Gspec_r
		 && o1->Gspec_b >= o2->Gspec_b
		 && o1->Gspec_p >= o2->Gspec_p
		 && o1->Gspec_m <= o2->Gspec_m
		 && o1->Gspec_M >= o2->Gspec_M
		 && o1->Gspec_R >= o2->Gspec_R
		 && o1->Gspec_S <= o2->Gspec_S)
			return SPEC1_GTR;

		if (o1->Gspec_r <= o2->Gspec_r
		 && o1->Gspec_b <= o2->Gspec_b
		 && o1->Gspec_p <= o2->Gspec_p
		 && o1->Gspec_m >= o2->Gspec_m
		 && o1->Gspec_M <= o2->Gspec_M
		 && o1->Gspec_R <= o2->Gspec_R
		 && o1->Gspec_S >= o2->Gspec_S)
			return SPEC1_LSS;

		return SPECS_USELUB;
		}

	    case CONTROLLED_LOAD_SERV:
		{ CL_flowspec_t *o1 = (CL_flowspec_t *) sp1;
		  CL_flowspec_t *o2 = (CL_flowspec_t *) sp2;

		if (o1->CLspec_r == o2->CLspec_r
		 && o1->CLspec_b == o2->CLspec_b
		 && o1->CLspec_m == o2->CLspec_m
		 && o1->CLspec_M == o2->CLspec_M)
			return SPECS_EQL;

		if (o1->CLspec_r >= o2->CLspec_r
		 && o1->CLspec_b >= o2->CLspec_b
		 && o1->CLspec_m <= o2->CLspec_m
		 && o1->CLspec_M >= o2->CLspec_M)
			return SPEC1_GTR;

		if (o1->CLspec_r <= o2->CLspec_r
		 && o1->CLspec_b <= o2->CLspec_b
		 && o1->CLspec_m >= o2->CLspec_m
		 && o1->CLspec_M <= o2->CLspec_M)
			return SPEC1_LSS;

		return SPECS_USELUB;
		}

	default:
		break;
	}
	return SPECS_INCOMPAT;
}

/*
 *  Merge two flowspecs by computing their LUB.
 *
 *	Be warned that LUB_of_Flowspecs() uses a static buffer to return
 * 	flowspecs, make sure to copy ...
 */
FLOWSPEC  *
LUB_of_Flowspecs(FLOWSPEC *s1, FLOWSPEC *s2)
	{
	IS_serv_hdr_t	*sp1, *sp2;
	static FLOWSPEC		sx;

	if (s1 == NULL)
		return(s2);
	else if (s2 == NULL)
		return(s1);

	if (Obj_CType(s1) != ctype_FLOWSPEC_Intserv0)
		return NULL;

	if (Intserv_Obj_size(&s1->flow_body.spec_mh) != Obj_Length(s1) ||
	  		 !Intserv_Version_OK(&s1->flow_body.spec_mh))
		return NULL;

	sp1 = (IS_serv_hdr_t *) &s1->flow_body.spec_u;
	sp2 = (IS_serv_hdr_t *) &s2->flow_body.spec_u;
	if (sp1->issh_service != sp2->issh_service)
		return(NULL);

	memset((char *)&sx, 0, sizeof(FLOWSPEC));
	Obj_Class(&sx) = class_FLOWSPEC;
	Obj_CType(&sx) = ctype_FLOWSPEC_Intserv0;
	sx.flow_body.spec_mh.ismh_version = INTSERV_VERSION0;

	switch (sp1->issh_service) {

	    case GUARANTEED_SERV:
		{ Guar_flowspec_t *gp1 = (Guar_flowspec_t *) sp1;
		  Guar_flowspec_t *gp2 = (Guar_flowspec_t *) sp2;
		  Guar_flowspec_t *gpx = (Guar_flowspec_t *) 
							&sx.flow_body.spec_u;
		
		Obj_Length(&sx) = sizeof(Guar_flowspec_t) +
				sizeof(IS_main_hdr_t) + sizeof(Object_header);
		Set_Main_Hdr(&sx.flow_body.spec_mh, sizeof(Guar_flowspec_t));
		Set_Serv_Hdr(&gpx->Guar_serv_hdr, GUARANTEED_SERV, Gspec_len);
		Set_Parm_Hdr(&gpx->Guar_Tspec_hdr, IS_WKP_TB_TSPEC,
						sizeof(TB_Tsp_parms_t));
		gpx->Gspec_r = MAX(gp1->Gspec_r, gp2->Gspec_r);
		gpx->Gspec_b = MAX(gp1->Gspec_b, gp2->Gspec_b);
		gpx->Gspec_p = MAX(gp1->Gspec_p, gp2->Gspec_p);
		gpx->Gspec_m = MIN(gp1->Gspec_m, gp2->Gspec_m);
		gpx->Gspec_M = MAX(gp1->Gspec_M, gp2->Gspec_M);
		Set_Parm_Hdr(&gpx->Guar_Rspec_hdr, IS_GUAR_RSPEC,
						sizeof(guar_Rspec_t));
		gpx->Gspec_R = MAX(gp1->Gspec_R, gp2->Gspec_R);
		gpx->Gspec_S = MIN(gp1->Gspec_S, gp2->Gspec_S);
		/*
		 * Sanity check of merged reservation can go here
		 */
		}
		break;

	    case CONTROLLED_LOAD_SERV:
		{ CL_flowspec_t *clp1 = (CL_flowspec_t *) sp1;
		  CL_flowspec_t *clp2 = (CL_flowspec_t *) sp2;
		  CL_flowspec_t *clpx = (CL_flowspec_t *) 
					&sx.flow_body.spec_u.CL_spec;

		Obj_Length(&sx) = sizeof(CL_flowspec_t) + sizeof(IS_main_hdr_t)
						+ sizeof(Object_header);
		Set_Main_Hdr(&sx.flow_body.spec_mh, sizeof(CL_flowspec_t));
		Set_Serv_Hdr(&clpx->CL_spec_serv_hdr, CONTROLLED_LOAD_SERV,
							CLspec_len);
		Set_Parm_Hdr(&clpx->CL_spec_parm_hdr, IS_WKP_TB_TSPEC,
							sizeof(TB_Tsp_parms_t));

		clpx->CLspec_r = MAX(clp1->CLspec_r, clp2->CLspec_r);
		clpx->CLspec_b = MAX(clp1->CLspec_b, clp2->CLspec_b);
		clpx->CLspec_p = INFINITY32f;
		clpx->CLspec_m = MIN(clp1->CLspec_m, clp2->CLspec_m);
		clpx->CLspec_M = MAX(clp1->CLspec_M, clp2->CLspec_M);
		}
		break;

	default:
		return NULL;
	}
	return (FLOWSPEC *) &sx;
}

/*
 *  Merge two flowspecs by computing their GLB.
 *
 *	Be warned that GLB_of_Flowspecs() uses a static buffer to return
 * 	flowspecs, make sure to copy ...
 */
FLOWSPEC  *
GLB_of_Flowspecs(FLOWSPEC *s1, FLOWSPEC *s2)
	{
	IS_serv_hdr_t	*sp1, *sp2;
	static FLOWSPEC		sx;

	if (s1 == NULL)
		return(s2);
	else if (s2 == NULL)
		return(s1);

	if (Obj_CType(s1) != ctype_FLOWSPEC_Intserv0)
		return NULL;

	if (Intserv_Obj_size(&s1->flow_body.spec_mh) != Obj_Length(s1) ||
	   		!Intserv_Version_OK(&s1->flow_body.spec_mh))
		return NULL;

	sp1 = (IS_serv_hdr_t *) &s1->flow_body.spec_u;
	sp2 = (IS_serv_hdr_t *) &s2->flow_body.spec_u;
	if (sp1->issh_service != sp2->issh_service)
		return(NULL);

	Obj_Class(&sx) = class_FLOWSPEC;
	Obj_CType(&sx) = ctype_FLOWSPEC_Intserv0;

	switch (sp1->issh_service) {

	    case GUARANTEED_SERV:
		{ Guar_flowspec_t *gp1 = (Guar_flowspec_t *) sp1;
		  Guar_flowspec_t *gp2 = (Guar_flowspec_t *) sp2;
		  Guar_flowspec_t *gpx = (Guar_flowspec_t *) 
							&sx.flow_body.spec_u;
		
		Obj_Length(&sx) = sizeof(Guar_flowspec_t) +
				sizeof(IS_main_hdr_t) + sizeof(Object_header);

		Set_Main_Hdr(&sx.flow_body.spec_mh, sizeof(Guar_flowspec_t));
		Set_Serv_Hdr(&gpx->Guar_serv_hdr, GUARANTEED_SERV, Gspec_len);
		Set_Parm_Hdr(&gpx->Guar_Tspec_hdr, IS_WKP_TB_TSPEC,
							sizeof(TB_Tsp_parms_t));

		gpx->Gspec_r = MIN(gp1->Gspec_r, gp2->Gspec_r);
		gpx->Gspec_b = MIN(gp1->Gspec_b, gp2->Gspec_b);
		gpx->Gspec_p = MIN(gp1->Gspec_p, gp2->Gspec_p);
		gpx->Gspec_m = MAX(gp1->Gspec_m, gp2->Gspec_m);
		gpx->Gspec_M = MIN(gp1->Gspec_M, gp2->Gspec_M);
		Set_Parm_Hdr(&gpx->Guar_Rspec_hdr, IS_GUAR_RSPEC,
							sizeof(guar_Rspec_t));
		gpx->Gspec_R = MIN(gp1->Gspec_R, gp2->Gspec_R);
		gpx->Gspec_S = MAX(gp1->Gspec_S, gp2->Gspec_S);
		/*
		 * Sanity check of merged reservation can go here
		 */
		}
		break;

	    case CONTROLLED_LOAD_SERV:
		{ CL_flowspec_t *clp1 = (CL_flowspec_t *) sp1;
		  CL_flowspec_t *clp2 = (CL_flowspec_t *) sp2;
		  CL_flowspec_t *clpx = (CL_flowspec_t *) &sx.flow_body.spec_u;

		Obj_Length(&sx) = sizeof(CL_flowspec_t) + sizeof(IS_main_hdr_t)
					 + sizeof(Object_header);
		Set_Main_Hdr(&sx.flow_body.spec_mh, sizeof(CL_flowspec_t));
		Set_Serv_Hdr(&clpx->CL_spec_serv_hdr, CONTROLLED_LOAD_SERV,
							CLspec_len);
		Set_Parm_Hdr(&clpx->CL_spec_parm_hdr, IS_WKP_TB_TSPEC,
							sizeof(TB_Tsp_parms_t));

		clpx->CLspec_r = MIN(clp1->CLspec_r, clp2->CLspec_r);
		clpx->CLspec_b = MIN(clp1->CLspec_b, clp2->CLspec_b);
		clpx->CLspec_p = INFINITY32f;
		clpx->CLspec_m = MAX(clp1->CLspec_m, clp2->CLspec_m);
		clpx->CLspec_M = MIN(clp1->CLspec_M, clp2->CLspec_M);
		}
		break;

	default:
		return NULL;
	}
	return (FLOWSPEC *) &sx;
}

/*
 *  De-Merge two flowspecs.  Used while processing a ResvErr message, to
 *	determine where to forward the error.  Call is:
 *		demerge_flowspecs(Err_Q, RSB_Q),
 *	where Err_Q = flowspec from ResvErr message, and RSB_Q = flowspec in
 *	a reservation.   May return:
 *		SPECS_NEQ if RSB_Q did not contribute to Err_Q
 *		SPECS_EQL if RSB_Q did contribute to Err_Q
 *		SPECS_USELUB if RSB_Q did contribute to Err_Q but only via LUB.
 */
int
demerge_flowspecs(FLOWSPEC *spe, FLOWSPEC *spr)
	{
	IS_serv_hdr_t	*sp1, *sp2;

	if (Obj_CType(spe) != ctype_FLOWSPEC_Intserv0)
		return SPECS_NEQ;

	if (Intserv_Obj_size(&spe->flow_body.spec_mh) != Obj_Length(spe) ||
	    !Intserv_Version_OK(&spe->flow_body.spec_mh))
		return SPECS_NEQ;

	sp1 = (IS_serv_hdr_t *) &spe->flow_body.spec_u;
	sp2 = (IS_serv_hdr_t *) &spr->flow_body.spec_u;

	if (sp1->issh_service != sp2->issh_service)
		return(NULL);

	switch (sp1->issh_service) {

	    case GUARANTEED_SERV:
		{ Guar_flowspec_t *o1 = (Guar_flowspec_t *) sp1;
		  Guar_flowspec_t *o2 = (Guar_flowspec_t *) sp2;
		
		if (o1->Gspec_r == o2->Gspec_r
		 || o1->Gspec_b == o2->Gspec_b
		 || o1->Gspec_p == o2->Gspec_p
		 || o1->Gspec_m == o2->Gspec_m
		 || o1->Gspec_M == o2->Gspec_M
		 || o1->Gspec_R == o2->Gspec_R
		 || o1->Gspec_S == o2->Gspec_S)
			return SPECS_EQL;
		else
			return SPECS_USELUB;
		}

	    case CONTROLLED_LOAD_SERV:
		{ CL_flowspec_t *o1 = (CL_flowspec_t *) sp1;
		  CL_flowspec_t *o2 = (CL_flowspec_t *) sp2;

		if (o1->CLspec_r == o2->CLspec_r
		 || o1->CLspec_b == o2->CLspec_b
		 || o1->CLspec_p == o2->CLspec_p
		 || o1->CLspec_m == o2->CLspec_m
		 || o1->CLspec_M == o2->CLspec_M)
			return SPECS_EQL;
		else
			return SPECS_USELUB;
		}

	default:
		break;
	}
	return SPECS_NEQ;
}


/*
 *	Compare two Sender Tspecs for equality, and return one of:
 *		SPECS_EQL:	Two are equal (identical)
 *		SPECS_NEQ:	Two are unequal
 *		SPECS_INCOMPAT: They are incompatible.
 *		SPEC1_BAD:	First contains error
 *		SPEC2_BAD:	Second contains error
 */
int
Compare_Tspecs(SENDER_TSPEC *st1, SENDER_TSPEC *st2)
	{
	IS_serv_hdr_t	*sp1, *sp2;

	if (!st1)
		return (!st2)? SPECS_EQL: SPECS_NEQ;
	else if (!st2)
		return SPECS_NEQ;

	if (Obj_CType(st1) != ctype_SENDER_TSPEC)
		return SPEC1_BAD;
	if (Obj_CType(st2) != ctype_SENDER_TSPEC)
		return SPEC2_BAD;

	if (Intserv_Obj_size(&st1->stspec_body.st_mh) > Obj_Length(st1) ||
	    !Intserv_Version_OK(&st1->stspec_body.st_mh))
		return SPEC1_BAD;
	if (Intserv_Obj_size(&st2->stspec_body.st_mh) > Obj_Length(st2) ||
	    !Intserv_Version_OK(&st2->stspec_body.st_mh))
		return SPEC2_BAD;

	sp1 = (IS_serv_hdr_t *) &st1->stspec_body.tspec_u;
	sp2 = (IS_serv_hdr_t *) &st2->stspec_body.tspec_u;

	/* For now, regard two different services as incompatible.
	 */
	if (sp1->issh_service != sp2->issh_service)
		return SPECS_INCOMPAT;

	switch (sp1->issh_service) {

	    case CONTROLLED_LOAD_SERV:	/* XXX OBSOLETE */
	    case GUARANTEED_SERV:	/* XXX OBSOLETE */
	    case GENERAL_INFO:
		{ gen_Tspec_t *o1 = (gen_Tspec_t *) sp1;
		  gen_Tspec_t *o2 = (gen_Tspec_t *) sp2;

		if (st1->stspec_body.st_mh.ismh_len32b !=
					wordsof(sizeof(gen_Tspec_t)))
			return SPEC1_BAD;
		if (st2->stspec_body.st_mh.ismh_len32b !=
					wordsof( sizeof(gen_Tspec_t)))
			return SPEC2_BAD;
		if (o1->gtspec_r == o2->gtspec_r
		 && o1->gtspec_b == o2->gtspec_b
		 && o1->gtspec_p == o2->gtspec_p
		 && o1->gtspec_m == o2->gtspec_m
		 && o1->gtspec_M == o2->gtspec_M)
			return SPECS_EQL;

		else
			return SPECS_NEQ;
		}

	default:
		break;
	}
	return SPECS_INCOMPAT;
}


/*	Compute sum of two Tspecs, in second argument.
 */
int
addTspec2sum(SENDER_TSPEC *tsp1, SENDER_TSPEC *tsp2)
	{
	IS_serv_hdr_t	*sp1, *sp2;

	assert((tsp1) && (tsp2));
	sp1 = (IS_serv_hdr_t *) &tsp1->stspec_body.tspec_u;
	sp2 = (IS_serv_hdr_t *) &tsp2->stspec_body.tspec_u;

	if (sp2->issh_service == 0) {
		/*
		 *	First call: *tsp2 is zero.  Initialize it
		 *	appropriately from *tsp1.
		 */
		return(newTspec_sum(tsp1, tsp2));
	}
		
	switch (sp1->issh_service) {

	    case GENERAL_INFO:
		/* Guaranteed service uses generic Tspec format
		 */
		{ gen_Tspec_t *gp1 = (gen_Tspec_t *) sp1;
		  gen_Tspec_t *gp2 = (gen_Tspec_t *) sp2;

		if (tsp1->stspec_body.st_mh.ismh_len32b != 
						wordsof(sizeof(gen_Tspec_t)))
			return(-1);
		if (gp1->gtspec_parmno != IS_WKP_TB_TSPEC) {
			return(-1);
		}
		if (gp1->gtspec_flags & ISPH_FLG_INV)
			return(0);
		gp2->gtspec_r += gp1->gtspec_r;
		gp2->gtspec_b += gp1->gtspec_b;
		gp2->gtspec_p += gp1->gtspec_p;
		gp2->gtspec_m = MIN(gp2->gtspec_m, gp1->gtspec_m);
		gp2->gtspec_M = MAX(gp2->gtspec_M, gp1->gtspec_M);
		}
		break;
		
	    default:
		return(-1);
	}
	return(0);
}

/*	Initialize sum of Tspecs *tsp from Tspec *tsp_in
 *
 */
int
newTspec_sum(SENDER_TSPEC *tsp_in, SENDER_TSPEC *tsp)
	{
	IS_tspbody_t	*ISp = &tsp->stspec_body;
	IS_serv_hdr_t	*sp_in, *sp;

	assert(tsp);
	assert(Obj_Length(tsp) >= sizeof(IS_tspbody_t)+sizeof(Object_header));

	ISp->st_mh.ismh_version = INTSERV_VERSION0;
	sp = (IS_serv_hdr_t *) &tsp->stspec_body.tspec_u;
	sp_in = (IS_serv_hdr_t *) &tsp_in->stspec_body.tspec_u;

	switch (sp_in->issh_service) {

	    case GENERAL_INFO:
	    case GUARANTEED_SERV:		/* XXX OBSOLETE */
	    case CONTROLLED_LOAD_SERV:		/* XXX OBSOLETE */
		/* Guaranteed service uses generic Tspec format
		 */
		{
		gen_Tspec_t *gp = &tsp->stspec_body.tspec_u.gen_stspec;
		gen_Tspec_t *gp_in = &tsp_in->stspec_body.tspec_u.gen_stspec;

		Set_Main_Hdr(&ISp->st_mh, sizeof(gen_Tspec_t));
		Set_Serv_Hdr(&gp->gen_Tspec_serv_hdr, GENERAL_INFO, gtspec_len);
		Set_Parm_Hdr(&gp->gen_Tspec_parm_hdr, IS_WKP_TB_TSPEC,
							sizeof(TB_Tsp_parms_t));
		gp->gtspec_flags = gp_in->gtspec_flags;
		gp->gtspec_r = gp_in->gtspec_r;
		gp->gtspec_b = gp_in->gtspec_b;
		gp->gtspec_p = gp_in->gtspec_p;
		gp->gtspec_m = gp_in->gtspec_m;
		gp->gtspec_M = gp_in->gtspec_M;
		}
		break;
		
	    default:
		return(-1);
	}
	return(0);
}

/*	Compare two ADSPECs
 *		Return 1 if different, 0 if identical.   XXX NOT USED
 */
int
Compare_Adspecs(ADSPEC *ap1, ADSPEC *ap2)
	{
	int		ctype;
	IS_serv_hdr_t	*asp1, *asp2;

	if (!ap1 && !ap2)
		return(0);
	else if (!ap1||!ap2)
		return(1);

	ctype = Obj_CType(ap1);

	/*
	if (ctype != ctype_ADSPEC_INTSERV || ctype != Obj_CType(ap2))
		return(1);
	*/

	asp1 = (IS_serv_hdr_t *) &ap1->adspec_body.adspec_genparms;
	asp2 = (IS_serv_hdr_t *) &ap2->adspec_body.adspec_genparms;

	switch (asp1->issh_service) {

	    case GENERAL_INFO:
		{
		/* XXX TBD */
		return 0;
		}

	    case GUARANTEED_SERV:
		{
		/* XXX TBD */
		return 0;
		}

	    default:
		break;
	}
	return(0);
}

/*
 *	Construct new (minimal) Adspec.  Called at sender node when sender
 *	app does not provide its own initial adspec.
 */
int
New_Adspec(ADSPEC *ap)
	{
	IS_adsbody_t		*ISap = &ap->adspec_body;
	genparm_parms_t 	*genp = &ISap->adspec_genparms;
	IS_serv_hdr_t		*shp;	/* Fragment pointer */

	memset(ISap, 0, DFLT_ADSPEC_LEN-sizeof(Object_header));
	Set_Main_Hdr(&ISap->adspec_mh, 0);
	
 	Set_Serv_Hdr(&genp->gen_parm_hdr, GENERAL_INFO,
				sizeof(genparm_parms_t)-sizeof(IS_serv_hdr_t));
	Set_Parm_Hdr(&genp->gen_parm_hopcnt_hdr, IS_WKP_HOP_CNT,
						sizeof(u_int32_t));
	genp->gen_parm_hopcnt = 0;
	Set_Parm_Hdr(&genp->gen_parm_pathbw_hdr, IS_WKP_PATH_BW,
						sizeof(float32_t));
	genp->gen_parm_path_bw = INFINITY32f;
	Set_Parm_Hdr(&genp->gen_parm_minlat_hdr, IS_WKP_MIN_LATENCY,
						sizeof(float32_t));
	genp->gen_parm_min_latency = 0;
	Set_Parm_Hdr(&genp->gen_parm_compmtu_hdr, IS_WKP_COMPOSED_MTU,
						sizeof(float32_t));
	genp->gen_parm_composed_MTU = 65535;
	shp = (IS_serv_hdr_t *)(genp+1);

	Set_Serv_Hdr(shp, GUARANTEED_SERV, 0);
	Set_Break_Bit(shp);	/* We do not support GUARANTEED */
	shp++;
	Set_Serv_Hdr(shp, CONTROLLED_LOAD_SERV, 0);
	Set_Break_Bit(shp);	/* We do not support CONTROLLED LOAD */
	shp++;
	ISap->adspec_mh.ismh_len32b = wordsof((char *)shp - (char *)genp);
	return(1);
}


static char 	spbuff[80];

char *
fmt_flowspec(FLOWSPEC *specp)
	{
	int		 ctype;

	if (!specp)
		return("[ ]");
	if ((ctype = Obj_CType(specp)) != ctype_FLOWSPEC_Intserv0) {
		sprintf(spbuff, "FLOWSPEC C-TYPE ??: %d\n", ctype);
		return(spbuff);
	}
	if (Intserv_Obj_size(&specp->flow_body) != Obj_Length(specp))
		return("Spec length?\n");
	return(rapi_fmt_specbody3(&specp->flow_body));
}



char *
fmt_tspec(SENDER_TSPEC *stsp)
	{
	int		 ctype;

	if (!stsp)
		return("[ ]");
	if ((ctype = Obj_CType(stsp)) != ctype_SENDER_TSPEC) {
		sprintf(spbuff, "Tspec C-Type ??: %d\n", ctype);
		return(spbuff);
	}
	if (Intserv_Obj_size(&stsp->stspec_body) != Obj_Length(stsp))
		return("Tspec length?\n");
	return(rapi_fmt_tspecbody3(&stsp->stspec_body));
}

/*
 *	Format ADSPEC [Shares internal buffer with fmt_tspec, fmt_flowspec]
 */
char *
fmt_adspec(ADSPEC *adsp)
	{
	int ctype;

	if (!adsp)
		return("Adspec()");
	if ((ctype = Obj_CType(adsp)) != ctype_ADSPEC_INTSERV) {
		sprintf(spbuff, "Tspec C-Type ??: %d\n", ctype);
		return(spbuff);
	}
	if (Intserv_Obj_size(&adsp->adspec_body) != Obj_Length(adsp))
		return("Adspec len??\n");
	return(rapi_fmt_adspecbody3(&adsp->adspec_body));
}


/*
 *  ntoh_flowspec(): Converts a flowspec in place, from network byte
 *	order into host byte order.
 */
void
ntoh_flowspec(FLOWSPEC *specp)
	{
	IS_serv_hdr_t	*sp;

	if ((!specp) || Obj_CType(specp) != ctype_FLOWSPEC_Intserv0)
		return;

	NTOH_IS_Main_Hdr(&specp->flow_body.spec_mh);
	if (Intserv_Obj_size(&specp->flow_body.spec_mh) != Obj_Length(specp))
		return;
	if (!Intserv_Version_OK(&specp->flow_body.spec_mh))
		return;

	sp = (IS_serv_hdr_t *) &specp->flow_body.spec_u;
	NTOH_IS_Serv_Hdr(sp);

	switch (sp->issh_service) {

	    case GUARANTEED_SERV:
		{
#if BYTE_ORDER == LITTLE_ENDIAN  /* Avoid compiler warning msg */
		Guar_flowspec_t *gp = (Guar_flowspec_t *) sp;

		/* Don't convert invalid fields */
		if (gp->Gspec_T_flags & ISPH_FLG_INV ||
		    gp->Gspec_R_flags & ISPH_FLG_INV)
			return;
		NTOH_IS_Parm_Hdr(&gp->Guar_Tspec_hdr);
		NTOH_IS_Parm_Hdr(&gp->Guar_Rspec_hdr);
		NTOHF32(gp->Gspec_b);
		NTOHF32(gp->Gspec_r);
		NTOHF32(gp->Gspec_p);
		NTOH32(gp->Gspec_m);
		NTOH32(gp->Gspec_M);
		NTOHF32(gp->Gspec_R);
		NTOH32(gp->Gspec_S);
#endif
		}
		break;


	    case CONTROLLED_LOAD_SERV:
		{
#if BYTE_ORDER == LITTLE_ENDIAN  /* Avoid compiler warning msg */
		CL_flowspec_t *clp = (CL_flowspec_t *) sp;

		/* Don't convert invalid fields */
		if (clp->CLspec_flags & ISPH_FLG_INV)
			return;
		NTOH_IS_Parm_Hdr(&clp->CL_spec_parm_hdr);
		NTOHF32(clp->CLspec_b);
		NTOHF32(clp->CLspec_r);
		NTOHF32(clp->CLspec_p);
		NTOH32(clp->CLspec_m);
		NTOH32(clp->CLspec_M);
#endif
		}
		break;

	default:
		break;
	}
}

/*
 *  hton_flowspec(): Converts a flowspec in place from host byte
 *	order into network byte order.
 */
void
hton_flowspec(FLOWSPEC *specp)
	{
	IS_serv_hdr_t	*sp;

	if ((!specp) || Obj_CType(specp) != ctype_FLOWSPEC_Intserv0)
		return;

	if (Intserv_Obj_size(&specp->flow_body.spec_mh) != Obj_Length(specp))
		return;
	HTON_IS_Main_Hdr(&specp->flow_body.spec_mh);
	if (!Intserv_Version_OK(&specp->flow_body.spec_mh))
		return;
	sp = (IS_serv_hdr_t *) &specp->flow_body.spec_u;
	HTON_IS_Serv_Hdr(sp);

	switch (sp->issh_service) {

	    case GUARANTEED_SERV:
		{

#if BYTE_ORDER == LITTLE_ENDIAN  /* Avoid compiler warning msg */
		Guar_flowspec_t *gp = (Guar_flowspec_t *) sp;

		/* Don't convert invalid fields */
		if (gp->Gspec_T_flags & ISPH_FLG_INV ||
		    gp->Gspec_R_flags & ISPH_FLG_INV)
			return;
		HTON_IS_Parm_Hdr(&gp->Guar_Tspec_hdr);
		HTONF32(gp->Gspec_b);
		HTONF32(gp->Gspec_r);
		HTONF32(gp->Gspec_p);
		HTON32(gp->Gspec_m);
		HTON32(gp->Gspec_M);
		HTON_IS_Parm_Hdr(&gp->Guar_Rspec_hdr);
		HTONF32(gp->Gspec_R);
		HTON32(gp->Gspec_S);
#endif
		}
		break;

	    case CONTROLLED_LOAD_SERV:
		{
#if BYTE_ORDER == LITTLE_ENDIAN  /* Avoid compiler warning msg */
		CL_flowspec_t *clp = (CL_flowspec_t *) sp;

		/* Don't convert invalid fields */
		if (clp->CLspec_flags & ISPH_FLG_INV)
			return;
		HTON_IS_Parm_Hdr(&clp->CL_spec_parm_hdr);
		HTONF32(clp->CLspec_b);
		HTONF32(clp->CLspec_r);
		HTONF32(clp->CLspec_p);
		HTON32(clp->CLspec_m);
		HTON32(clp->CLspec_M);
#endif
		}
		break;

	default:
		break;
	}
}

void
hton_tspec(SENDER_TSPEC *tspecp)
	{
	IS_serv_hdr_t	*stp;

	if ((!tspecp) || Obj_CType(tspecp) != ctype_SENDER_TSPEC)
		return;

	if (Intserv_Obj_size(&tspecp->stspec_body.st_mh) != Obj_Length(tspecp))
		return;
	HTON_IS_Main_Hdr(&tspecp->stspec_body.st_mh);
	if (!Intserv_Version_OK(&tspecp->stspec_body.st_mh))
		return;

	stp = (IS_serv_hdr_t *) &tspecp->stspec_body.tspec_u;
	HTON_IS_Serv_Hdr(stp);
	switch (stp->issh_service) {

	    case CONTROLLED_LOAD_SERV:	/**** OBSOLETE ***/
	    case GENERAL_INFO:
		{
#if BYTE_ORDER == LITTLE_ENDIAN  /* Avoid compiler warning msg */
		gen_Tspec_t *gp = (gen_Tspec_t *) stp;

		/* Don't convert invalid fields */
		if (gp->gtspec_flags & ISPH_FLG_INV)
			return;
		HTON_IS_Parm_Hdr(&gp->gen_Tspec_parm_hdr);
		HTONF32(gp->gtspec_b);
		HTONF32(gp->gtspec_r);
		HTONF32(gp->gtspec_p);
		HTON32(gp->gtspec_m);
		HTON32(gp->gtspec_M);
#endif
		}
		break;

	default:
		break;
	}
}


void
ntoh_tspec(SENDER_TSPEC *tspecp)
	{
	IS_serv_hdr_t	*stp;

	if ((!tspecp) || Obj_CType(tspecp) != ctype_SENDER_TSPEC)
		return;

	NTOH16(tspecp->stspec_body.st_mh.ismh_len32b);
	if (Intserv_Obj_size(&tspecp->stspec_body.st_mh) != Obj_Length(tspecp))
		return;
	if (!Intserv_Version_OK(&tspecp->stspec_body.st_mh))
		return;
	stp = (IS_serv_hdr_t *) &tspecp->stspec_body.tspec_u;
	NTOH_IS_Serv_Hdr(stp);

	switch (stp->issh_service) {

	    case GENERAL_INFO:
	    case CONTROLLED_LOAD_SERV:	/**** OBSOLETE ***/
		{
#if BYTE_ORDER == LITTLE_ENDIAN  /* Avoid compiler warning msg */
		gen_Tspec_t *gp = (gen_Tspec_t *) stp;

		/* Don't convert invalid fields */
		if (gp->gtspec_flags & ISPH_FLG_INV)
			return;		
		NTOH_IS_Parm_Hdr(&gp->gen_Tspec_parm_hdr);
		NTOHF32(gp->gtspec_b);
		NTOHF32(gp->gtspec_r);
		NTOHF32(gp->gtspec_p);
		NTOH32(gp->gtspec_m);
		NTOH32(gp->gtspec_M);
#endif
		}
		break;

	default:
		break;
	}
}

void
hton_adspec(ADSPEC *adsp)
	{
	IS_main_hdr_t	*mhp = (IS_main_hdr_t *) Obj_data(adsp);
	IS_serv_hdr_t	*shp = (IS_serv_hdr_t *) (mhp+1);
	IS_serv_hdr_t	*lastshp;
	IS_parm_hdr_t	*php, *lastphp, *cphp;

	lastshp  = (IS_serv_hdr_t *) Next_Main_Hdr(mhp);
	while (shp < lastshp) {
	    /*
	     *  For each service fragment...
	     */
	    lastphp = (IS_parm_hdr_t *)Next_Serv_Hdr(shp);
	    switch (shp->issh_service) {

	    case GENERAL_INFO:
		php = (IS_parm_hdr_t *)(shp+1);
		break;

	    case GUARANTEED_SERV:
		{
		Gads_parms_t *gap = (Gads_parms_t *) shp;

		if (shp->issh_len32b == 0) {
			php = (IS_parm_hdr_t *)(shp+1);
			break;
		}
		HTON_IS_Parm_Hdr(&gap->Gads_Ctot_hdr);
		HTON32(gap->Gads_Ctot);
		HTON_IS_Parm_Hdr(&gap->Gads_Dtot_hdr);
		HTON32(gap->Gads_Dtot);
		HTON_IS_Parm_Hdr(&gap->Gads_Csum_hdr);
		HTON32(gap->Gads_Csum);
		HTON_IS_Parm_Hdr(&gap->Gads_Dsum_hdr);
		HTON32(gap->Gads_Dsum);
		php = (IS_parm_hdr_t *)(gap+1);
		break;
		}

	    case CONTROLLED_LOAD_SERV:
		php = (IS_parm_hdr_t *)(shp+1);
		break;

	    default:
		/* XXX ?? */
		break;
	    }

	    /*
	     *		Loop to convert general characterization parms,
	     *		either default or override.
	     */
	    while (php < lastphp) {
		hton_genparm(php);
		php = Next_Parm_Hdr(cphp = php);
		HTON_IS_Parm_Hdr(cphp);
	    }
	    /*
	     *	Continue with next service fragment, if any...
	     */
	    HTON_IS_Serv_Hdr(shp);
	    shp = (IS_serv_hdr_t *)php;
	}
	HTON_IS_Main_Hdr(mhp);
}

void
ntoh_adspec(ADSPEC *adsp)
	{
	IS_main_hdr_t	*mhp = (IS_main_hdr_t *) Obj_data(adsp);
	IS_serv_hdr_t	*shp = (IS_serv_hdr_t *) (mhp+1);
	IS_serv_hdr_t	*lastshp;
	IS_parm_hdr_t	*php, *lastphp;
	
	NTOH_IS_Main_Hdr(mhp);
	lastshp = (IS_serv_hdr_t *) Next_Main_Hdr(mhp);

	while (shp < lastshp) {
	    /*
	     *  For each service fragment...
	     */
	    NTOH_IS_Serv_Hdr(shp);
	    lastphp = (IS_parm_hdr_t *)Next_Serv_Hdr(shp);
	    switch (shp->issh_service) {

	    case GENERAL_INFO:
		php = (IS_parm_hdr_t *)(shp+1);
		break;

	    case GUARANTEED_SERV:
		{
		Gads_parms_t *gap = (Gads_parms_t *) shp;

		if (shp->issh_len32b == 0) {
			php = (IS_parm_hdr_t *)(shp+1);
			break;
		}
		NTOH_IS_Parm_Hdr(&gap->Gads_Ctot_hdr);
		NTOH32(gap->Gads_Ctot);
		NTOH_IS_Parm_Hdr(&gap->Gads_Dtot_hdr);
		NTOH32(gap->Gads_Dtot);
		NTOH_IS_Parm_Hdr(&gap->Gads_Csum_hdr);
		NTOH32(gap->Gads_Csum);
		NTOH_IS_Parm_Hdr(&gap->Gads_Dsum_hdr);
		NTOH32(gap->Gads_Dsum);
		php = (IS_parm_hdr_t *)(gap+1);
		break;
		}

	    case CONTROLLED_LOAD_SERV:
		lastphp = (IS_parm_hdr_t *)Next_Serv_Hdr(shp);
		php = (IS_parm_hdr_t *)(shp+1);
		break;

	    default:
		/* XXX ?? */
		break;
	    }

	    /*
	     *		Loop to convert general characterization parms,
	     *		either default or override.
	     */
	    while (php < lastphp) {
		NTOH_IS_Parm_Hdr(php);
		ntoh_genparm(php);
		php = Next_Parm_Hdr(php);
	    }
	    /*
	     *	Continue with next service fragment, if any...
	     */
	    shp = (IS_serv_hdr_t *)php;
	}
}

void
hton_genparm(IS_parm_hdr_t *pp)
	{
	switch (pp->isph_parm_num) {
	    case IS_WKP_HOP_CNT:
	    case IS_WKP_MIN_LATENCY:
	    case IS_WKP_COMPOSED_MTU:
		HTON32(*(u_int32_t *)(pp+1));
		break;

	    case IS_WKP_PATH_BW:
		HTONF32(*(float32_t *)(pp+1));
		break;

	    default:
		/* XXX ? */
		break;
	}
}


void
ntoh_genparm(IS_parm_hdr_t *pp)
	{
	switch (pp->isph_parm_num) {
	    case IS_WKP_HOP_CNT:
	    case IS_WKP_MIN_LATENCY:
	    case IS_WKP_COMPOSED_MTU:
		NTOH32(*(u_int32_t *)(pp+1));
		break;

	    case IS_WKP_PATH_BW:
		NTOHF32(*(float32_t *)(pp+1));
		break;

	    default:
		/* XXX ? */
		break;
	}
}
