/*
 * @(#) $Id: rapi_fmt.c,v 1.7 1998/11/25 08:43:36 eddiem Exp $
 */

/************************ rapi_fmt.c *****************************
 *                                                               *
 *  RSVP API library routines for formatting flowspecs, Tspecs,  *
 *  adspecs, and filter specs.                                   *
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

#ifdef RSVP_TCPDUMP
#define _MACHINE_ENDIAN_H_ 1
#endif

#include "rsvp_types.h"
#include "rsvp.h"
#include "rsvp_specs.h"
#include "rsvp_api.h"

#ifdef RSVP_TCPDUMP
extern char *getname();
#define ipaddr_string(p) getname(p)	/* tcpdump print routine */
#else
#define ipaddr_string(p) fmt_hostname(p)	/* local routine */
char	*fmt_hostname();
#endif

void	rapi_fmt_flowspec(rapi_flowspec_t *, char *, int);
void	rapi_fmt_tspec(rapi_tspec_t *, char *, int);
void	rapi_fmt_adspec(rapi_adspec_t *, char *, int);
void	rapi_fmt_filtspec(rapi_filter_t *, char *, int);
#ifdef OBSOLETE_API
char *	rapi_fmt_specbody1(qos_flowspec_t *);
char *	rapi_fmt_tspecbody1(qos_flowspec_t *);
#endif /* OBSOLETE_API */
char *	rapi_fmt_specbody2(qos_flowspecx_t *);
char *	rapi_fmt_tspecbody2(qos_tspecx_t *);
char *  rapi_fmt_specbody3(IS_specbody_t *);
char *	rapi_fmt_tspecbody3(IS_tspbody_t *);
char *	rapi_fmt_gen_Tspecbody3(TB_Tsp_parms_t *gp);
char *	rapi_fmt_adspecbody2(qos_adspecx_t *);
char *	rapi_fmt_adspecbody3(IS_adsbody_t *);
char *	fspec_fmtf(float32_t, char *);

int     IP_NumOnly;  /* Global switch in fmt_hostname */
#include <setjmp.h>
jmp_buf savej;  /* longjmp environ for fmt_hostname */

static 	char	out[256];
static  char	out1[80];
static  char	*serv_name[] = {"", "Gen", "G", "P", "C-D", "CL"};
 /* conversion buffers */
char	b1[8], b2[8], b3[8], b4[8], b5[8], b6[8], b7[8];

 
/*
 *	Format RAPI flowspec into buffer of given length.
 */
void
rapi_fmt_flowspec(rapi_flowspec_t * specp, char *buff, int len)
	{
	char *	cp;
	int	body_len;

	if (len < 16)
		return;
	if (!specp) {
		strcpy(buff, " [ ]");
		return;
	}
	switch (specp->form) {

            case RAPI_EMPTY_OTYPE:
                sprintf(buff, " [ ]");
		return;

#ifdef OBSOLETE_API
	    case RAPI_FLOWSTYPE_CSZ:
		if (specp->len != sizeof(qos_flowspec_t) + sizeof(rapi_hdr_t)){
			sprintf(buff, "CSZ spec len?? %d\n", specp->len);
			return;
		}
		cp = rapi_fmt_specbody1(&specp->specbody_qos);
		break;
#endif /* OBSOLETE_API */

	    case RAPI_FLOWSTYPE_Simplified:
		if (specp->len != sizeof(qos_flowspecx_t) + sizeof(rapi_hdr_t)){
			sprintf(buff, "CSZX spec len?? %d\n", specp->len);
			return;
		}
		cp = rapi_fmt_specbody2(&specp->specbody_qosx);
		break;

	    case RAPI_FLOWSTYPE_Intserv:
		body_len = Intserv_Obj_size(&specp->specbody_IS) 
				-sizeof(Object_header);
		if (specp->len < body_len+8){
			sprintf(buff, "IS spec len?? %d\n", specp->len);
			return;
		}
		cp = rapi_fmt_specbody3(&specp->specbody_IS);
		break;

	    default:
		cp = "??RAPI Flowspec format\n";
		break;
	}
	if (strlen(cp) > len)
		strncpy("!Fmt overflow\n", buff, len);
	else
		strcpy(buff, cp);
}

