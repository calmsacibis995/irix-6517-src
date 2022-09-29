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
 * 	%$(Description)$%
 *
 * Original Author: %$(author)$%	Created on: %$(created-on)$%
 *
 * Revision Control Information:
 *
 * $Id: parse.c,v 1.3 1996/10/04 12:10:56 cwilson Exp $
 *
 * This file created by RCS from:
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/na/RCS/parse.c,v $
 *
 * Revision History:
 *
 * $Log: parse.c,v $
 * Revision 1.3  1996/10/04 12:10:56  cwilson
 * latest rev
 *
 * Revision 2.109.102.5  1995/01/11  15:52:58  carlson
 * SPR 3342 -- Removed TMUX (not supported until R9.3).
 *
 * Revision 2.109.102.4  1995/01/06  16:09:20  bullock
 * spr.3715 - Fix parsing in annex_pair_list of group_val and vcli_groups
 * and parsing of authorized_groups in port_pair_list.  Reviewd by Russ L.
 *
 * Revision 2.109.102.3  1994/12/06  09:31:13  carlson
 * Removed references to authorized groups and latb enable in favor of
 * P_LAT_CAT.
 *
 * Revision 2.109.102.2  1994/11/22  14:13:27  couto
 * Only report sync port count if some are present (ie. sync_count != 0).
 *
 * Revision 2.109.102.1  1994/11/03  10:26:17  barnes
 * SPR 2904: correct misspellings of asynchronous and synchronous
 *
 * Revision 2.109  1994/09/21  17:05:07  sasson
 * SPR 3529: fixed alignment problem in all the routines
 * that use union INTERN.
 *
 * Revision 2.108  1994/07/13  13:57:08  bullock
 * spr.2990 - if we're setting rip_advertise or rip_accept, don't assume
 * the rest of the string is the parameter value, instead, look for the
 * include/exclude and deal with it correctly.
 *
 * Revision 2.107  1994/05/25  10:30:44  russ
 * Bigbird na/adm merge.
 *
 * Revision 2.106  1994/04/07  09:35:02  russ
 * allow read and write to handle all parameters. write of keys and
 * passwords are written as comment lines.
 *
 * Revision 2.105  1994/02/01  16:37:19  russ
 * Added another argument to get_internal_vers...  This is a True or
 * False to ask the user for prompting.  TRUE from parsing the annex list.
 *
 * Revision 2.104  1993/12/30  14:04:09  carlson
 * SPR 2084 -- made compatible with 64 bit machines.
 *
 * Revision 2.103.2.10  1994/05/04  19:40:47  couto
 * Added SYNC_PORT_SECURITY_GROUP case to sync_show_header().
 *
 * Revision 2.103.2.9  1994/03/13  10:20:00  russ
 * reorganized the code... fixed problems in calcualtions of
 * interfaces.  sync code was using port count instead of sync count
 * in do_set_sync.
 *
 * Revision 2.103.2.8  1994/03/07  13:11:46  russ
 * removed all X25 trunk code.
 * ran formatter.
 *
 * Revision 2.103.2.7  1994/03/04  16:53:56  russ
 * Allow parameters for keyed off or disabled modules to be set!
 * We still don't allow them to be shown!  Just set.
 *
 * Revision 2.103.2.6  1994/03/02  11:23:07  russ
 * minor display changes to look more like admin output.
 *
 * Revision 2.103.2.5  1994/03/01  15:30:48  russ
 * fixed about 20 logic errors.  left the other 475.
 *
 * Revision 2.103.2.4  1994/02/22  11:22:32  russ
 * start na with empty port sync printer and interface sets.  Don't try
 * to tell the user what interfaces exist on annex command.  This info
 * is misleading.
 *
 * Revision 2.103.2.3  1994/02/14  09:34:35  russ
 * support changes for warning user when port, printer, interface set
 * is unset.
 *
 * Revision 2.103.2.2  1994/02/04  16:54:38  russ
 * changed na banner ports to async ports.
 *
 * Revision 2.103.2.1  1994/01/06  15:16:37  wang
 * Rel8.1 NA/Admin params support.
 *
 * Revision 2.103  1993/10/06  14:54:52  bullock
 * spr.2125 - Picky ANSI compiler needs strncpy parameters casted to
 * (char *), can't handle the fact that they are unsigned char *
 *
 * Revision 2.102  1993/09/20  14:22:46  reeve
 * Added checks to see how option key was set. Reviewed by David Wang.
 * [SPR 1738]
 *
 * Revision 2.101  1993/07/30  10:38:45  carlson
 * SPR 1923 -- rewrote pager to use file descriptor instead of file pointer.
 *
 * Revision 2.100  1993/07/27  20:27:19  wang
 * na/admin interface redesign changes.
 *
 * Revision 2.99  1993/06/30  18:00:34  wang
 * Removed DPTG support.
 *
 * Revision 2.98  93/06/30  15:14:26  wang
 * Fixed spr1816, report error if no pritner attached.
 * 
 * Revision 2.97  93/06/10  11:46:01  wang
 * Added support for "copy interface" command and fix spr1715.
 * Reviewed by Thuan Tran.
 * 
 * Revision 2.96  93/05/25  15:38:57  wang
 * Minor change from previous checkin.
 * 
 * Revision 2.95  93/05/25  11:41:57  wang
 * Fixed Spr1633, NA doesn't know how many printer Annex has. 
 * Reviewed by Scott Reeve.
 * 
 * Revision 2.94  93/05/20  18:01:15  carlson
 * SPR 1167 -- cleaned up error handling and passed back codes.
 * Reviewed by David Wang.
 * 
 * Revision 2.93  93/05/20  16:26:45  wang
 * Fixed interface number out of sync problem. Reviewed by Jim Carlson.
 * 
 * Revision 2.92  93/05/12  16:07:54  carlson
 * R7.1 -- merged error states and call sub.c routine for printing
 * netadm errors (token_error).
 * 
 * Revision 2.91  93/03/30  10:58:58  raison
 * do not display anything about annex parms that are void_cat
 * 
 * Revision 2.90  93/03/24  15:14:28  wang
 * Display interface as well as port info if annex command is given from NA.
 * 
 * Revision 2.89  93/03/22  13:05:59  wang
 * Show serial cat params when show port slip or ppp categories.
 * 
 * Revision 2.88  93/02/26  09:32:03  barnes
 * correct character array sizes to prevent stack corruption when parsing
 * long router address lists
 * 
 * Revision 2.87  93/02/24  13:25:22  carlson
 * Removed pager.
 * 
 * Revision 2.86  93/02/20  13:44:24  wang
 * Removed interface parameters related defines to na.h
 * 
 * Revision 2.85  93/02/18  13:31:32  wang
 * Removed sync card interface support.
 * 
 * Revision 2.84  93/02/09  16:43:23  wang
 * Change the default to show annex generic parameters and show port generic 
 * and flow catagory parameters only.
 * 
 * Revision 2.83  93/02/09  13:16:00  wang
 * Fixed annex rip_routers problem due to different syntax from 
 * rip_accept/rip_advertise. Removed include/exclude display.
 * 
 * Revision 2.82  93/02/05  18:11:08  wang
 * Fixed the support for setting network routing list.
 * 
 * Revision 2.81  93/01/28  15:09:29  wang
 * Converted interface logical index into human-eye form.
 * 
 * Revision 2.80  93/01/27  17:59:45  wang
 * Added in TN3270 port parameters category header.
 * 
 * Revision 2.79  93/01/22  18:16:19  wang
 * New parameters support for Rel8.0
 * 
 * Revision 2.78  92/12/17  15:20:19  bullock
 * spr.1207 - Must specify which printer using "name" when calling set_ln_param
 * to change a printer parameter.
 * 
 * Revision 2.77  92/12/10  11:04:04  russ
 * 
 * Revision 2.75.1.1  92/12/10  10:53:52  russ
 * Made DEV2 parameters (of which only one exists: port_password) show up
 * in "show port device".
 * 
 * Revision 2.76  92/10/15  14:27:43  wang
 * Added new NA parameters for ARAP support.
 * 
 * Revision 2.75  92/08/14  15:23:42  reeve
 * Made Annex II/IIE and RDRP determination backward compatible.
 * 
 * Revision 2.74  92/08/03  14:13:37  reeve
 * All platforms now pick up flag upon return from get_port_eib.
 * 
 * Revision 2.73  92/07/26  21:26:13  raison
 * allow verbose-help to be in disabled_modules list for non-AOCK,non-ELS
 * images
 * 
 * Revision 2.72  92/07/19  17:43:12  reeve
 * Made port_password settable in port_set_sub().
 * 
 * Revision 2.71  92/07/08  15:40:34  wang
 * Fixed spr949, na set port parameters problem.
 * 
 * Revision 2.70  92/06/25  15:03:53  barnes
 * fix pointer problem that causes a dump when doing "show port lat"
 * when running NA on Suns (spr 927)
 * 
 * Revision 2.69  92/06/15  15:00:20  raison
 * separate attn_char_string so that Annex II's know it as attn_char and
 * 
 * Annex 3's and Micro's know it as attn_string.
 * 
 * Revision 2.68  92/06/12  14:14:14  barnes
 * mucho changes for new NA displays
 * 
 * Revision 2.67  92/04/27  14:32:29  carlson
 * Rewrote annex_list -- no longer recursive, and more robust in the
 * face of errors; small changes to annex_name to fix problems where
 * return value was uninitialized.
 * 
 * Revision 2.66  92/03/03  15:15:28  reeve
 * All calls to Ap_support() now also call Ap_support_check().
 * 
 * Revision 2.65  92/03/02  11:30:41  raison
 * check for UNASSIGNED parm value (all keyword)
 * 
 * Revision 2.63  92/02/18  17:51:18  sasson
 * Added support for 3 LAT parameters(authorized_group, vcli_group, lat_q_max).
 * 
 * Revision 2.62  92/02/13  11:07:07  raison
 * added support for requesting self boot option
 * 
 * Revision 2.61  92/02/03  14:03:31  raison
 * fixed printer_set bug, sorry
 * 
 * Revision 2.60  92/02/03  09:46:19  carlson
 * SPR 519 - fixed annex name length checking - host name, not file name.
 * 
 * Revision 2.59  92/01/31  22:50:15  raison
 * change printer to have printer_set and added printer_speed
 * 
 * Revision 2.58  92/01/31  12:23:03  reeve
 * Changed references of Sp_support to also check Sp_support_check.
 * 
 * Revision 2.57  91/12/13  15:23:02  raison
 * changed attn_char version type.
 * 
 * Revision 2.56  91/11/04  09:16:59  carlson
 * Need to check for DEV_CAT along with DEV_ATTN ...
 * 
 * Revision 2.55  91/10/28  09:50:54  carlson
 * For SPR 361 -- handle string or char for attn_char, and adapt to box needs.
 * 
 * Revision 2.54  91/10/18  11:50:10  carlson
 * Updated for SysV compilers -- don't declare pclose.
 * 
 * Revision 2.53  91/10/14  16:55:30  russ
 * Added pager.
 * 
 * Revision 2.52  91/09/23  13:01:57  raison
 * increased buffer to correct (maximum) size
 * 
 * Revision 2.51  91/08/01  09:01:22  dajer
 * Added check to port_range() for a low > high range.
 * 
 * Revision 2.50  91/07/12  15:39:30  sasson
 * Added code to port_show_list() and port_pair_list() to reject
 * obsolete port parameters.
 * 
 * Revision 2.49  91/06/13  12:13:42  raison
 * fixed bug for string parms when annex-ii
 * 
 * Revision 2.48  91/05/19  11:36:26  emond
 * Merge changes in 2.45.1.1 & 2.45.1.2, TLI interface and remove SysV
 * compiler warnings.
 * 
 * Revision 2.47  91/04/10  16:37:19  raison
 * used correct macro instead of hard value
 * 
 * Revision 2.46  91/04/02  09:18:12  russ
 * Added Micro annex support.
 * 
 * Revision 2.45.1.2  91/04/10  16:24:48  raison
 * fixed annex copy for annex group_value parm
 * 
 * Revision 2.45.1.1  91/04/08  23:33:25  emond
 * Fix some compile errors on SysV and accommodate generic API I/F
 * 
 * Revision 2.45  91/01/23  21:52:00  raison
 * added lat_value na parameter and GROUP_CODE parameter type.
 * 
 * Revision 2.44  91/01/09  09:43:38  raison
 * fixed "image_name" not supportted problem.
 * 
 * Revision 2.43  90/12/14  14:51:08  raison
 * added LAT param and category code.
 * 
 * Revision 2.42  90/12/04  13:08:01  sasson
 * The image_name was increased to 100 characters on annex3.
 * 
 * Revision 2.41  90/08/16  11:25:36  grant
 * 64 port support
 * 
 * Revision 2.40  90/04/12  15:47:44  loverso
 * We don't need <sys/time.h>, only <time.h> as we're only looking for
 * struct tm.  This helps out sysV machines.
 * 
 * Revision 2.39  90/03/21  15:41:52  russ
 * correctly figure out boot time overlapping into the following day.
 * 
 * Revision 2.38  90/02/07  16:12:07  russ
 * Added fix for na write command timing out. seems that the ANNEX_ID structure
 * passed to annex_name must be empty, so we just bzero it on the way in...
 * 
 * Revision 2.37  90/01/24  14:42:18  russ
 * Added compile time switches to turn off X25 trunk functionality.
 * 
 * Revision 2.36  90/01/23  16:11:24  russ
 * Parse all internal parameter strings for separators.
 * 
 * Revision 2.35  89/12/19  16:02:28  russ
 * Added some reverse compatability code for annex parameters.
 * 
 * Revision 2.34  89/11/20  16:33:19  grant
 * Removed some debug and added annex-2-e displays.
 * 
 * Revision 2.33  89/11/01  15:44:03  grant
 * Added call to get_port_eib.
 * 
 * Revision 2.32  89/08/21  15:07:47  grant
 * Looks for new table size and reset nameserver parameter.
 * 
 * Revision 2.31  89/06/15  16:33:42  townsend
 * Added support for trunks
 * 
 * Revision 2.30  89/05/18  10:12:54  grant
 * Added warning_message() to parse a shutdown warning message.
 * 
 * Revision 2.29  89/05/11  16:55:54  loverso
 * Fix typo (missing "/" in close-comment)
 * 
 * Revision 2.28  89/05/11  14:12:57  maron
 * Modified this module to support the new security parameters, vcli_password,
 * vcli_sec_ena, and port_password.
 * 
 * Revision 2.27  89/04/11  01:00:42  loverso
 * fix extern declarations, inet_addr is u_long, remove extern htonl et al
 * 
 * Revision 2.26  89/04/05  12:42:09  loverso
 * Changed copyright notice
 * 
 * Revision 2.25  88/12/28  11:26:23  parker
 * Added a date/time stamp to the broadcast header.
 * 
 * Revision 2.24  88/06/02  14:19:32  harris
 * Punt when an annex list is empty.  Let user know about invalid hostnames.
 * 
 * Revision 2.23  88/05/25  09:49:27  harris
 * Annex should be dropped from list if hostname is not found as well.
 * 
 * Revision 2.22  88/05/24  18:38:08  parker
 * Changes for new install-annex script
 * 
 * Revision 2.21  88/05/10  17:25:25  harris
 * Allow Annexes to be dropped from the Annex list rather than punt.
 * 
 * Revision 2.20  88/05/04  23:03:04  harris
 * Might as well pull some lint out of this guy, or he'll feel left out!
 * 
 * Revision 2.19  88/04/15  12:44:12  mattes
 * SL/IP integration
 * 
 * Revision 2.18  88/04/12  19:13:57  parker
 * Fixes for Xenix
 * 
 * Revision 2.17  88/03/15  14:36:50  mattes
 * Understand "virtual", "serial", and different "all"
 * 
 * Revision 2.16  87/12/29  11:45:41  mattes
 * Added 4.0 SL/IP parameters
 * 
 * Revision 2.15  87/09/28  11:53:12  mattes
 * Generalized box name
 * 
 * Revision 2.14  87/08/20  10:43:39  mattes
 * Added 32 port support
 * 
 * Revision 2.13  87/08/12  15:35:21  mattes
 * Pass Annex ID to encode() so it can figure out how to truncate strings
 * 
 * Revision 2.12  87/06/10  18:11:45  parker
 * Added support for Xenix/Excelan PC.
 * 
 * Revision 2.11  86/12/08  12:17:48  parker
 * R2.1 NA parameter changes
 * 
 * Revision 2.10  86/11/14  17:05:30  harris
 * Include files for XENIX and UMAX V.
 * 
 * Revision 2.9  86/07/03  15:07:16  harris
 * Simplified code by using tables which describe eerom parameters.
 * 
 * Revision 2.8  86/06/16  15:31:59  harris
 * Port parameter names not associated with correct parameter.  Fixed by
 * adding "allow_broadcast", in 2 places after "port_multiplex".
 * 
 * Revision 2.7  86/06/16  09:37:39  parker
 * added do_leap and allow_broadcast paramters
 * 
 * Revision 2.6  86/05/07  11:00:32  goodmon
 * Added broadcast command.
 * 
 * Revision 2.5  86/04/18  16:46:16  goodmon
 * Simplified grammar by removing meaningless optional symbols.
 * 
 * Revision 2.4  86/03/07  15:01:35  goodmon
 * Needed to differentiate between parameters for show and set commands since
 * catagory names are only valid for show command.
 * 
 * Revision 2.3  86/03/07  13:30:34  goodmon
 * Modified to allow hyphens in filenames and string parameters.
 * 
 * Revision 2.2  86/02/27  19:04:11  goodmon
 * Standardized string lengths.
 * 
 * Revision 2.1  86/02/26  11:50:50  goodmon
 * Changed configuration files to script files.
 * 
 * Revision 1.16  86/02/19  15:17:04  goodmon
 * Changed the nesting of loops in parameter setting subroutines to better
 * handle parameter lists with invalid values.
 * 
 * Revision 1.15  86/02/18  14:23:13  goodmon
 * Removed case-insensitive changes.
 * 
 * Revision 1.14  86/02/05  18:43:46  goodmon
 * Made na input case-insensitive.
 * 
 * Revision 1.13  86/01/16  15:53:32  goodmon
 * Shortened some parameter names.
 * 
 * Revision 1.12  86/01/14  11:54:55  goodmon
 * Modified to extract erpc port number from /etc/services.
 * 
 * Revision 1.11  86/01/10  12:14:27  goodmon
 * New improved error messages.
 * 
 * Revision 1.10  86/01/10  11:26:43  goodmon
 * Fixed a bug introduced by the dash-in-annex-names fix, which caused
 * annex names to be printed out incorrectly.
 * 
 * Revision 1.9  86/01/07  15:48:37  goodmon
 * Removed check of inet addresses against /etc/hosts for inet addresses
 * given in numeric format ("dot notation").
 * 
 * Revision 1.8  86/01/02  10:35:17  goodmon
 * Fixed a bug that caused annex names with dashes to be truncated when
 * printed out by the show command.
 * 
 * Revision 1.7  85/12/18  11:16:36  goodmon
 * Modifications to autobaud support.
 * 
 * Revision 1.6  85/12/17  17:13:01  goodmon
 * Changes suggested by the market analyst types.
 * 
 * Revision 1.5  85/12/12  11:45:23  goodmon
 * Modified to allow hyphens in annex names and to complain if no
 * parameter list is given to the set command.
 * 
 * Revision 1.4  85/12/06  16:14:46  goodmon
 * Added port_mode parameter value 'adaptive' and character parameter
 * value minimum uniqueness.
 * 
 * Revision 1.3  85/12/05  18:22:49  goodmon
 * Added ^c handling and fixed port_multiplexing bug.
 * 
 * Revision 1.2  85/11/15  17:58:32  parker
 * added port_multiplex parameter.
 * 
 * Revision 1.1  85/11/01  17:41:09  palmer
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *****************************************************************************
 */

