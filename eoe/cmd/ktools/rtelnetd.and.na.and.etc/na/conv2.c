/*
 *****************************************************************************
 *
 *        Copyright 1989,1990 Xylogics, Inc.  ALL RIGHTS RESERVED.
 *
 * ALL RIGHTS RESERVED. Licensed Material - Property of Xylogics, Inc.
 * This software is made available solely pursuant to the terms of a
 * software license agreement which governs its use.
 * Unauthorized duplication, distribution or sale are strictly prohibited.
 *
 * Module Function:
 *	Second part of conversion routines. It had to be split in 2 parts
 *	(conv1.c conv2.c) so it would compile on wonderful XENIX PC (ran
 *	 out of HEAP before).
 *
 * Original Author: D. Emond	Created on: 4/2/90 for R5.0.2
 *
 * Revision Control Information:
 *
 * $Id: conv2.c,v 1.3 1996/10/04 12:10:21 cwilson Exp $
 *
 * This file created by RCS from:
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/na/RCS/conv2.c,v $
 *
 * Revision History:
 *
 * $Log: conv2.c,v $
 * Revision 1.3  1996/10/04 12:10:21  cwilson
 * latest rev
 *
 * Revision 3.69.102.4  1995/03/09  08:54:31  carlson
 * Removed mistaken inclusion of api_if.h.
 *
 * Revision 3.69.102.3  1995/01/19  11:21:47  bullock
 * Added case for version 9.2 - set to same value as version 9.0
 *
 * Revision 3.69.102.2  1995/01/11  16:32:06  russ
 * Added support for all keyed modules in disabled modules.
 *
 * Revision 3.69.102.1  1995/01/03  14:41:59  russ
 * put disabled modules in correct order.
 *
 * Revision 3.69  1994/08/03  13:45:25  geiser
 * make DECserver interface (disabled_mod = vci) a disable-able module
 *
 * Revision 3.68  1994/07/13  14:07:52  barnes
 * SPR 2832 - change the encode_nodeid routine to allow net addresses
 * in the range 0 - 65534 (0xfffe) and node addresses in the range
 * 0 - 254 (0xfe)
 *
 * Revision 3.67  1994/06/02  15:01:04  russ
 * We document that port 1 is not a valid load_dump_sequence entry (in
 * NA help at least)...  The code has been fixed to not allow setting
 * sl1 in the load_dump_sequence.
 *
 * Revision 3.66  1994/05/24  10:53:34  russ
 * Bigbird na/adm merge.
 *
 * Revision 3.57.2.5  1994/04/12  21:29:31  geiser
 * new conversion routine str_to_mop_passwd() to handle MOP passwords
 *
 * Revision 3.57.2.4  1994/03/13  17:23:37  russ
 * removed option dptg for disabled modules. always disabled.
 *
 * Revision 3.57.2.3  1994/03/03  11:59:02  russ
 * Changed littleboy reference to 8.1
 *
 * Revision 3.57.2.2  1994/02/04  16:51:41  russ
 * Added VERS_LITTLE_BOY and VERS_BIG_BIRD with the default software
 * version using 9.0 VERS_BIG_BIRD.  We are now officially run out
 * of VERS_SLOTS.
 *
 * Revision 3.57.2.1  1994/01/06  15:25:05  wang
 * Bigbird NA/Admin params support.
 *
 * Revision 3.65  1994/04/07  16:25:47  carlson
 * Fixed errno definition problem (need extern on some systems), and
 * fixed a misspelling.
 *
 * Revision 3.64  1994/04/07  10:01:00  russ
 * removed most of the XX_support_check tests.  We really want to be
 * able to read write and set parameters of disabled modules.
 *
 * Revision 3.63  1994/03/25  10:28:25  carlson
 * Added ftpd as a disabled module.
 *
 * Revision 3.62  1994/02/24  11:10:23  sasson
 * Added R8.1 version.
 *
 * Revision 3.61  1994/02/01  16:46:42  russ
 * Added another argument to get_internal_vers...  This is a True or
 * False to ask the user for prompting.  It is only used when the
 * annex software version is newer than the na version.
 *
 * Revision 3.60  1994/01/25  13:36:52  russ
 * added fingerd as a disabled_modules option. removed dptg.
 *
 * Revision 3.59  1993/12/30  14:03:49  carlson
 * SPR 2084 -- made compatible with 64 bit machines.
 *
 * Revision 3.58  1993/12/07  11:52:24  carlson
 * "config.h" should not be included in admin builds!
 *
 * Revision 3.57  1993/10/19  14:52:38  russ
 * Added *NOSTR* comments for AOCK pre-parser.
 *
 * Revision 3.56  1993/09/20  14:21:41  reeve
 * Added checks to see how option key was set. Reviewed by David Wang. [SPR 1738]
 *
 * Revision 3.55  1993/09/13  14:52:48  gmalkin
 * Modified some conversion routines to accept "default"
 *
 * Revision 3.54  1993/09/03  15:18:10  grant
 * Allowed appletalk zones to have embedded spaces.  Also added length and
 * value checking.  the default_zone_list is now stored as a series of
 * counted strings in E2.
 *
 * Revision 3.53  1993/07/27  20:27:38  wang
 * Fixed non-portable code problem.
 *
 * Revision 3.52  1993/06/11  10:57:36  wang
 * Fixed Spr1736, AppleTalk nodeid range problem.
 *
 * Revision 3.51  93/05/12  16:05:27  carlson
 * R7.1 -- Added support for new ELS hardware type, removed vselectable_mods
 * and generally cleaned up a bit.
 * 
 * Revision 3.50  93/05/10  16:33:30  wang
 * Fixed Spr1508, set an cli_prompt problem.
 * 
 * Revision 3.49  93/04/19  17:27:22  raison
 * added R7.1 version
 * 
 * Revision 3.48  93/04/16  14:05:50  wang
 * Fixed spr1430, AppleTalk params convert routine problem.
 * 
 * Revision 3.47  93/04/16  10:30:14  wang
 * Fixed spr1421, str_to_inet to return error to caller instead of punt.
 * 
 * Revision 3.46  93/04/01  17:15:22  carlson
 * Removed needless externs.
 * 
 * Revision 3.45  93/03/24  15:13:28  wang
 * Added in a new routine to convert bit pattern to port number.
 * 
 * Revision 3.44  93/03/17  18:37:28  wang
 * Removed unused variables.
 * 
 * Revision 3.43  93/03/17  15:26:47  wang
 * Added encode routine for AppleTalk default_zone_list param.
 * 
 * Revision 3.42  93/03/08  12:46:34  barnes
 * add routine to encode appletalk node-id parameters
 * 
 * Revision 3.41  93/03/01  13:05:48  wang
 * Changed comment to follow the code.
 * 
 * Revision 3.40  93/02/26  09:29:10  barnes
 * return status info from encode/decode rip_router routines, fix character
 * array sizes to prevent stack corruption when parsing long router address
 * lists, use bcopy instead of strncpy when moving encoded address lists
 * 
 * Revision 3.39  93/02/20  15:34:32  wang
 * Minor fix.
 * 
 * Revision 3.38  93/02/18  21:45:44  raison
 * added DECserver Annex parameters
 * 
 * Revision 3.37  93/02/18  16:02:02  barnes
 * move encode and decode routines for rip_routers, rip_accept,
 * rip_advertise and this_net from conv.c to conv2.c so snmp can
 * use them
 * 
 * Revision 3.36  93/01/22  18:15:24  wang
 * New parameters support for Rel8.0
 * New parameter support for rel8.0
 * 
 * Revision 3.35  92/08/14  15:27:42  reeve
 * Have get_internal_vers set ANNEX_ID RDRP flag, and Sp_support_check
 * properly pick out default_modem_hangup parameter.
 * 
 * Revision 3.34  92/08/03  14:16:18  reeve
 * Have Sp_support_check look for RDRP flag to decide if that param is supported.
 * 
 * Revision 3.33  92/07/26  21:25:36  raison
 * allow verbose-help to be indisabled_modules list for non-AOCK,non-ELS
 * images
 * 
 * Revision 3.32  92/05/12  16:51:18  raison
 * can not disable ppp, slip, or admin on ELS
 * 
 * Revision 3.31  92/05/08  01:54:36  raison
 * changed name of cli-edit selectable mod to edit
 * 
 * Revision 3.30  92/04/29  12:59:43  raison
 * removed sprintf define (hurts NCR machines) - thanks don
 * 
 * Revision 3.29  92/04/23  18:18:50  raison
 * allow lat to be disabled
 * 
 * Revision 3.28  92/04/22  21:16:33  raison
 * remove dptg option from selectable modules - since dptg is turned off
 * 
 * Revision 3.27  92/04/16  13:47:16  barnes
 * lat is now silently disabled when lat_key is invalid, don't allow
 * admin or na to disable it
 * 
 * Revision 3.26  92/04/14  17:21:57  barnes
 * move serv_options and selectable_mods structs to conv2.c
 * 
 * Revision 3.25  92/04/01  18:58:57  barnes
 * in decode_mask, make local variable a short to match parameter
 * type passed in
 * 
 * Revision 3.24  92/03/27  16:18:21  bullock
 * spr.660 - added range checking in print_to_c to prevent user's entries
 * being changed if they entered bad cntrl characters (i.e. ^9 or ^!, etc..)
 * 
 * Revision 3.23  92/03/26  15:35:40  reeve
 * Changed Ap_support_check to not give support for
 * control_lines on V.11 platform.
 * 
 * Revision 3.22  92/03/16  08:03:30  carlson
 * SPR 664 -- let a boolean procedure run off the end.
 * 
 * Revision 3.21  92/03/11  16:44:51  russ
 * Added nostr comments
 * 
 * Revision 3.20  92/03/03  15:16:32  reeve
 * Added routine, Ap_support_check() to ensure that certain annex parameters
 * are supported or not.  Also is a hook to check values for certain
 * annex params.
 * 
 * Revision 3.12.1.5  92/02/28  17:05:57  reeve
 * Created Ap_support_check(), to do special check on
 * box params on certain platforms.
 * 
 * Revision 3.12.1.4  92/01/31  11:41:38  reeve
 * Put in Sp_support_check() to check on hw sub-types.
 * 
 * Revision 3.17  92/01/31  13:33:17  reeve
 * Added routine Sp_support_check.
 * 
 * Revision 3.16  92/01/23  09:59:44  carlson
 * SPR 484 -- inc/config.h must be included first.
 * 
 * Revision 3.15  92/01/16  16:37:46  reeve
 * Disabled setting of annex parameter, load_dump_sequence to sl<#> for
 * an Annex not supporting SLIP.
 * 
 * Revision 3.14  92/01/02  12:47:14  barnes
 * move hex_digits and oct_digits from conv.c to eliminate dependency
 *   of conv2.c on conv.c
 * move syslog_levels from conv.c to eliminate dependency of
 *   snmp_anx_mib.c on conv.c
 * cleanup to remove some compiler warning messages
 * 
 * Revision 3.13  91/12/20  15:42:24  raison
 * added flash as a new type of load_dump_seq
 * 
 * Revision 3.12  91/11/15  13:40:19  russ
 * Add version 6.2 test.
 * 
 * Revision 3.11  91/10/30  15:06:14  grant
 * Added support for VER_7.
 * 
 * Revision 3.10  91/10/28  09:47:02  carlson
 * For SPR 361 -- allow NULL Pcardinal in print_to_c for quick check.
 * 
 * Revision 3.9  91/07/15  14:28:18  raison
 * separated str_to_inet() function from admin conditional
 * 
 * Revision 3.8  91/06/13  12:17:47  raison
 * ifdef'ed out unneccessary structs, strings, and globals for cli admin
 * 
 * Revision 3.7  91/05/19  11:00:47  emond
 * Merge 3.5.1.1 changes. Accommodate for interface to generic API network
 * layer and fix a SysV warning.
 * 
 * Revision 3.6  91/04/02  09:22:09  russ
 * Added Micro annex support.
 * 
 * Revision 3.5  90/08/21  13:55:40  emond
 * Fixed problem so file would compile. Added comma to products array.
 * 
 * Revision 3.4  90/08/16  11:24:21  grant
 * Added Annex3 MX secret code (43)
 * 
 * Revision 3.3  90/08/09  14:03:59  parker
 * Added product id string for Annex-3 MX.
 * 
 * Revision 3.2  90/06/08  11:05:47  loverso
 * Finish linkage for R6.0, add Annex-3 hw type, and fix broken copyright
 * 
 * Revision 3.1  90/04/11  14:51:05  grant
 * Added Annex 3 to product list
 * 
 * Revision 1.1  90/04/03  15:24:15  emond
 * Initial revision
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *****************************************************************************
 */