/*
 *	Format RAPI Tspec into buffer of given length.
 */
void
rapi_fmt_tspec(rapi_tspec_t * tspecp, char *buff, int len)
	{
	char *	cp;
	int	body_len;

	if (len < 16)
		return;
	if (!tspecp) {
		strcpy(buff, " [ ]");
		return;
	}	
	switch (tspecp->form) {
            case RAPI_EMPTY_OTYPE:
                sprintf(buff, " TS[ ]");
		return;

#ifdef OBSOLETE_API
	    case RAPI_FLOWSTYPE_CSZ:
		if (tspecp->len != sizeof(qos_flowspec_t) + sizeof(rapi_hdr_t)){
			sprintf(buff, "CSZ Tspec len?? %d\n", tspecp->len);
			return;
		}
		cp = rapi_fmt_specbody1(&tspecp->specbody_qos);
		break;
#endif /* OBSOLETE_API */

	    case RAPI_TSPECTYPE_Simplified:
		if (tspecp->len != sizeof(qos_tspecx_t) + sizeof(rapi_hdr_t)){
			sprintf(buff, "CSZX spec len?? %d\n", tspecp->len);
			return;
		}
		cp = rapi_fmt_tspecbody2(&tspecp->tspecbody_qosx);
		break;

	    case RAPI_TSPECTYPE_Intserv:
		body_len = Intserv_Obj_size(&tspecp->tspecbody_IS) 
							-sizeof(Object_header);
		if (tspecp->len < body_len+8){
			sprintf(buff, "IS Tspec len?? %d\n", tspecp->len);
			return;
		}
		cp = rapi_fmt_tspecbody3(&tspecp->tspecbody_IS);
		break;

	    default:
		cp = "??RAPI Tspec format\n";
		break;
	}
	if (strlen(cp) > len)
		strncpy("!Fmt overflow\n", buff, len);
	else
		strcpy(buff, cp);
}


/*
 *	Format RAPI Adspec into buffer of given length.
 */
void
rapi_fmt_adspec(rapi_adspec_t * adspecp, char *buff, int len)
	{
	char *	cp;

	if (len < 16)
		return;
	if (!adspecp) {
		strcpy(buff, " [ ]");
		return;
	}	
	switch (adspecp->form) {
            case RAPI_EMPTY_OTYPE:
                sprintf(buff, " Adspec()");
		return;

	    case RAPI_ADSTYPE_Simplified:
		if (adspecp->len != sizeof(qos_adspecx_t)+sizeof(rapi_hdr_t)){
			sprintf(buff, "CSZX adspec len?? %d\n", adspecp->len);
			return;
		}
		cp = rapi_fmt_adspecbody2(&adspecp->adspecbody_qosx);
		break;

	    case RAPI_ADSTYPE_Intserv:
		if (size_api2d(adspecp->len) !=
				Intserv_Obj_size(&adspecp->adspecbody_IS)) {
		sprintf(buff, "IS Adspec len?? %d\n", adspecp->len);
			return;
		}
		cp = rapi_fmt_adspecbody3(
				(IS_adsbody_t *)RAPIObj_data(adspecp));
		break;

	    default:
		cp = "??RAPI Adspec format\n";
		break;
	}
	if (strlen(cp) > len)
		strncpy("!Fmt overflow\n", buff, len);
	else
		strcpy(buff, cp);
}

#ifdef OBSOLETE_API
/*	Format body of "Legacy" flowspec or Tspec
 */
char *
rapi_fmt_specbody1(qos_flowspec_t *cszp)
        {
	char *qos_name[] = {"Tsp", "C-D", "Pred", "G"};

	if (cszp->spec_type > QOS_GUARANTEED)
		sprintf(out, "[?spec_type %d ]", cszp->spec_type);
	else
		sprintf(out, " [%s T=[%d(%d)] d=%d sl=%d]",
			qos_name[cszp->spec_type],
			cszp->spec_r, cszp->spec_b,
			cszp->spec_d, cszp->spec_sl);
        return(out);
}
#endif /* OBSOLETE_API */

/*
 *	Format body of "Simplified format" flowspec
 */