#define RCSDATE $Date: 1996/10/04 12:10:56 $
#define RCSREV	$Revision: 1.3 $
#define RCSID   "$Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/na/RCS/parse.c,v 1.3 1996/10/04 12:10:56 cwilson Exp $"

#ifndef lint
static char rcsid[] = RCSID;
#endif

/*
 *	Include Files
 */
#include "../inc/config.h"

#include "port/port.h"
#include <sys/types.h>
#include "../libannex/api_if.h"
#include <time.h>		/* we only want "struct tm" */
#include <netinet/in.h>
#include <netdb.h>
#include <strings.h>
#include <sys/uio.h>

#include <setjmp.h>
#include "../inc/erpc/netadmp.h"
#include <stdio.h>
#include "na.h"
#define CMD_H_PARAMS_ONLY
#include "../inc/na/cmd.h"
#include "../inc/na/displaytext.h"
#include "../inc/na/interface.h"
#include "../netadm/netadm_err.h"

/*
 *	External Definitions
 */

extern char *annex_params[];
extern char *port_params[];
extern char *interface_params[];
extern char *sync_params[];
extern char *printer_params[];

extern parameter_table annexp_table[];
extern parameter_table portp_table[];
extern parameter_table interfacep_table[];
extern parameter_table syncp_table[];
extern parameter_table printp_table[];


extern char *display_sw_id();
extern void convert_bit_to_num();
extern int get_internal_vers();
extern void encode(), decode();

extern char		*split_string();

extern time_t		time();
extern struct tm	*localtime();
extern char		*ctime(), *getlogin(), *malloc();
extern UINT32		inet_addr();
extern int		Sp_support_check();

/*
 *	Defines and Macros
 */

#define isdigit(x) (x >= '0' && x <= '9')

#define UNASSIGNED -1

/* interface type define */
#define ETH_TYPE  	0
#define ASY_TYPE 	1
#define SYNC_TYPE 	2

int  	if_offset;
int	if_type;
int	if_ptr = 0;

#ifdef SPLIT_LINES

int	wrap = 0;

#define MAX_SPLIT_STRING 16

#define INITWRAP	wrap = 0

#define LONGWRAP(x) { if (strlen(x) > MAX_SPLIT_STRING) { \
		      if (wrap) printf("\n"); else wrap = 1;} }

#define WRAP { if (wrap) { printf("\n"); wrap = 0;} else wrap = 1; }

#define WRAP_END { if (wrap) printf("\n"); wrap = 0; }

#define FMT	"%20s: %-17s"

#else	/* no SPLIT_LINES */

#define INITWRAP
#define LONGWRAP(x)
#define WRAP
#define WRAP_END
#define FMT	"%28s: %s\n"

#endif

/*
 *	Local structures
 */

struct options {	/* structure for passing an array of information */
    u_short len;		/* string length */
    u_char lat;			/* lat info passed */
    u_char self_boot;		/* self_boot info passed */
    u_char vhelp;		/* can vhelp disable */
    u_char dec;			/* is DEC version */
#define MISC_LEN (LONGPARAM_LENGTH - 4)
    char misc[MISC_LEN];	/* leaving room for the future */
};

/*
 *	Forward Routine Definitions
 */

void	annex_show_header(),
	port_show_header(),
	sync_show_header(),
	interface_show_header();

/*
 *	Global Data Declarations
 */

UINT32 masks[] =  /* masks[i] == 2**(i-1) for 1 <= i <= 32 */
{
    0x00000000,
    0x00000001, 0x00000002, 0x00000004, 0x00000008,
    0x00000010, 0x00000020, 0x00000040, 0x00000080,
    0x00000100, 0x00000200, 0x00000400, 0x00000800,
    0x00001000, 0x00002000, 0x00004000, 0x00008000,
    0x00010000, 0x00020000, 0x00040000, 0x00080000,
    0x00100000, 0x00200000, 0x00400000, 0x00800000,
    0x01000000, 0x02000000, 0x04000000, 0x08000000,
    0x10000000, 0x20000000, 0x40000000, 0x80000000
};

int first_broadcast = TRUE;

char header[100] = "\007\n*** NA Broadcast from ";


/*
 * Functions to decode and define port/sync/printer/interface sets
 */

port_set(PPport_set, PPport_tail, virtual_ok)
PORT_SET **PPport_set,
	 **PPport_tail;
int      virtual_ok;
{
    port_group(PPport_set, PPport_tail, virtual_ok);
    if (symbol_length == 1 && symbol[0] == ';') {
	(void)lex();
	if (eos)
	    punt("missing port identifier", (char *)NULL);
	port_set(PPport_set, PPport_tail, virtual_ok);
    }
}

sync_set(PPsync_set, PPsync_tail)
SYNC_SET **PPsync_set,
	 **PPsync_tail;
{
    sync_group(PPsync_set, PPsync_tail);
    if (symbol_length == 1 && symbol[0] == ';') {
	(void)lex();
	if (eos)
	    punt("missing sync identifier", (char *)NULL);
	sync_set(PPsync_set, PPsync_tail);
    }
}

printer_set(PPprinter_set, PPprinter_tail)
PRINTER_SET **PPprinter_set,
	    **PPprinter_tail;
{
    printer_group(PPprinter_set, PPprinter_tail);
    if (symbol_length == 1 && symbol[0] == ';') {
	(void)lex();
	if (eos)
	    punt("missing printer identifier", (char *)NULL);
	printer_set(PPprinter_set, PPprinter_tail);
    }
}

interface_set(PPinterface_set, PPinterface_tail)
INTERFACE_SET **PPinterface_set,
	      **PPinterface_tail;
{
    interface_group(PPinterface_set, PPinterface_tail);
    if (symbol_length == 1 && symbol[0] == ';') {
	(void)lex();
	if (eos)
	    punt("missing interface identifier", (char *)NULL);
	interface_set(PPinterface_set, PPinterface_tail);
    }
}

/*
 * Functions to decode port/sync/printer/interface groups (called from above)
 */

port_group(PPport_set, PPport_tail, virtual_ok)
PORT_SET **PPport_set,
	 **PPport_tail;
int      virtual_ok;
{
    PORT_SET *Port_s;
    int	p;
    
    /* Add a new PORT_SET structure to the end of the port set
       pointed to by the given head and tail pointers. */
    
    Port_s = (PORT_SET *)malloc(sizeof(PORT_SET));
    Port_s->next = NULL;
    
    if (!*PPport_set)
	*PPport_set = Port_s;
    else
	(*PPport_tail)->next = Port_s;
    
    *PPport_tail = Port_s;
    
    Port_s->ports.pg_bits = 0;
    
    bzero(Port_s->ports.serial_ports,
	  sizeof(Port_s->ports.serial_ports));
    
    port_list(&Port_s->ports, virtual_ok);
    
    if (symbol_length == 1 && symbol[0] == '@') {
	(void)lex();
	if (eos)
	    punt(NO_BOX, (char *)NULL);
	(void)annex_name(&Port_s->annex_id,
			 Port_s->name, 0);
    }
    else
	Port_s->annex_id.addr.sin_addr.s_addr = 0;
}

sync_group(PPsync_set, PPsync_tail)
SYNC_SET **PPsync_set,
	 **PPsync_tail;
{
    SYNC_SET *Sync_s;
    int	p;
    
    /* Add a new SYNC_SET structure to the end of the sync set
       pointed to by the given head and tail pointers. */
    
    Sync_s = (SYNC_SET *)malloc(sizeof(SYNC_SET));
    Sync_s->next = NULL;
    
    if (!*PPsync_set)
	*PPsync_set = Sync_s;
    else
	(*PPsync_tail)->next = Sync_s;
    
    *PPsync_tail = Sync_s;
    
    Sync_s->syncs.pg_bits = 0;
    
    bzero(Sync_s->syncs.sync_ports, sizeof(Sync_s->syncs.sync_ports));
    
    sync_list(&Sync_s->syncs);
    
    if (symbol_length == 1 && symbol[0] == '@') {
	(void)lex();
	if (eos)
	    punt(NO_BOX, (char *)NULL);
	(void)annex_name(&Sync_s->annex_id,
			 Sync_s->name, 0);
    }
    else
	Sync_s->annex_id.addr.sin_addr.s_addr = 0;
}

printer_group(PPprinter_set, PPprinter_tail)
PRINTER_SET **PPprinter_set,
	    **PPprinter_tail;
{
    PRINTER_SET *Print_s;
    int	p;
    
    /* Add a new PRINTER_SET structure to the end of the printer set
       pointed to by the given head and tail pointers. */
    
    Print_s = (PRINTER_SET *)malloc(sizeof(PRINTER_SET));
    Print_s->next = NULL;
    
    if (!*PPprinter_set)
	*PPprinter_set = Print_s;
    else
	(*PPprinter_tail)->next = Print_s;
    
    *PPprinter_tail = Print_s;
    
    Print_s->printers.pg_bits = 0;
    
    bzero(Print_s->printers.ports,
	  sizeof(Print_s->printers.ports));
    
    printer_list(&Print_s->printers);
    
    if (symbol_length == 1 && symbol[0] == '@') {
	(void)lex();
	if (eos)
	    punt(NO_BOX, (char *)NULL);
	(void)annex_name(&Print_s->annex_id,
			 Print_s->name, 0);
    }
    else
	Print_s->annex_id.addr.sin_addr.s_addr = 0;
}