/*
 *****************************************************************************
 *                                                                           *
 *		     Include Files                                           *
 *                                                                           *
 *****************************************************************************
 */

#ifdef NA

/* This file must be first -- in the host NA only! */
#include "../inc/config.h"

#include "port/port.h"
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <strings.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <setjmp.h>
#include <ctype.h>
#define CMD_H_PARAMS_ONLY
#include "../inc/na/cmd.h"
#else
#include "../inc/port/port.h"
#include "types.h"
#include "../dfe/stdio.h"
#include "../netinet/in.h"
#include "netdb.h"
#include "strings.h"
#include "uio.h"
#include "setjmp.h"
#include "ctype.h"
#endif

#include "../inc/na/na.h"
#include "../inc/na/iftype.h"
#include "../inc/na/server.h"
#include "../inc/syslog.h"
#include "../inc/erpc/netadmp.h"
#include "../inc/na/na_selmods.h"
/*
 ***********************************************************************
 *                                                                     *
 *		     Defines and Macros                                *
 *                                                                     *
 ***********************************************************************
 */

#define lower_case(x) 	(isalpha(x) ? (islower(x) ? x : tolower(x)) : x)
#define min(a, b)	(((a) > (b)) ? (b) : (a))

#define CHUNK_SZ	5

#define NETID_MIN	0x0000
#define NETID_MAX	0xfffe
#define NODEID_MAX	0xfe

/*
 *****************************************************************************
 *                                                                           *
 *		     Structure Definitions                                   *
 *                                                                           *
 *****************************************************************************
 */



/*
 *****************************************************************************
 *                                                                           *
 *		     External Data Declarations                              *
 *                                                                           *
 *****************************************************************************
 */

extern UINT32 
	inet_addr();
extern char 
	*inet_ntoa();
extern int
	str_to_inet();

extern int errno;

/*
 *****************************************************************************
 *                                                                           *
 *		     Global Data Declarations                                *
 *                                                                           *
 *****************************************************************************
 */



/*
 *****************************************************************************
 *                                                                           *
 *		     Static Declarations                                     *
 *                                                                           *
 *****************************************************************************
 */

static char hex_digits[] = "0123456789abcdef";
static char oct_digits[] = "01234567";