char *
rapi_fmt_specbody2(qos_flowspecx_t *csxp)
	{
#ifdef OBSOLETE_API
	char *qos_name[] = {"0?", "1?", "2?", "3?", "Gx", "CL", "?6"};

	if (csxp->spec_type > QOS_TSPECX)
#else
	char *qos_name[] = {"T", "CL", "G"};


	if (csxp->spec_type > QOS_GUARANTEED)
#endif
		sprintf(out, "[?spec_type %d ]", csxp->spec_type);
	else
		sprintf(out, " [%s [%s(%s) p=%s m=%s M=%s] R=%s S=%s]",
			qos_name[csxp->spec_type],
			fspec_fmtf(csxp->xspec_r, b1),
			fspec_fmtf(csxp->xspec_b, b2),
			fspec_fmtf(csxp->xspec_p, b3),
			fspec_fmtf((float32_t)csxp->xspec_m, b4),
			fspec_fmtf((float32_t)csxp->xspec_M, b5),
			fspec_fmtf(csxp->xspec_R, b6),
			fspec_fmtf((float32_t)csxp->xspec_S, b7));
        return(out);
}

/*
 *	Format body of "Integrated Services format" flowspec
 */
char *
rapi_fmt_specbody3(IS_specbody_t *sbp)
	{
	IS_serv_hdr_t	*shp = (IS_serv_hdr_t *) &sbp->spec_u;
	char		 out1[80], out2[80];
	int		 vers;

	vers = sbp->spec_mh.ismh_version & INTSERV_VERS_MASK;
	if (vers != INTSERV_VERSION0) {
		sprintf(out, "IS flowspec version? %d\n", vers);
		return(out);
	}
	if (Issh_len32b(shp) != sbp->spec_mh.ismh_len32b - 1) {
		sprintf(out, ">1 serv hdr: unsupported.\n");
		return(out);
	}
	switch (shp->issh_service) {

	    case GUARANTEED_SERV:
		{
		Guar_flowspec_t *gp = &sbp->spec_u.G_spec;

		/* Don't print if invalid */
		if (gp->Gspec_T_flags & ISPH_FLG_INV ||
		    gp->Gspec_R_flags & ISPH_FLG_INV) {
			strcpy(out1, "INV");
			out2[0] = '\0';
		} else {
			sprintf(out1, "R=%s S=%s",
				fspec_fmtf(gp->Gspec_R, b1),
				fspec_fmtf((float32_t)gp->Gspec_S, b2));
			strcpy(out2,
				rapi_fmt_gen_Tspecbody3(&gp->Guar_Tspec_parms));
		}
		}
		break;

	    case CONTROLLED_LOAD_SERV:
		{
		CL_flowspec_t *clp = &sbp->spec_u.CL_spec;
			
		out2[0] = '\0';
		if (clp->CLspec_flags & ISPH_FLG_INV) {
			strcpy(out1, "INV");
		} else {
			strcpy(out1,
				rapi_fmt_gen_Tspecbody3(&clp->CL_spec_parms));
		}
		}
		break;
	}
	sprintf(out, "[%s %s %s]", serv_name[shp->issh_service], out1, out2);
	return(out);
}


/*
 *	Format body of "Simplified format" tspec
 */
char *
rapi_fmt_tspecbody2(qos_tspecx_t *ctxp)
	{
#ifdef OBSOLETE_API
	char *qos_name[] = {"0?", "1?", "2?", "3?", "4?", "5?", "Tx"};

	if (ctxp->spec_type > QOS_TSPECX)
#else
	char *qos_name[] = {"T"};

	if (ctxp->spec_type != QOS_TSPEC)
#endif
		sprintf(out, "[?spec_type %d ]", ctxp->spec_type);
	else
		sprintf(out, " [%s [%s(%s) p=%s m=%s M=%s]]",
			qos_name[ctxp->spec_type],
			fspec_fmtf(ctxp->xtspec_r, b1),
			fspec_fmtf(ctxp->xtspec_b, b2),
			fspec_fmtf(ctxp->xtspec_p, b3),
			fspec_fmtf((float32_t)ctxp->xtspec_m, b4),
			fspec_fmtf((float32_t)ctxp->xtspec_M, b5));
        return(out);
}

