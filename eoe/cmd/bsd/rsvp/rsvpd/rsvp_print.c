
/*
 * @(#) $Id: rsvp_print.c,v 1.6 1998/11/25 08:43:14 eddiem Exp $
 */
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

/*
 *			rsvp_print.c
 *
 *	Common routine for formatting and printing/logging an RSVP packet.
 */

#include <stdio.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <syslog.h>
#include <string.h>

#ifdef RSVP_TCPDUMP
#define _MACHINE_ENDIAN_H_ 1
#endif

#include "rsvp_types.h"
#include "rsvp.h"

#ifdef RSVP_TCPDUMP	/* tcpdump */
#include "interface.h"
#include "addrtoname.h"
#define OUT0(x) printf((x))
#define OUT1(x, y) printf((x), (y))
#define OUT2(x, y, z) printf((x), (y), (z))
#define OUT3(w, x, y, z) printf((w), (x), (y), (z))
#define OUT4(w, x, y, z, a) printf((w), (x), (y), (z), (a))
#else
char 	*fmt_hostname(struct in_addr *);
#define iptoname(p)   fmt_hostname(p)	
#define ipaddr_string fmt_hostname
#define OUT0(x) log(LOG_DEBUG, 0, (x))
#define OUT1(x, y) log(LOG_DEBUG, 0, (x), (y))
#define OUT2(x, y, z) log(LOG_DEBUG, 0, (x), (y), (z))
#define OUT3(w, x, y, z) log(LOG_DEBUG, 0, (w), (x), (y), (z))
#define OUT4(w, x, y, z, a) log(LOG_DEBUG, 0, (w), (x), (y), (z), (a))
void log(int, int, const char *, ...);
#endif

#define Next_Object(x)  (Object_header *)((char *) (x) + (x)->obj_length)

       char *fmt_filtspec();
extern char *fmt_flowspec();
extern char *fmt_tspec();
extern char *fmt_adspec();
	
void	 fmt_object(Object_header *, int type);
char	*fmt_style(style_t);
char	*cnv_flags(char *, u_char);

static 	int objcol;
	/* Not sure best way to format sender descriptor lists and flow
	 * descriptor lists.  For now, simply put each two objects into
	 * a single line.
	 */

#ifdef RSVP_TCPDUMP	/* tcpdump */
char *Type_name[] = {"", "PATH  ", "RESV  ", "PATH-ERR", "RESV-ERR",
			 "PATH-TEAR", "RESV-TEAR"};
#else
extern char *Type_name[];
#endif

char	*style_names[] = {"?", "WF", "FF", "SE"};
style_t  style_optvs[] = {0, STYLE_WF, STYLE_FF, STYLE_SE};

/*
 *	Format and print RSVP packet
 */
void
rsvp_print_pkt(hdrp, length)
	common_header	*hdrp;
	int		length;		/* Actual length */
	{
	Object_header	*objp;
	char		*end_of_data = (char *) hdrp + length;
	int		flags = RSVP_FLAGS_OF(hdrp);
	
	NTOHS(hdrp->rsvp_length);
	if (hdrp->rsvp_type > RSVP_MAX_MSGTYPE) {
		OUT1(" ??Type= %d\n", hdrp->rsvp_type);
		return;
	}
	OUT1("  %s", Type_name[hdrp->rsvp_type]);
	if (flags)
		OUT1(" %x", flags);

	objp = (Object_header *) (hdrp + 1);
	objcol = 0;
	while (objp <  (Object_header *)end_of_data) {
		if ((Obj_Length(objp)&3) || Obj_Length(objp)<4 ||
				(char *)Next_Object(objp) > end_of_data) {
			if (length < hdrp->rsvp_length)
				OUT0(" ... Truncated by tcpdump\n");
			else
				OUT0(" ... Bad object format\n");
			return;
		}
		fmt_object(objp, hdrp->rsvp_type);
		objp = Next_Object(objp);
	}
	if (length < hdrp->rsvp_length)
		OUT0(" ... Truncated by tcpdump\n");
	else if (length > hdrp->rsvp_length)
		OUT0(" ... ?? Too long\n");
	return;
    
}

static char *Sess_types[4] = {"", "GPI", "v6", "GPI6"};