struct mask_options syslog_levels[] = {
    { 1<< LOG_EMERG, "emergency" },
    { 1<< LOG_ALERT, "alert" },
    { 1<< LOG_CRIT, "critical" },
    { 1<< LOG_ERR, "error" },
    { 1<< LOG_WARNING, "warning" },
    { 1<< LOG_NOTICE, "notice" },
    { 1<< LOG_INFO, "info" },
    { 1<< LOG_DEBUG, "debug" },
    { 0, (char *)0 }
    };

struct mask_options serv_options[] = {
    { SERVE_IMAGE, "image" },
    { SERVE_CONFIG, "config" },
#ifndef MICRO_ELS
    { SERVE_MOTD, "motd" },
#endif
    { 0, (char *)0 }
    };

struct mask_options selectable_mods[] = {
    { OPT_ADMIN, "admin" },
    { OPT_ATALK, "atalk" },
    { OPT_DIALOUT, "dialout" },
    { OPT_CLIEDIT, "edit" },
    { OPT_FINGERD, "fingerd" },
    { OPT_FTPD, "ftpd" },
    { OPT_IPX, "ipx" },
    { OPT_LAT, "lat" },
    { OPT_NAMESERVER, "nameserver" },
    { OPT_PPP, "ppp" },
    { OPT_SLIP, "slip" },
    { OPT_SNMP, "snmp" },
    { OPT_TN3270, "tn3270" },
    { OPT_DEC, "vci" },
    { 0, (char *)0 }
    };

#ifdef NA
static struct {
	int code;
	char	*hw_name;
	char 	*sw_name;
} products[] = {
	{ 0,  "Annex",       "" },	{ 11, "Annex",       "-MX" },
	{ 13, "Annex",       "-UX" },	{ 16, "Annex-II",    "-MX" },
	{ 17, "Annex-II",    "-UX" },	{ 32, "Annex-X.25",  ""  },
	{ 42, "Annex-3",     "-UX" },	{ 43, "Annex-3",     "-MX" },
	{ 52, "Micro-Annex", "-UX" },	{ 53, "Micro-Annex", "-MX" },
	{ 55, "Micro-Annex-ELS", "-UX" },
	{ -1, NULL, NULL }
};

static char sw_id_buff[128];
#endif	/* NA */


/*
 *****************************************************************************
 *                                                                           *
 *		     Forward Routine Declarations                            *
 *                                                                           *
 *****************************************************************************
 */



UINT32 parse_sequence();
UINT32 parse_list();
#ifdef NA
int Sp_support_check();
int Ap_support_check();
#endif
char	*lex_token();



int
print_to_c(string, Pcardinal)	/* printable string (representing a character)
				   to cardinal conversion */
	char    string[];
	unsigned short *Pcardinal;
{
	int len;

	len = strlen(string);
	if (len == 1) {
	    if (Pcardinal)
		*Pcardinal = (unsigned short)string[0];
	    }
	else if (len == 2 && string[0] == '^')
	    {
	    if (Pcardinal)
		if (string[1] == '@')
			*Pcardinal = 0;
		else if (string[1] == '?')
			*Pcardinal = 0177;
		else if (((string[1] > 040) && (string[1] < 077)) 
			|| (string[1] == 0134)  || (string[1] == '`') ||
			(string[1] == '|') || (string[1] == '~')) 
			punt("invalid character:", string);
		else	
			*Pcardinal = (unsigned short)string[1] & 037;
	    }
	else if (len <= 4 && string[0] == '0')
	    {
	    unsigned short value;
	    char *Pstring,
		 *Pdigit;

	    value = 0;

	    if (string[1] == 'x' || string[1] == 'X')
		{
		Pstring = &string[1];

		while (*++Pstring)
		    { 
		    Pdigit = index(hex_digits, lower_case(*Pstring));

		    if (Pdigit)
			value = value * 16 + (Pdigit - hex_digits);
		    else if (Pcardinal)
			punt("invalid hex number: ", string);
		    else
			return 1;
		    }
		}
	    else
	        {
		Pstring = &string[1];

		while (*Pstring)
		    { 
		    Pdigit = index(oct_digits, *Pstring++);
		    if (Pdigit)
			value = value * 8 + (Pdigit - oct_digits);
		    else if (Pcardinal)
			punt("invalid octal number: ", string);
		    else
			return 1;
		    }
		}

	    *Pcardinal = value;
	    }
        else if (Pcardinal)
	    punt("invalid character: ", string);
	else
	    return 1;

	return(0);
} /* print_to_c() */


void
c_to_print(cardinal, string) /* cardinal to printable character conversion */

	u_short cardinal;
	char    string[];

{
	if (cardinal == 0)
	    (void)strcpy(string, /*NOSTR*/"^@");
	else if (cardinal == 0177)
	    (void)strcpy(string, /*NOSTR*/"^?");
	else if (cardinal < ' ')
	    {
	    string[0] = '^';
	    string[1] = cardinal + 0x40;
	    string[2] = '\0';
	    }
	else
	    {
	    string[0] = cardinal;
	    string[1] = '\0';
	    }

}	/* c_to_print */


	/*
	 *  string to inet addr conversion
	 */

#ifdef NA
str_to_inet(string, Ps_addr, zero_ok, oblivious)

	char          string[];
	UINT32        *Ps_addr;
	int           zero_ok,
		      oblivious;	/* oblivious to errors - dont punt */

{
	struct hostent *Phostent;

	/* If the string begins with a digit, assume a "dot notation" inet
	   address; otherwise, assume a /etc/hosts name. */

	if (isdigit(string[0]))
	{
	    *Ps_addr = inet_addr(string);
	    if (*Ps_addr == -1 && strcmp(string,/*NOSTR*/"255.255.255.255"))
	    {
		if(oblivious)
		    return -1;
		else
		    punt(BAD_BOX, string);
	    }

	    if (*Ps_addr == 0 && !zero_ok)
	    {
		if(oblivious)
		    return -1;
		else
		    punt(NONYMOUS_BOX, (char *)NULL);
	    }
	}
	else
	{
	    if (!(Phostent = gethostbyname(string)))
	    {
		if(oblivious)
		    return -1;
		else
		    punt(WHAT_BOX, string);
	    }
	    bcopy(Phostent->h_addr, (char *)Ps_addr, Phostent->h_length);
	}
	return (0);

}	/* str_to_inet() */
#endif