interface_group(PPinterface_set, PPinterface_tail)
INTERFACE_SET **PPinterface_set,
	      **PPinterface_tail;
{
    INTERFACE_SET *Interf_s;
    int	p;
    
    /* Add a new INTERFACE_SET structure to the end of the interface set
       pointed to by the given head and tail pointers. */
    
    Interf_s = (INTERFACE_SET *)malloc(sizeof(INTERFACE_SET));
    Interf_s->next = NULL;
    
    if (!*PPinterface_set)
	*PPinterface_set = Interf_s;
    else
	(*PPinterface_tail)->next = Interf_s;
    
    *PPinterface_tail = Interf_s;
    
    Interf_s->interfaces.pg_bits = 0;
    
    bzero(Interf_s->interfaces.interface_ports,
	  sizeof(Interf_s->interfaces.interface_ports));
    
    interface_list(&Interf_s->interfaces);
    
    if (symbol_length == 1 && symbol[0] == '@') {
	(void)lex();
	if (eos)
	    punt(NO_BOX, (char *)NULL);
	(void)annex_name(&Interf_s->annex_id,
			 Interf_s->name, 0);
    }
    else
	Interf_s->annex_id.addr.sin_addr.s_addr = 0;
}

/*
 * Functions to decode port/sync/printer/interface lists (called from above)
 */

port_list(Pports, virtual_ok)
PORT_GROUP *Pports;
int        virtual_ok;
{
    port_range(Pports, virtual_ok);
    if (symbol_length == 1 && symbol[0] == ',') {
	(void)lex();
	if (eos)
	    punt("missing port identifier", (char *)NULL);
	port_list(Pports, virtual_ok);
    }
}

sync_list(Pports)
SYNC_GROUP *Pports;
{
    sync_range(Pports);
    if (symbol_length == 1 && symbol[0] == ',') {
	(void)lex();
	if (eos)
	    punt("missing sync identifier", (char *)NULL);
	sync_list(Pports);
    }
}

printer_list(Pprinters)
PRINTER_GROUP *Pprinters;
{
    printer_range(Pprinters);
    if (symbol_length == 1 && symbol[0] == ',') {
	(void)lex();
	if (eos)
	    punt("missing printer identifier", (char *)NULL);
	printer_list(Pprinters);
    }
}

interface_list(Pinterfaces)
INTERFACE_GROUP *Pinterfaces;
{
    interface_range(Pinterfaces); 
    if (symbol_length == 1 && symbol[0] == ',') {
	(void)lex();
	if (eos)
	    punt("missing interface identifier", (char *)NULL);
	interface_list(Pinterfaces);
    }
}

/*
 * Range chacking for port/sync/printer/interface number (called from below)
 */

unsigned short
port_number()
{
    unsigned short value = 0;
    int            loop;
    
    if (symbol_length > 2)
	punt("invalid port identifier: ", symbol);
    
    for (loop = 0; loop < symbol_length; loop++) {
	if (!index("0123456789", symbol[loop]))
	    punt("invalid port identifier: ", symbol);
	value = value * 10 + symbol[loop] - '0';
    }
    
    if (value < 1 || value > ALL_PORTS)
	punt("invalid port identifier: ", symbol);
    (void)lex();
    return value;
}

unsigned short
sync_number()
{
    unsigned short value = 0;
    int            loop;
    
    if (symbol_length > 2)
	punt("invalid sync port identifier: ", symbol);
    
    for (loop = 0; loop < symbol_length; loop++) {
	if (!index("0123456789", symbol[loop]))
	    punt("invalid sync port identifier: ", symbol);
	value = value * 10 + symbol[loop] - '0';
    }
    
    if (value < 1 || value > ALL_SYNC_PORTS)
	punt("invalid sync port identifier: ", symbol);
    (void)lex();
    return value;
}

unsigned short
printer_number()
{
    unsigned short value = 0;
    int            loop;
    
    if (symbol_length > 1)
	punt("invalid printer identifier: ", symbol);
    
    for (loop = 0; loop < symbol_length; loop++) {
	if (!index("0123456789", symbol[loop]))
	    punt("invalid printer identifier: ", symbol);
	value = value * 10 + symbol[loop] - '0';
    }
    
    if (value < 1 || value > ALL_PRINTERS)
	punt("invalid printer identifier: ", symbol);
    (void)lex();
    return value;
}

unsigned short
interface_number(parse_token)
int	parse_token;
{
    unsigned short value = 0;
    int            loop;
    
    /* 
     * parse the interface type token, indicate the type
     * and logical offset for later processing
     */ 
    if (parse_token) {
	if (!strncmp(symbol, "en",2)) {
	    if_ptr = 2;			/* offset to the digit */
	    if_type = ETH_TYPE;		/* interface type */
	    if_offset = M_ETHERNET;	/* interface index offset */
	}
	else if (!strncmp(symbol, "asy",3)) {
	    if_ptr = 3;
	    if_type = ASY_TYPE;
	    if_offset = M_ETHERNET;
	}
	else if (!strncmp(symbol, "syn",3)) {
	    if_ptr = 3;
	    if_type = SYNC_TYPE;
	    if_offset = M_ETHERNET + ALL_PORTS;
	}
	else 
	    punt("invalid interface identifier: ", symbol);
    }
    /* 
     * now parse the numeric digit
     */ 
    for (loop = if_ptr; loop < symbol_length; loop++) {
	if (!index("0123456789", symbol[loop]))
	    punt("invalid interface identifier: ", symbol);
	value = value * 10 + symbol[loop] - '0';
    }
    
    /* 
     * check the range for each interface type 
     */ 
    switch (if_type) {
	case ETH_TYPE:
	    if (value > 0)
		punt("invalid interface identifier: ", symbol);
	    break;
	case ASY_TYPE:
	    if (value > M_SLUS || value < 1)
		punt("invalid interface identifier: ", symbol);
	    break;
	case SYNC_TYPE:
	    if (value > M_SYNC || value < 1)
		punt("invalid interface identifier: ", symbol);
	    break;
    }
    
    /* adjust the offset into r2rom index */
    value = value + if_offset;
    if (value < 1 || value > (unsigned short)ALL_INTERFACES)
	punt("invalid interface identifier: ", symbol);
    
    /* reset the pointer */ 
    if_ptr = 0;
    (void)lex();
    return value;
}

/*
 * Calculates ranges ala 1-3,12-22
 */

port_range(Pports, virtual_ok)
PORT_GROUP *Pports;
int	   virtual_ok;
{
    unsigned short low,
		   high;
    int            loop,
		   p;
    
    if (symbol_length >= 1 && symbol_length <= 7 &&
	strncmp(symbol, "virtual", symbol_length) == 0) {
	if (virtual_ok) {
	    (void)lex();
	    Pports->pg_bits |= PG_VIRTUAL;
	}
	else
	    punt("virtual invalid in this context", (char *)NULL);
    }
    else if (symbol_length >= 1 && symbol_length <= 6 &&
	     strncmp(symbol, "serial", symbol_length) == 0) {
	(void)lex();
	Pports->pg_bits |= PG_SERIAL;
	for(p=1; p <= ALL_PORTS; p++)
	    SETPORTBIT(Pports->serial_ports,p);
    }
    else if (symbol_length >= 1 && symbol_length <= 3 &&
	     strncmp(symbol, "all", symbol_length) == 0) {
	(void)lex();
	Pports->pg_bits |= PG_ALL;
	for(p=1; p <= ALL_PORTS; p++)
	    SETPORTBIT(Pports->serial_ports,p);
    }
    else {
	low = port_number();
	
	if (symbol_length == 1 && symbol[0] == '-') {
	    (void)lex();
	    
	    if (eos)
		punt("missing port identifier", (char *)NULL);
	    high = port_number();
	    if (low > high)
		punt("invalid upper boundary on port range: ", symbol);
	    
	    for (loop = (int)low; loop <= (int)high; loop++)
		SETPORTBIT(Pports->serial_ports,loop);
	}
	else
	    SETPORTBIT(Pports->serial_ports,(int)low);
    }
}

sync_range(Pports)
SYNC_GROUP *Pports;
{
    unsigned short low,
		   high;
    int            loop,
		   p;
    
    if (symbol_length >= 1 && symbol_length <= 3 &&
	strncmp(symbol, "all", symbol_length) == 0) {
	(void)lex();
	Pports->pg_bits |= PG_ALL;
	for(p=1; p <= ALL_PORTS; p++)
	    SETPORTBIT(Pports->sync_ports,p);
    }
    else {
	low = sync_number();
	if (symbol_length == 1 && symbol[0] == '-') {
	    (void)lex();
	    
	    if (eos)
		punt("missing sync port identifier", (char *)NULL);
	    
	    high = sync_number();
	    if (low > high)
		punt("invalid upper boundary on sync port range: ", symbol);
	    
	    for (loop = (int)low; loop <= (int)high; loop++)
		SETPORTBIT(Pports->sync_ports,loop);
	}
	else
	    SETPORTBIT(Pports->sync_ports,(int)low);
    }
}

printer_range(Pprinters)
PRINTER_GROUP *Pprinters;
{
    unsigned short low,
		   high;
    int            loop,
		   p;
    
    if (symbol_length >= 1 && symbol_length <= 3 &&
	strncmp(symbol, "all", symbol_length) == 0) {
	(void)lex();
	Pprinters->pg_bits |= PG_ALL;
	for(p=1; p <= ALL_PRINTERS; p++)
	    SETPRINTERBIT(Pprinters->ports,p);
    }
    else {
	low = printer_number();
	if (symbol_length == 1 && symbol[0] == '-') {
	    (void)lex();
	    
	    if (eos)
		punt("missing printer identifier", (char *)NULL);
	    
	    high = printer_number();
	    if (low > high)
		punt("invalid upper boundary on printer range: ", symbol);
	    
	    for (loop = (int)low; loop <= (int)high; loop++)
		SETPRINTERBIT(Pprinters->ports,loop);
	}
	else
	    SETPRINTERBIT(Pprinters->ports,(int)low);
    }
}

interface_range(Pinterfaces)
INTERFACE_GROUP *Pinterfaces;
{
    unsigned short low,
		   high;
    int            loop,
		   p;
    
    if (symbol_length >= 1 && symbol_length <= 3 &&
	strncmp(symbol, "all", symbol_length) == 0) {
	(void)lex();
	Pinterfaces->pg_bits |= PG_ALL;
	
	/* example for micro-annex M_SLUS = 16
	 *		calling SETINTERFACEBIT(xxx, 1)  sets en0
	 *		calling SETINTERFACEBIT(xxx, 2)  sets asy1
	 *		calling SETINTERFACEBIT(xxx, 18) sets syn1
	 */
	for(p=1; p <= ALL_INTERFACES; p++)
	    SETINTERFACEBIT(Pinterfaces->interface_ports,p); 
    }
    else {
	low = interface_number(1);
	if (symbol_length == 1 && symbol[0] == '-') {
	    (void)lex();
	    
	    if (eos)
		punt("missing port identifier", (char *)NULL);
	    high = interface_number(0);
	    if (low > high)
		punt("invalid upper boundary on interface range: ", symbol);
	    
	    for (loop = (int)low; loop <= (int)high; loop++)
		SETINTERFACEBIT(Pinterfaces->interface_ports,loop); 
	}
	else
	    SETINTERFACEBIT(Pinterfaces->interface_ports,(int)low);  
    }
    if_offset = 0;
    if_type = 0;
}

/*
 * to parse single port/etc numbers... user by the copy command.
 */

single_port(Pport_number, Pannex_id)
unsigned short     *Pport_number;
ANNEX_ID	   *Pannex_id;
{
    *Pport_number = port_number();
    
    if (symbol_length == 1 && symbol[0] == '@')
	(void)lex();
    else
	punt(NO_BOX, (char *)NULL);
    if (!eos)
	(void)annex_name(Pannex_id, (char *)NULL, 0);
    else
	punt(NO_BOX, (char *)NULL);
}

single_sync(Psync_number, Pannex_id)
unsigned short     *Psync_number;
ANNEX_ID	   *Pannex_id;
{
    *Psync_number = sync_number();
    
    if (symbol_length == 1 && symbol[0] == '@')
	(void)lex();
    else
	punt(NO_BOX, (char *)NULL);
    if (!eos)
	(void)annex_name(Pannex_id, (char *)NULL, 0);
    else
	punt(NO_BOX, (char *)NULL);
}

single_printer(Pprinter_number, Pannex_id)
unsigned short     *Pprinter_number;
ANNEX_ID	   *Pannex_id;
{
    *Pprinter_number = printer_number();
    
    if (symbol_length == 1 && symbol[0] == '@')
	(void)lex();
    else
	punt(NO_BOX, (char *)NULL);
    if (!eos)
	(void)annex_name(Pannex_id, (char *)NULL, 0);
    else
	punt(NO_BOX, (char *)NULL);
}

single_interface(Pinterface_number, Pannex_id)
unsigned short     *Pinterface_number;
ANNEX_ID	   *Pannex_id;
{
    *Pinterface_number = interface_number(1);
    
    if (symbol_length == 1 && symbol[0] == '@')
	(void)lex();
    else
	punt(NO_BOX, (char *)NULL);
    if (!eos)
	(void)annex_name(Pannex_id, (char *)NULL, 0);
    else
	punt(NO_BOX, (char *)NULL);
}

single_virtual(Pannex_id)
ANNEX_ID	   *Pannex_id;
{
    /* This is only called when the symbol "virtual" has already been
       seen, so don't bother looking at it again. */
    (void)lex();
    
    if (symbol_length == 1 && symbol[0] == '@')
	(void)lex();
    else
	punt(NO_BOX, (char *)NULL);
    if (!eos)
	(void)annex_name(Pannex_id, (char *)NULL, 0);
    else
	punt(NO_BOX, (char *)NULL);
}

/*
 * parse a list of annexnames.
 */

annex_list(PPannex_list, PPannex_tail)
ANNEX_LIST **PPannex_list,
    **PPannex_tail;
{
    int        error;
    ANNEX_LIST *Annex_l;
    
    for (;;) {

	if (symbol_length == 1 && symbol[0] == ',') {
	    (void)lex();
	    printf("Null box name ignored.\n");
	    continue;
	}

	/* Add a new ANNEX_LIST structure to the end of the annex list
	   pointed to by the given head and tail pointers. */
	
	if ((Annex_l = (ANNEX_LIST *)malloc(sizeof(ANNEX_LIST))) == NULL)
	    punt("No memory - malloc() failed", (char *)NULL);
	
	error = annex_name(&Annex_l->annex_id, Annex_l->name,
			   -1);
	if (!error) {
	    Annex_l->next = NULL;
	    
	    if (!*PPannex_list)
		*PPannex_list = Annex_l;
	    else
		(*PPannex_tail)->next = Annex_l;
	    
	    *PPannex_tail = Annex_l;
	} else
	    free((char *)Annex_l);
	
	if (symbol_length == 1 && symbol[0] == ',') {
	    (void)lex();
	    
	    if (eos) {
		printf("Null trailing box name ignored.\n");
		break;
	    }
	} else
	    break;
    }
    if (!*PPannex_list)
	punt(BOX, " list was empty - ignored");
}