/*
 *	Format body of "Integrated Services format" tspec
 */
char *
rapi_fmt_tspecbody3(IS_tspbody_t *tbp)
	{
	IS_serv_hdr_t	*shp = (IS_serv_hdr_t *) &tbp->tspec_u;
	int		 vers;

	vers = Intserv_Version(tbp->st_mh.ismh_version);
	if (vers != INTSERV_VERSION0) {
		sprintf(out, "IS Tspec version? %d\n", vers);
		return(out);
	}
	if (shp->issh_len32b != tbp->st_mh.ismh_len32b - 1) {
		sprintf(out, ">1 serv hdr: unsupported.\n");
		return(out);
	}

	switch (shp->issh_service) {

	    case GENERAL_INFO:
		{
		gen_Tspec_t *gp =&tbp->tspec_u.gen_stspec;

		if (tbp->st_mh.ismh_len32b != wordsof(sizeof(gen_Tspec_t))) {
			sprintf(out1, "!Tspec: mh_len? %d ",
						tbp->st_mh.ismh_len32b);
			break;
		}
		if (gp->gtspec_parmno != IS_WKP_TB_TSPEC) {
			sprintf(out1, "!Tspec: parm#? %d ", gp->gtspec_parmno);
			break;
		}
		/* Don't print if invalid */
		if (gp->gtspec_flags & ISPH_FLG_INV) {
			strcpy(out1, "INV");
			break;
		}
		strcpy(out1, rapi_fmt_gen_Tspecbody3(&gp->gen_Tspec_parms));
		}
		break;

	    case CONTROLLED_LOAD_SERV:
	    case GUARANTEED_SERV:
		sprintf(out1, "T=[Wrong/obsolete Tspec service id %d]",
			shp->issh_service);
		break;

	}
	sprintf(out, "%s", out1);
	return(out);
}

char *
rapi_fmt_adspecbody2(qos_adspecx_t *adsp)
	{
	char		out1[80];
	char		*brk_str;

	if (adsp->xaspec_flags & XASPEC_FLG_BRK)
		brk_str = "br! ";
	else
		brk_str = "";
	sprintf(out1, "{%s%d hop %sB/s %sus %dB}",
		brk_str,
		adsp->xaspec_hopcnt,
		fspec_fmtf(adsp->xaspec_path_bw, b1),
		fspec_fmtf((float32_t)adsp->xaspec_min_latency, b2),
		adsp->xaspec_composed_MTU);

	if (!(adsp->xGaspec_flags & XASPEC_FLG_INV)) {
		if (adsp->xGaspec_flags & XASPEC_FLG_BRK)
			brk_str = "br! ";
		else
			brk_str = "";
		sprintf(out, "%s  G={%s%d %d %d %d}",
			out, brk_str,
			adsp->xGaspec_Ctot, adsp->xGaspec_Dtot,
			adsp->xGaspec_Csum, adsp->xGaspec_Dsum);
	}		
	return(out);
}