#ifdef NA
int
get_internal_vers(id, vers, hw, flag, ask)
UINT32 	id; 
UINT32  *vers, *hw, *flag;
int	ask;
{

	/* Pick off the version number */
	switch (id & 0x0000FFFF) {
        case 0x00000100:	/* VER. 1.0 */
		*vers = VERS_1;
		break;
        case 0x00000200:	/* VER. 2.0 */
	case 0x00000201:	/* VER. 2.1 */
		*vers = VERS_2;
		break;
	case 0x00000300:	/* VER. 3.0 */
		*vers = VERS_3;
		break;
	case 0x00000400:	/* VER. 4.0 */
		*vers = VERS_4;
		break;
	case 0x00000401:	/* VER. 4.1 */
		*vers = VERS_4_1;
		break;
	case 0x00000500:	/* VER. 5.0 */
		*vers = VERS_5;
		break;
	case 0x00000600:	/* VER. 6.0 */
		*vers = VERS_6;
		break;
	case 0x00000601:	/* VER. 6.1 */
		*vers = VERS_6_1;
		break;
	case 0x00000602:	/* VER. 6.2 */
		*vers = VERS_6_2;
		break;
	case 0x00000700:	/* VER. 7.0 */
		*vers = VERS_7;
		break;
	case 0x00000701:	/* VER. 7.1 */
		*vers = VERS_7_1;
		break;
	case 0x00000800:	/* VER. 8.0 */
		*vers = VERS_8;
		break;
	case 0x00000801:        /* VER. 8.1 */
		*vers = VERS_8_1;
		break;
	case 0x00000900:	/* VER. 9.0 */
	case 0x00000902:	/* VER. 9.2 */
		*vers = VERS_BIG_BIRD;
		break;
	default:
#define DEFAULT_VERS	VERS_BIG_BIRD
/* This define should be kept at the current version!  Don't forget to
   fix the prompt string below */

		if (ask == FALSE) {
		    *vers = DEFAULT_VERS;
		    break;
		}
		if (script_input) {
		    printf("Unknown software version, using default\n");
		    *vers = DEFAULT_VERS;
		}
		else {
#define REPLY_LENGTH 10
		    char	query_reply[REPLY_LENGTH];
		    char	*cmd_p = query_reply;
		    int		cmd_cnt = sizeof(query_reply);

		    fprintf(stdout, 
"Annex \"%s\" is running software which is newer than NA R9.0.\nSome parameters will not be accessible.\n\tDo you wish to continue anyway? (y/n) [n]: ",
			    symbol);
		    while (!fgets(cmd_p, cmd_cnt, stdin)) {
			if(ferror(cmd_file) && (errno == EINTR))
			    continue;
			return 1;
		    }
		    while (*cmd_p && index(WHITE_SPACE, *cmd_p))
			cmd_p++;

		    if (*cmd_p == 'y' || *cmd_p == 'Y') {
			*vers = DEFAULT_VERS;
			break;
		    }
		    else
			return 1;
		}
		break;
	}

	/* Pick off the product ID */
	switch ((id >> 16) & 0x0000FFFF) {
	case 11:			/* ANNEX-I MX */
		*flag |= ANX_RDRP;
	case 13:			/* ANNEX-I UX */
		*hw = ANX_I;
		break;

	case 16:			/* ANNEX-II MX */
		*flag |= ANX_RDRP;
	case 17:			/* ANNEX-II UX */
		*hw = ANX_II;
		break;

	case 32:			/* ANNEX-X25 */
		*hw = X25;
		break;

	case 43:			/* ANNEX-3 MX */
		*flag |= ANX_RDRP;
	case 42:			/* ANNEX-3 UX */
		*hw = ANX3;
		break;

	case 53:			/* MICRO-ANNEX MX */
		*flag |= ANX_RDRP;
	case 52:			/* MICRO-ANNEX UX */
		*hw = ANX_MICRO;
		break;

	case 55:			/* MICRO-ANNEX-ELS UX */
		*hw = ANX_MICRO_ELS;
		break;

	default:
		printf("Unknown hardware type, using default\n");
		*hw = ANX_I;
		break;
	}
   return 0;
}

#define PRODUCT 	1
#define MAJOR_REV	2
#define MINOR_REV	3

char *
display_sw_id(sw_id,hw_id)
	UINT32  sw_id, hw_id;
{
	union {
		UINT32	id_long;
		char	id_chars[4];
	} id_union;

	char *product_str = NULL, *sw_str = NULL;
	int i;

	if(!sw_id)
		return("Annex R1.0 - R2.0");

	id_union.id_long = htonl(sw_id);

	for (i = 0; product_str == NULL; i++)
	    if (products[i].code < 0) {
		product_str = /*NOSTR*/"??";
		sw_str = /*NOSTR*/"";
	    } else if (products[i].code ==
		(int)id_union.id_chars[PRODUCT]) {
		product_str = products[i].hw_name;
		sw_str = products[i].sw_name;
	    }

	sprintf(sw_id_buff, /*NOSTR*/"%s%s%s R%d.%d", product_str,
		hw_id == ANX_II_EIB ? /*NOSTR*/"e":/*NOSTR*/"",
		sw_str,
		id_union.id_chars[MAJOR_REV],
		id_union.id_chars[MINOR_REV]);

	return(sw_id_buff);
}
#endif	/* NA */

UINT32
parse_sequence(b, port_count)	/* returned in net order */
char *b;
INT32 port_count;
{
    int entered = 0;
    char chunk[CHUNK_SZ], *comma;
    union {
	UINT32 as_long;
	unsigned char seq[sizeof(UINT32)];
	} r;
    unsigned int i, slipper;
    int	length;

    if(!strcmp(b, "default"))
	return(0L);

    for(i = 0; i < sizeof(r.seq); ++i)
	r.seq[i] = IFBYTE_NONE;

    while(*b) {

	comma = index(b, ',');

	if(!comma) {			/* End of string */
	    (void)strncpy(chunk, b, CHUNK_SZ);
	    b += strlen(b);
	    }
	else {				/* Comma-delimited item */
	    (void)strncpy(chunk, /*NOSTR*/"", CHUNK_SZ);
	    (void)strncpy(chunk, b, min(CHUNK_SZ, comma - b));
	    b = comma + 1;
	    }

	if(chunk[CHUNK_SZ - 1]) {
	    chunk[CHUNK_SZ - 1] = '\0';
	    punt("interface name too long: ", chunk);
	    }

	if(entered == PDL_IS_SIZ)
	    punt("only 1-4 interfaces can be entered", (char *)0);

	/*
	 * Some broken machines (NCR) implement string macros in assembly and
	 * don't allow nested assembler macro calls (gak!)
	 */
	length = strlen(chunk);
	if(!strncmp(chunk, "net", length))
	    r.seq[entered++] = 0;

	else if(!strncmp(chunk, "sl", 2) && isdigit(chunk[2])) {
#if NSLIP == 0
	    punt("invalid interface name: ", chunk);
#else
	    slipper = atoi(&chunk[2]);
	    if(slipper < 2 || slipper > port_count)
		punt("invalid port number: ", chunk);
	    r.seq[entered++] = IFTYPE_SLIP | (slipper - 1);
#endif
	    }

	else if (!strncmp(chunk, "self", length)) {
	    r.seq[entered++] = IFTYPE_FLASH;
	    }

	else
	    punt("invalid interface name: ", chunk);

	} /* end of while(*b) loop */

    if(!entered)
	punt("invalid sequence", (char *)0);

    return(r.as_long);
}


void
decode_sequence(result, internal)
char *result;
UINT32 internal;	/* network order */
{
    unsigned char *u = (unsigned char *)&internal;
    int i, any = 0;
    unsigned char ifbyte;
    char *s;

    if(internal == 0L) {
	(void)strcpy(result, "net");
	return;
	}


    *result = '\0';

    for(i = 0; i < PDL_IS_SIZ; ++i) {

	ifbyte = *u++;

	if(ifbyte == IFBYTE_NONE)
	    continue;

	if(any++)
	    (void)strcat(result, /*NOSTR*/",");

	s = index(result, '\0');

	switch(ifbyte & IFTYPE_MASK) {
	case 0:
	    if(ifbyte & ~IFTYPE_MASK)
		sprintf(s, /*NOSTR*/"%s%d", "net",(ifbyte & ~IFTYPE_MASK) + 1);
	    else
		sprintf(s, "net");
	    break;
	case IFTYPE_SLIP:
	    sprintf(s, /*NOSTR*/"%s%d", "sl", (ifbyte & ~IFTYPE_MASK) + 1);
	    break;
	case IFTYPE_FLASH:
	    sprintf(s, "self");
	    break;
	default:
	    sprintf(s, /*NOSTR*/"??%d", (ifbyte & ~IFTYPE_MASK) + 1);
	    break;
	    }
	}

	if(!any)
	    (void)strcpy(result, "net");
}


UINT32
parse_list(b,table)	/* returned in host order */
char *b;
struct mask_options *table;
{
    char *comma, *bnext;
    int i, clen;
    UINT32 result;