/*
 * or how about just one name at a time?
 */

int
annex_name(Pannex_id, copy_dest, oblivious)
ANNEX_ID	*Pannex_id;
char            *copy_dest;
int		oblivious;			/* print errors */
{
    int			error;
    u_short		eib = 0;
    struct options	options;
    char		*str;
    
    bzero(Pannex_id, sizeof(ANNEX_ID));
    
    while (*Psymbol && !index(TERMINATORS, *Psymbol))
	symbol[symbol_length++] = *Psymbol++;
    
    symbol[symbol_length] = '\0';
    if (symbol_length > BOX_LMAX) {
	printf("%s\n",LONG_BOX);
	error = -1;
	goto oops;
    }
    if (symbol_length <= 0) {
	printf("Missing hostname or internet address\n");
	error = -2;
	goto oops;
    }
    if (error = str_to_inet(symbol, &Pannex_id->addr.sin_addr.s_addr, FALSE,
			    oblivious)) {
	printf("%s: invalid hostname or internet address\n", symbol);
	error = -1;
	goto oops;
    }
    
    Pannex_id->addr.sin_family = AF_INET;
    Pannex_id->addr.sin_port = erpc_port;
    
    /* Determine the software rev and hardware type */
    error = get_annex_rev(&Pannex_id->addr, LONG_CARDINAL_P,
			  (caddr_t)&(Pannex_id->sw_id));
    switch (error) {
	case 0:
	    if (get_internal_vers(Pannex_id->sw_id,
			      (UINT32 *)&Pannex_id->version,
			      (UINT32 *)&Pannex_id->hw_id,
			      (UINT32 *)&Pannex_id->flag, TRUE)) {
		error = -1;
		goto err_oops;
	    }
	    break;
	case NAE_PROC:
	    Pannex_id->version = VERS_1;
	    Pannex_id->hw_id = ANX_I;
	    break;
	default:
	    goto err_oops;
	    break;
    }
    
    /* Determine if it has ennhanced interface hardware */
    error = get_port_eib(&Pannex_id->addr, CARDINAL_P, (caddr_t)&eib);
    switch (error) {
	case 0:
	    if ((eib & ANX_IIE) && Pannex_id->hw_id == ANX_II)
		Pannex_id->hw_id = ANX_II_EIB;
	    Pannex_id->flag |= eib;
	    break;
	case NAE_PROC:
	    break;
	default:
	    goto err_oops;
	    break;
    }
    
    /* Determine the number of ports */
    error = get_port_count(&Pannex_id->addr, CARDINAL_P,
			   (caddr_t)&(Pannex_id->port_count));
    switch (error) {
	case 0:
	    break;
	case NAE_PROC:
	    Pannex_id->port_count = 16;
	    break;
	default:
	    goto err_oops;
	    break;
    }
    
    /* Determine the number of sync ports */
    error = get_sync_count(&Pannex_id->addr, CARDINAL_P, 
			   (caddr_t)&(Pannex_id->sync_count));
    switch (error) {
	case 0:
	    break;
	case NAE_PROC:
	    Pannex_id->sync_count = 0;
	    break;
	default:
	    goto err_oops;
	    break;
    }
    
    /* Determine the number of printers */
    
    error = get_printer_count(&Pannex_id->addr, CARDINAL_P,
			      (caddr_t)&(Pannex_id->printer_count));
    switch (error) {
	case 0:
	    break;
	case NAE_PROC:
	    if (Pannex_id->hw_id == ANX_MICRO)
		Pannex_id->printer_count = 0;
	    else
		Pannex_id->printer_count = 1;
	    break;
	default:
	    goto err_oops;
	    break;
    }
    
    
    if (Pannex_id->version >= VERS_6) {
	error = get_annex_opt(&Pannex_id->addr, STRING_P, (caddr_t)&options);
	switch(error) {
	    case 0:
	        Pannex_id->lat = options.lat;
		Pannex_id->self_boot = options.self_boot;    
		Pannex_id->vhelp = options.vhelp;
		break;
	    case NAE_PROC:
		break;
	    default:
		goto err_oops;
		break;
	}
    }
    
    /* print status string for the annex. */
    if (Pannex_id->sync_count) { /* Show sync ports only if present! */
      printf("%s: %s%s, %d async ports, %d sync ports, %d printer ports\n",
	     symbol, str = display_sw_id(Pannex_id->sw_id, Pannex_id->hw_id),
	     (Pannex_id->lat) ? " w/LAT" : "", Pannex_id->port_count,
	     Pannex_id->sync_count, Pannex_id->printer_count);
    } else {
      printf("%s: %s%s, %d async ports, %d printer ports\n",
	     symbol, str = display_sw_id(Pannex_id->sw_id, Pannex_id->hw_id),
	     (Pannex_id->lat) ? " w/LAT" : "", Pannex_id->port_count,
	     Pannex_id->printer_count);
    }
    
    if (strcmp("ANNEX-802.5",str) == 0)
	Pannex_id->flag |= ANX_802_5;
    
    if (copy_dest)
	(void)strcpy(copy_dest, symbol);
    
    (void)lex();
    return 0;

    /* If error, punt if requested, or return error (-1) */
err_oops:
    token_error(error);
    error = -1;
oops:
    if(oblivious) {
	printf("Warning: %s has been dropped from the list\n", symbol);
	(void)lex();
	return error;
    }
    else
	punt((char *)NULL, (char *)NULL);
    return 0;
}


/*
 * Small lexical parser
 */

lex_string()
{
    while (*Psymbol && !index(WHITE_SPACE, *Psymbol))
	symbol[symbol_length++] = *Psymbol++;
    symbol[symbol_length] = '\0';
}

lex_end()
{
    while (*Psymbol && *Psymbol != ' ')
	symbol[symbol_length++] = *Psymbol++;
    symbol[symbol_length] = '\0';
}

/*
 * Finally some real functionality.
 */

/*
 * Annex level functionality
 */

annex_show_list(Pannex_list)
ANNEX_LIST	*Pannex_list;
{
    SHOW_LIST	*Show_l;
    int		p_num, parm;
    
    free_show_list();
    
    while (!eos) {
	p_num = match(symbol, annex_params, BOX_PARM_NAME);
	if (Ap_category(p_num) == VOID_CAT) {
	    /* obsolete parameter */
	    char error_msg[80];
	    error_msg[0] = '\0';
	    (void)strcat(error_msg, "port parameter name: ");
	    (void)strcat(error_msg, symbol);
	    punt("invalid ", error_msg);
	}
	
	/* Add a new SHOW_LIST structure to the end of the show list
	   pointed to by the given head and tail pointers. */
	
	Show_l = (SHOW_LIST *)malloc(sizeof(SHOW_LIST));
	Show_l->next = NULL;
	Show_l->param_num = p_num;
	
	if (!Pshow_list)
	    Pshow_list = Show_l;
	else
	    Pshow_tail->next = Show_l;
	Pshow_tail = Show_l;
	(void)lex();
    }
    
    /* Print the per-annex parameters (from the show list) for each annex
       on the given annex list. */
    
    while(Pannex_list) {
	WRAP_END;
	printf("\n\t\t%s Name:  %s\n\n", Box, Pannex_list->name);
	Show_l = Pshow_list;
	do {				/* for each "show annex" parameter */
	    
	    if(!Show_l)		/* default */
		parm = UNASSIGNED;	
	    else
		parm = Show_l->param_num;
	    
	    if((parm != UNASSIGNED) &&
	       ((Ap_category(parm) == LAT_CAT ||
		 ((Ap_category(parm) == GRP_CAT) &&
		  (Ap_catid(parm) == LAT_CAT)))
		&& !Pannex_list->annex_id.lat)) {
		printf("\t%s does not support %s\n",Pannex_list->name,
		       annex_params[parm]);
		if (Show_l)
		    Show_l = Show_l->next;
		continue;
	    }
	    
	    if((parm != UNASSIGNED) &&
	       (Ap_category(parm) == DLA_CAT || 
		Ap_category(parm) == LAT_CAT ||
		Ap_category(parm) == ARAP_CAT ||
		Ap_category(parm) == RIP_CAT ||
		Ap_category(parm) == DFE_CAT)) {
		if(Ap_support(&Pannex_list->annex_id,parm) &&
		   Ap_support_check(&Pannex_list->annex_id,parm))
		    annex_show_sub(&Pannex_list->annex_id, parm);
		else
		    printf("\t%s does not support %s\n",
			   Pannex_list->name, annex_params[parm]);
	    } 
	    else {
		
		/*
		 * The default will show only generic category parameters.
		 */
		if (parm == UNASSIGNED) { /* default */
		    for(p_num = 0; Ap_index(p_num) != -1; p_num++) {
			
			if (Ap_support(&Pannex_list->annex_id,p_num) &&
			    Ap_support_check(&Pannex_list->annex_id,
					     p_num) &&
			    ((Ap_displaycat(p_num)==B_GENERIC_CAT))) {
			    annex_show_sub(&Pannex_list->annex_id,p_num);
			}
		    }
		    
		    /* else annex category list */
		}
		else if (Ap_category(parm) == GRP_CAT) {
		    
		    for (p_num = 0; Ap_index(p_num) != -1; p_num++) {
			if (Ap_support(&Pannex_list->annex_id,p_num) &&
			    Ap_support_check(&Pannex_list->annex_id,
					     p_num) &&
			    ((Ap_displaycat(p_num)==Ap_catid(parm))
			     || (Ap_catid(parm) == ALL_CAT)) &&
			    ((Ap_category(p_num) != LAT_CAT
			      || Pannex_list->annex_id.lat))) {
			    annex_show_header(p_num);
			    annex_show_sub(&Pannex_list->annex_id,p_num);
			}
		    }
		}
	    }
	    if(Show_l)
		Show_l = Show_l->next;
	    
	}	while(Show_l);
	
	Pannex_list = Pannex_list->next;
    }
    printf("\n");
}

/*
 * Output the annex category headers.
 */

void 
annex_show_header(p_num) 
int		p_num;
{
    switch (p_num) {
	case BOX_GENERIC_GROUP:	WRAP_END;	printf(box_generic);	break;
	case BOX_VCLI_GROUP:	WRAP_END;	printf(box_vcli);	break;
	case BOX_NAMESERVER_GROUP:WRAP_END;	printf(box_nameserver);	break;
	case BOX_SECURITY_GROUP:WRAP_END;	printf(box_security);	break;
	case BOX_TIME_GROUP:	WRAP_END;	printf(box_time);	break;
	case BOX_SYSLOG_GROUP:	WRAP_END;	printf(box_syslog);	break;
	case BOX_LAT_GROUP:	WRAP_END;	printf(box_lat);	break;
	case BOX_ARAP_GROUP:	WRAP_END;	printf(box_arap);	break;
	case BOX_RIP_GROUP:	WRAP_END;	printf(box_rip);	break;
#if NKERB > 0
	case BOX_KERBEROS_GROUP:WRAP_END;	printf(box_kerberos);	break;
#endif
	case BOX_IPX_GROUP:	WRAP_END;	printf(box_ipx);	break;
	case BOX_VMS_GROUP:	WRAP_END;	printf(box_vms);	break;
	default:							break;
    }
}

/*
 * Show the parameter
 */

annex_show_sub(Pannex_id, p_num)
ANNEX_ID	   *Pannex_id;
int		   p_num;
{
    int		   category,		/*  Param category  */
		   id,			/*  Number w/in cat */
		   type;		/*  Data type	    */
    int		   error;
    long	   align_internal[(MAX_STRING_100 + 4)/sizeof(long) + 1];
    char	   *internal = (char *)align_internal,	/*  Machine format  */
		   external[LINE_LENGTH];		/*  Human format    */
    char	   *start_delim;
    
    /* Print the value of an annex parameter. */
    category = Ap_category(p_num);
    
    if (category == DLA_CAT || category == DFE_CAT || category == LAT_CAT ||
	category == ARAP_CAT || category == RIP_CAT) {

	id = Ap_catid(p_num);
	type = Ap_type(p_num);
	if(error = get_dla_param(&Pannex_id->addr, (u_short)category,
				 (u_short)id, (u_short)type, internal))
	    netadm_error(error);
	else {
	    decode(Ap_convert(p_num), internal, external,Pannex_id);
	    LONGWRAP(external);
	    if (start_delim = split_string(annex_params[p_num], FALSE))
		printf(FMT, start_delim, external);
	    else
		printf(FMT, annex_params[p_num], external);
	    WRAP;
	}
    }
    return;
}

/*
 * Asynchronous port level functionality
 */

port_show_list(Pport_set)
PORT_SET	*Pport_set;
{
    ANNEX_LIST	*Annex_l;
    SHOW_LIST	*Show_l;
    int		p_num;
    
    free_show_list();
    
    while(!eos) {
	p_num = match(symbol, port_params, "port parameter name");
	if (Sp_category(p_num) == VOID_CAT) {
	    /* obsolete parameter */
	    char error_msg[80];
	    error_msg[0] = '\0';
	    (void)strcat(error_msg, "port parameter name: ");
	    (void)strcat(error_msg, symbol);
	    punt("invalid ", error_msg);
	}
	
	/* Add a new SHOW_LIST structure to the end of the show list
	   pointed to by the given head and tail pointers. */
	
	Show_l = (SHOW_LIST *)malloc(sizeof(SHOW_LIST));
	Show_l->next = NULL;
	Show_l->param_num = p_num;
	
	if (!Pshow_list)
	    Pshow_list = Show_l;
	else
	    Pshow_tail->next = Show_l;
	Pshow_tail = Show_l;
	(void)lex();
    }
    
    /* Print the serial port parameters (from the show list) for each
       port in the given port set. */
    
    while (Pport_set) {
	/* If an annex id was specified, use it; otherwise, use the
	   default annex list. */
	if (Pport_set->annex_id.addr.sin_addr.s_addr)
	    do_show_port(&Pport_set->annex_id, Pport_set->name,
			 &Pport_set->ports);
	else
	    if (Pdef_annex_list)
		for (Annex_l = Pdef_annex_list; Annex_l; Annex_l=Annex_l->next)
		    do_show_port(&Annex_l->annex_id, Annex_l->name,
				 &Pport_set->ports);
	    else
		punt(NO_BOXES, (char *)NULL);
	
	Pport_set = Pport_set->next;
    }
    printf("\n");
}

