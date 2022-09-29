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
 * 	Network administrator command format conversion routines.
 *
 * Original Author: %$(author)$%	Created on: %$(created-on)$%
 *
 * Revision Control Information:
 *
 * $Id: conv.c,v 1.3 1996/10/04 12:10:13 cwilson Exp $
 *
 * This file created by RCS from:
 *
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/na/RCS/conv.c,v $
 *
 * Revision History:
 *
 * $Log: conv.c,v $
 * Revision 1.3  1996/10/04 12:10:13  cwilson
 * latest rev
 *
 * Revision 2.170.102.7  1995/03/09  09:01:04  carlson
 * Removed mistaken inclusion of api_if.h.
 *
 * Revision 2.170.102.6  1995/01/11  15:52:34  carlson
 * SPR 3342 -- Removed TMUX (not supported until R9.3).
 *
 * Revision 2.170.102.5  1995/01/06  16:15:18  bullock
 * spr.3715 - Fix parsing of case CNV_GROUP_CODE type.  Reviewed by Russ L.
 *
 * Revision 2.170.102.4  1995/01/06  14:14:38  russ
 * The ascii to integer function was allowing non-digits.
 *
 * Revision 2.170.102.3  1994/12/09  10:53:51  russ
 * non-aock images need to report disabled modules correctly via admin.
 * I suppose this needs to include verbose-help.
 *
 * Revision 2.170.102.2  1994/10/27  09:28:49  geiser
 * Allow 'none' as a correct value for lat_queue_limit, change default value of
 * session_limit to 1152
 *
 * Revision 2.170.102.1  1994/09/21  17:00:40  sasson
 * SPR 3529: fixed some casting problems to keep the compiler happy.
 *
 * Revision 2.170  1994/07/15  15:51:52  carlson
 * SPR 3049 -- yet another merge problem; don't declare library routines
 * locally -- it upsets certain picky compilers!
 *
 * Revision 2.169  1994/07/12  19:50:03  sasson
 * Enabled 115K baud code.
 *
 * Revision 2.168  1994/07/11  13:51:07  geiser
 * Change spelling of 'pasthru'. Remove references to CNV_DUIFC
 *
 * Revision 2.167  1994/05/24  10:10:40  russ
 * Bigbird na/adm merge.
 *
 * Revision 2.159.2.8  1994/05/04  19:42:31  couto
 * Fixed smode_values to use the same offsets as async ports.  Also,
 *  temporary merge of 115.2Kbps support from common tree.
 *
 * Revision 2.159.2.7  1994/04/18  15:14:42  coffey
 * add 'default' case for ppp_ncps values
 *
 * Revision 2.159.2.6  1994/04/12  21:28:30  geiser
 * new conversion routines for DEC user interface parameters
 *
 * Revision 2.159.2.5  1994/03/07  13:09:02  russ
 * removed all X25 trunk code.
 *
 * Revision 2.159.2.4  1994/03/04  16:50:26  russ
 * minor fix to ipx string length error prompt.
 *
 * Revision 2.159.2.3  1994/02/22  17:34:32  russ
 * added auto_adapt port mode.
 *
 * Revision 2.159.2.2  1994/02/14  09:25:29  russ
 * Add IPX string cases; 700 is the default tmux_max_mpx value;
 * Add new port modes (ndp, auto_detect, ipx); Add chap-pap and pap
 * to ppp security case.
 *
 * Revision 2.159.2.1  1994/01/06  15:19:08  wang
 * Bigbird NA/Admin params support.
 *
 * Revision 2.159.1.1  1993/12/16  16:58:43  couto
 * Added support for 115.2Kbps
 *
 * Revision 2.166  1994/05/19  17:47:12  thuan
 * Change PM_MAX to 13 for new IPX port modes.
 * Fix buglet in encode() where a = was used instead of ==.
 *
 * Revision 2.165  1994/05/13  09:57:51  thuan
 * IPX merge attempt
 *
 * Revision 2.164  1994/02/24  11:10:02  sasson
 * Disabled 115.2 support in littleboy because hardware is not ready.
 *
 * Revision 2.163  1994/02/17  17:26:17  defina
 * *** empty log message ***
 *
 * Revision 2.162  1993/12/30  14:03:22  carlson
 * SPR 2084 -- made compatible with 64 bit machines.
 *
 * Revision 2.161  1993/12/14  10:54:46  bullock
 * spr.2293 - Correct places where ANX_MICRO was assumed to be XL AND
 * ELS but was really XL only.
 * Reviewed by Russ L. and Jim C.
 *
 * Revision 2.160  1993/12/07  11:51:01  carlson
 * "config.h" should not be included in admin builds!
 *
 * Revision 2.159.1.1  1993/12/16  16:58:43  couto
 * Added support for 115.2Kbps
 *
 * Revision 2.159.3.2  1994/04/20  11:40:18  dfox
 * Checked in for thuan
 *
 * Revision 2.159  1993/10/19  14:52:30  russ
 * Added *NOSTR* comments for AOCK pre-parser.
 *
 * Revision 2.158  1993/09/13  14:52:19  gmalkin
 * Modified some conversion routines to accept "default"
 *
 * Revision 2.157  1993/09/12  01:28:53  gmalkin
 * Fix to allow slip_mtu_size (and anything else using the large/small
 * conversion routine) to default to small
 *
 * Revision 2.156  1993/09/03  15:16:55  grant
 * Allowed appletalk zones to have embedded spaces.  Also added length and
 * value checking.  the default_zone_list is now stored as a series of
 * counted strings in E2.
 *
 * Revision 2.155  1993/08/27  15:23:38  carlson
 * SPR 1122 -- Added support for PAS_BOTH flow control.
 * Reviewed by P. Couto, M. Sasson and D. Wang.
 *
 * Revision 2.154  1993/07/14  23:06:28  raison
 * since circuit timer is one byte, we must limit its value to less than 256.
 *
 * Revision 2.153  93/05/20  17:59:11  carlson
 * SPR 1580 -- added SLIP and DNS on ELS for R7.1.
 * Reviewed by David Wang.
 * 
 * Revision 2.152  93/05/12  16:02:52  carlson
 * R7.1 -- Merged multiple string-length calls, removed references to
 * vselectable_mods and localized the verbose-help code.  Added ELS as a
 * supported hardware type.
 * 
 * Revision 2.151  93/05/10  17:16:04  wang
 * Removed atcp option from ppp_ncp parameter.
 * 
 * Revision 2.150  93/05/06  15:53:12  wang
 * Fixed spr1450, set net_inactivity param problem.
 * 
 * Revision 2.149  93/05/06  14:23:29  wang
 * Fixed the parser more robust in setting boolean values to NA parameters.
 * 
 * Revision 2.148  93/04/27  18:19:23  wang
 * Fixed Spr1494, the missing "r".
 * 
 * Revision 2.147  93/04/16  16:14:50  wang
 * Print more meaningful error message for mal-formatted IP address setting.
 * 
 * Revision 2.146  93/04/16  14:03:41  wang
 * Fixed spr1430, AppleTalk params convert routine problem.
 * 
 * Revision 2.145  93/04/01  17:21:32  carlson
 * Removed needless defines and added "port mode printer" for nprint.
 * 
 * Revision 2.144  93/03/30  10:58:06  raison
 * added decserver stuff (only to reserve fields and CNV_ types
 * 
 * Revision 2.143  93/03/17  18:38:45  wang
 * Use match function instead of strncmp for ppp_ncp param.
 * 
 * Revision 2.142  93/03/17  15:25:36  wang
 * Added encode routine for AppleTalk default_zone_list param.
 * 
 * Revision 2.141  93/03/08  12:44:37  barnes
 * move code used for encoding the appletalk node-id parameters into
 * a subroutine in conv2 to make sure it is accessible to snmp
 * 
 * Revision 2.140  93/03/03  13:13:35  wang
 * Changed AppleTalk at_nodeid from BYTE to LONG_CARDINAL_P to hold 
 * net and node value.
 * 
 * Revision 2.139  93/02/26  09:22:17  barnes
 * handle status returned from encode/decode rip_routers routines
 * now located in conv2.c
 * 
 * Revision 2.138  93/02/23  13:56:38  wang
 * Added 57600 into baudrate table.
 * 
 * Revision 2.137  93/02/18  21:45:11  raison
 * added DECserver Annex parameters
 * 
 * Revision 2.136  93/02/18  15:58:25  barnes
 * move encode and decode routines for rip_routers, rip_accept,
 * rip_advertise and this_net from conv.c to conv2.c so snmp can
 * use them 
 * 
 * Revision 2.135  93/02/09  13:14:06  wang
 * Fixed annex rip_routers problem due to different syntax from rip_accpet/rip_advertise. Removed include/exclude display.
 * 
 * Revision 2.134  93/02/08  15:52:34  wang
 * Change CNV_RIP_ROUTERS encode routine sharable by NA and ADMIN.
 * 
 * Revision 2.133  93/02/05  18:13:01  wang
 * Fixed the support for setting network routing list.
 * 
 * Revision 2.132  93/01/29  16:26:09  carlson
 * Overzealous use of ifdef/ifndef madness in last check-in.
 * 
 * Revision 2.131  93/01/28  17:18:40  carlson
 * SPR 654 -- removed many NA-only tests -- these need to be here for
 * Annex II support.
 * 
 * Revision 2.130  93/01/27  16:46:31  reeve
 * Removed port mode, xremote.
 * 
 * Revision 2.129  93/01/26  19:54:35  wang
 * Changed the encode and decode values for rip parameters.
 * 
 * Revision 2.128  93/01/26  16:44:13  wang
 * Changed some rip default parameter values to follow routed.h
 * 
 * Revision 2.127  93/01/22  18:17:34  wang
 * New parameters support for Rel8.0
 * 
 * Revision 2.126  92/12/10  16:44:38  bullock
 * spr.1228 - checking if unsigned shorts are less than 0 is a not portable
 * to all compilers.  Modification creates new int types to hold values for
 * checking against less than 0, and then recasts these to unsigned shorts
 * 
 * Revision 2.125  92/12/04  15:17:00  russ
 * Added CNV_INT5OFF for FORWARDING_TIMER, also fixed test for 
 * conversion == CNV_INACTCLI.
 * for assignment back to original u_short variables.
 * 
 * Revision 2.124  92/10/15  14:26:27  wang
 * Added new NA parameters for ARAP support.
 * 
 * Revision 2.123  92/08/09  14:19:27  emond
 * Removed a pile of rcslogs.
 * 
 * Revision 2.122  92/07/26  21:24:21  raison
 * allow verbose-help to be in disabled_modules list for non-AOCK,non-ELS
 * images.
 * 
 * Revision 2.121  92/06/12  14:13:25  barnes
 * small changes to change PAP to pap and add new values for port type
 * parameter
 * 
 * Revision 2.120  92/05/27  17:14:27  wang
 * Take out unneed debugging statement from NA read command.
 * 
 * Revision 2.119  92/05/27  15:22:16  wang
 * Don't accept none as legal range-of-value of group code.
 * 
 * Revision 2.118  92/05/21  16:48:28  reeve
 * Made inputting of PPP_acm easier; don't need to type all 8 chars.
 * I was just trying to avoid confusion, really!
 * 
 * Revision 2.117  92/05/11  16:32:43  sasson
 * Changed the default and the min.value for lat_queue_max.
 * New default is 4, new min. is 1.
 * 
 * Revision 2.116  92/05/09  17:16:08  raison
 * make all lat groups the same conversion type
 * 
 * Revision 2.115  92/04/29  12:58:49  raison
 * removed second config.h include file ref
 * 
 * Revision 2.114  92/04/22  21:14:56  raison
 * added default for selectable modules
 * 
 * Revision 2.113  92/04/21  17:46:05  bullock
 * Changes needed to compile code for SGI
 * 
 * Revision 2.112  92/04/16  13:44:43  barnes
 * correct translations of port parameter attn_char_string
 * 
 * Revision 2.111  92/04/14  17:21:35  barnes
 * move serv_options and selectable_mods structs to conv2.c
 * 
 * Revision 2.110  92/04/01  22:56:32  russ
 * Add new slave buffering command.
 * 
 * Revision 2.109  92/04/01  18:58:07  barnes
 * acc cli_edit to selectable modules
 * 
 * Revision 2.108  92/03/26  15:34:44  reeve
 * Took out value checking for port param, control_lines,
 * on V.11 platform.  It has been removed completely.
 * 
 * Revision 2.107  92/03/24  15:53:59  emond
 * Missed the ";" on an extern. 
 * 
 * Revision 2.106  92/03/24  14:58:36  emond
 * Fixed messy extern of ntoh and hton fucntions which cause problems
 * when compiling na on some hosts.
 * 
 * Revision 2.105  92/03/11  17:49:17  raison
 * range on syslog_port includes 0
 * 
 * Revision 2.104  92/03/11  17:07:44  russ
 * yet more nostr comments.
 * 
 * Revision 2.103  92/03/11  16:37:02  russ
 * Added nostr comments.
 * 
 * Revision 2.102  92/03/09  13:56:12  raison
 * added conversion types for timezonewest, network_turnaround, slip_metric,
 * and input_buffer_size.
 * 
 * Revision 2.101  92/03/07  13:03:50  raison
 * added CNV_PORT
 * 
 * Revision 2.100  92/02/28  16:26:00  bullock
 * spr.560 - Added parameter range checking
 * 
 * Revision 2.99 92/02/19 16:52:14 reeve
 * Disabled settings of port mode to ppp on an Annex II (or IIe).
 *
 * Revision 2.98 92/02/18 17:49:05 sasson
 * Added support for 3 LAT parameters (authorized group, vlci_group, lat_q_max). * 
 * Revision 2.97  92/02/07  09:44:31  barnes
 * cleanup:  remove duplicate defines for RCSDATE, RCSREV, RCSID
 * 
 * Revision 2.96  92/02/07  09:32:22  barnes
 * don't include selmods.h from operational code, instead use new
 * file inc/na/na_selmods.h
 * 
 * Revision 2.95  92/02/05  11:32:58  reeve
 * Defined local vars high and low to encode() routine.
 * 
 * Revision 2.94  92/02/05  11:09:33  reeve
 * Made 'Y' and 'N' aliases for 'ien_116' and 'none' for name_server
 * parameter on an ELS.
 * 
 * Revision 2.90  92/01/24  14:10:42  reeve
 * Use #ifndef NA on forward declarations where possible.
 *
 * Revision 2.89  92/01/24  13:27:24  reeve
 * Fixed up forward declaration of sprintf().
 *
 * Revision 2.88  92/01/23  09:57:37  carlson
 * SPR 484 -- inc/config.h must be included first.
 *
 * Revision 2.87  92/01/22  16:28:13  emond
 * Don't extern the conversion routines (ntohs, htonl, etc) if already macros.
 *
 * Revision 2.86  92/01/15  09:35:59  reeve
 * Fixes for lint warnings.
 *
 * Revision 2.85  92/01/10  14:58:05  reeve
 * For ELS, disabled setting of port mode to ppp and slip.  Also
 * disabled setting of name_server to dns or bind.
 *
 * Revision 2.85  92/01/02  12:42:55  barnes
 * move hex_digits and oct_digits from conv.c to conv2.c to eliminate
 *   dependency of conv2.c on conv.c
 * move syslog_levels from conv.c to conv2.c to eliminate dependency
 *   of snmp_anx_mib.c on conv.c
 * cleanup some of the @#$% compiler warning messages
 *
 * Revision 2.84  91/12/13  15:14:54  raison
 * oops, take out unneeded functions.
 * 
 * Revision 2.83  91/12/13  13:17:03  raison
 * add conversion routines for attn char (string)
 * 
 * Revision 2.82  91/12/11  08:30:11  russ
 * Changed the slip_mtu_size default to small.
 * 
 * Revision 2.81  91/12/10  12:27:38  carlson
 * SPR 446 -- made boolean value interpretation stricter.
 * 
 * Revision 2.80  91/12/09  16:26:49  reeve
 * Added proper error message for PPP security.
 * 
 * Revision 2.79  91/11/08  10:33:43  reeve
 * Put PPP_mru back to a short to save E2ROM space.
 * 
 * Revision 2.78  91/11/05  10:32:43  russ
 * Add slip_mtu_size to allow booting out of cslip ports.
 * 
 * Revision 2.77  91/11/04  11:34:40  carlson
 * Fixed type mismatches in RESET_IDLE, RBCAST and PTYPE conversion types --
 * caused SPR 381 problems.
 * 
 * Revision 2.76  91/10/30  15:42:26  raison
 * fixed load_dump_sequence value for CLI admin (was reversing order of first
 * two sequence values).
 * 
 * Revision 2.75  91/10/28  09:45:06  carlson
 * Changes for SPR 361 -- added string/char conversion routines.
 * 
 * Revision 2.74  91/10/22  15:17:16  reeve
 * Rectified automatic 1st and 2nd byte-switching for admin
 * on CNV_BML.
 * 
 * Revision 2.73  91/10/22  14:27:45  reeve
 * Changed PPP_MRU to be stored in a long word.
 * 
 * Revision 2.72  91/10/15  16:03:45  carlson
 * New autobaud functionallity for SPR 11 fix -- single bit for autobaud flag.
 * 
 * Revision 2.71  91/10/10  10:20:09  carlson
 * Remerged my 2.69 changes.
 * 
 * Revision 2.70  91/10/10  09:27:29  reeve
 * Added conversion routines for PPP:
 * CNV_BML - bit-mask (longword) for PPP_acm
 * CNV_SEC - for PPP_security, and CNV_MRU for PPP_mru.
 * 
 * Revision 2.69  91/09/30  10:25:52  carlson
 * Added extra array for DST options to fix spelling/grammar problems.
 * 
 * Revision 2.68  91/09/23  13:00:30  raison
 * fixed buffer overruns
 * 
 * Revision 2.67  91/09/19  12:35:31  emond
 * Had to change "index" varialbe to indx so it would not get confused
 * with macro index(). This was causing problems on Xenix machine.
 *
 *
 * Removed a pile of rcs logs from here.
 *
 * 
 * Revision 1.1  85/11/01  17:40:20  palmer
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *****************************************************************************
 */


/*
 *	Include Files
 */

#ifdef NA

/* This file must be first -- in the host NA only! */

#include "../inc/config.h"

#include "port/port.h"
#include <sys/types.h>
#include <stdio.h>
#include <netinet/in.h>
#include <netdb.h>
#include <strings.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <setjmp.h>
#include <ctype.h>
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
#include "externs.h"
#include "../machine/endian.h"
#endif

#include "../inc/na/na.h"
#include "../inc/na/iftype.h"
#include "../inc/na/server.h"
#include "../inc/na/na_selmods.h"
#include "../inc/syslog.h"
#include "../inc/erpc/netadmp.h"
/*
 *	Defines and Macros
 */

/*
 * Max # of elements, excluding NULL, of the arrays with corresponding prefix.
 * Shouldn\'t they be defined in term of sizeof(array_name) ?
 */
#define PS_MAX  22	/* port speed value index maximum */
#define TS_MAX  17	/* trunk speed value index maximum */
#define BPC_MAX 4	/* bits per character value index maximum */
#define SB_MAX  3	/* stop bits value index maximum */
#define P_MAX   3	/* parity value index maximum */
#define MC_MAX  4	/* modem control value index maximum */
#define PT_MAX  8	/* port type value index maximum */
#define PM_MAX  13	/* port mode value index maximum */
#define SEC_MAX 4	/* PPP security mode value index maximum */
#define FC_MAX  5	/* flow control value index maximum */
#define NS_MAX  4	/* name service value index maximum */
#define DLST_MAX 8	/* daylight savings time index max */
#define THIS_NET_RANGE_MAX  	0xfefe	/* this_net_range parameter maximum */

#define lower_case(x) 	(isalpha(x) ? (islower(x) ? x : tolower(x)) : x)
#define min(a, b)	(((a) > (b)) ? (b) : (a))

#define CHUNK_SZ	5

/*
 * TCP port definitions for dedicated ports.
 *
 * The first two (and all three on UMAX) should be obtained portably via
 * getservbyname(), but /etc/services is subject to local variation and
 * the Annex has fixed ideas as to what these ports are.
 */
#ifndef IPPORT_TELNET
#define IPPORT_TELNET		23
#endif
#ifndef IPPORT_LOGINSERVER
#define IPPORT_LOGINSERVER	513
#endif
#ifndef HRPPORT
#define HRPPORT			1027
#endif

#if NDPTG > 0
/*
 * Defines for the characters allowed in dptg_settings string
 */
#define	DPTG_LEN	16
#define	DPTG_SPLIT	8
#define	DPTG_CLI	"CcDE"
#define	DPTG_SLAVE	"SsTU"
#define	DPTG_ALL	"CcDESsTU"
#endif /* NDPTG */

/*
 *	External Routine Definitions
 */

extern UINT32
	inet_addr();
extern char 
	*inet_ntoa(); 
extern int
	print_to_c(), str_to_inet(), matchit(), matchflag(),
	encode_rip_routers(), encode_box_rip_routers(), encode_nodeid(),
	decode_rip_routers(), decode_box_rip_routers(), decode_kerberos_list(),
	encode_zone(), encode_def_zone_list(), encode_kerberos_list(),
	str_to_mop_passwd();
extern void
	c_to_print(), decode_sequence(), decode_mask(), trans_prompt();

extern int
	str_to_enet(),
#ifdef NA
        match(),
        match_flag();
#else
        matchit(),
        matchflag();
#endif

/*
 *	Forward Routine Definitions
 */

UINT32
	parse_sequence(), parse_list();
unsigned short 
	parse_scap();
int
	encode_range_ck();
static void
	decode_range_ck();

char	*lex_token();

/*
 *	Global Data Declarations
 */

#ifdef NA
static char *range_error = "number out of range, see Help: ";
#endif

/*
 * IMPORTANT: Do not forget to update the corresponding defines when
 * the arrays below are changed.
 */
static char   *ps_values[] =    /* port speed values */
    {
	/*NOSTR*/"default",
	/*NOSTR*/"50",
	/*NOSTR*/"75",
	/*NOSTR*/"110",
	/*NOSTR*/"134.5",
	/*NOSTR*/"150",
	/*NOSTR*/"200",
	/*NOSTR*/"300",
	/*NOSTR*/"600",
	/*NOSTR*/"1200",
	/*NOSTR*/"1800",
	/*NOSTR*/"2000",
	/*NOSTR*/"2400",
	/*NOSTR*/"3600",
	/*NOSTR*/"4800",
	/*NOSTR*/"7200",
	/*NOSTR*/"9600",
	/*NOSTR*/"19200",
	/*NOSTR*/"38400",
	/*NOSTR*/"57600",
	/*NOSTR*/"64000",
	/*NOSTR*/"76800",
	/*NOSTR*/"115200",
	(char *)NULL
    };
static char *bpc_values[] =    /* bits per character values */
    {
	"default",
	/*NOSTR*/"5",
	/*NOSTR*/"6",
	/*NOSTR*/"7",
	/*NOSTR*/"8",
	(char *)NULL
    };
static char   *sb_values[] =   /* stop bits values */
    {
	"default",
	/*NOSTR*/"1",
	/*NOSTR*/"1.5",
	/*NOSTR*/"2",
	(char *)NULL
    };
static char   *p_values[] =    /* parity values */
    {"default", "none", "even", "odd", (char *)NULL};
static char   *mc_eib_values[] =    /* eib modem control values */
    {"default", "none", "flow_control","modem_control","both", (char *)NULL};
static char   *pt_values[] =    /* port type values */
    {"default", "hardwired", "dial_in", "x.25", "3270", "pc", "terminal",
     "printer", "modem", (char *)NULL};
static char   *sec_values[] =    /* port security mode values */
    {"default", "none", "pap", "chap", "chap-pap", (char *)NULL};
static char   *pm_values[] =    /* port mode values */
/*	0	  1	  2	    3		4	5 */
    {"default", "cli", "slave", "adaptive", "unused", "slip",
/*	6	    7	   8	    9            10           11 */
     "dedicated", "ppp", "arap", "printer","auto_detect", "auto_adapt",
/*     12      13      */
     "ndp",  "ipx", (char *)NULL};

static char   *duipm_values[] =    /* port mode values */
    {"default", "local", "remote", "dynamic", "none", (char *)NULL};

static char   *sess_mode_values[] =    /* dui session mode value */
    {"default", "interactive", "pasthru", "passall", "transparent", (char *)NULL};

static char   *dui_values[] =    /* dui user interface type */
    {"default", "uci", "vci", (char *)NULL};

static char   *fc_values[] =    /* flow control values */
    {"default", "none", "eia", "start/stop", "bell", "both", (char *)NULL};

static char   *ns_values[] =    /* name server values */
    {"default", "none", "ien_116", "dns", "bind", (char *)NULL};

static char   *dlst_values[] =    /* Daylight savings values */
    {"default", "us", "australian", "west_european", "mid_european",
     "east_european", "canadian", "british", "none", (char *)NULL};
/* These are used for compatibility with earlier (bad) spelling */
static char   *dlst_values_bad[] =    /* Daylight savings values */
    {"default", "usa", "australian", "west_europe", "mid_europe",
     "east_europe", "canadian", "great_britian", "none", (char *)NULL};
static char   *sf_values[] =    /* syslog facility values */
    {"default", "log_local0", "log_local1", "log_local2", "log_local3",
     "log_local4", "log_local5", "log_local6", "log_local7", (char *)NULL};

static char *ht_values[] = /* host table size keywords */
    {"default","none","unlimited", (char *)NULL};

static char *ncp_values[] = /* ppp ncp values */
    {"default", "all","ipcp","atcp", (char *)NULL};

static char *ipxfmy_values[] = /* IPX Frame Type values */
    {"raw802_3", "ethernetII", "802_2", "802_2snap", (char*)NULL };

static char *boolean_values[] = /* boolean values */
    {"yes","true","enabled","no","false","disabled", (char *)NULL};

static char *ipso_values[] = /* ipso class values */
    {"none", "secret", "topsecret", "confidential", "unclassified", (char *)NULL};

/* NOTE: the nulls in the following table are needed to allow synchronous
 *  ports to make use of the same mode defines as asynchronous ports, so
 *  THESE SHOULD NOT BE CHANGED!
 */
static char *smode_values[] = /* Synchronous Mode values */
    {"", "", "", "", "unused", "", "", "ppp", (char *)NULL };

static char *sclock_values[] = /* Synchronous clocking values */
    {"external", "56000", "64000", "112000", "128000", (char *)NULL };

static char *crc_values[] = /* Synchronous CRC Types */
    {"crc16", "ccitt", (char *)NULL };


#define FAC_BASE (((LOG_LOCAL0) >> 3) - 1)
#define FAC_HIGH ((LOG_LOCAL7) >> 3)

#define PPP_MRU_MIN	64
#define PPP_MRU_MAX	1500
#define BML_LEN		8
#define NONE	1
#define ALL	2
#ifdef NA
static char   *ts_values[] =    /* trunk speed values */
    {
    "default", "150", "200", "300", "600", "1200", "1800", "2000",
    "2400", "3600", "4800", "7200", "9600", "19200", "38400", "48000",
    "56000", "64000", (char *)NULL
    };
#endif	/* NA */

extern char oct_digits[];
extern char hex_digits[];
extern struct mask_options syslog_levels[];
extern struct mask_options serv_options[];
extern struct mask_options selectable_mods[];

#define SYSLOG_ALL	0xff



/* Check for autobaud string.  This should be case-insensitive, but ... */
static int
is_autob(str,len)
char *str;
int len;
{
	if (len <= 0)
		return 0;
	return	strncmp(str,"AUTOBAUD",len)==0 ||
		strncmp(str,"autobaud",len)==0;
}

/*  Human to machine conversion */

#ifdef NA
void
#else
int
#endif
encode(conversion, external, internal, Pannex_id)

int  conversion;		/*  type of conversion to be performed	*/
char *external;			/*  external representation (for human) */
char *internal;			/*  internal representation (for Annex) */
ANNEX_ID *Pannex_id;		/*  Annex converting to			*/
{
	INTERN		Internal;	/*  union of pointers to types	*/
	int		length = 0,	/*  length of a string		*/
			max_length,	/*  maximum string length	*/
			err,		/*  error flag			*/
			flag,		/*  flag for cvn_ht		*/
			i, j,
			indx;		/*  index to array of (char *)	*/
	char		*ptr, c;
        int             high_int;
	u_char          low;
	int		Sinternal_int;
#ifdef NA
	u_short		group,
			disable = 0,
			enable = 0,
			loop;
	u_char		high;
	int		low_int;
#endif

	UINT32	 	sum,factor;
	u_char		num;

	num = err = 0;

	Cinternal = internal;	/*  move to pointer to various types	*/
	length = strlen(external);

	/*  Perform conversion from external to internal formats	*/


	switch(conversion)
	{
	    case CNV_PRINT:

		err = print_to_c(external, Sinternal);
		break;

	    case CNV_DFT_Y:
		num = 1;
	    case CNV_DFT_N:
		indx = match(external, boolean_values, "boolean value");
		/* No match in table */
		if (indx < 0) 
			punt("Invalid boolean value.",(char *)0);
		/*
		 * The first three entries in boolean_values indicate "YES"
		 */
		else if ( indx < 3 )
			*Sinternal = !num;
		else
			*Sinternal = num;
		break;

	    case CNV_BYTE:

		if (encode_range_ck(Sinternal, external, 1, 255, 1)) {
			punt(range_error, external);
		}
		break;

	    case CNV_BYTE_ZERO_OK:

		if (encode_range_ck(Sinternal, external, 0, 255, 0)) {
			punt(range_error, external);
		}
		break;

	    case CNV_INT:

		*Sinternal = (unsigned short)(atoi(external));
		break;

	    case CNV_PROMPT:

		trans_prompt(external);
		/* fall through to normal string processing */

	    case CNV_STRING:

		if (Pannex_id->hw_id < MIN_HW_FOR_LONG)
		    max_length = SHORTPARAM_LENGTH;
		else if (Pannex_id->hw_id == ANX3 ||
			 Pannex_id->hw_id == ANX_MICRO ||
			 Pannex_id->hw_id == ANX_MICRO_ELS)
		    max_length = MAX_STRING_100;
		else
		    max_length = LONGPARAM_LENGTH;

		if(length > max_length)
		{
		    printf(/*NOSTR*/"\t%s: %s %d %s.\n",
			   "Warning", "String parameter truncated to",
			   max_length, "characters");
		    length = max_length;
		}
		CS_length = length;
		(void)strncpy(CS_string, external, length);
		break;

	    case CNV_ATTN:
		if(Pannex_id->hw_id < MIN_HW_FOR_LONG)
		    max_length = SHORTPARAM_LENGTH;
		else if (Pannex_id->hw_id == ANX3 || 
			 Pannex_id->hw_id == ANX_MICRO ||
			 Pannex_id->hw_id == ANX_MICRO_ELS)
		    max_length = MAX_STRING_100;
		else
		    max_length = LONGPARAM_LENGTH;

		if(length > max_length)
		{
		    printf(/*NOSTR*/"\t%s: %s %d %s.\n",
			   "Warning", "String parameter truncated to",
			   max_length, "characters");
		    length = max_length;
		}

		for (i = 0, j = 0; i < length; i++, j++) {
		    if (external[i] == '^' && external[i+1] != '\0') {
		        if (external[i+1] == '?') 
			    CS_string[j] = RIP_LEN_MASK;
			else if (external[i+1] == '@')
			    CS_string[j] = 0x80;
		        else 
			    CS_string[j] = external[i+1] & 0x1f;
			i++;
		    } else {
		        if (external[i] == '\\' && external[i+1] != '\0')
			    i++;
			CS_string[j] = external[i];
		    }
		}
		CS_length = j;
		break;

#if NDPTG > 0
	    case CNV_DPTG:
		/*
		 * Special case a zero length string
		 */
		if(length == 0){
			CS_length = 0;
			(void)strcpy(CS_string, /*NOSTR*/"");
			break;
		}

		/*
		 * Otherwise we must have exactly DPTG_LEN characters
		 */
		if(length != DPTG_LEN){
			printf("\tString must be empty or of length %d",
								DPTG_LEN);
			punt((char *)NULL,(char *)NULL);
		}

		/*
		 * It's correct length, so check to see if
		 * the characters are acceptable ones.
		 */
		if(index(DPTG_CLI,external[0]) == (char *)NULL){
			punt("First character must be one of: ",DPTG_CLI);
		}

		for(indx = 1; indx < DPTG_SPLIT; indx++){
			if(index(DPTG_ALL,external[indx]) == (char *)NULL){
				printf("\tCharacter %d must be one of ",indx+1);
				punt((char *)NULL,DPTG_ALL);
			}
		}

		for(indx = DPTG_SPLIT; indx < DPTG_LEN; indx++){
			if(index(DPTG_SLAVE,external[indx]) == (char *)NULL){
				printf("\tCharacter %d must be one of ",indx+1);
				punt((char *)NULL,DPTG_SLAVE);
			}
		}

		CS_length = length;
		(void)strcpy(CS_string, external);
		break;
#endif /* NDPTG */

	    case CNV_FC:

		indx = match(external, fc_values, "flow_control value");
		if (indx < 0)
			err = -1;
		else if (
		    (Pannex_id->flag == ANX_MICRO_V11 &&
			(indx == 2 || indx == 5)) ||
		    ((Pannex_id->hw_id == ANX_II ||
		     Pannex_id->hw_id == ANX_II_EIB) &&
			indx == 5))
			punt("Invalid flow_control value", (char *)0);

		*Sinternal = (unsigned short)(indx);
		break;

            case CNV_SESS_MODE:

                indx = match(external, sess_mode_values, "session_mode value");                if (indx < 0)
                        err = -1;
                *Sinternal = (unsigned short)(indx);
                break;

            case CNV_USER_INTF:

                indx = match(external, dui_values, "user_interface value");
                if (indx < 0)
                        err = -1;
                *Sinternal = (unsigned short)(indx);
                break;

            case CNV_IPSO_CLASS:

                indx = match(external, ipso_values, "ipso_class value");
                if (indx < 0)
                        err = -1;
                *Sinternal = (unsigned short)(indx);
                break;

	    case CNV_IPX_FMTY:

                indx = match(external, ipxfmy_values, "ipx_frame_type value");
                if (indx < 0)
                        err = -1;
                *Sinternal = (unsigned short)(indx);
                break;

	    case CNV_SYNC_MODE:

                indx = match(external, smode_values, "mode value");
                if (indx < 0)
                        err = -1;
                *Sinternal = (unsigned short)(indx);
                break;

	    case CNV_SYNC_CLK:

                indx = match(external, sclock_values, "clocking value");
                if (indx < 0)
                        err = -1;
                *Sinternal = (unsigned short)(indx);
                break;

	    case CNV_CRC_TYPE:

                indx = match(external, crc_values, "crc_type value");
                if (indx < 0)
                        err = -1;
                *Sinternal = (unsigned short)(indx);
                break;

	    case CNV_BML:
		  
		if (external[0] == '0' && external[1] == 'x')
		  low = 2;
		else
		  low = 0; 
		if ((indx = length) - (int)low > BML_LEN)
                  punt("Incorrect format for PPP_acm bit mask.",(char *)0);
		for (factor = 1, sum = 0;
		     indx > (int)low;
		     indx--, factor *= 0x10) {
		  c = external[indx-1];
		  if (isxdigit(c)) {
		    if (isupper(c)) 
		      num = (u_char)c - (u_char)'A' + 10;
		    else if (islower(c))
		      num = (u_char)c - (u_char)'a' + 10;
		    else if (isdigit(c))
		      num = (u_char)c - (u_char)'0';
		    sum += factor * num; 
		  }
		  else
                    punt("Invalid character in PPP_acm bit mask.",(char *)0);
		}
		*Linternal = sum;
#ifndef NA
		*Linternal = htonl(*Linternal);
		*Sinternal = htons(*Sinternal);  /* gets switched back below */
#endif
		break;

	    case CNV_NET_Z:

		err = str_to_inet(external, Linternal, TRUE, 1);
		if (err)
		    punt("invalid parameter value: ", external);
#ifndef NA
		*Sinternal = htons(*Sinternal);	/* gets switch back below */
#endif
		break;


	    case CNV_NET:

		err = str_to_inet(external, Linternal, FALSE, 1);
		if (err)
		    punt("invalid parameter value: ", external);
#ifndef NA
		*Sinternal = htons(*Sinternal);	/* gets switch back below */
#endif
		break;

            case CNV_ENET_ADDR:

                err = str_to_enet(external, Linternal);
		if (err)
		    punt("invalid parameter value: ", external);
#ifndef NA
                *Sinternal = htons(*Sinternal); /* gets switch back below */
#endif
                break;

	    case CNV_MOP_PASSWD:

		err = str_to_mop_passwd(external, Linternal);
		if (err)
		    punt("invalid parameter value: ", external);
#ifndef NA
		*Sinternal = htons(*Sinternal); /* gets switch back below */
#endif
		break;

	    case CNV_PS:

		flag = 0;
		ptr = index(external,'/');
		if (ptr != (char *)NULL)
			if (is_autob(external,ptr-external)) {
				flag = 0x80;
				ptr++;
				}
			else if (is_autob(ptr+1,strlen(ptr+1))) {
				flag = 0x80;
				*ptr = '\0';
				ptr = external;
				}
			else
				punt("invalid speed value: ",external);
		else if (is_autob(external,length)) {
			*Sinternal = (unsigned short)0xff;
			break;
			}
		else
			ptr = external;

		indx = match(ptr, ps_values, "speed value");
		if (indx < 0)
			err = -1;
		*Sinternal = (unsigned short)(indx | flag);
		break;

	    case CNV_BPC:

		indx = match(external, bpc_values, "data_bits value");
		if (indx < 0)
			err = -1;
		*Sinternal = (unsigned short)(indx);
		break;

	    case CNV_SB:

		indx = match(external, sb_values, "stop_bits value");
		if (indx < 0)
			err = -1;
		*Sinternal = (unsigned short)(indx);
		break;

	    case CNV_P:

		indx = match(external, p_values, "parity value");
		if (indx < 0)
			err = -1;
		*Sinternal = (unsigned short)(indx);
		break;

	    case CNV_MC:
		indx = match(external,mc_eib_values,
			"control_lines value");
		if (indx == 4 &&
		    Pannex_id->hw_id != ANX_II_EIB &&
		    Pannex_id->hw_id != ANX3 &&
		    Pannex_id->hw_id != ANX_MICRO &&
		    Pannex_id->hw_id != ANX_MICRO_ELS)
			punt("invalid control_lines value: ",external);

		if (indx < 0)
			err = -1;
		*Sinternal = (unsigned short)(indx);
		break;

	    case CNV_PT:

		indx = match(external, pt_values, "type value");
		if (indx < 0) 
			err = -1;
		*Sinternal = (unsigned short)(indx);
		break;

	    case CNV_PM:

		indx = match(external, pm_values, "mode value");

#ifndef NA
                /* if not in pm_values, check duipm_values */
                if (indx < 0) {
                    indx = match(external, duipm_values, "access value");
                }
#endif

		if (indx == 7 && /* Check for platforms without PPP */
		    (Pannex_id->hw_id == ANX_II ||
		     Pannex_id->hw_id == ANX_II_EIB ||
		     Pannex_id->hw_id == ANX_MICRO_ELS))
			punt("invalid mode value: ",external);

		if (indx < 0)
			err = -1;
		*Sinternal = (unsigned short)(indx);
		break;

            case CNV_MRU:

		high_int = atoi(external);

                if (high_int >= PPP_MRU_MIN && high_int <= PPP_MRU_MAX)
		  *Sinternal = (u_short) high_int;
                else {
	          err = -1;
		  punt("invalid value for mru.",(char *)0);
		}
		break;

            case CNV_SEC:

		indx = match(external, sec_values, "ppp_security_protocol value");
		if (indx < 0)
			err = -1;
		*Sinternal = (unsigned short)(indx);
		break;

	    case CNV_NS:
		indx = match(external, ns_values, "name server value");
		/* Map "bind" to "dns". */
		if (indx == 4)
		    indx = 3;
		else if (indx < 0)
		    err = -1;
		*Sinternal = (unsigned short)(indx);
		break;

	    case CNV_PORT:
		if (encode_range_ck(Sinternal, external,
					0, Pannex_id->port_count, 0))
			punt(range_error, external);
		break;

	    case CNV_SMETRIC:

                if (encode_range_ck(Sinternal, external,
                                        1, 15, 1))
                        punt(range_error, external);
                break;

	    case CNV_HT:

#ifdef NA
		if(Pannex_id->version < VERS_5) {
		    if (isdigit(*external)) {
			*Sinternal = (unsigned short)(atoi(external) / 2);
		    }
		    else {
			punt("invalid non-numeric value: ", external);
		    }
		}
		else
#endif
		{
		    if (isdigit(*external)) {
			if (encode_range_ck(Sinternal, external, 1, 250, 64))
			    punt(range_error, external);
		    }
		    else { 
			indx = match(external, ht_values, "host table value");
			if (indx < 0)
				err = -1;
			switch (indx) {
			    case 1:		/*none*/
				*Sinternal = (unsigned short)HTAB_NONE;
				break;
			    case 2:		/*unlimited*/
				*Sinternal = (unsigned short)HTAB_UNLIMITED;
				break;
			    case 0:		/*default*/
			    default:
			    	*Sinternal = (unsigned short)0;
				break;
			}
		    }
		}
		break;

	    case CNV_MS:

                if (!strcasecmp(external,"none")) {
                        *Sinternal = 16;
                } else if (encode_range_ck(Sinternal, external, 1, 16, 3)) {
			punt(range_error, external);
		}

		break;

	    case CNV_SEQ:

		*Linternal = parse_sequence(external, Pannex_id->port_count);
		if (*Linternal == (UINT32) -1)
			err = -1;
#ifndef NA
		*Sinternal = htons(*Sinternal);  /* gets switched back below */
#endif
		break;

	    case CNV_IPENCAP:

		indx = match_flag(external, "ethernet", "ieee802",
			   "IP encapsulation type", 0);
		if (indx < 0)
			err = -1;
		*Sinternal = (unsigned short)(indx);
		break;

	    case CNV_RNGPRI:

		Sinternal_int = atoi(external);
		if ((Sinternal_int < 0) || (Sinternal_int > 3))
			punt ("invalid priority selection, see Help",
				(char *)0);
		*Sinternal = (u_short)Sinternal_int;
		break;

	    case CNV_SCAP:

		if(!strcmp(external,"none"))
			*Sinternal = 0;
		else if(!strcmp(external,"all"))
			*Sinternal = SERVE_ALL;
		else {
			*Sinternal = (unsigned short) parse_list(external,
						serv_options);
			if (*Sinternal == (unsigned short) -1)
			    err = -1;
		}
		break;

	    case CNV_SELECTEDMODS:

                if (!strcmp(external,"none"))
                    *Sinternal = OPT_SELEC_NONE;
                else if (!strcmp(external,"all"))
                    *Sinternal = (unsigned short)OPT_ALL;
                else if (!strcmp(external,"default"))
                    *Sinternal = (unsigned short) OPT_DEFAULT;
#ifdef NA
		else if (Pannex_id->vhelp == 1 && length > 0 &&
			strncmp(external,"verbose-help",length) == 0)
                    *Sinternal = (unsigned short) OPT_VERBOSEHELP;
#else
#ifndef AOCK
		else if (length > 0 &&
			 strncmp(external,"verbose-help",length) == 0)
		    *Sinternal = (unsigned short) OPT_VERBOSEHELP;
#endif
#endif
		else {
		    *Sinternal = (unsigned short) parse_list(external,
				selectable_mods);

		    if (*Sinternal == (unsigned short) -1)
		    		err = -1;
		}
		break;

	    case CNV_SYSLOG:

		if(isdigit(*external))
			*Sinternal = (unsigned short)atoi(external);
		else if(!strcmp(external,"none"))
			*Sinternal = 0;
		else if(!strcmp(external,"default"))
			*Sinternal = 0;
		else if(!strcmp(external,"all"))
			*Sinternal = SYSLOG_ALL;
		else {
			*Sinternal = (unsigned short) parse_list(external,
						syslog_levels);
			if (*Sinternal == (unsigned short) -1)
			    err = -1;
		}
		break;

	    case CNV_SYSFAC:

		if(isdigit(*external)) {
			if (encode_range_ck(Sinternal, external, 0, 255, -1)) {
				punt(range_error, external);
			}
		} else if(!strcmp(external,"default")) {
			*Sinternal = (unsigned short)-1;
		} else {
			indx = match(external, sf_values,
				    "syslog facility code");
		        if (indx < 0)
		        	err = -1;
			*Sinternal = (unsigned short)(indx + FAC_BASE);
		}

		(*Sinternal)++;
		break;

	    case CNV_VCLILIM:

		if(isdigit(*external)) {
			*Sinternal = (unsigned short)atoi(external);
			if(*Sinternal > 254)
				punt("invalid max_vcli: ", external);
			if(*Sinternal == 0)
				*Sinternal = 255;
		}
		else if(!strncmp(external, "default", length)
		     || !strncmp(external, "unlimited", length))
			*Sinternal = 0;
		else
			punt("invalid max_vcli: ", external);
		break;

	    case CNV_DLST:

		indx = match(external, dlst_values, NULL);
		if (indx < 0) {
			indx = match(external, dlst_values_bad, "Daylight Savings Time");
			if (indx < 0)
				err = -1;
			}
#ifdef NA
		if(Pannex_id->version < VERS_5 && indx >= DLST_MAX) {
		    if (!strncmp(external, "great_britian", length) ||
		        !strncmp(external, "british", length)) {
			punt("invalid option on annex software: ", external);
		    }
		    else {
			indx = DLST_MAX - 1;
		    }
		}
#endif
		*Sinternal = (unsigned short)(indx);
		break;

	    case CNV_NET_TURN:

		if (encode_range_ck(Sinternal, external, 1, 10, 0))
			punt(range_error, external);
		break;

	    case CNV_TZ_MIN:
		if (encode_range_ck(Sinternal, external, -720, 720, 0))
			punt(range_error, external);
		(*Sinternal)++;
		break;

	    case CNV_ZERO_OK:

		if((isdigit(*external)) ||
		            (*external == '-' && isdigit(external[1])))
			*Sinternal = (unsigned short)(atoi(external) + 1);
		else
		{
			if(!strncmp(external,"default", length))
			    *Sinternal = 0;
			else
			    punt("invalid value: ", external);
		}
		break;

	    case CNV_RESET_IDLE:

		indx = match_flag(external, "input", "output",
			   "reset idle time", 0);
		if (indx < 0)
			err = -1;
		*Sinternal = (unsigned short)indx;
		break;

	    case CNV_DPORT:
		if(!strncmp(external, "default", length) ||
		   !strncmp(external, "telnet", length))
		    *Sinternal = 0;
		else if(!strncmp(external, "rlogin", length) ||
			!strncmp(external, "login", length))
		    *Sinternal = IPPORT_LOGINSERVER;
		else if(!strncmp(external, "call", length) ||
			!strncmp(external, "mls", length))
		    *Sinternal = HRPPORT;
		else
		{
		    *Sinternal = atoi(external);
		    if(!*Sinternal && strncmp(external, /*NOSTR*/"0", length))
			punt("invalid TCP port: ", external);
		}
		break;

	    case CNV_INT0OFF:
	    case CNV_INT5OFF:
	    case CNV_INACTCLI:

		if (isdigit(*external)) {
			*Sinternal = (unsigned short)atoi(external);
			if (*Sinternal > 255)
				punt("invalid timer value: ", external);
		} else {
			if (!strncmp(external, "off", length))
				*Sinternal = 0;
			else
			if (conversion == CNV_INACTCLI &&
			    !strncmp(external, "immediate", length))
				*Sinternal = 255;
			else
				punt("invalid timer value: ", external);
		}
		break;

	    case CNV_INACTDUI:
		if (encode_range_ck(Sinternal, external, 1, 255, 30))
		    punt(range_error, external);
		break;

	    case CNV_RBCAST:
		indx = match_flag(external, "port","network",
					"broadcast direction value",0);
		if (indx < 0)
			err = -1;
		*Sinternal = (unsigned short)indx;
		break;

	    case CNV_PTYPE:
		indx = match_flag(external, "centronics", "dataproducts",
				   "printer type value", 0);
		if (indx < 0)
			err = -1;
		*Sinternal = (unsigned short)indx;
		break;
		
	    case CNV_PSPEED:
		indx = match_flag(external, "normal", "high_speed",
				"printer speed value", 0);
		if (indx < 0)
			err = -1;
		*Sinternal = (unsigned short) indx;
		break;

	    case CNV_HOST_NUMBER:

		if (encode_range_ck(Sinternal, external, 0, 32767, 42))
		    punt(range_error, external);
		break;

	    case CNV_SERVICE_LIMIT:

		if (encode_range_ck(Sinternal, external, 16, 2048, 256))
		    punt(range_error, external);
		break;

	    case CNV_KA_TIMER:

		if (encode_range_ck(Sinternal, external, 10, 255, 20))
		    punt(range_error, external);
		break;

            case CNV_MULTI_TIMER:

                if (encode_range_ck(Sinternal, external, 10, 180, 30))
                    punt(range_error, external);
                break;

	    case CNV_CIRCUIT_TIMER:

		if (encode_range_ck(Sinternal, external, 1, 25, 8))
		    punt(range_error, external);
		break;

	    case CNV_RETRANS_LIMIT:

		if (encode_range_ck(Sinternal, external, 4, 120, 8))
		    punt(range_error, external);
		break;

	    case CNV_QUEUE_MAX:

                if (!strcasecmp(external,"none")) {
                        *Sinternal = 255;
		} else {
		    if (encode_range_ck(Sinternal, external, 1, 255, 4))
		        punt(range_error, external);
		}
		break;

            case CNV_PASSLIM:

		if (!strcmp(external,"none")) {
		    *Sinternal = (unsigned short) 10;
                } else {
		    if (encode_range_ck(Sinternal, external, 0, 10, 3))
                        punt(range_error, external);
		}
                break;

	    case CNV_ADM_STRING:

		if(length > MAX_ADM_STRING)
		{
		    printf(/*NOSTR*/"\t%s: %s %d %s.\n",
			   "Warning", "String parameter truncated to",
			   MAX_ADM_STRING, "characters");
		    length = MAX_ADM_STRING;
		}
		CS_length = length;
		(void)strncpy(CS_string, external, length);
		break;

	    case CNV_IPX_STRING:

		if(length > MAX_IPX_STRING)
		{
		    printf(/*NOSTR*/"\t%s: %s %d %s.\n",
			   "Warning", "String parameter truncated to",
			   MAX_IPX_STRING, "characters");
		    length = MAX_IPX_STRING;
		}
		CS_length = length;
		(void)strncpy(CS_string, external, length);
		break;

	    case CNV_LG_SML:

		if(!strncmp(external, "SMALL", length) ||
		   !strncmp(external, "small", length) ||
		   !strncmp(external, "default", length)) {
		    *Sinternal = (unsigned short)(0x0);
		}
		else {
		    if(!strncmp(external, "LARGE", length) ||
		       !strncmp(external, "large", length)) {
			*Sinternal = (unsigned short)(0x1);
		    }
		    else {
			err = -1;
			punt("invalid size value: ", external);
		    }
		}
		break;

#ifdef NA
	    case CNV_GROUP_CODE:
{	
		u_short shortcut = FALSE;	
   		eos = 0;
		bzero(internal, LAT_GROUP_SZ);
		Psymbol = external;
		(void)lex();
		while (!eos) {

			if (enable || disable) {
				punt ("command syntax error, see Help",
						(char *)0);
			}

			if (strncmp(symbol, "enable", strlen(symbol)) == 0) {
                                if (shortcut == NONE)
			           disable = 1;
                                else
				   enable=1;
                                shortcut = FALSE;
				(void)lex();
				continue;
			}

			if (strncmp(symbol, "disable", strlen(symbol)) == 0) {
				if (shortcut == NONE)
                                   enable = 1;
                                else
				   disable=1;
                                shortcut = FALSE;
				(void)lex();
				continue;
			}

			if (symbol[0] == ',') {
				(void)lex();
				continue;
			}

			if (strcmp(symbol, ALL_STR) == 0) {
				(void)lex();
				for(group = 0; group < LAT_GROUP_SZ; group++) {
				    internal[group] = 0xff;
                                }
                                shortcut = ALL;

			} else if (strcmp(symbol, NONE_STR) == 0) {
				(void)lex();
				for(group = 0; group < LAT_GROUP_SZ; group++) {
				    internal[group] = 0xff;
                                }
                                shortcut = NONE;
			} else {
				low_int = atoi(symbol);
				if ((low_int < 0) || (low_int > 255)) {
					punt ("number out of range, see Help",
						(char *)0);
				}
				low = (u_short)low_int;
				(void)lex();

				if (symbol[0] == '-') {

				    (void)lex();
				    high_int = atoi(symbol);
				    if ((high_int < 0) || (high_int > 255)) {
					punt ("number out of range, see Help",
						(char *)0);
				    }
				    high = (u_short)high_int;
				    (void)lex();

				    for (loop = low; loop <= high; loop++) {
					SETGROUPBIT(internal,loop);
				    }
				} else {
				    SETGROUPBIT(internal,low);
				}
			}
		}
                if (shortcut )
		   if (shortcut == NONE)
			disable = TRUE;
		   else
			enable = TRUE;
		if (!enable && !disable) {
			punt ("command syntax error - format of command is:\n set <ann | port>  <param name> <all | none | group range> <enable | disable>\n see Help", (char *)0);
		}
		if (enable) {
			internal[LAT_GROUP_SZ] = TRUE;
		} else {
			internal[LAT_GROUP_SZ] = FALSE;
		}

		break;
}
#endif

	    case CNV_HIST_BUFF:

		if (encode_range_ck(Sinternal, external, 0, 32767, 0))
		    punt(range_error, external);
		break;

	    case CNV_PPP_NCP:

		indx = match(external, ncp_values, "ppp interface type");
		if (indx < 0) 
		    punt("invalid interface type: ", external);
		*Sinternal = (unsigned short)(indx);
		break;

	    case CNV_ARAP_AUTH:

		if (!strncmp(external, "none", length) ||
		    !strncmp(external, "NONE", length))
		    *Sinternal = (unsigned short)(1);

		else if (!strncmp(external, "des", length) ||
		         !strncmp(external, "DES", length))
		    *Sinternal = (unsigned short)(2);

		else 
		    punt("invalid parameter value : ", external);

		break;

	    case CNV_A_BYTE:
		if (encode_range_ck(Sinternal, external, 0, 253, 0))
		    punt(range_error, external);
		break;

	    case CNV_ZONE:
		if ((err = encode_zone( external, internal)) != 0)
		  if (err == -1)
		    punt("Zone name too long", (char *)0);
                  else
		    punt("Invalid character in this parameter : ",external);
                break;
                 
		
	    case CNV_DEF_ZONE_LIST:

		err = encode_def_zone_list(external, internal);
		switch (err) {
		    case -1:
			punt("Default zone list too long", (char *)0);
			break;
		    case -2:
			punt("Invalid character in this parameter : ",external);
			break;
		    case -3:
			punt("Default zone name too long", (char *)0);
			break;
		    default:
			break;
		}
		break;

	    case CNV_THIS_NET_RANGE:
		err = encode_nodeid(external, internal);
		switch (err) {
		    case -1:
			punt("Incorrect format for this parameter: ", external);
			break;
		    case -2:
			punt("Invalid character in this parameter: ", external);
			break;
		    case -3:
			punt("Invalid range in this parameter: ", external);
			break;
		    default:
			break;
		}

#ifndef NA
		*Linternal = htonl(*Linternal);
		*Sinternal = htons(*Sinternal);  /* gets switched back below */
#endif
		break;

	    case CNV_RIP_ROUTERS:

		err = encode_rip_routers(external, internal);
		if (err == 1)
		    punt("invalid parameter value: ", external);
		else if (err == -1)
		    punt("invalid syntax: ", external);
		else if (err != 0)
		    punt("error while parsing router list: ", external);
		break;

	    case CNV_RIP_SEND_VERSION:

		if(isdigit(*external)) {
			*Sinternal = (unsigned short)atoi(external);
			if (*Sinternal == 1 )
		    		*Sinternal = (unsigned short)(2);
			else if (*Sinternal == 2 )
		    		*Sinternal = (unsigned short)(4);
			else			
		    		punt("invalid parameter value: ", external);
			break;
		}
		else if (!strncmp(external, "compatibility", length) ||
			 !strncmp(external, "COMPATIBILITY", length) ||
			 !strncmp(external, "default", length)) {
		    	*Sinternal = (unsigned short)(3);
		    	break;
		}
		else				
		    	punt("invalid parameter value: ", external);

		break;

	    case CNV_RIP_RECV_VERSION:

		if (!strncmp(external, "both", length) ||
		    !strncmp(external, "BOTH", length) ||
		    !strncmp(external, "default", length)) {
		    	*Sinternal = (unsigned short)(3);
			break;
		}

		*Sinternal = (unsigned short)(atoi(external));
		if (*Sinternal < 1 || *Sinternal > 2)
		    punt("invalid parameter value: ", external);

		break;

	    case CNV_RIP_HORIZON:

		if (!strncmp(external, "poison", length) ||
		    !strncmp(external, "POISON", length) ||
		    !strncmp(external, "default", length))
		    *Sinternal = (unsigned short)(3);

		else if (!strncmp(external, "off", length) ||
		    !strncmp(external, "OFF", length))
		    *Sinternal = (unsigned short)(1);

		else if (!strncmp(external, "split", length) ||
		    !strncmp(external, "", length))
		    *Sinternal = (unsigned short)(2);

		else 
		    punt("invalid parameter value : ", external);
		break;

	    case CNV_RIP_DEFAULT_ROUTE:

		if (!strncmp(external, "off", length) ||
		    !strncmp(external, "OFF", length) ||
		    !strncmp(external, "default", length)) {
		    	*Sinternal = (unsigned short)(0);
			break;
		}

		if (encode_range_ck(Sinternal, external, 0, 15, 0)) 
			punt(range_error, external);
		break;

	    case CNV_RIP_OVERRIDE_DEF:

		if (!strncmp(external, "none", length) ||
		    !strncmp(external, "NONE", length) ||
		    !strncmp(external, "default", length)) {
		    	*Sinternal = (unsigned short)(0);
			break;
		}

		if (encode_range_ck(Sinternal, external, 0, 15, 0)) 
			punt(range_error, external);

		break;

	    case CNV_PASS_LIM:

		if (encode_range_ck(Sinternal, external, 1, 10, 3)) 
			punt(range_error, external);
		break;

	    case CNV_TIMER:

		if (encode_range_ck(Sinternal, external, 1, 60, 30)) 
			punt(range_error, external);
		break;

	    case CNV_RIP_DOMAIN:

		if (encode_range_ck(Sinternal, external, 0, 65535, 0))
		    punt(range_error, external);
		break;

	    case CNV_SESS_LIM:

		if (!strncmp(external, "none", length) ){
		    	*Sinternal = (unsigned short)(0);
			break;
		}
		if( encode_range_ck(Sinternal, external, 1, ALL_PORTS*16, 1152))
			punt( range_error, external );
		break;

	    case CNV_BOX_RIP_ROUTERS:

		err = encode_box_rip_routers(external, internal);
		if (err == 1)
		    punt("invalid parameter value: ", external);
		else if (err == -1)
		    punt("invalid syntax: ", external);
		else if (err != 0)
		    punt("error while parsing router list: ", external);
		break;

	    case CNV_KERB_HOST:

                err = encode_kerberos_list(external, internal);
                if (err == 1)
                    punt("invalid parameter value: ", external);
                else if (err == -1)
                    punt("invalid syntax: ", external);
                else if (err != 0)
                    punt("error while parsing kerberos list: ", external);
                break;

	    default:

		*Linternal = (UINT32)(0);
	}
#ifndef NA
	*Sinternal = htons(*Sinternal);
	return(err);
#else
	return;	/* void */
#endif

}	/*  encode()  */

/* Machine to human conversion   */

void
decode(conversion, internal, external,Pannex_id)

int  conversion;		/*  type of conversion to be performed	*/
char *internal;			/*  internal representation (for Annex) */
char *external;			/*  external representation (for human) */
ANNEX_ID *Pannex_id;		/*  Annex converting from		*/
{
	char		*ptr;
	INTERN		Internal;	/*  union of pointers to types	*/
	int		length,		/*  length of a string		*/
			byte, err,
			lastone = LAT_GROUP_SZ * NBBY, /* initially large */
			span = FALSE,
			i, j = 0,
			bit;
	short		num;
	u_short		net_value;
	u_short		node_value;  	
	UINT32	 	sum,sum2;
	Cinternal = internal;	/*  move to pointer to various types	*/

	
	/*  Perform conversion from internal to external formats	*/

#ifndef NA
	*Sinternal = ntohs(*Sinternal);		/* assume we need a change */
#endif

	switch(conversion)
	{
	    case CNV_PRINT:

		c_to_print(*Sinternal, external);
		break;

	    case CNV_DFT_N:

		external[0] = (*Sinternal ? 'Y' : 'N');
		external[1] = 0;
		break;

	    case CNV_DFT_Y:

		external[0] = (*Sinternal ? 'N' : 'Y');
		external[1] = 0;
		break;

	    case CNV_MOP_PASSWD:

	    /* this is a special case */
	    /* it is not a string, but a string */
	    /* (<set> or <unset>) is returned */

	    case CNV_STRING:
	    case CNV_ZONE:
	    case CNV_DPTG:
		length = CS_length;

		if ((length > LONGPARAM_LENGTH) &&
		    (Pannex_id->hw_id < ANX3))
		{
		    sprintf(external, /*NOSTR*/"%s (%d)", "Exceeded length",
			    LONGPARAM_LENGTH);
		}
		else
		{
		    if ((length > MAX_STRING_100) && 
			((Pannex_id->hw_id == ANX3 || 
				Pannex_id->hw_id == ANX_MICRO ||
			        Pannex_id->hw_id == ANX_MICRO_ELS)))
		        sprintf(external,/*NOSTR*/"%s (%d)", "Exceeded length",
				MAX_STRING_100);
		    else
			{
			external[0] = '\"';
			(void)strncpy(&external[1], CS_string, length);
			external[length + 1] = '\"';
			external[length + 2] = 0;
			}
		}
		break;

	    case CNV_DEF_ZONE_LIST:

		length = CS_length;

		if ((length > LONGPARAM_LENGTH) &&
		    (Pannex_id->hw_id < ANX3))
		{
		    sprintf(external, /*NOSTR*/"%s (%d)", "Exceeded length",
			    LONGPARAM_LENGTH);
		}
		else
		{
		    if ((length > MAX_STRING_100) && 
			((Pannex_id->hw_id == ANX3 || 
				Pannex_id->hw_id == ANX_MICRO ||
			        Pannex_id->hw_id == ANX_MICRO_ELS)))
		        sprintf(external,/*NOSTR*/"%s (%d)", "Exceeded length",
				MAX_STRING_100);
		    else
			{
                        char *in_ptr, *out_ptr;
                        int i =0, err =0, zone_len;

                        in_ptr = CS_string;
                        out_ptr = external;
			*out_ptr++ = '\"';
			while (i<length) {
                          zone_len = *in_ptr++; /* first bytes is a zone len */
                          if (zone_len > 32 || zone_len <= 0 || 
                              (i+zone_len+1 > length)) {
                            sprintf(external,/*NOSTR*/"%s %d",
				    "Invalid zone length", zone_len);
                            err = 1;
                            break;
                          }
                          i += (zone_len +1);
                          while( zone_len-- ) {
			    if (*in_ptr == ',')
			      *out_ptr++ = '\\';
                            *out_ptr++ = *in_ptr++;
                          }
                          if (i < length) /* another zone is comming */
			    *out_ptr++ = ',';
		        }
              		if (!err) {
			  *out_ptr++ = '\"';
			  *out_ptr =  0;
			}
                        }
		}
		break;

	    case CNV_ATTN:

		length = CS_length;
		if ((length > LONGPARAM_LENGTH) && (Pannex_id->hw_id < ANX3))
		{
		    sprintf(external,  /*NOSTR*/"%s (%d)", "Exceeded length", 
			    LONGPARAM_LENGTH);
		}
		else
		{
		    if ((length > MAX_STRING_100) && 
			((Pannex_id->hw_id == ANX3 || 
				Pannex_id->hw_id == ANX_MICRO)))
		        sprintf(external,/*NOSTR*/"%s (%d)", "Exceeded length",
				MAX_STRING_100);
		    else {
			external[0] = '\"';
		        for (i = 0, j = 1; i < length; i++, j++) {
			    if (CS_string[i] < ' ') {
			        external[j++] = '^';
			        external[j] = CS_string[i] + 0x40;
		            } else if (CS_string[i] == RIP_LEN_MASK) {
			        external[j++] = '^';
			        external[j] = '?';
			    } else if ((u_char)CS_string[i] == 
							(u_char)0x80) {
				external[j++] = '^';
				external[j] = '@';
		            } else {
				external[j++] = '\\';
				external[j] = CS_string[i];
			    }
		        }
		    }
			external[j++] = '\"';
			external[j] = 0;
		}

		break;

	    case CNV_FC:

		decode_range_ck(*Sinternal, external, fc_values, 1, FC_MAX);
		break;

            case CNV_SESS_MODE:

                decode_range_ck(*Sinternal,external,sess_mode_values,1,4);
                break;

            case CNV_USER_INTF:

                decode_range_ck(*Sinternal, external, dui_values, 1, 2);
                break;

	    case CNV_IPSO_CLASS:

                decode_range_ck(*Sinternal, external, ipso_values, 0, 4);
                break;

	    case CNV_IPX_FMTY:

                decode_range_ck(*Sinternal, external, ipxfmy_values, 0, 3);
                break;


            case CNV_SYNC_MODE:

                decode_range_ck(*Sinternal, external, smode_values, 0, 7);
                break;

            case CNV_SYNC_CLK:

                decode_range_ck(*Sinternal, external, sclock_values, 0, 4);
                break;

            case CNV_CRC_TYPE:

                decode_range_ck(*Sinternal, external, crc_values, 0, 1);
                break;

	    /*  CNV_NETZ and CNV_NET are identical when decoding  */

	    case CNV_NET_Z:
	    case CNV_NET:

#ifdef NA
		(void)strcpy(external, inet_ntoa(*Ninternal));
#else
		*Sinternal = ntohs(*Sinternal);	/* we need a change back */
		inet_ntoa(external, *Ninternal);
#endif
		break;

            case CNV_ENET_ADDR:

#ifndef NA
                *Sinternal = ntohs(*Sinternal); /* change back */
#endif
                (void)sprintf(external,/*NOSTR*/"%02x-%02x-%02x-%02x-%02x-%02x",
                        (u_char) internal[0], (u_char) internal[1],
                        (u_char) internal[2], (u_char) internal[3],
                        (u_char) internal[4], (u_char) internal[5]);
                break;

	    case CNV_BML:
#ifndef NA
		*Sinternal = ntohs(*Sinternal); /* we need a change back */
		*Linternal = ntohl(*Linternal);
#endif
		sprintf(external, /*NOSTR*/"0x%x",*Linternal);
		break;

	    case CNV_PS:

		if (*Sinternal == (unsigned short)0xff)
			(void)strcpy(external, "autobaud");
		else {
			external[0] = '\0';
			if (*Sinternal & (unsigned short)0x80)
				(void)strcpy(external,"autobaud/");
			decode_range_ck(*Sinternal&0x7F,
				external+strlen(external),
				ps_values, 1, PS_MAX);
			}
		break;

	    case CNV_BPC:

		decode_range_ck(*Sinternal, external, bpc_values, 1, BPC_MAX);
		break;

	    case CNV_SB:

		decode_range_ck(*Sinternal, external, sb_values, 1, SB_MAX);
		break;

	    case CNV_P:

		decode_range_ck(*Sinternal, external, p_values, 1, P_MAX);
		break;

	    case CNV_MC:

		decode_range_ck(*Sinternal, external, mc_eib_values, 1,
			(Pannex_id->hw_id == ANX_II_EIB ||
			 Pannex_id->hw_id == ANX3 ||
			 Pannex_id->hw_id == ANX_MICRO ||
			 Pannex_id->hw_id == ANX_MICRO_ELS) ?
				MC_MAX : (MC_MAX-1));
		break;

	    case CNV_PT:

		decode_range_ck(*Sinternal, external, pt_values, 1, PT_MAX);
		break;

	    case CNV_MRU:

                if ((num = *Sinternal) >= PPP_MRU_MIN &&
                    (num <= PPP_MRU_MAX) )
                  sprintf(external,/*NOSTR*/"%d",num);
                else 
                  *external = '\0'; 
		break;

	    case CNV_SEC:
		decode_range_ck(*Sinternal, external, sec_values, 1, SEC_MAX);
		break;

	    case CNV_PM:
		if (*Sinternal == 7 && /* platforms without PPP */
		    (Pannex_id->hw_id == ANX_II ||
		     Pannex_id->hw_id == ANX_II_EIB ||
		     Pannex_id->hw_id == ANX_MICRO_ELS))
			strcpy(external,/*NOSTR*/"?");
		else
			decode_range_ck(*Sinternal, external, pm_values,
				1, PM_MAX);
		break;

	    case CNV_NS:
		decode_range_ck(*Sinternal,external,ns_values,1,NS_MAX);
		break;

	    case CNV_HT:

#ifdef NA
		if(Pannex_id->version < VERS_5)
			sprintf(external, /*NOSTR*/"%d", *Sinternal * 2);
		else
#endif
			if (*Sinternal == HTAB_NONE)
				sprintf(external, /*NOSTR*/"%s", "none");
			else 
				if (*Sinternal == HTAB_UNLIMITED)
					sprintf(external, /*NOSTR*/"%s",
						"unlimited");
				else
					sprintf(external,/*NOSTR*/"%d",
						*Sinternal);
		break;

	    case CNV_SEQ:

#ifndef NA
		*Sinternal = ntohs(*Sinternal);	/* we need a change back */
#endif
		decode_sequence(external, *Linternal);
		break;

	    case CNV_IPENCAP:

		sprintf(external, /*NOSTR*/"%s", *Sinternal ? "ieee802" :
			"ethernet");
		break;

	    case CNV_SCAP:

		if(*Sinternal == 0)
			(void)strcpy(external, "none");
		else if (*Sinternal == SERVE_ALL)
			(void)strcpy(external, "all");
		else
			decode_mask(external, *Sinternal, serv_options);
		break;

	    case CNV_SELECTEDMODS:

		if (*Sinternal == OPT_SELEC_NONE)
		    (void)strcpy(external, "none");
		else if (*Sinternal == (unsigned short)OPT_ALL)
		    (void)strcpy(external, "all");
		else {
		    decode_mask(external, *Sinternal, selectable_mods);
#ifdef NA
		    if (Pannex_id->vhelp == 1 &&
			(*Sinternal & OPT_VERBOSEHELP) != 0) {
			if (*external != '\0')
			    strcat(external,/*NOSTR*/",");
			strcat(external,"verbose-help");
		    }
#else
#ifndef AOCK
		    if ((*Sinternal & OPT_VERBOSEHELP) != 0) {
			if (*external != '\0')
			    strcat(external,/*NOSTR*/",");
			strcat(external,"verbose-help");
		    }
#endif
#endif
		}
		break;

	    case CNV_SYSLOG:

		if(*Sinternal == 0)
			(void)strcpy(external, "none");
		else if (*Sinternal == SYSLOG_ALL)
			(void)strcpy(external, "all");
		else
			decode_mask(external, *Sinternal, syslog_levels);
		break;

	    case CNV_SYSFAC:

		(*Sinternal)--;
		if(   *Sinternal <= (u_short)FAC_BASE
		   || *Sinternal >  (u_short)FAC_HIGH)
		    sprintf(external, /*NOSTR*/"%d", *Sinternal);
		else
		    (void)strcpy(external, sf_values[*Sinternal - FAC_BASE]); 
		break;

	    case CNV_VCLILIM:

		if(*Sinternal == 0)
			(void)strcpy(external, "unlimited");
		else if(*Sinternal == 255)
			(void)strcpy(external, /*NOSTR*/"0");
		else
			sprintf(external, /*NOSTR*/"%d", *Sinternal);
		break;

	    case CNV_DLST:

#ifdef NA
		if (Pannex_id->version < VERS_5 &&
		    *Sinternal == DLST_MAX - 1)
			*Sinternal = DLST_MAX;
#endif
		decode_range_ck(*Sinternal, external, dlst_values, 1, DLST_MAX);
		break;


	    case CNV_TZ_MIN:
	    case CNV_ZERO_OK:

		sprintf(external, /*NOSTR*/"%d", (short)*Sinternal - 1);
		break;

	    case CNV_RESET_IDLE:

		sprintf(external, /*NOSTR*/"%s", *Sinternal ? "output" :
			"input");
		break;

	    case CNV_PROMPT:

		length = CS_length;

		if(length > LONGPARAM_LENGTH)
		{
		    sprintf(external,  /*NOSTR*/"%s (%d)", "Exceeded length", 
			    LONGPARAM_LENGTH);
		}
		else
		{
		    char *s = CS_string;
		    char *t = external;

		    *t++ = '\"';
		    while(length--) {
			if(*(unsigned char *)s < ' ') {
			    *t++ = '%';
			    *t++ = *s + 'a' - 1;
			    }
			else if(*s == '%') {
			    *t++ = '%';
			    *t++ = '%';
			    }
			else
			    *t++ = *s;
			++s;
			}
		    *t++ = '\"';
		    *t = '\0';
		}
		break;

	    case CNV_DPORT:
		switch(*Sinternal)
		{
		    case HRPPORT:		/* mls ("hrp") port, "call" */
			strcpy(external, "call");
			break;

		    case IPPORT_LOGINSERVER:	/* login port, "rlogin" */
			strcpy(external, "rlogin");
			break;

		    case IPPORT_TELNET:		/* "telnet" */
		    case 0:
			strcpy(external, "telnet");
			break;

		    default:
			sprintf(external, /*NOSTR*/"%d", *Sinternal);
			break;
		}
		break;

	    case CNV_INT0OFF:
	    case CNV_INT5OFF:
	    case CNV_INACTCLI:
	    case CNV_INACTDUI:

		if (conversion == CNV_INACTCLI && *Sinternal == 255)
			(void)strcpy(external, "immediate");
		else if(*Sinternal == 0 ||
			(*Sinternal == 5 && conversion == CNV_INT5OFF))
			(void)strcpy(external, "off");
		else
			sprintf(external, /*NOSTR*/"%d", *Sinternal);
		break;

	    case CNV_RBCAST:
		
		if (*Sinternal)
			(void)strcpy(external,"network");
		else
			(void)strcpy(external,"port");
		break;

	    case CNV_PTYPE:
		
		sprintf(external, /*NOSTR*/"%s", *Sinternal ? "dataproducts"
						   : "centronics");
		break;
		
	    case CNV_PSPEED:
		
		sprintf(external, /*NOSTR*/"%s", *Sinternal ? "high_speed"
						   : "normal");
		break;
		
	    case CNV_INT:
	    case CNV_RNGPRI:
	    case CNV_MS:
	    case CNV_HOST_NUMBER:
	    case CNV_SERVICE_LIMIT:
	    case CNV_KA_TIMER:
            case CNV_MULTI_TIMER:
            case CNV_PASSLIM:
	    case CNV_CIRCUIT_TIMER:
	    case CNV_RETRANS_LIMIT:
	    case CNV_QUEUE_MAX:
	    case CNV_PORT:
	    case CNV_NET_TURN:
	    case CNV_BYTE:
	    case CNV_BYTE_ZERO_OK:
	    case CNV_HIST_BUFF:
	    case CNV_A_BYTE:
	    case CNV_RIP_DOMAIN:
	    case CNV_PASS_LIM:
	    case CNV_TIMER:
	    case CNV_SMETRIC:

		sprintf(external, /*NOSTR*/"%d", *Sinternal);
		break;

	    case CNV_THIS_NET_RANGE:
#ifndef NA
		*Sinternal = ntohs(*Sinternal); /* we need a change back */
		*Linternal = ntohl(*Linternal);
#endif
		sum = *Linternal;
		sum2 = sum;
		/*
		 * See encode mechanism for datail.
		 */
		net_value = (unsigned short)(sum >> 16);
		node_value = (unsigned short)(sum2 & 0x0000ffff);
		sprintf(external, /*NOSTR*/"%d.%d",net_value, node_value);
		break;

	    case CNV_ADM_STRING:

		length = CS_length;

		if (length > MAX_ADM_STRING) {
			sprintf(external,/*NOSTR*/"%s (%d)", "Exceeded length",
				MAX_ADM_STRING);
		} else {
			external[0] = '\"';
			(void)strncpy(&external[1], CS_string, length);
			external[length + 1] = '\"';
			external[length + 2] = 0;
		}
		break;

	    case CNV_IPX_STRING:
		length = CS_length;
		if (length > MAX_IPX_STRING) {
			sprintf(external,/*NOSTR*/"%s (%d)", "Exceeded length",
				MAX_IPX_STRING);
		} else {
			external[0] = '\"';
			(void)strncpy(&external[1], CS_string, length);
			external[length + 1] = '\"';
			external[length + 2] = 0;
		}
		break;

	    case CNV_GROUP_CODE:
#ifndef NA
		*Sinternal = ntohs(*Sinternal);	/* change it back */
#endif
		external[0] = '\0';
		ptr = Cinternal;
		for (byte = 0; byte < LAT_GROUP_SZ; byte++) {
		    for (bit = 0; bit < NBBY; bit++) {
			length = strlen(external);
			if (*ptr & 0x01) {
			    if ((lastone + 1) == (byte * NBBY) + bit) {
				span = TRUE;
			    } else {
			        sprintf(&external[length], /*NOSTR*/"%d,",
				        (byte * NBBY) + bit);
			    }
			    lastone = (byte * NBBY) + bit;
			} else {
			    if (span) {
				length--;
			        sprintf(&external[length], /*NOSTR*/"-%d,",
					lastone);
			    }
			    span = FALSE;
			}
			*ptr >>= 1;
		    }
		    ptr++;
		}
		if (span) {
		    length--;
		    sprintf(&external[length], /*NOSTR*/"-%d", lastone);
		}
		length = strlen(external);
		if (length) {
		    if (external[length - 1] == ',') {
		        external[length - 1] = '\0';
		    }
		    if (!strcmp(external, /*NOSTR*/ ALL_LAT_GROUPS)) {
			strcpy(external, ALL_STR);
		    }
		} else {
		    strcpy(external, NONE_STR);
		}
		break;

	    case CNV_LG_SML:

		if (*Sinternal)
			(void)strcpy(external,"large");
		else
			(void)strcpy(external,"small");
		break;

	    case CNV_PPP_NCP:

		decode_range_ck(*Sinternal, external, ncp_values, 0, 2);
		break;

	    case CNV_ARAP_AUTH:

		if (*Sinternal == (unsigned short)1)
		    (void)strcpy(external, "none");
		else if (*Sinternal == (unsigned short)2)
		    (void)strcpy(external, "des");
		break;

	    case CNV_SESS_LIM:

		if( *Sinternal == (unsigned short)0 ){
			(void)strcpy( external, "none" );
		}
		else{
			sprintf(external, /*NOSTR*/"%d", *Sinternal);
		}
		break;

	    case CNV_RIP_ROUTERS:

		err = decode_rip_routers(internal, external);
		if (err)
		    punt("bad value read from eeprom ", (char *)0);
		break;
		
	    case CNV_RIP_SEND_VERSION:

		if (*Sinternal == (unsigned short)3) {
		    	(void)strcpy(external, "compatibility");
			break;
		}
		else if (*Sinternal == (unsigned short)4)
		 	*Sinternal = (unsigned short)2; 

		else if (*Sinternal == (unsigned short)2)
		 	*Sinternal = (unsigned short)1;
		
		sprintf(external, /*NOSTR*/"%d", *Sinternal);
		break;

	    case CNV_RIP_RECV_VERSION:

		if (*Sinternal == (unsigned short)3)
		    	(void)strcpy(external, "both");
		else
			sprintf(external, /*NOSTR*/"%d", *Sinternal);
		break;

	    case CNV_RIP_HORIZON:

		if (*Sinternal == (unsigned short)3)
		    (void)strcpy(external, "poison");

		else if (*Sinternal == (unsigned short)1)
		    (void)strcpy(external, "off");

		else if (*Sinternal == (unsigned short)2)
		    (void)strcpy(external, "split");
		break;

	    case CNV_RIP_DEFAULT_ROUTE:

		if (*Sinternal == (unsigned short)0)
		    	(void)strcpy(external, "off");
		else
			sprintf(external, /*NOSTR*/"%d", *Sinternal);
		break;

	    case CNV_RIP_OVERRIDE_DEF:

		if (*Sinternal == (unsigned short)0)
		    	(void)strcpy(external, "none");
		else
			sprintf(external, /*NOSTR*/"%d", *Sinternal);
		break;

	    case CNV_BOX_RIP_ROUTERS:

		err = decode_box_rip_routers(internal, external);
		if (err)
		    punt("bad value read from eeprom ", (char *)0);
		break;

	    case CNV_KERB_HOST:

		err = decode_kerberos_list(internal, external);
                if (err)
                    punt("bad value read from eeprom ", (char *)0);
                break;


	    default:

		sprintf(external, /*NOSTR*/"%s (%d)", "Bad conversion code",
			conversion);
	}
	return;	/* void */

}	/*  decode()  */

int
encode_range_ck(to, from, lo, hi, def)
unsigned short *to;
char *from;
int lo, hi, def;
{
    int val;

    if (!strncmp(from, "default", strlen(from))) {
        *to = (unsigned short) def;
    } else {

        val = strlen(from);
        while (val-- > 0) {
            if (!(isdigit(from[val])))
                return(-1);
        }

	val = atoi(from);
        *to = (unsigned short) val;

        if (val < lo || val > hi) {
            return(-1);
        }
    }
    return(0);
}

static void
decode_range_ck(indx, external, table, lo, hi)
unsigned short indx;
char *external;
char *table[];
unsigned short lo, hi;
{

if (indx < lo || indx > hi)
    *external = '\0';
else
    strcpy(external, table[indx]);

return;
}