    result = 0;

    while(*b) {

	comma = index(b, ',');

	if(!comma) {			/* End of string */
	    clen = strlen(b);
	    bnext = b + strlen(b);
	    }
	else {				/* Comma-delimited item */
	    clen = comma - b;
	    bnext = comma + 1;
	    }

	if(!clen) {
	  ++b;
	  continue;
	  }

	for(i = 0; table[i].name; ++i)
	    if(!strncmp(b, table[i].name, clen)) {
		result |= table[i].mask;
		break;
		}
	if(!table[i].name)
	    punt("invalid option: ", b);

	b = bnext;

	} /* end of while(*b) loop */

    return(result);
}


void
decode_mask(result, internal, table)
char *result;
unsigned short internal;	/* host order */
struct mask_options *table;
{
    unsigned short c = internal; /* (unsigned char)ntohs(internal); */
    int any = 0;
    int i;

	(void)strcpy(result, /*NOSTR*/"");
	any = 0;
	for(i = 0; table[i].mask; ++i)
	    if(c & table[i].mask) {

		if(any)
		    (void)strcat(result, /*NOSTR*/",");
		any = 1;
		(void)strcat(result, table[i].name);
	    }
}

int
trans_prompt(p)
char *p;
{
    char *src = p;
    char *dest = p;
    char *str = p;
    char fc;

    while(*src) {
	if(*src == '%') {			/* percent sign... */
    		fc = *(src + 1);
		switch (fc) {
       		case 'A':	
       		case 'C':	
       		case 'D':	
       		case 'I':	
       		case 'J':	
       		case 'L':	
       		case 'N':	
       		case 'P':	
        	case 'R':	
        	case 'S':	
        	case 'T':	
        	case 'U':	
			*dest++ = fc - 'A' + 1;	/* uppercase control codes */
			++src;
			break;
        	case 'a':	
        	case 'c':	
        	case 'd':	
        	case 'i':	
        	case 'j':	
        	case 'l':	
        	case 'n':	
        	case 'p':	
        	case 'r':	
        	case 's':	
        	case 't':	
        	case 'u':	
			*dest++ = fc - 'a' + 1;
			++src;
			break;
		default:
			punt("invalid formatting code: ", str);
			break;
		}
	}
	else					/* literal */
	    *dest++ = *src;
	++src;
	}

    *dest = '\0';
}

#ifdef NA
int
Ap_support_check(id, param)
ANNEX_ID *id;
int param;
{
	return 1;
}
int Ip_support_check(id, param)
ANNEX_ID *id;
int param;
{
	return (1);
}

int Sp_support_check(id, param)
ANNEX_ID *id;
int param;
{
	if (id->flag & ANX_MICRO_V11 && 
		(param == CONTROL_LINE_USE || param == NEED_DSR))
	  return(0);
  	return(1); 
}
#endif 


/*
 * encode interface parameters rip_accept and rip_advertise
 */
int
encode_rip_routers (external, internal)

char *external;                 /*  external representation (for human) */
char *internal;                 /*  internal representation (for Annex) */
{
	INTERN	Internal;
	int 	i, indx, length,
		err = 0;
	char 	*ptr;
        u_short         exclude = 0,
			include = 0;
	char            address_list[MAX_RIP_INT_STRING+1];
        char            token[MAX_RIP_EXT_STRING+1];     /* max length of network routing list */

	/* initialize first */
	Cinternal = internal;
	i = 1;
	indx = 1;
	CS_length = 1;
	length = 1;
	ptr = external;
	if (strlen(ptr) > MAX_RIP_EXT_STRING) {
		/*invalid parameter value*/
		return(1);
	}
	ptr = lex_token(ptr, token, (char *) NULL);
	/* 
	 * The acceptable parameter values for
	 * set interface rip_accept/rip_advertise are:
	 * 1. none
	 * 2. all or default
	 * 3. include/exclude xx.xx.xx.xx,xx....
	 * where xx.xx.xx.xx,xx... up to eight IP address.
	 */

	/*
	 * Here is the encode mechanism:
	 * There are 33 bytes in EEPROM, the first byte indicates
	 * the parameter type and the rest of 32 bytes store 
	 * the list of IP address in long format (four-byte hex).
	 * The first byte is encoded as follow:
	 * 	0x00 -> all (this is default)
	 *	0x01 -> none
	 * 	other -> the length of total bytes including the
	 *	         first byte. For example, 
	 *		 5 -> one IP ( 1 + 4)
	 *		 9 -> two IPs (1 + 8) and so on. 
	 *      Also, the highest bit of the first byte masks
	 *      either include or exclude.
	 *      mask off -> include
	 *      mask on -> exclude
	 */
	while (TRUE) {
		if (strncmp(token, "include", strlen(token)) == 0) {
			include=1;
			if (!ptr || !*ptr) {
				/*invalid syntax*/
				return(-1);
			}
			ptr = lex_token(ptr, token, /*NOSTR*/",");
			continue; 
		}
		if (strncmp(token, "exclude", strlen(token)) == 0) {
			exclude=1;
			if (!ptr || !*ptr) {
				/*invalid syntax*/
				return(-1);
			}
			ptr = lex_token(ptr, token, /*NOSTR*/",");
			continue;
		}
		if (token[0] == ',') {
			if (!ptr || !*ptr) {
				/*invalid syntax*/
				return(-1);
			}
			ptr = lex_token(ptr, token, /*NOSTR*/",");
			continue;
		}
		/*
		 * all -> length one byte and value is 0
		 */
		if ((strcmp(token, ALL_STR) == 0) ||
		    (strcmp(token, "default") == 0)) {
			length = 1;
			CS_length = 1;
			CS_string[0] = 0;
			break;
		} else if (strcmp(token, NONE_STR) == 0) {
			length = 1;
			CS_length = 1;
			CS_string[0] = 1;
			break;
		} else {
			if (!include && !exclude) 
				/*invalid syntax*/
				return(-1);
			/* 
			 * convert dot-decimal format into inet format
			 */
			err = str_to_inet(token, Linternal, TRUE, 1);
			if ( err )
				/*invalid parameter value */
				return(1);
			/* 
			 * stores as string byte 
			 */
			*(INT32 *)internal = *Linternal; 
			indx = indx + 4;
			if (length >= MAX_RIP_INT_STRING) {
				/*invalid parameter value */
				return(1);
			}
			for ( i = length; i < indx; i++)
				address_list[i] = *internal++;
			length = length + 4;
			if (!ptr || !*ptr)
				break;
			ptr++;
			ptr = lex_token(ptr, token, /*NOSTR*/",");
			continue; 
		}
	}
	if ( include || exclude ) {
		CS_length = length;
		if ( exclude )
			address_list[0] = length | 0x80;
		else
			address_list[0] = length;
		ptr = address_list;
		bcopy(ptr, CS_string, length);
	}
	return (0);
}


/*
 * encode annex rip_routers parameter
 */
int
encode_box_rip_routers (external, internal)