void 
port_show_header(p_num) 
int		p_num;
{
    switch (p_num) {
	case PORT_GENERIC_GROUP:WRAP_END;	printf(port_generic);	break;
	case PORT_FLOWCONTROL_GROUP:WRAP_END;	printf(port_flow);	break;
	case PORT_TIMER_GROUP:	WRAP_END;	printf(port_timers);	break;
	case PORT_LOGINUSR_GROUP:WRAP_END;	printf(port_login);	break;
	case PORT_SECURITY_GROUP:WRAP_END;	printf(port_security);	break;
	case PORT_CHAR_GROUP:	WRAP_END;	printf(port_edit);	break;
	case PORT_NETADDR_GROUP:WRAP_END;	printf(port_serialproto);break;
	case PORT_SLIP_GROUP:	WRAP_END;	printf(port_slip);	break;
	case PORT_PPP_GROUP:	WRAP_END;	printf(port_ppp);	break;
	case PORT_ARAP_GROUP:	WRAP_END;	printf(port_arap);	break;
	case PORT_TN3270_GROUP:	WRAP_END;	printf(port_tn3270);	break;
	case PORT_LAT_GROUP:	WRAP_END;	printf(port_lat);	break;
	default:							break;
    }
}

port_show_sub(Pannex_id, port, p_num)
ANNEX_ID	   *Pannex_id;
unsigned short     port;
int                p_num;
{
    int		   category,		/*  Param category  */
		   id,			/*  Number w/in cat */
		   type,		/*  Data type	    */
		   convert;		/*  Conversion type */
    int		   error;
    long	   align_internal[(MAX_STRING_100 + 4)/sizeof(long) + 1];
    char	   *internal = (char *)align_internal,	/*  Machine format  */
		   external[LINE_LENGTH],		/*  Human format    */
		   *cp;
    char	   latter = FALSE;
    char	   *start_delim;
    
    /* Print the value of a serial port parameter. */
    category = Sp_category(p_num);
    if(category == INTF_CAT || category == DEV_CAT || category == EDIT_CAT ||
       category == DEV2_CAT || category == SLIP_CAT) {

	id = (u_short) Sp_catid(p_num);
	type = (u_short) Sp_type(p_num);
	convert = (u_short) Sp_convert(p_num);

#ifdef NA
	if (category == DEV_CAT && id == DEV_ATTN) {
	    if((Pannex_id->version < VERS_6_2)||(Pannex_id->hw_id < ANX3)) {
		convert = CNV_PRINT;
		type = CARDINAL_P;
		latter = TRUE;
	    }
	}
#endif
	if(error = get_ln_param(&Pannex_id->addr, (u_short)SERIAL_DEV, port,
				(u_short)category, (u_short)id, (u_short)type,
				internal))
	    netadm_error(error);
	else {
	    decode(convert,internal,external,Pannex_id);
	    LONGWRAP(external);
	    if (start_delim = split_string(port_params[p_num], latter))
		printf(FMT, start_delim, external);
	    else
		printf(FMT, port_params[p_num], external);
	    WRAP;
	}
    }
    return;    
}

do_show_port(Pannex_id, name, Pports)
ANNEX_ID	  	*Pannex_id;
char			name[];
PORT_GROUP 		*Pports;
{
    SHOW_LIST	*Show_l;
    int		parm,
		p_num,
    		loop,
		shown = 0;
    
    if (Pannex_id->port_count == 0) {
	printf("\n%s %s no asynchronous ports\n", BOX, name);
	return;
    }

    /* Print the value of the serial port parameter for each port
       whose bit is set in the port mask. */
    
    for (loop = 1; loop <= Pannex_id->port_count; loop++) {
	if (PORTBITSET(Pports->serial_ports, loop)) {
	    shown++;
	    WRAP_END;
	    printf("\n%s %s port asy%d:\n", BOX, name, loop);
	    Show_l = Pshow_list;
	    do {			/* for each "show port" parameter */

		if(!Show_l)		/* default */
		    p_num = UNASSIGNED;		/* to all  */
		else
		    p_num = Show_l->param_num;
		
		if((p_num == UNASSIGNED) || (Sp_category(p_num) == GRP_CAT)) {
		    
		    for(parm = 0; Sp_index(parm) != -1; parm++) {

			/*
			 * Only add those parms of this category
			 * exception:
			 *	 "show port slip" shows slip and serial cats.
			 *       "show port ppp" shows ppp and serial cats.
			 */
			if(Sp_support(Pannex_id, parm) &&
			   Sp_support_check(Pannex_id, parm)) {

			    if((p_num == UNASSIGNED) ||
			       (Sp_catid(p_num) == ALL_CAT) ||
			       (Sp_displaycat(parm) == Sp_catid(p_num)) ||
			       ((Sp_displaycat(parm) == P_SERIAL_CAT) && 
				Sp_index(p_num) == PORT_SLIP) ||
			       ((Sp_displaycat(parm) == P_SERIAL_CAT) && 
				Sp_index(p_num) == PORT_PPP) ||
			       ((Sp_catid(p_num) == DEV_CAT) &&
				(Sp_category(parm) == DEV2_CAT))) {

				/*
				 * don't display LAT group code if LAT is off
				 */
				if (Pannex_id->lat == 0 &&
				    Sp_displaycat(parm) == P_LAT_CAT)
				    ;
				else {
				    /*
				     * The default will show only generic and
				     * flow category parameters.
				     */
				    if (p_num == UNASSIGNED) {
					if (Sp_displaycat(parm) == P_GENERIC_CAT                                    || Sp_displaycat(parm) == P_FLOW_CAT) {
					    port_show_header(parm);
					    port_show_sub(Pannex_id, loop, parm);
					}
				    }
				    else {
					port_show_header(parm);
					port_show_sub(Pannex_id, loop, parm);
				    }
				}
			    }
			}
		    }
		}
		else {
		    if (Sp_support(Pannex_id,p_num) &&
			Sp_support_check(Pannex_id,p_num)) {
			/*
			 * don't display authorized_groups and
			 * latb_enable if LAT is off 
			 */
			if (Pannex_id->lat == 0 &&
			    Sp_displaycat(p_num) == P_LAT_CAT)
			    printf("\t%s does not support %s\n",name,
				   port_params[p_num]);
			else
			    port_show_sub(Pannex_id, (u_short)loop, p_num);
		    }
		    else
			printf("\t%s does not support %s\n", name,
			       port_params[p_num]);
		}
		if(Show_l)
		    Show_l = Show_l->next;
		
	    } while(Show_l);
	}
    }
    if (shown == 0)
	printf("\n%s %s asynchronous port set has not been defined\n", BOX,
	       name);
    return;    
}

/*
 * Synchronous port level functionality
 */

sync_show_list(Psync_set)
SYNC_SET	*Psync_set;
{
    ANNEX_LIST	*Annex_l;
    SHOW_LIST	*Show_l;
    int		p_num;
    
    free_show_list();
    
    while(!eos) {
	p_num = match(symbol, sync_params, "sync port parameter name");
	if (Syncp_category(p_num) == VOID_CAT) {
	    /* obsolete parameter */
	    char error_msg[80];
	    error_msg[0] = '\0';
	    (void)strcat(error_msg, "sync port parameter name: ");
	    (void)strcat(error_msg, symbol);
	    punt("invalid ", error_msg);
	}
	
	/* Add a new SHOW_LIST structure to the end of the show list
	   pointed to by the given head and tail pointers. */
	
	Show_l = (SHOW_LIST *)malloc(sizeof(SHOW_LIST));
	Show_l->next = NULL;
	Show_l->param_num = p_num;
	
	if (!Pshow_list)
	    Pshow_list = Show_l;
	else
	    Pshow_tail->next = Show_l;
	Pshow_tail = Show_l;
	(void)lex();
    }
    
    /* Print the sync port parameters (from the show list) for each
       port in the given port set. */
    while (Psync_set) {
	/* If an annex id was specified, use it; otherwise, use the
	   default annex list. */
	if (Psync_set->annex_id.addr.sin_addr.s_addr)
	    do_show_sync(&Psync_set->annex_id, Psync_set->name,
			 &Psync_set->syncs);
	else
	    if (Pdef_annex_list)
		for (Annex_l = Pdef_annex_list; Annex_l; Annex_l=Annex_l->next)
		    do_show_sync(&Annex_l->annex_id,
				 Annex_l->name,
				 &Psync_set->syncs);
	    else
		punt(NO_BOXES, (char *)NULL);
	
	Psync_set = Psync_set->next;
    }
    printf("\n");
}

void 
sync_show_header(p_num) 
int		p_num;
{
    switch (p_num) {
	case SYNC_GENERIC_GROUP:WRAP_END;	printf(sync_generic);	break;
	case SYNC_PORT_SECURITY_GROUP:WRAP_END;	printf(sync_security);	break;
	case SYNC_NETADDR_GROUP:WRAP_END;	printf(sync_netaddr);	break;
	case SYNC_PPP_GROUP:	WRAP_END;	printf(sync_ppp);	break;
	default:							break;
    }
}

sync_show_sub(Pannex_id, port, p_num)
ANNEX_ID	   *Pannex_id;
unsigned short     port;
int                p_num;
{
    int		   category,		/*  Param category  */
		   id,			/*  Number w/in cat */
		   type,		/*  Data type	    */
		   convert;		/*  Conversion type */
    int		   error;
    long	   align_internal[(MAX_STRING_100 + 4)/sizeof(long) + 1];
    char	   *internal = (char *)align_internal,	/*  Machine format  */
		   external[LINE_LENGTH],		/*  Human format    */
		   *cp;
    char	   latter = FALSE;
    char	   *start_delim;
    
    /* Print the value of a serial port parameter. */
    category = Syncp_category(p_num);
    if(category == SYNC_CAT) {

	id = (u_short) Syncp_catid(p_num);
	type = (u_short) Syncp_type(p_num);
	convert = (u_short) Syncp_convert(p_num);
	if(error = get_sy_param(&Pannex_id->addr, (u_short)SYNC_DEV, port,
				(u_short)category, (u_short)id, (u_short)type,
				internal))
	    netadm_error(error);
	else {
	    decode(convert,internal,external,Pannex_id);
	    LONGWRAP(external);
	    if (start_delim = split_string(sync_params[p_num], latter))
		printf(FMT, start_delim, external);
	    else
		printf(FMT, sync_params[p_num], external);
	    WRAP;
	}
    }
    return;    
}

do_show_sync(Pannex_id, name, Psyncs)
ANNEX_ID	  	*Pannex_id;
char			name[];
SYNC_GROUP 		*Psyncs;
{
    SHOW_LIST	*Show_l;
    int		p_num,
		parm,
		loop,
		shown = 0;

    if (Pannex_id->sync_count == 0) {
	printf("\n%s %s no synchronous ports\n", BOX, name);
	return;
    }

    /* Print the value of the serial port parameter for each port
       whose bit is set in the port mask. */
    for (loop = 1; loop <= Pannex_id->sync_count; loop++) {
	if (PORTBITSET(Psyncs->sync_ports, loop)) {
	    WRAP_END;
	    printf("\n%s %s port syn%d:\n", BOX, name, loop);
	    Show_l = Pshow_list;
	    do {			/* for each "show sync" parameter */

		shown++;
		if(!Show_l)		/* default */
		    parm = UNASSIGNED;		/* to all  */
		else
		    parm = Show_l->param_num;
		
		if((parm == UNASSIGNED) || (Syncp_category(parm) == GRP_CAT)) {
		    for(p_num = 0; Syncp_index(p_num) != -1; p_num++)
			
			if(Syncp_support(Pannex_id,p_num))


			    /*
			     * Only add those parms of this category
			     * exception:
			     *  "show port slip" shows slip and serial cats.
			     *  "show port ppp" shows ppp and serial cats.
			     */
			    if((parm == UNASSIGNED) ||
			       (Syncp_catid(parm) == ALL_CAT) ||
			       (Syncp_displaycat(p_num) == 
				Syncp_catid(parm)) ||
			       ((Syncp_displaycat(p_num) == S_NET_CAT) &&
				Syncp_index(parm) == S_PPP)) {
				/*
				 * The default will show only generic parameters
				 */
				if (parm == UNASSIGNED) {
				    if (Syncp_displaycat(p_num) == S_GENERIC_CAT) { 
					sync_show_header(p_num);
					sync_show_sub(Pannex_id, loop, p_num);
				    }
				}
				else {
				    sync_show_header(p_num);
				    sync_show_sub(Pannex_id, loop, p_num);
				}
			    }
		}
		else
		    if(Syncp_support(Pannex_id,parm)) {
			sync_show_sub(Pannex_id, (u_short)loop, parm);
		    }
		    else
			printf("\t%s does not support %s\n",name,
			       sync_params[parm]);
		if(Show_l)
		    Show_l = Show_l->next;
		
	    } while(Show_l);
	}
    }
    if (shown == 0)
	printf("\n%s %s synchronous port set has not been defined\n", BOX,name);
    return;
}

/*
 *  Printer port level functionality
 */

printer_show_list(Pprinter_set)
PRINTER_SET	*Pprinter_set;
{
    ANNEX_LIST	*Annex_l;
    SHOW_LIST	*Show_l;
    int		p_num;
    
    free_show_list();
    
    while(!eos) {
	p_num = match(symbol, printer_params, "printer parameter name");
	if (Cp_category(p_num) == VOID_CAT) {
	    /* obsolete parameter */
	    char error_msg[80];
	    error_msg[0] = '\0';
	    (void)strcat(error_msg, "printer parameter name: ");
	    (void)strcat(error_msg, symbol);
	    punt("invalid ", error_msg);
	}
	
	/* Add a new SHOW_LIST structure to the end of the show list
	   pointed to by the given head and tail pointers. */
	
	Show_l = (SHOW_LIST *)malloc(sizeof(SHOW_LIST));
	Show_l->next = NULL;
	Show_l->param_num = p_num;
	
	if (!Pshow_list)
	    Pshow_list = Show_l;
	else
	    Pshow_tail->next = Show_l;
	Pshow_tail = Show_l;
	(void)lex();
    }
    
    /* Print the serial printer parameters (from the show list) for each
       printer in the given printer set. */
    
    while (Pprinter_set) {
	/* If an annex id was specified, use it; otherwise, use the
	   default annex list. */
	if (Pprinter_set->annex_id.addr.sin_addr.s_addr)
	    do_show_printer(&Pprinter_set->annex_id, Pprinter_set->name,
			    &Pprinter_set->printers);
	else
	    if (Pdef_annex_list)
		for (Annex_l = Pdef_annex_list; Annex_l; Annex_l=Annex_l->next)
		    do_show_printer(&Annex_l->annex_id, Annex_l->name,
				    &Pprinter_set->printers);
	    else
		punt(NO_BOXES, (char *)NULL);
	
	Pprinter_set = Pprinter_set->next;
    }
    printf("\n");
}