void
fmt_object(Object_header *op, int type)
	{
	struct in_addr	*destaddrp, *scopep;
	int		n, i, t = 0;
	style_t		style;
	char		*flagstr;
	u_char		flags;

	if(!op)
		return;

	switch (Obj_Class(op)) {

	    case class_NULL:
		OUT0("  (Null Obj)");
		break;

	    case class_SESSION:
		switch (Obj_CType(op)) {

		    case ctype_SESSION_ipv4GPI:
			t++;
		    case ctype_SESSION_ipv4:
			/*  ipv4 GPI session has same format as ipv4 SESSION.
			 */
			destaddrp = &((SESSION *)op)->sess4_addr;
			flags = ((SESSION *)op)->sess4_flgs;
			OUT4(" Sess: %s:%d[%d]%s ", ipaddr_string(destaddrp),
				ntoh16(((SESSION *)op)->sess4_port),
				((SESSION *)op)->sess4_prot, Sess_types[t]);
			break;

		    default:			
			OUT1("Unknown SESSION ctype= %d\n", Obj_CType(op));
			break;
		}
		OUT0(cnv_flags("??????P", flags));  /* P => Police */
		break;

	    case class_SESSION_GROUP:
		OUT0(" Sess_Grp!!");
		break;

	    case class_RSVP_HOP:
		if (Obj_CType(op) != ctype_RSVP_HOP_ipv4) {
			OUT1("Unknown RSVP_HOP ctype= %d\n", Obj_CType(op));
			break;
		}
		OUT3(" %s: <%s LIH=%d>\n", (type&1)?"  PHOP":"  NHOP", 
			ipaddr_string(
				&((RSVP_HOP *)op)->hop4_addr),
				((RSVP_HOP *)op)->hop4_LIH
		    );
                break;

	    case class_INTEGRITY:
		if (Obj_CType(op) != ctype_INTEGRITY_MD5_ipv4) {
			OUT1("Unknown INTEGRITY ctype= %d\n", Obj_CType(op));
			break;
		}
		OUT3("  Intgrty{Kid=%d Seq#=%d %s :",
			((INTEGRITY *)op)->intgr4_keyid,
			((INTEGRITY *)op)->intgr4_seqno,
			ipaddr_string( &((INTEGRITY *)op)->intgr4_sender));
		for (i = 0; i < MD5_LENG; i += 4) {
			OUT1(" %8.8x",
				ntoh32(*(u_int32_t *)
					&((INTEGRITY *)op)->intgr4_digest[i]));
		}
		OUT0("}\n");		
		break;
		
	    case class_TIME_VALUES:
		if (Obj_CType(op) != ctype_TIME_VALUES) {
			OUT1("Unknown TIME_VALUES ctype= %d\n", Obj_CType(op));
			break;
		}
		OUT1("   R: %d", ((TIME_VALUES *)op)->timev_R);
		break;

	    case class_ERROR_SPEC:
		if (Obj_CType(op) != ctype_ERROR_SPEC_ipv4) {
			OUT1("Unknown ERROR_SPEC ctype= %d\n", Obj_CType(op));
			break;
		}
		OUT2("\tERR#: %d Value: %d",
			((ERROR_SPEC *)op)->errspec4_code,
			((ERROR_SPEC *)op)->errspec4_value);
		flagstr = cnv_flags("??????NI",
					 ((ERROR_SPEC *)op)->errspec4_flags);
		OUT2("\tNode: %s  Flags: %s\n",
			ipaddr_string(&((ERROR_SPEC *)op)->errspec4_enode),
			flagstr);
		break;

	    case class_CONFIRM:
		if (Obj_CType(op) != ctype_CONFIRM_ipv4) {
			OUT1("Unknown CONFIRM ctype= %d\n", Obj_CType(op));
			break;
		}
		OUT1("   CONFIRMto: %s\n",
			ipaddr_string(&((CONFIRM *)op)->conf4_addr));
		break;

	    case class_SCOPE:
		if (Obj_CType(op) != ctype_SCOPE_list_ipv4) {
			OUT1("Unknown SCOPE ctype= %d\n", Obj_CType(op));
			break;
		}
		n = (Obj_Length(op) - sizeof(Object_header))/
				sizeof(struct in_addr);
		OUT0("\tScope={");
		scopep =  ((SCOPE *)op)->scope4_addr;
		for (i = 0; i < n; i++) {
			if (i)
				OUT1(" %s", ipaddr_string(scopep));
			else
				OUT1("%s", ipaddr_string(scopep));
			scopep++;
			if (((i+1)|3) == 0) OUT0("\n\t\t");
		}
		OUT0("}");
		break;

	    case class_STYLE:
		if (Obj_CType(op) != ctype_STYLE) {
			OUT1("\tSTYLE(??%d)\n", Obj_CType(op));
			break;
		}
		style = ((STYLE *)op)->style_word;
		OUT2("\t%s %x", fmt_style(style), (u_int32_t) style);
		break;

	    case class_FLOWSPEC:
		if (objcol++ >= 2) {
			objcol = 0;
			OUT0("\n");
		}
		OUT1("\t%s", fmt_flowspec((FLOWSPEC *)op) );
		break;

	    case class_SENDER_TEMPLATE:
	    case class_FILTER_SPEC:
		if (objcol++ >= 2) {
			objcol = 0;
			OUT0("\n");
		}
		OUT1("\t{%s}", fmt_filtspec((FILTER_SPEC *)op) );
		break;

	    case class_SENDER_TSPEC:
		if (objcol++ >= 2) {
			objcol = 0;
			OUT0("\n");
		}
		OUT1("\t%s", fmt_tspec((FLOWSPEC *)op) );
		break;

	    case class_ADSPEC:
		OUT1("  %s", fmt_adspec((ADSPEC *)op));
		break;

	    case class_POLICY_DATA:
	    default:
		OUT3("\tUnkn Object class %d ctype %d len %d\n",
			Obj_Class(op), Obj_CType(op), Obj_Length(op));
		break;
	}
}