char *external;                 /*  external representation (for human) */
char *internal;                 /*  internal representation (for Annex) */
{
	INTERN	Internal;
	int 	i, indx, length,
		err = 0;
	char 	*ptr;
        char            address_list[MAX_RIP_INT_STRING+1];
        char            token[MAX_RIP_EXT_STRING+1];     /* max length of network routing list */

	/* initialize first */
	Cinternal = internal;
	i = 1;
	indx = 1;
	CS_length = 1;
	length = 1;
	ptr = external;
	if (strlen(ptr) > MAX_RIP_EXT_STRING) {
		/*invalid parameter value */
		return(1);
	}
	ptr = lex_token(ptr, token, /*NOSTR*/",");
	/* 
	 * The acceptable parameter values for
	 * set annex rip_routers are:
	 * 1. all or default
	 * 2. xx.xx.xx.xx,xx....
	 * where xx.xx.xx.xx,xx... up to eight IP address.
	 * check CNV_RIP_ROUTERS for detail encode mechanism:
	 */

	while (TRUE) {
		/*
		 * all -> length one byte and value is 0
		 */
		if ((strcmp(token, ALL_STR) == 0) ||
		    (strcmp(token, "default") == 0)) {
			CS_string[0] = 0;
			break;
		} else if (token[0] == ',') {
			if (!ptr || !*ptr) {
				/*invalid syntax*/
				return(-1);
			}
			ptr = lex_token(ptr, token, /*NOSTR*/",");
			continue;
		} else {
			/* 
			 * convert dot-decimal format into inet format
			 */
			err = str_to_inet(token, Linternal, TRUE, 1);
			if ( err )
				/*invalid parameter value */
				return(1);
			/* 
			 * stores as string byte 
			 */
			*(INT32 *)internal = *Linternal; 
			indx = indx + 4;
			if (length >= MAX_RIP_INT_STRING) {
				/*invalid parameter value */
				return(1);
			}
			for ( i = length; i < indx; i++)
				address_list[i] = *internal++;
			length = length + 4;
			if (!ptr || !*ptr)
				break;
			ptr++;
			ptr = lex_token(ptr, token, /*NOSTR*/",");
			continue; 
		}
	}
	/*
	 * if not "all" string, must be IP address
	 */
	if ( length != 1) {
		CS_length = length;
		address_list[0] = length;
		ptr = address_list;
		bcopy(ptr, CS_string, length);
	}
	return (0);	

}	

/*
 * decode interface rip_accept and rip_advertise parameters
 */
int
decode_rip_routers(internal, external)

char *internal;			/*  internal representation (for Annex) */
char *external;			/*  external representation (for human) */
{
	char		*ptr;
	INTERN		Internal;	/*  union of pointers to types	*/
	int		length; 	/*  length of a string		*/
	char            address_list[MAX_RIP_EXT_STRING+1];

	Cinternal = internal;	/*  move to pointer to various types	*/
	
	length = CS_length;

	address_list[0] = '\0';
	ptr = CS_string;
		
	/*
	 * none -> the value of first byte is 1
	 */
	if (length == 1 && *ptr == 1) {
		strcpy(external, NONE_STR);
		return(0);
	}
	/*
	 * all -> default
	 */
	else if (length <= 1) {
		strcpy(external, ALL_STR);
		return(0);
	}
	else if ((*ptr & RIP_LEN_MASK) < 5 || 
			(*ptr & RIP_LEN_MASK) > MAX_RIP_INT_STRING ||
			(((*ptr &  RIP_LEN_MASK) - 1) % 4) != 0) {
		/*bad format of eeprom router address string*/
		strcpy(external, NONE_STR);
		return(-1);
	}
	/*
	 * contains include/exclude IP address. Decode the way
	 * as it encodes. See the comments on encode routine 
	 * for detail.
	 */
	else {
		(void)strcat(address_list, "include ");
		length = *ptr++;
		if (length & 0x80) {
			length = length & RIP_LEN_MASK;
			(void)strcpy(address_list, "exclude ");
		}
		length = length - 1;

		while (length > 0) {
			 bcopy(ptr, Linternal, sizeof(INT32));
#ifdef NA
			(void)strcpy(external, inet_ntoa(*Ninternal));
#else
			inet_ntoa(external, *Ninternal);
#endif
			(void)strcat(address_list, external);
			ptr = ptr + 4;
			length = length - 4;
			if (length > 0)
				(void)strcat(address_list, /*NOSTR*/",");
		}
		(void)strcpy(external, address_list);
	}
	return(0);
}

/*
 * decode annex rip_routers parameter
 */
int
decode_box_rip_routers(internal, external)

char *internal;			/*  internal representation (for Annex) */
char *external;			/*  external representation (for human) */
{
	char		*ptr;
	INTERN		Internal;	/*  union of pointers to types	*/
	int		length; 	/*  length of a string		*/
        char            address_list[MAX_RIP_EXT_STRING+1];

	Cinternal = internal;	/*  move to pointer to various types	*/

	length = CS_length;
	address_list[0] = '\0';
	ptr = CS_string;
	/*
	 * all -> default
	 */
	if (length <= 1) {
		strcpy(external, ALL_STR);
		return(0);
	}
	else if (*ptr < 5 || *ptr > MAX_RIP_INT_STRING ||
		    ((u_short) ((*ptr) - 1) % 4) != 0) {
		/*bad format of eeprom router address string*/
		strcpy(external, ALL_STR);
		return(-1);
	}
	/*
	 * contains IP address. Decode the way as it encodes.
	 * See the comments on encode routine for detail.
	 */
	else {
		length = *ptr++;
		length = length - 1;
		while (length > 0) {
			 bcopy(ptr, Linternal, sizeof(INT32));
#ifdef NA
			(void)strcpy(external, inet_ntoa(*Ninternal));
#else
			inet_ntoa(external, *Ninternal);
#endif
			(void)strcat(address_list, external);
			ptr = ptr + 4;
			length = length - 4;
			if (length > 0)
				(void)strcat(address_list, /*NOSTR*/",");
		}
		(void)strcpy(external, address_list);
	}
	return(0);
}

/*
 * encode node_id and at_nodeid appletalk parameters
 */
int
encode_nodeid (external, internal)

char *external;                 /*  external representation (for human) */
char *internal;                 /*  internal representation (for Annex) */
{
	INTERN		Internal;
	int 		indx, err;
	UINT32 	sum, factor;
	u_short		shift, node_value, net_value;
	u_char		num;
	char		c;
	char		*ptr2, *ptr;
	
	/* initialize first */
	Cinternal = internal;
	num = err = 0;

	/*
	 * Check net_value.node_value format first
	 * Format: xxxx.xx
         *     where xxxx -> range from 1 .. 0xfffe
	 *           xx   -> range from 0 .. 0xfe
	 *     xxxx and xx can be either hex or decimal format.
         */
	ptr2 = index(external,'.');
	if (ptr2 == (char *)NULL)
	    /* Incorrect format for this parameter */
	    return(-1);
        /* initialize */
        ptr = external;
	indx = 0;
	/*
	 * Hex or decimal ?
         */
	if (external[0] == '0' && external[1] == 'x') {
	    ptr = ptr + 2;
	    shift = 0x10;
	}
	else {
	    shift = 10;
        }
										  /* Look for period character and find the length of xxxx */
	for (; *ptr != '.'; ptr++)
	    indx++;
	if (indx > 5)
	    /* Incorrect format for this parameter */
	    return(-1);
	/*
	 * Convert xxxx into net_value
         */
        for (factor = 1, net_value = 0;
		   indx > 0;
		   indx--, factor *= shift) {
	    c = *--ptr;;
	    if (isxdigit(c)) {
	        if (isupper(c))
	      	    num = (u_char)c - (u_char)'A' + 10;
	        else if (islower(c))
		    num = (u_char)c - (u_char)'a' + 10;
		else if (isdigit(c))
		    num = (u_char)c - (u_char)'0';
		/*
		 * Don't accept non-digit chars if non-hex format
		 */
		if ((shift == 10) && (num > 9))
			return(-2);
	        net_value += factor * num;
	    }
	    else
	        /* Invalid character in this parameter */
		return(-2);
        }
						       
        /* Make sure range falls between 1 .. 0xfffe */
	if (net_value < NETID_MIN || net_value > NETID_MAX)
	    /* Invalid range in this parameter */
	    return(-3);
				     
	/* Look for node_value now */
	ptr2++;
	ptr = ptr2;
	indx = 0;
	if ( *ptr2 == '0' && *++ptr2 == 'x') {
	    ptr = ptr + 2;
	    shift = 0x10;
        }
        else  {
	    shift = 10;
        }
        for (; *ptr != '\0'; ptr++)
	    indx++;
	if (indx > 3)
	    /* Incorrect format for this parameter */
	    return(-1);
        /*
         * Convert xx into node_value
	 */
	for (factor = 1, node_value = 0; indx > 0;
		  indx--, factor *= shift) {
	    c = *--ptr;
	    if (isxdigit(c)) {
	        if (isupper(c))
		    num = (u_char)c - (u_char)'A' + 10;
		else if (islower(c))
		    num = (u_char)c - (u_char)'a' + 10;
	        else if (isdigit(c))
		    num = (u_char)c - (u_char)'0';
		/*
		 * Don't accept non-digit chars if non-hex format
		 */
		if ((shift == 10) && (num > 9))
			return(-2);
		node_value += factor * num;
	    }
	    else
		/* Invalid character in this parameter */
		return(-2);
	}

	/* Make sure range fall between 0 ..0xfd */
	if (node_value > NODEID_MAX)
	    /* Invalid range in this parameter */
	    return(-3);
				     
	/*
	 * Store net_value and node_value (both unsigned short) into
	 * 4 bytes (UINT32) in EEPROM now
	 */
        sum = (net_value << 16) | node_value;
        *Linternal = sum;
	return (0);
}