printer_show_sub(Pannex_id, printer, p_num)
ANNEX_ID	   *Pannex_id;
unsigned short     printer;
int                p_num;
{
    int		   category,		/*  Param category  */
		   id,			/*  Number w/in cat */
		   type;       		/*  Data type	    */
    int		   error;
    long	   align_internal[(MAX_STRING_100 + 4)/sizeof(long) + 1];
    char	   *internal = (char *)align_internal,	/*  Machine format  */
		   external[LINE_LENGTH];		/*  Human format    */
    char	   *start_delim;
    
    /* Print the value of a printer (parallel) port parameter. */
    category = Cp_category(p_num);
    if(category == LP_CAT) {

	id = Cp_catid(p_num);
	type = Cp_type(p_num);
	if(error = get_ln_param(&Pannex_id->addr, (u_short)P_PRINT_DEV,
				(u_short)printer, (u_short)category, 
				(u_short)id, (u_short)type, internal)) 
	    netadm_error(error);
	else {
	    decode(Cp_convert(p_num), internal, external, Pannex_id);
	    LONGWRAP(external);
	    if (start_delim=split_string(printer_params[p_num], FALSE))
		printf(FMT, start_delim, external);
	    else
		printf(FMT, printer_params[p_num], external);
	    WRAP;
	}
    }
    return;    
}

do_show_printer(Pannex_id, name, Pprinters)
ANNEX_ID	  	*Pannex_id;
char			name[];
PRINTER_GROUP 		*Pprinters;
{
    SHOW_LIST	*Show_l;
    int		p_num,
		parm,
		loop,
		shown = 0;
    
    /* Print the value of the printer parameter for each printer
       whose bit is set in the printer mask. */
    
    if(Pannex_id->printer_count == 0) {
	printf("\n%s %s no printer ports\n", BOX, name);
	return;
    }

    for (loop = 1; loop <= Pannex_id->printer_count; loop++) {
	if (PRINTERBITSET(Pprinters->ports,loop)) {
	    shown++;
	    WRAP_END;
	    printf("\n%s %s printer %d:\n", BOX, name, loop);
	    Show_l = Pshow_list;
	    do {		/* for each "show printer" parameter */
		
		if(!Show_l)		/* default */
		    parm = UNASSIGNED;		/* to all  */
		else
		    parm = Show_l->param_num;
		
		if((parm == UNASSIGNED) || (Cp_category(parm) == GRP_CAT)) {
		    for(p_num = 0; Cp_index(p_num) != -1; p_num++) {
			if(Cp_support(Pannex_id,p_num)) {
			    if((parm == UNASSIGNED) ||
			       (Cp_catid(parm) == ALL_CAT) ||
			       (Cp_category(p_num) == Cp_catid(parm))) {
				printer_show_sub(Pannex_id, loop, p_num);
			    }
			}
		    }
		}
		else
		    if(Cp_support(Pannex_id,parm))
			printer_show_sub(Pannex_id, (u_short)loop, parm);
		    else
			printf("\t%s does not support %s\n",name,
			       printer_params[parm]);
		if(Show_l)
		    Show_l = Show_l->next;
		
	    } while(Show_l);
	}
    }
    if (shown == 0)
	printf("\n%s %s printer set has not been defined\n", BOX, name);
    return;    
}

/*
 *  Interface level functionality
 */

interface_show_list(Pinterface_set)
INTERFACE_SET	*Pinterface_set;
{
    ANNEX_LIST	*Annex_l;
    SHOW_LIST	*Show_l;
    int		p_num;
    
    free_show_list();
    
    while(!eos) {
	p_num =match(symbol, interface_params, "interface parameter name");
	if (Ip_category(p_num) == VOID_CAT) {
	    /* obsolete parameter */
	    char error_msg[80];
	    error_msg[0] = '\0';
	    (void)strcat(error_msg, "interface parameter name: ");
	    (void)strcat(error_msg, symbol);
	    punt("invalid ", error_msg);
	}
	
	/* Add a new SHOW_LIST structure to the end of the show list
	   pointed to by the given head and tail pointers. */
	
	Show_l = (SHOW_LIST *)malloc(sizeof(SHOW_LIST));
	Show_l->next = NULL;
	Show_l->param_num = p_num;
	
	if (!Pshow_list)
	    Pshow_list = Show_l;
	else
	    Pshow_tail->next = Show_l;
	Pshow_tail = Show_l;
	(void)lex();
    }
    
    /* Print the interface parameters (from the show list) for each
       interface in the given interface set. */
    
    while (Pinterface_set) {
	/* If an annex id was specified, use it; otherwise, use the
	   default annex list. */
	if (Pinterface_set->annex_id.addr.sin_addr.s_addr)
	    do_show_interface(&Pinterface_set->annex_id, Pinterface_set->name,
			      &Pinterface_set->interfaces);
	else
	    if (Pdef_annex_list)
		for (Annex_l = Pdef_annex_list; Annex_l; Annex_l=Annex_l->next)
		    do_show_interface(&Annex_l->annex_id, Annex_l->name,
				      &Pinterface_set->interfaces);
	    else
		punt(NO_BOXES, (char *)NULL);
	
	Pinterface_set = Pinterface_set->next;
    }
    printf("\n");
}

void 
interface_show_header(p_num) 
int	p_num;
{
    switch (p_num) {
	
	case INTERFACE_RIP_GROUP:WRAP_END;	printf(interface_rip);	break;
	default:							break;
    }
}

interface_show_sub(Pannex_id, interface, p_num)
ANNEX_ID	   *Pannex_id;
unsigned short     interface;
int                p_num;
{
    int		   category,		/*  Param category  */
		   id,			/*  Number w/in cat */
		   type,		/*  Data type	    */
		   convert;		/*  Conversion type */
    int		   error;
    long	   align_internal[(MAX_STRING_100 + 4)/sizeof(long) + 1];
    char	   *internal = (char *)align_internal,	/*  Machine format  */
		   external[LINE_LENGTH],		/*  Human format    */
		   *cp;
    char	   latter = FALSE;
    char	   *start_delim;
    
    /* Print the value of an interface parameter. */
    category = Ip_category(p_num);
    if(category == IF_CAT) {

	id = (u_short) Ip_catid(p_num);
	type = (u_short) Ip_type(p_num);
	convert = (u_short) Ip_convert(p_num);
	if(error = get_if_param(&Pannex_id->addr, (u_short)INTERFACE_DEV,
				interface, (u_short)category, (u_short)id,
				(u_short)type, internal))
	    netadm_error(error);  
	else {
	    decode(convert,internal,external,Pannex_id);
	    LONGWRAP(external);
	    if (start_delim = split_string(interface_params[p_num], latter))
		printf(FMT, start_delim, external);
	    else
		printf(FMT, interface_params[p_num], external);
	    WRAP;
	}
    }
    return;    
}

do_show_interface(Pannex_id, name, Pinterfaces)
ANNEX_ID	  	*Pannex_id;
char			name[];
INTERFACE_GROUP 	*Pinterfaces;
{
    SHOW_LIST	*Show_l;
    int		p_num,
		parm,
		loop,
		asy_end,
		syn_end,
		if_num,
		shown = 0;
    
    /* Print the value of the each interface parameter for each interface 
       whose bit is set in the interface mask. */
    
    asy_end = (int)Pannex_id->port_count + 1;
    syn_end = (int)Pannex_id->sync_count + 1 + ALL_PORTS;
    for (loop = 1; loop <= ALL_INTERFACES; loop++) {
	
	if ((loop <= asy_end) || ((loop > ALL_PORTS+1) && (loop <= syn_end))) {
	    
	    if (INTERFACEBITSET(Pinterfaces->interface_ports,loop)) {
		
	    	/*
	     	 * Convert the logical index into async interface
	     	 * number to make sure within the port range.
	     	 */
		if_num = loop;
		
		if (if_num > (M_ETHERNET + ALL_PORTS)) {
		    if_num = if_num - M_ETHERNET - ALL_PORTS;
		    if (if_num > (int)Pannex_id->sync_count) {
			printf("\n%s %s does not have an synchronous interface %d\n",
			       BOX, name, if_num);
			continue;
		    }
		}
		else {
		    if (if_num > M_ETHERNET) {
		    	if_num = if_num - M_ETHERNET;
		 	if (if_num > (int)Pannex_id->port_count) {
			    printf("\n%s %s does not have an asynchronous interface %d\n",
				   BOX, name, if_num);
			    continue;
			}
		    }
		}
		WRAP_END;
		
		/* convert the interface logical index into human-eye form */
		if (loop == M_ETHERNET)
		    printf("\n%s %s interface en%d:\n", BOX, name, 
			   loop-M_ETHERNET);
		else 
		    if(loop <= (M_ETHERNET + ALL_PORTS) )
			printf("\n%s %s interface asy%d:\n", BOX, name, 
			       loop-M_ETHERNET);
		    else
			printf("\n%s %s interface syn%d:\n", BOX, name, 
			       loop-M_ETHERNET-ALL_PORTS);
		
	        Show_l = Pshow_list;
	        do {		/* for each "show interface" parameter */
		    
		    shown++;
		    if(!Show_l)		/* default */
			parm = UNASSIGNED;	/* to all  */
		    else
			parm = Show_l->param_num;
		    
		    if((parm == UNASSIGNED) ||(Ip_category(parm) == GRP_CAT)) {

			for(p_num = 0; Ip_index(p_num) != -1; p_num++) {
			    if(Ip_support(Pannex_id,p_num) && Ip_support_check(Pannex_id, p_num)) {
				if((parm == UNASSIGNED) ||
				   (Ip_catid(parm) == ALL_CAT) ||
				   (Ip_category(p_num) == Ip_catid(parm))) {
				    interface_show_header(p_num); 
				    interface_show_sub(Pannex_id, loop, p_num);
				}
			    }
			}
		    }
		    else
			if(Ip_support(Pannex_id,parm) && Ip_support_check(Pannex_id, parm))
			    interface_show_sub(Pannex_id, (u_short)loop, parm);
			else
			    printf("\t%s does not support %s\n",name,
				   interface_params[parm]);
		    if(Show_l)
			Show_l = Show_l->next;
		    
		} while(Show_l);
	    }
	}
    }
    if (shown == 0)
	printf("\n%s %s interface set has not been defined\n", BOX, name);
    return;    
}

/*
 * The following section is all set code.
 */

/*
 * Set annex functions.
 */

int
annex_pair_list(Pannex_list)
ANNEX_LIST	*Pannex_list;
{
    SET_LIST	*Set_l;
    ANNEX_LIST	*Annex_l;
    int		len;
    int	        p_num, vcli_passwd_parm;
    int		error,terror;
    char        *pass, nullstring = 0;
    char 	dont_lex = FALSE;
    
    free_set_list();
    while (!eos) {
	vcli_passwd_parm = !strcmp(symbol, "vcli_password");
	p_num = match(symbol, annex_params, BOX_PARM_NAME);
	if (Ap_category(p_num) == VOID_CAT) {
	    /* obsolete parameter */
	    char error_msg[80];
	    error_msg[0] = '\0';
	    (void)strcat(error_msg, "port parameter name: ");
	    (void)strcat(error_msg, symbol);
	    punt("invalid ", error_msg);
	}
	
	/* Add a new SET_LIST structure to the end of the set list
	   pointed to by the given head and tail pointers. */
	
	Set_l = (SET_LIST *)malloc(sizeof(SET_LIST));
	Set_l->next = NULL;
	Set_l->param_num = p_num;
	
	if (!Pset_list)
	    Pset_list = Set_l;
	else
	    Pset_tail->next = Set_l;
	Pset_tail = Set_l;
	
	(void)lex();
	
	if (eos)
	    if (vcli_passwd_parm) {
		if (script_input)
		    pass = &nullstring;
		else {
		    pass = (char *)get_password((struct in_addr *)0);
		    (void)strcpy(Set_l->value, pass);
		    break;
		}
	    }
	    else  
		punt("missing parameter value", (char *)NULL);
	
	lex_string();
	(void)strcpy(Set_l->value, symbol);
	
	if (((Ap_catid(p_num) == LAT_GROUP_CODE) ||
	     (Ap_catid(p_num) == LAT_VCLI_GROUPS)) &&
	    (Ap_category(p_num) == LAT_CAT)) {
	    (void)lex();
	    if (!eos) {
		len = strlen(Set_l->value);
                if (strcmp(Set_l->value,"none")==0  ||
                     strcmp(Set_l->value,"all")==0) {
                    if (strcmp(symbol,"enable")==0 || 
                      strcmp(symbol,"disable")==0){ 
		       Set_l->value[len++] = ' ';
		       Set_l->value[len] = '\0';
		       (void)strcpy(&Set_l->value[len], symbol);
                    }
                    else /* didn't get enable or disable */  {
                      Set_l->value[len++] = ' ';
                      Set_l->value[len] = '\0';
                      dont_lex = TRUE;
                    }
                }
                else {
                     Set_l->value[len++] = ' ';
                     Set_l->value[len] = '\0';
                     (void)strcpy(&Set_l->value[len], symbol);
	        }
            }
	}
	if (dont_lex == FALSE)
	   (void)lex();
        else
	   dont_lex = FALSE;
    }
    
    /* Assign the per-annex parameters (from the set list) for each
       annex in the annex list. */
    error = -1;
    for (Set_l = Pset_list;Set_l; Set_l = Set_l->next) {
	for (Annex_l = Pannex_list; Annex_l; Annex_l = Annex_l->next) {

	    /* If any succeed, then return success. */
	    terror = annex_set_sub(&Annex_l->annex_id, Set_l, Annex_l->name);
	    if (error != 0)
		error = terror;
	}
    }
    return(error);
}