char *
rapi_fmt_adspecbody3(IS_adsbody_t *ISap)
	{
	IS_main_hdr_t	*mhp = (IS_main_hdr_t *)ISap;
	IS_serv_hdr_t	*shp = (IS_serv_hdr_t *)(mhp+1); /* Ptr to fragment */
	IS_serv_hdr_t	*lastshp; 
	IS_parm_hdr_t	*php, *lastphp;
	char		 out2[80], *brk_str, *end_str;

	if (mhp->ismh_version != INTSERV_VERSION0) {
		sprintf(out, "IS Adspec version? %d\n", mhp->ismh_version);
		return(out);
	}
	strcpy(out, "Adspec(");
	lastshp  = (IS_serv_hdr_t *) Next_Main_Hdr(mhp);

	/* TEMPORARY...!
	 */
	if (shp->issh_len32b & ISSH_BREAK_BIT) {
		strcat(out, "rsvp-use-00!)\n");
		return(out);
	}
	while (shp < lastshp) {
	    lastphp = (IS_parm_hdr_t *)Next_Serv_Hdr(shp);
	    brk_str = (shp->issh_flags & ISSH_BREAK_BIT) ? "br!" : "";
	    end_str = "";

	    switch(shp->issh_service) {

		case GENERAL_INFO:
			php = (IS_parm_hdr_t *)(shp+1);
			break;

		case GUARANTEED_SERV:
			{
			Gads_parms_t *gap = (Gads_parms_t *) shp;

			if (gap->Gads_serv_hdr.issh_len32b == 0) {
				sprintf(out2, ", G={%s", brk_str);
				php = (IS_parm_hdr_t *)(shp+1);
			}
			else {
				sprintf(out2, ", G={%s%d %d %d %d",
					brk_str,
					gap->Gads_Ctot, gap->Gads_Dtot,
					gap->Gads_Csum, gap->Gads_Dsum);
				php = (IS_parm_hdr_t *)(gap+1);
			}
			strcat(out, out2);
			end_str = "}";
			break;
			}

		case CONTROLLED_LOAD_SERV:
			sprintf(out2, ", CL={%s", brk_str);
			strcat(out, out2);
			end_str = "}";
			php = (IS_parm_hdr_t *)(shp+1);
			break;

		default:
			sprintf(out2, "??Serv#=%d)\n", shp->issh_service);
		        strcat(out, out2);
			return(out);
	    }
	    while(php < lastphp) {
		switch (php->isph_parm_num) {
		    case IS_WKP_HOP_CNT:
			sprintf(out2, " %d hop", *(u_int32_t *)(php+1));
			break;
		    case IS_WKP_MIN_LATENCY:
			sprintf(out2, " %sus",
				fspec_fmtf((float32_t)
						*(u_int32_t *)(php+1), b2));
			break;
		    case IS_WKP_COMPOSED_MTU:
			sprintf(out2, " %dB", *(u_int32_t *)(php+1));
			break;
		    case IS_WKP_PATH_BW:
			sprintf(out2, " %sB/s",
				fspec_fmtf(*(float32_t *)(php+1), b1));
			break;
		    default:
			strcpy(out2, "?parm");
			break;
		}
		strcat(out, out2);
		php = Next_Parm_Hdr(php);
	    }
	    strcat(out, end_str);
	    shp = (IS_serv_hdr_t *)php;
	}
	strcat(out, ")\n");
	return(out);
}
	
/*
 *	Format generic Tspec
 */
char *
rapi_fmt_gen_Tspecbody3(TB_Tsp_parms_t *gp)
	{
	sprintf(out1, "T=[%s(%s) %sB/s %s %s]",
			fspec_fmtf(gp->TB_Tspec_r, b1),
			fspec_fmtf(gp->TB_Tspec_b, b2),
			fspec_fmtf(gp->TB_Tspec_p, b3),
			fspec_fmtf((float32_t)gp->TB_Tspec_m, b4),
			fspec_fmtf((float32_t)gp->TB_Tspec_M, b5));
	return(out1);
}

/*
 *  Format RAPI filter spec into buffer
 */
void
rapi_fmt_filtspec(rapi_filter_t *afp, char *buff, int len)
	{
	struct sockaddr_in	*sadp;
	char			tc[32];

	if (len < 32)
		return;
	if (!afp || afp->form == RAPI_EMPTY_OTYPE) {
		strcpy(buff, "( )");
		return;
	}
	switch (afp->form) {

#ifdef OBSOLETE_API
	    case RAPI_FILTERFORM_1:
		sadp = &afp->filter.t1.sender;
		if (afp->filter.t1.len) {
			strcpy(buff, "!?Bit mask unsupported");
			return;
		}
#endif
	   case RAPI_FILTERFORM_BASE:
		sadp = &afp->filt_u.base.sender;
		if (sadp->sin_family != AF_INET) {
			sprintf(buff, "Filtspec: Addr family %d?", 
							sadp->sin_family);
			return;
		}
		if (sadp->sin_port)
			sprintf(tc, "%d", ntoh16(sadp->sin_port));
		else
			sprintf(tc, "*");
		if (sadp->sin_addr.s_addr == INADDR_ANY)
			sprintf(buff, "(*):%s", tc);
		else
			sprintf(buff, "%.24s:%s",
				fmt_hostname(&sadp->sin_addr), tc);
		break;

	   case RAPI_FILTERFORM_GPI:
		if (afp->filt_u.gpi.sender.s_addr == INADDR_ANY)
			sprintf(buff, "(*)<g>%d", afp->filt_u.gpi.gpi);
		else
			sprintf(buff, "%.16s<g>%d",
			 	fmt_hostname(&afp->filt_u.gpi.sender),
				afp->filt_u.gpi.gpi);
		break;				

	   default:
		strcpy(buff, "?Filt type");
		return;
	}
}