/*
 * encode AppleTalk default_zone_list parameters.
 * external is a string with quotes (") and backslashes (\) already escaped.
 * appletalk zone list can have embedded spaces so they can also be escaped.
 * this is a list of zones comma or space separated o comma and spaces need
 * to be escaped. 
 */
int
encode_def_zone_list (external, internal)

char *external;                 /*  external representation (for human) */
char *internal;                 /*  internal representation (for Annex) */
{
	INTERN		Internal;
	unsigned char	*ptr, *zone_len_ptr, *zone_ptr, c;
	int		length, i, zone_len, total_zones_length;
	int		in_escape = 0;
	
#define iszonechar(c) (((c >=0x20) && (c != 0x7f) && (c <= 0xd8))?1:0)

	/* initialize first */
	Cinternal = internal;
	ptr = (unsigned char *)external;
	length = strlen(external);
        zone_len_ptr = (unsigned char *)CS_string;
	zone_ptr = zone_len_ptr +1; 
        zone_len = 0;
        total_zones_length = 1; /* count first length byte */
	
	/* 
         * scan each input byte and check for:
	 * 1. is it a valid zone character?
	 * 2. if prior byte was an escape "\" then keep this one and exit
         *    escaping state
         * 3. if its an escape "\" then enter ecape state and test next byte.
	 * 4. Is it a zone separator " " or "," or tab then write the len, setup
	 *    the next zone and set the current len to 0.  If len is already
	 *    0 then we are in the case of duplicate separators or trailling
	 *    separators like "blue    red,,,green".  Just strip the extras.
	 * 5. just data move and count it.
         * 6. make sure we don't exceed the max zone len 32 or the max zone 
	 *    list len of 100 including length bytes.
         */
	for (i=0; i < length; i++) {
          c = *ptr++;
	  if (in_escape) {
	    in_escape = 0;
            if (iszonechar(c)) {
              *zone_ptr++ = c;
	      zone_len++;
            } else
	      return (-2);
          } else if ( c == '\\' ) {
	    in_escape = 1;
            continue;
          } else if ( c == ' ' || c == ',' || c == '\t') {
            if (zone_len == 0)
              continue;
	    *zone_len_ptr = zone_len;
            zone_len_ptr = zone_ptr++;
            zone_len = 0;
          } else {
	    if (iszonechar(c)) {
	       zone_len++;
	       *zone_ptr++ = c;
	    } else
	       return (-2);
          }
          if ( zone_len > 32 )
            return (-3);
          total_zones_length++;
          if (total_zones_length > 100)
	    return (-1);
	}
        *zone_len_ptr = zone_len;
	CS_length = total_zones_length;
	return (0);
}

/*
 * encode AppleTalk zone parameter.
 * external is a string with quotes (") and backslashes (\) already escaped.
 * appletalk zones can have embedded spaces so they can also be escaped.
 */
int
encode_zone (external, internal)

char *external;                 /*  external representation (for human) */
char *internal;                 /*  internal representation (for Annex) */
{
	INTERN		Internal;
	unsigned char		*ptr, *zone_ptr, c;
	int		length, i, zone_len;
	int		in_escape = 0;
	
	/* initialize first */
	Cinternal = internal;
	ptr = (unsigned char *)external;
	length = strlen(external);
        zone_ptr = (unsigned char *)CS_string;
        zone_len = 0;
	
	/* 
         * scan each input byte and check for:
	 * 1. is it a valid zone character?
	 * 2. if prior byte was an escape "\" then keep this one and exit
         *    escaping state
         * 3. if its an escape "\" then enter ecape state and test next byte.
	 * 4. just data move and count it.
         * 5. make sure we don't exceed the max zone len 32 
         */
	for (i=0; i < length; i++) {
          c = *ptr++;
	  if ( !iszonechar(c) ) 
	    return (-2);
	  else if (in_escape) {
	    in_escape = 0;
            *zone_ptr++ = c;
	    zone_len++;
          } else if ( c == '\\' ) {
	    in_escape = 1;
            continue;
          } else {
	    zone_len++;
	    *zone_ptr++ = c;
          }
          if ( zone_len > 32 )
            return (-1);
	}
	CS_length = zone_len;
	return (0);
}

/*
 *****************************************************************************
 *
 * Function Name:	lex_token()
 *
 * Functional Description:	As the name implies, this function performs
 *				a type of lexical analysis.  It "peels"
 *				out the next parameter in the string, from,
 *				and copies it to an output field, to.
 *				Also, there is a string of "special"
 *				characters that may are used as delimiters
 *				in addition to the normal white space.
 *
 * Parameters:		from - that pointer to the pointer of the string;
 *			to   - pointer to buffer to copy next arg;
 *			special - string of chars that are delimiters, in
 *				conjunction to normal white space.
 *
 * Return Value:	pointer to next arguement in list
 *
 * Side Effects:	This function updates the from pointer to the next
 *			argument location.
 *
 * Exceptions:
 *
 * Assumptions:
 *
 *****************************************************************************
 */

char *
lex_token (from, to, special)
    char *from,		/* ptr to char ptr (string to parse) */
         *to,			/* dst of next arg */
         *special;		/* other delimeters */
{
	char *ptr = from;	/* ptr to string to parse */

	if (!ptr || !*ptr) {	/* if nothing to parse */
		*to = '\0';
		return(ptr);
	}

	for (; *ptr && isspace(*ptr); ptr++);	/* skip spaces */

	/* parse up to white space or special delimiter */
	while (*ptr && (isgraph(*ptr) || iscntrl(*ptr)) &&
		( !special || !index(special, *ptr))) {
	    *to++ = *ptr++;
	}
	*to = '\0';	/* null terminate parsed arg */

	for (; *ptr && isspace(*ptr); ptr++);	/* skip spaces */

	return(ptr);		/*    return ptr to the rest */
}