int
annex_set_sub(Pannex_id, Set_l, name)
ANNEX_ID	   *Pannex_id;
SET_LIST           *Set_l;
char		   name[];
{
    int	   category,		/*  Param category  */
	   p_num;		/*  Parameter num.  */
    long   align_internal[(MAX_STRING_100 + 4)/sizeof(long) + 1];
    char   *internal = (char *)align_internal,	/*  Machine format  */
	   *external;				/*  Human format    */
    int	   error = -1;
    
    
    /* Assign a per-annex parameter. */
    external = Set_l->value;
    p_num = Set_l->param_num;
    category = Ap_category(p_num);
    
    if (category == DLA_CAT || category == DFE_CAT || category == LAT_CAT ||
	category == ARAP_CAT || category == RIP_CAT) {
	if (Ap_support(Pannex_id,p_num)) {
	    encode(Ap_convert(p_num), external, internal, Pannex_id);
	    if ((Pannex_id->hw_id != ANX3) && (Pannex_id->hw_id != ANX_MICRO)&&
		(Ap_type(p_num) == STRING_P_100)) {

		/* use type STRING_P when writing to old annexes */
		if (error = set_dla_param(&Pannex_id->addr, (u_short)category,
					  (u_short)Ap_catid(p_num),
					  (u_short)STRING_P, internal)) {
		    netadm_error(error);
		}
	    } 
	    else {
		if (error = set_dla_param(&Pannex_id->addr, (u_short)category,
					  (u_short)Ap_catid(p_num),
					  (u_short)Ap_type(p_num), internal)) {
		    netadm_error(error);
		}
	    }
	} else
	    printf("\t%s does not support parameter: %s\n\n", name,
		   annex_params[p_num]);
    } else
	printf("\t%s is not a settable annex parameter:\n\n",
	       annex_params[p_num]);
    return(error);
}

/*
 * Set Asynchronous port functions.
 */

int
port_pair_list(Pport_set)
PORT_SET	*Pport_set;
{
    ANNEX_LIST	*Annex_l;
    PORT_SET	*Port_s;
    SET_LIST	*Set_l;
    int	        p_num,
		pt_passwd_parm;
    char        *pass,
		nullstring = 0;
    int		error,
		terror;
    char        dont_lex = FALSE;
    
    free_set_list();
    while (!eos) {
	pt_passwd_parm = !strcmp(symbol, "port_password");
	p_num = match(symbol, port_params, "port parameter name");
	if (Sp_category(p_num) == VOID_CAT) {
	    /* obsolete parameter */
	    char error_msg[80];
	    error_msg[0] = '\0';
	    (void)strcat(error_msg, "port parameter name: ");
	    (void)strcat(error_msg, symbol);
	    punt("invalid ", error_msg);
	}
	
	/* Add a new SET_LIST structure to the end of the set list
	   pointed to by the given head and tail pointers. */
	
	Set_l = (SET_LIST *)malloc(sizeof(SET_LIST));
	Set_l->next = NULL;
	Set_l->param_num = p_num;
	
	if (!Pset_list)
	    Pset_list = Set_l;
	else
	    Pset_tail->next = Set_l;
	Pset_tail = Set_l;
	(void)lex();
	
	if (eos)
	    if (pt_passwd_parm) {
		if (script_input)
		    pass = &nullstring;
		else {
		    pass = (char *)get_password((struct in_addr *)0);
		    (void)strcpy(Set_l->value, pass);
		    break;
		}
	    }
	    else
		punt("missing parameter value", (char *)NULL);
	
	lex_string();
	(void)strcpy(Set_l->value, symbol);

	if (Sp_displaycat(p_num) == P_LAT_CAT) {
	    (void)lex();
	    if (!eos) {
		int len = strlen(Set_l->value);
                if (strcmp(Set_l->value,"none")==0  ||
                     strcmp(Set_l->value,"all")==0) {
                    if (strcmp(symbol,"enable")==0 || 
                      strcmp(symbol,"disable")==0){ 
		       Set_l->value[len++] = ' ';
		       Set_l->value[len] = '\0';
		       (void)strcpy(&Set_l->value[len], symbol);
                    }
                    else /* didn't get enable or disable */  {
                      Set_l->value[len++] = ' ';
                      Set_l->value[len] = '\0';
                      dont_lex = TRUE;
                    }
                }
                else {
                     Set_l->value[len++] = ' ';
                     Set_l->value[len] = '\0';
                     (void)strcpy(&Set_l->value[len], symbol);
	        }
            }
	}
	if (dont_lex == FALSE)
	   (void)lex();
        else
	   dont_lex = FALSE;
    }
    
    /* Assign the serial port parameters (from the set list) for each
       port in the given port set.  If an annex id was specified, use it;
       otherwise, use the default annex list. */
    
    error = -1;
    for (Set_l = Pset_list;Set_l; Set_l = Set_l->next) {
	for (Port_s = Pport_set; Port_s; Port_s = Port_s->next) {
	    if (Port_s->annex_id.addr.sin_addr.s_addr) {
		terror = do_set_port(&Port_s->annex_id, &Port_s->ports,Set_l,
				     Port_s->name);
		if (error != 0)
		    error = terror;
	    }
	    else if (Pdef_annex_list != NULL)
		for (Annex_l = Pdef_annex_list; Annex_l != NULL;
		     Annex_l = Annex_l->next) {
		    terror = do_set_port(&Annex_l->annex_id, &Port_s->ports,
					Set_l, Annex_l->name);
		    if (error != 0)
			error = terror;
		}
	    else
		punt(NO_BOXES, (char *)NULL);
	}
    }
    return error;
}

int
do_set_port(Pannex_id, Pports, Set_l, name)
ANNEX_ID	  	*Pannex_id;
PORT_GROUP		*Pports;
SET_LIST		*Set_l;
char			name[];
{
    int	loop,
	param = Set_l->param_num,
	error = -1,
	terror;
    
    /* Assign serial port parameters to each port whose bit is set
       in the port mask. */

    if(Sp_support(Pannex_id,param)) {
	for (loop = 1; loop <= Pannex_id->port_count; loop++)
	    if (PORTBITSET(Pports->serial_ports,loop)) {
		terror = port_set_sub(Pannex_id,(u_short)loop, Set_l);
		if (error != 0)
		    error = terror;
	    }
    }
    else
	printf("\t%s does not support parameter: %s\n\n", name,
	       port_params[Set_l->param_num]);
    return error;
}

int
port_set_sub(Pannex_id, port, Set_l)
ANNEX_ID	   *Pannex_id;
unsigned short     port;
SET_LIST           *Set_l;
{
    int	   category,		/*  Param category  */
	   p_num;		/*  Parameter num.  */
    long   align_internal[MAX_STRING_100/sizeof(long) + 1];
    char   *internal = (char *)align_internal,	/*  Machine format  */
	   *external;				/*  Human format    */
    int	   error = -1;
    
    /* Assign a serial port parameter. */
    
    external = Set_l->value;
    p_num = Set_l->param_num;
    category = Sp_category(p_num);
    
    if(category == INTF_CAT || category == DEV_CAT || category == DEV2_CAT ||
       category == EDIT_CAT || category == SLIP_CAT || category == NET_CAT) {

	u_short id,type,convert;
	id = (u_short)Sp_catid(p_num);
	type = (u_short)Sp_type(p_num);
	convert = (u_short) Sp_convert(p_num);
#ifdef NA
	if (category == DEV_CAT && id == DEV_ATTN) {
	    if((Pannex_id->version < VERS_6_2)||(Pannex_id->hw_id < ANX3)) {
		convert = CNV_PRINT;
		type = CARDINAL_P;
	    }
	}
#endif
	encode(convert, external, internal, Pannex_id);
	
	if(error = set_ln_param(&Pannex_id->addr, (u_short)SERIAL_DEV,
				(u_short)port, (u_short)category, id, type,
				internal))
	    netadm_error(error);
    }
    return error;
}

/*
 * Set Synchronous port functions.
 */

int
sync_pair_list(Psync_set)
SYNC_SET	*Psync_set;
{
    ANNEX_LIST	*Annex_l;
    SYNC_SET	*Sync_s;
    SET_LIST	*Set_l;
    int	        p_num,
		pt_passwd_parm;
    char        *pass,
		nullstring = 0;
    int		error,
		terror;
    
    free_set_list();
    
    while (!eos) {
	pt_passwd_parm = !strcmp(symbol, "port_password");
	p_num = match(symbol, sync_params, "sync port parameter name");
	if (Syncp_category(p_num) == VOID_CAT) {
	    /* obsolete parameter */
	    char error_msg[80];
	    error_msg[0] = '\0';
	    (void)strcat(error_msg, "sync port parameter name: ");
	    (void)strcat(error_msg, symbol);
	    punt("invalid ", error_msg);
	}
	
	/* Add a new SET_LIST structure to the end of the set list
	   pointed to by the given head and tail pointers. */
	
	Set_l = (SET_LIST *)malloc(sizeof(SET_LIST));
	Set_l->next = NULL;
	Set_l->param_num = p_num;
	
	if (!Pset_list)
	    Pset_list = Set_l;
	else
	    Pset_tail->next = Set_l;
	Pset_tail = Set_l;
	
	(void)lex();
	
	if (eos)
	    if (pt_passwd_parm) {
		if (script_input)
		    pass = &nullstring;
		else {
		    pass = (char *)get_password((struct in_addr *)0);
		    (void)strcpy(Set_l->value, pass);
		    break;
		}
	    }
	    else
		punt("missing parameter value", (char *)NULL);
	
	lex_string();
	(void)strcpy(Set_l->value, symbol);
	(void)lex();
    }
    
    /* Assign the sync port parameters (from the set list) for each
       port in the given port set.  If an annex id was specified, use it;
       otherwise, use the default annex list. */
    
    error = -1;
    for (Set_l = Pset_list; Set_l; Set_l = Set_l->next) {
	for (Sync_s = Psync_set; Sync_s; Sync_s = Sync_s->next) {
	    if (Sync_s->annex_id.addr.sin_addr.s_addr) {
		terror = do_set_sync(&Sync_s->annex_id, &Sync_s->syncs, Set_l,
				     Sync_s->name);
		if (error != 0)
		    error = terror;
	    }
	    else if (Pdef_annex_list != NULL)
		for (Annex_l = Pdef_annex_list; Annex_l != NULL;
		     Annex_l = Annex_l->next) {
		    terror = do_set_sync(&Annex_l->annex_id, &Sync_s->syncs,
					 Set_l, Annex_l->name);
		    if (error != 0)
			error = terror;
		}
	    else
		punt(NO_BOXES, (char *)NULL);
	}
    }
    return error;
}

int
do_set_sync(Pannex_id, Psyncs, Set_l, name)
ANNEX_ID	  	*Pannex_id;
SYNC_GROUP		*Psyncs;
SET_LIST		*Set_l;
char			name[];
{
    int	loop,
	param,
	error,
	terror;
    
    /* Assign sync port parameters to each port whose bit is set
       in the port mask. */
    param = Set_l->param_num;
    error = -1;

    if(Syncp_support(Pannex_id,param)) {
	for (loop = 1; loop <= Pannex_id->sync_count; loop++) {
	    if (PORTBITSET(Psyncs->sync_ports,loop)) {
		terror = sync_set_sub(Pannex_id, (u_short)loop, Set_l);
		if (error != 0)
		    error = terror;
	    }
	}
    }
    else
	printf("\t%s does not support parameter: %s\n\n",name,
	       sync_params[Set_l->param_num]);
    return error;
}

int
sync_set_sub(Pannex_id, port, Set_l)
ANNEX_ID	   *Pannex_id;
unsigned short     port;
SET_LIST           *Set_l;
{
    int	    category,		/*  Param category  */
	    p_num;		/*  Parameter num.  */
    long   align_internal[MAX_STRING_100/sizeof(long) + 1];
    char   *internal = (char *)align_internal,	/*  Machine format  */
	    *external;				/*  Human format    */
    int	    error = -1;
    
    /* Assign a sync port parameter. */
    external = Set_l->value;
    p_num = Set_l->param_num;
    category = Syncp_category(p_num);
    
    if(category == SYNC_CAT) {
	u_short id,type,convert;
	id = (u_short)Syncp_catid(p_num);
	type = (u_short)Syncp_type(p_num);
	convert = (u_short) Syncp_convert(p_num);
	
	encode(convert, external, internal, Pannex_id);
	if(error = set_sy_param(&Pannex_id->addr, (u_short)SYNC_DEV,
				(u_short)port, (u_short)category, id, type, internal))
	    netadm_error(error);
    }
    return error;
}

/*
 * Set Printer port functions.
 */

int
do_set_printer(Pannex_id, Pprinters, Set_l, name)
ANNEX_ID	  	*Pannex_id;
PRINTER_GROUP		*Pprinters;
SET_LIST		*Set_l;
char			name[];
{
    int	loop,
	error = -1,
	terror;
    
    /* Assign serial printer parameters to each printer whose bit is set
       in the printer mask. */
    
    if (Cp_support(Pannex_id, Set_l->param_num)) {
	for (loop = 1; loop <= Pannex_id->printer_count; loop++) {
	    if (PRINTERBITSET(Pprinters->ports,loop)) {
		terror = printer_set_sub(Pannex_id,Set_l, (u_short)loop);
		if (error != 0)
		    error = terror;
	    }
	}
    } else
	printf("\t%s does not support parameter: %s\n\n",name,
	       printer_params[Set_l->param_num]);
    return error;
}

int
    printer_pair_list(Pprinter_set)
PRINTER_SET	*Pprinter_set;
{
    ANNEX_LIST	*Annex_l;
    PRINTER_SET	*Print_s;
    SET_LIST	*Set_l;
    int	         p_num, pt_passwd_parm;
    char            *pass, nullstring = 0;
    int		error,terror;
    
    free_set_list();
    
    while (!eos) {
	pt_passwd_parm = !strcmp(symbol, "printer_password");
	p_num = match(symbol, printer_params, "printer parameter name");
	
	if (Cp_category(p_num) == VOID_CAT) {
	    /* obsolete parameter */
	    char error_msg[80];
	    error_msg[0] = '\0';
	    (void)strcat(error_msg, "printer parameter name: ");
	    (void)strcat(error_msg, symbol);
	    
	    punt("invalid ", error_msg);
	}
	
	/* Add a new SET_LIST structure to the end of the set list
	   pointed to by the given head and tail pointers. */
	
	Set_l = (SET_LIST *)malloc(sizeof(SET_LIST));
	Set_l->next = NULL;
	Set_l->param_num = p_num;
	
	if (!Pset_list)
	    Pset_list = Set_l;
	else
	    Pset_tail->next = Set_l;
	Pset_tail = Set_l;
	
	(void)lex();
	
	if (eos)
	    if (pt_passwd_parm) {
		if (script_input)
		    pass = &nullstring;
		else {
		    pass = (char *)get_password((struct in_addr *)0);
		    (void)strcpy(Set_l->value, pass);
		    break;
		}
	    }
	    else
		punt("missing parameter value", (char *)NULL);
	
	lex_string();
	(void)strcpy(Set_l->value, symbol);
	
	(void)lex();
    }
    
    /* Assign the serial printer parameters (from the set list) for each
       printer in the given printer set.  If an annex id was specified, use it;
       otherwise, use the default annex list. */
    
    error = -1;
    for (Set_l = Pset_list; Set_l; Set_l = Set_l->next)
	for (Print_s = Pprinter_set; Print_s; Print_s = Print_s->next)
	    if (Print_s->annex_id.addr.sin_addr.s_addr) {
		terror = do_set_printer(&Print_s->annex_id,
					&Print_s->printers,
					Set_l,Print_s->name);
		if (error != 0)
		    error = terror;
	    }
	    else if (Pdef_annex_list)
		for (Annex_l = Pdef_annex_list; Annex_l;
		     Annex_l = Annex_l->next) {
		    terror = do_set_printer(
					    &Annex_l->annex_id,
					    &Print_s->printers,
					    Set_l,Annex_l->name);
		    if (error != 0)
			error = terror;
		}
	    else
		punt(NO_BOXES, (char *)NULL);
    return error;
}	/* printer_pair_list() */