/*
 *  Format RSVP filter spec and return pointer to string in static area.
 */
char *
fmt_filtspec(filtp)
	FILTER_SPEC *filtp;
	{
	static char	FIbuff[256];
	static char	tbuff[32];
	Filter_Spec_IPv4 *fp4;

	if (!filtp)
		return "(null)";
	FIbuff[0] = '\0';
	switch (Obj_CType(filtp)) {

	    case ctype_FILTER_SPEC_ipv4:
		fp4 = &filtp->filt_u.filt_ipv4;
		if (fp4->filt_ipaddr.s_addr == INADDR_ANY)
			sprintf(FIbuff, " *");
		else
			sprintf(FIbuff, "%s", ipaddr_string(&fp4->filt_ipaddr));
		if (fp4->filt_port)
			sprintf(tbuff, ":%d", ntoh16(fp4->filt_port));
		else
			sprintf(tbuff, ":*");
		strcat(FIbuff, tbuff);
		break;

	    case ctype_FILTER_SPEC_ipv4GPI:
		if (filtp->filt_u.filt_ipv4gpi.filt_ipaddr.s_addr == INADDR_ANY)
			sprintf(FIbuff, " *");
		else
			sprintf(FIbuff, "%s",
			 ipaddr_string(&filtp->filt_u.filt_ipv4gpi.filt_ipaddr));
		sprintf(tbuff, "<g>%d",
				 filtp->filt_u.filt_ipv4gpi.filt_gpi);
		strcat(FIbuff, tbuff);
		break;

	    default:
		sprintf(FIbuff, "Filtspec CType %d ??", Obj_CType(filtp));
		break;
	}
	return(FIbuff);
}


char *
fmt_style(style_t style)
	{
	int i, n = sizeof(style_names)/sizeof(char *);

	for (i=1; i < n; i++)
		if (style_optvs[i] == style)
			break;
	return (i == n)? "?Sty" : style_names[i];
}

static char out_flgs[80];

char *
cnv_flags(char *equiv, u_char flgs)
	{
	int	 i, bit;
	char	*p = out_flgs;

	*p++ = ' ';
	for (bit = 0x80, i = 0; bit; bit>>=1, i++) {
		if (flgs&bit) {
			out_flgs[0] = '*';
			*p++ = equiv[i];
		}
	}
	*p = '\0';
	return(out_flgs);
}

	