int
str_to_enet(string, addr)
char *string;
unsigned char *addr;
{
    int cnt = 0;
    unsigned int val = 0;

    for (cnt = 0; *string; string++) {
        if (isdigit(*string)) {
            val = (val << 4) + (*string - '0');
        } else if (isxdigit(*string)) {
            val = (val << 4) + (*string + 10 - (islower(*string) ? 'a' : 'A'));
        } else if ((*string == '-') || (*string == ' ')) {
            if (val > 255 || *(string + 1) == '\0') {
                return(-1);
            }
            *addr++ = (unsigned char) val;
            cnt++;
            val = 0;
        } else {
            return(-1);
        }
    }
    if (val > 255 || ++cnt != 6) {
        return(-1);
    }
    *addr++ = (unsigned char) val;
    return(0);
}

int
str_to_mop_passwd(string, addr)
char *string;
unsigned char *addr;
{
    int iid = 0;        /* input index */
    int oid = 0;        /* output index */
    int i;
    int sz;
    unsigned int val = 0;
    u_char *ptr;

    iid = strlen(string);
    if (iid > (MOP_PASSWD_SZ * 2))
        return(-1);
    oid = 0;

    bzero(addr, MOP_PASSWD_SZ);

    /* convert 16-hex ascii into 8 bin values - mop password */
    /* who can figure */

    /* convert from the string end, to string begin */
    /* the -1 in loop insures that we get all data */
    /* of strings that have an odd length */
    /* converted, reversed and stored into addr */

    for (iid -= 2, sz = 2; iid >= -1; iid -= 2) {
        if (iid < 0) {
            iid = 0;
            sz = 1;
        }
        val = 0;
        ptr = (u_char *)&string[iid];
        for (i = 0; i < sz; i++, ptr++) {
            if (isdigit(*ptr)) {
                val = (val << 4) + (*ptr - '0');
            } else if (isxdigit(*ptr)) {
                *ptr |= 0x20;   /* make lower case */
                val = (val << 4) + (*ptr + 10 - 'a');
            } else
                return(-1);
        }

        if (oid >= MOP_PASSWD_SZ)
            return(-1);
        addr[oid++] = (unsigned char) val;
    }
    return(0);
}

void
convert_bit_to_num(bitstring, pattern)
	char *bitstring;
	char *pattern;
{
	char	*ptr;
	int 	length,		/*  length of a string		*/
		byte, 
		lastone = MAX_BIT_STRING * NBBY, /* initially large */
		span = FALSE,
		bit;


	/* start from scratch */
	*pattern = '\0';
	ptr = bitstring;
	for (byte = 0; byte < MAX_BIT_STRING; byte++) {
	    for (bit = 0; bit < NBBY; bit++) {
		length = strlen(pattern);
		if (*ptr & 0x01) {
		    if ((lastone + 1) == (byte * NBBY) + bit) {
			span = TRUE;
		    } else {
			/*
			 * plus one from zero_base to one_base
			 */
		        sprintf(&pattern[length], /*NOSTR*/"%d,",
			        (byte * NBBY) + bit + 1);
		    }
		    lastone = (byte * NBBY) + bit;
		} else {
		    if (span) {
			length--;
			/*
			 * plus one from zero_base to one_base
			 */
		        sprintf(&pattern[length], /*NOSTR*/"-%d,",
				lastone + 1);
		    }
		    span = FALSE;
		}
		*ptr >>= 1;
	    }
	    ptr++;
	}
	if (span) {
	    length--;
	    sprintf(&pattern[length], /*NOSTR*/"-%d", lastone + 1);
	}
	length = strlen(pattern);
	if (length) {
	    if (pattern[length - 1] == ',') 
	        pattern[length - 1] = '\0';
	 }
}

/*
 * encode annex kerberos hosts and tags parameter
 */
int
encode_kerberos_list (external, internal)

char *external;                 /*  external representation (for human) */
char *internal;                 /*  internal representation (for Annex) */
{
	INTERN	Internal;
	int 	i, indx, length,
		err = 0;
	char 	*ptr;
        char            address_list[MAX_KERB_INT_STRING+1];
        char            token[MAX_KERB_EXT_STRING+1];     /* max length of kerberos list */

	/* initialize first */
	Cinternal = internal;
	i = 1;
	indx = 1;
	CS_length = 1;
	length = 1;
	ptr = external;
	if (strlen(ptr) > MAX_KERB_EXT_STRING) {
		/*invalid parameter value */
		return(1);
	}
	ptr = lex_token(ptr, token, /*NOSTR*/",");
	/* 
	 * The acceptable parameter values for
	 * set annex kerberos lists are:
	 * 1. default (Null list)
	 * 2. xx.xx.xx.xx,xx....
	 * where xx.xx.xx.xx,xx... up to four IP address.
	 */

	while (TRUE) {
		/*
		 * default -> length one byte and value is 0
		 */
		if ((strcmp(token, "default") == 0)) {
			CS_string[0] = 0;
			break;
		} else if (token[0] == ',') {
			if (!ptr || !*ptr) {
				/*invalid syntax*/
				return(-1);
			}
			ptr = lex_token(ptr, token, /*NOSTR*/",");
			continue;
		} else {
			/* 
			 * convert dot-decimal format into inet format
			 */
			err = str_to_inet(token, Linternal, TRUE, 1);
			if ( err )
				/*invalid parameter value */
				return(1);
			/* 
			 * stores as string byte 
			 */
			*(long *)internal = *Linternal; 
			indx = indx + 4;
			if (length >= MAX_KERB_INT_STRING) {
				/*invalid parameter value */
				return(1);
			}
			for ( i = length; i < indx; i++)
				address_list[i] = *internal++;
			length = length + 4;
			if (!ptr || !*ptr)
				break;
			ptr++;
			ptr = lex_token(ptr, token, /*NOSTR*/",");
			continue; 
		}
	}
	/*
	 * if not "default" string, must be IP address
	 */
	if ( length != 1) {
		CS_length = length;
		address_list[0] = length;
		ptr = address_list;
		bcopy(ptr, CS_string, length);
	}
	return (0);	

}	


/*
 * decode annex kerberos hosts and tags parameter
 */
int
decode_kerberos_list(internal, external)

char *internal;			/*  internal representation (for Annex) */
char *external;			/*  external representation (for human) */
{
	char		*ptr;
	INTERN		Internal;	/*  union of pointers to types	*/
	int		length; 	/*  length of a string		*/
        char            address_list[MAX_KERB_EXT_STRING+1];

	Cinternal = internal;	/*  move to pointer to various types	*/

	length = CS_length;
	address_list[0] = '\0';
	ptr = CS_string;
	/*
	 * default...no addresses in the list.
	 */
	if (length <= 1) {
		strcpy(external, "0.0.0.0");
		return(0);
	}
	else if (*ptr < 5 || *ptr > MAX_KERB_INT_STRING ||
		    ((u_short) ((*ptr) - 1) % 4) != 0) {
		/*bad format of eeprom kerberos address string*/
		strcpy(external, "0.0.0.0");
		return(-1);
	}
	/*
	 * contains IP address. Decode the way as it encodes.
	 * See the comments on encode routine for detail.
	 */
	else {
		length = *ptr++;
		length = length - 1;
		while (length > 0) {
			 bcopy(ptr, Linternal, sizeof(long));
#ifdef NA
			(void)strcpy(external, inet_ntoa(*Ninternal));
#else
			inet_ntoa(external, *Ninternal);
#endif
			(void)strcat(address_list, external);
			ptr = ptr + 4;
			length = length - 4;
			if (length > 0)
				(void)strcat(address_list, /*NOSTR*/",");
		}
		(void)strcpy(external, address_list);
	}
	return(0);
}