int
    printer_set_sub(Pannex_id, Set_l, name)
ANNEX_ID	   *Pannex_id;
SET_LIST           *Set_l;
char		   name[];
{
    int		   category,		/*  Param category  */
    p_num;		/*  Parameter num.  */
    long	   align_internal[MAXVALUE/sizeof(long) + 1];
    char	   *internal = (char *)align_internal,	/*  Machine format  */
		   *external;				/*  Human format    */
    int		   error = -1;
    
    /* Assign a centronics port parameter. */
    
    /* Check for existance of a printer */
    if (Pannex_id->printer_count < 1) {
	punt("\n%s does not have a printer\n", name);
    }
    
    external = Set_l->value;
    p_num = Set_l->param_num;
    category = Cp_category(p_num);
    
    if(category == LP_CAT)
	if(Cp_support(Pannex_id,p_num)) {
	    encode(Cp_convert(p_num), external, internal, Pannex_id);
	    if(error = set_ln_param(&Pannex_id->addr, 
				    (u_short)P_PRINT_DEV, (u_short)name, (u_short)category,
				    (u_short)Cp_catid(p_num), (u_short)Cp_type(p_num),
				    internal))
		netadm_error(error);
	}
	else
	    printf("\t%s does not support parameter: %s\n\n",name,
		   printer_params[p_num]);
    return error;
}	/* printer_set_sub() */

/*
 * Set Interface functions.
*/

int
    interface_pair_list(Pinterface_set)
INTERFACE_SET	*Pinterface_set;
{
    ANNEX_LIST	*Annex_l;
    INTERFACE_SET	*Interf_s;
    SET_LIST	*Set_l;
    int	         p_num, pt_passwd_parm;
    char            *pass, nullstring = 0;
    int		len,error,terror;
    free_set_list();
    while (!eos) {
	pt_passwd_parm = !strcmp(symbol, "interface_password");
	p_num = match(symbol, interface_params, "interface parameter name");
	
	if (Ip_category(p_num) == VOID_CAT) {
	    /* obsolete parameter */
	    char error_msg[80];
	    error_msg[0] = '\0';
	    (void)strcat(error_msg, "interface parameter name: ");
	    (void)strcat(error_msg, symbol);
	    
	    punt("invalid ", error_msg);
	}
	
	/* Add a new SET_LIST structure to the end of the set list
	   pointed to by the given head and tail pointers. */
	
	Set_l = (SET_LIST *)malloc(sizeof(SET_LIST));
	Set_l->next = NULL;
	Set_l->param_num = p_num;
	
	if (!Pset_list)
	    Pset_list = Set_l;
	else
	    Pset_tail->next = Set_l;
	Pset_tail = Set_l;
	
	(void)lex();
	
	if (eos)
	    if (pt_passwd_parm) {
		if (script_input)
		    pass = &nullstring;
		else {
		    pass = (char *)get_password((struct in_addr *)0);
		    (void)strcpy(Set_l->value, pass);
		    break;
		}
	    }
	    else 
		punt("missing parameter value", (char *)NULL);
	
	lex_string();
	(void)strcpy(Set_l->value, symbol);
	
	/* 
	 * The parameter format of rip_accept and rip_advertise
	 * is a bit different from the rest of parameters.    
	 * include/exclude xx.xx.xx.xx,xx.xx.xx.xx .....
	 */ 
	if ( ((Ip_catid(p_num) == IF_RIP_ACCEPT) ||
	      (Ip_catid(p_num) == IF_RIP_ADVERTISE)) && 
              (!strncmp(symbol,"include",strlen(symbol)) || !strncmp(symbol,"exclude",strlen(symbol))) ){
	    (void)lex();
	    /*
	     * concatenate the rest of line into the buffer, the
	     * encode routine will do the parsing
	     */
	    if (!eos) {
		(void)lex_end(); 
		len = strlen(Set_l->value);
		Set_l->value[len++] = ' ';
		Set_l->value[len] = '\0';
		(void)strcpy(&Set_l->value[len], symbol);
	    }
	}
	
	(void)lex();
    }
    
    /* Assign the interface parameters (from the set list) for each
       interface in the given interface set.  If an annex id was specified,
       use it; otherwise, use the default annex list. */
    
    error = -1;
    for (Set_l = Pset_list; Set_l; Set_l = Set_l->next)
	for (Interf_s = Pinterface_set; Interf_s; Interf_s = Interf_s->next)
	    if (Interf_s->annex_id.addr.sin_addr.s_addr) {
		terror = do_set_interface(&Interf_s->annex_id,
					 &Interf_s->interfaces, Set_l,
					 Interf_s->name);
		if (error != 0)
		    error = terror;
	    }
	    else if (Pdef_annex_list)
		for (Annex_l = Pdef_annex_list; Annex_l;
		     Annex_l = Annex_l->next) {
		    terror = do_set_interface(&Annex_l->annex_id,
					      &Interf_s->interfaces,
					      Set_l, Annex_l->name);
		    if (error != 0)
			error = terror;
		}
	    else
		punt(NO_BOXES, (char *)NULL);
    return error;
}	/* interface_pair_list() */

int
do_set_interface(Pannex_id, Pinterfaces, Set_l, name)
ANNEX_ID	  	*Pannex_id;
INTERFACE_GROUP		*Pinterfaces;
SET_LIST		*Set_l;
char			name[];
{
    int 	loop, loop_limit, param;
    int	if_num, asy_end,syn_end;
    int	error = -1,terror;
    
    /* Assign interface parameters to each interface whose bit is set
       in the interface mask. */
    
    /* en0 plus asy */
    asy_end = (int)Pannex_id->port_count + 1;
    syn_end = (int)Pannex_id->sync_count + 1 + ALL_PORTS;

    param = Set_l->param_num;
    
    if (Ip_support(Pannex_id,param)) {
	loop_limit = ALL_INTERFACES; 
	for (loop = 1; loop <= loop_limit; loop++)

	    if ((loop <= asy_end) || ((loop > ALL_PORTS+1) &&
				      (loop <= syn_end))) {
		
		if (INTERFACEBITSET(Pinterfaces->interface_ports,loop)) {
		    
		    /*
		     * Convert the logical index into async interface
		     * number to make sure within the port range.
		     */
		    if_num = loop;
		    
		    
               	    if (if_num > (M_ETHERNET + ALL_PORTS)) {
                        if_num = if_num - M_ETHERNET - ALL_PORTS;
                        if (if_num > (int)Pannex_id->sync_count) {
			    printf("\n%s %s does not have an synchronous interface %d\n",
				   BOX, name, if_num);
			    continue;
			}
                    } else
			if (if_num > M_ETHERNET) {
			    if_num = if_num - M_ETHERNET;
			    if (if_num > (int)Pannex_id->port_count) {
				printf("\n%s %s does not have an asynchronous interface %d\n"
				       ,
				       BOX, name, if_num);
				continue;
			    }
			}
		    
		    
	            terror = interface_set_sub(Pannex_id,(u_short)loop,
					       Set_l);
		    if (error != 0)
			error = terror;
		}
	    }
    }
    else
	printf("\t%s does not support parameter: %s\n\n",
	       name,interface_params[Set_l->param_num]);
    return error;
}	/* do_set_interface() */

int
    interface_set_sub(Pannex_id, interface, Set_l)
ANNEX_ID	   *Pannex_id;
unsigned short     interface;
SET_LIST           *Set_l;
{
    int		   category,		/*  Param category  */
    p_num;		/*  Parameter num.  */
    long	   align_internal[MAX_STRING_100/sizeof(long) + 1];
    char	   *internal = (char *)align_internal,	/*  Machine format  */
		   *external;				/*  Human format    */
    int		   error = -1;
    
    
    
    external = Set_l->value;
    p_num = Set_l->param_num;
    category = Ip_category(p_num);
    
    if (category == IF_CAT) {
	u_short id,type,convert;
	
	id = (u_short)Ip_catid(p_num);
	type = (u_short)Ip_type(p_num);
	convert = (u_short) Ip_convert(p_num);
	
	encode(convert, external, internal, Pannex_id);
	
	if(error = set_if_param(&Pannex_id->addr, (u_short)INTERFACE_DEV,
				(u_short)interface, (u_short)category, id, type, internal))
	    netadm_error(error);
    }
    return error;
}	/* interface_set_sub() */

/*
 * Do a message!
 */

message(text)
char text[];
{
    time_t time_val;
    
    if (first_broadcast) {
	char *Pusername,
	hostname[33];
	
	Pusername = getlogin();
	
	if (Pusername) {
	    (void)strncat(header, Pusername, 32);
	    (void)strcat(header, "@");
	}
	else
	    (void)strcat(header, "unknown@");
	
	(void)gethostname(hostname, 32);
	
	if (strlen(hostname))
	    (void)strcat(header, hostname);
	else
	    (void)strcat(header, "unknown");
	
	(void)strcat(header, " [");
	time_val = time((time_t *)0);
	(void)strcat(header,ctime(&time_val));
	header[strlen(header)-1] = '\0'; /* Remove the \n from ctime() */
	(void)strcat(header, "] ***\n\n");
	
	first_broadcast = FALSE;
    }
    
    text[0] = '\0';
    
    (void)strcat(text, header);
    
    (void)strcat(text, symbol);
    
    (void)strcat(text, Psymbol);
    
    eos = TRUE;
    
    if ((int)strlen(text) > 1)
	while (text[strlen(text) - 2] == '\\') {
	    text[strlen(text) - 2] = '\n';
	    text[strlen(text) - 1] = '\0';
	    
	    prompt("\tcontinue", NULLSP, TRUE);
	    
	    if ((int)(strlen(text) + strlen(command_line)) > MSG_LENGTH)
		punt("message too long", (char *)NULL);
	    
	    (void)strcat(text, command_line);
	    
	    eos = TRUE;
	}
    
}	/* message() */

/* warning_message :  takes the current symbol and the rest of the
 *                    input up to WARNING_LENGTH and places them
 *		      into text.  It assumes a lex called was
 *		      made prior to calling.
 */
warning_message(text)
char text[];
{
    text[0] = '\0';
    
    (void)strcat(text, symbol);
    (void)strcat(text, Psymbol);
    
    eos = TRUE;
    
    if ((int)strlen(text) > 1)
	while (text[strlen(text) - 2] == '\\') {
	    text[strlen(text) - 2] = '\n';
	    text[strlen(text) - 1] = '\0';
	    prompt("\tcontinue", NULLSP, TRUE);
	    
	    if((int)(strlen(text) + strlen(command_line)) > WARNING_LENGTH)
		punt("warning too long", (char *)NULL);
	    
	    (void)strcat(text, command_line);
	    
	    eos = TRUE;
	}
    
}	/* warning_message() */

/*
 * free linked lists
 */

free_show_list()
{
    SHOW_LIST *Show_l;
    
    while(Pshow_list) {
	Show_l = Pshow_list;
	Pshow_list = Pshow_list->next;
	free((char *)Show_l);
    }
    INITWRAP;
}

free_set_list()
{
    SET_LIST *Set_l;
    
    while(Pset_list) {
	Set_l = Pset_list;
	Pset_list = Pset_list->next;
	free((char *)Set_l);
    }
}

/*
 * Reads the [+][HH:]MM time format and returns it in boot_time.
 * The abs_only flag will cause it to not accept the [+] offset
 * time format and only search for absolute time.
 */

time_t
    delay_time(abs_only)
u_short abs_only;
{
    time_t
	tim,
	t = 0,
	t1 = 0;
    struct tm
	*lt;
    int
	i = 0,
	hhmm = 0;
    
    if(symbol_length < 1)
	return (BADTIME);
    if(symbol[i] == '+') {                 
	i++;
	while(isdigit(symbol[i])) {
	    t = t * 10 + (symbol[i] - '0');
	    i++;
	}
	
	if (symbol[i] == ':') {   /* get the minutes */
	    i++;
	    hhmm = 1;    /*time in hhmm format */
	    while(isdigit(symbol[i])) {
		t1 = t1 * 10 + (symbol[i] - '0');
		i++;
	    }
	} /* of read minutes */
	
	if (symbol[i] != '\0')   /*extra junk BADTIME */
	    return (BADTIME);
	
	if (hhmm)  /*range check the values*/
	    if ((t>99) || (t1 > 59))
		return (BADTIME);
	    else 
		tim = (t * 3600) + (t1 * 60);
	else
	    if (t>59)
		return (BADTIME);
	    else 
		tim = (t * 60);
	if(abs_only)
	    return (BADTIME);
	return (tim);  /* good time */
    }  /* end +[HH:]MM format */
    
    /* look for absolute time format HH:MM */
    if (!isdigit(symbol[i]))
	return (BADTIME);
    
    while (isdigit(symbol[i])) {            /* get hours */
	t = t * 10 + symbol[i] - '0';
	i++;
    }
    
    if (symbol[i] == ':')
	i++;
    if (t > 23)
	return (BADTIME);
    t1 = t*60;   /* convert hours to minutes */
    t = 0;
    
    while (isdigit(symbol[i])) {            /* get minutes */
	t = t * 10 + symbol[i] - '0';
	i++;
    }
    
    if (t > 59)
	return (BADTIME);
    t1+= t; 			/* add minutes and hours */
    t1 *= 60;                       /* convert total to seconds */
    tim = time((time_t *)0);
    lt = localtime(&tim);
    t = (lt->tm_min*60) + (lt->tm_hour*3600); /* seconds past midnight */
    if (t>t1)	/* boot past midnight */
	tim = (24 * 3600) - t + t1;
    else
	tim =  t1 - t;
    if (symbol[i] != '\0')   /*extra junk BADTIME */
	return (BADTIME);
    return (tim); 
    /*NOTREACHED*/
} /* delay_time */