/* 
 *  Values in flowspecs actually contain limited precision.
 *  Reflect this by printing 3 significant digits, and float them using
 *  K or M suffix when appropriate.
 */
char *fspec_fmtf(float32_t x, char *buff)
	{
	static char *fspec_units[] = {"",  "K",  "M", "G", "T"};
	int i = 0;
 
	if (x >= 1.0e15)
		sprintf(buff, "Inf");
	else {
		while (x >= 1000) {
			i++;
			x /= 1000;
		}
		sprintf(buff, "%.3g%s", x, fspec_units[i]);
	}
	return (buff);
}

/*
 *  char *fmt_hostname( ipaddr )
 *
 *      Convert IP address to domain name string.  Look first in private cache.
 *      If don't find it, call gethostbyaddr(), and if successful, cache it and
 *      return string.  If DNS lookup fails or times out, return dotted-decimal
 *      string, and cache negative answer.  (Could periodically flush cached
 *      negative answers).
 */
#define IPC_CACHE_MAX 64        /* Number of IPC cache entries */
#define IPC_NAME_LEN 64         /* Max hostname length to cache */
#define IPC_TIMEOUT 5           /* Timeout time in secs */
        
/*
 *      Define private cache data structures
 */
typedef struct ipc_cache {
        struct ipc_cache *ipcc_next;
        struct in_addr ipcc_addr;
        char ipcc_name[IPC_NAME_LEN];   /* Name, or empty if neg result */
        } ipc_cache_t;

ipc_cache_t ipc_cvec[IPC_CACHE_MAX], *ipc_head = NULL;
static int ipc_avail = 0;
static char ipc_result[IPC_NAME_LEN];
int Num_DNS_Timeouts = 0;

char *
fmt_hostname( struct in_addr *addrp )
{
        ipc_cache_t *icp;
        char *volatile namep = ""; /* volatile is to shut GCC up */
        struct hostent *hp;
	extern char *inet_ntoa();

        if (IP_NumOnly)
                return(inet_ntoa(*addrp));

        for (icp= ipc_head; icp; icp = icp->ipcc_next) {
                if (icp->ipcc_addr.s_addr == addrp->s_addr) {
                        strcpy(ipc_result, icp->ipcc_name);
                        return ((ipc_result[0])?
                                 ipc_result : (char *) inet_ntoa(*addrp));
                }
        }
        
        if (setjmp(savej) == 0) {
#ifdef TIMEOUT
		prev_func = signal(SIGALRM, giveup);
		alarm(IPC_TIMEOUT);
                hp = gethostbyaddr((char *) addrp, sizeof(struct in_addr),
								 AF_INET);
		alarm(0);
                if (hp) namep = hp->h_name;
        }
	signal(SIGALRM, prev_func);
#else
                hp = gethostbyaddr((char *) addrp, sizeof(struct in_addr),
								 AF_INET);
                if (hp) namep = hp->h_name;
	}
#endif /* TIMEOUT */

                /* Cache the result, positive or negative */
        if (ipc_avail < IPC_CACHE_MAX) {
                icp = &ipc_cvec[ipc_avail++];
                icp->ipcc_next = ipc_head;
                ipc_head = icp;
                icp->ipcc_addr.s_addr = addrp->s_addr;
                strncpy(icp->ipcc_name, namep, IPC_NAME_LEN);
        }
        
        strcpy(ipc_result, namep);
        return ((ipc_result[0])? ipc_result : (char *) inet_ntoa(*addrp));
}

#ifdef TIMEOUT
static void giveup()
        {
        extern Xdebug;
        if (Xdebug)
                printf("DNS Timeout!\n");
        Num_DNS_Timeouts++;
        fflush(stdout);
        longjmp(savej, 1);
}
#endif /* TIMEOUT */


