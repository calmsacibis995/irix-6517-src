/*
 *****************************************************************************
 *
 *        Copyright 1989, Xylogics, Inc.  ALL RIGHTS RESERVED.
 *
 * ALL RIGHTS RESERVED. Licensed Material - Property of Xylogics, Inc.
 * This software is made available solely pursuant to the terms of a
 * software license agreement which governs its use.
 * Unauthorized duplication, distribution or sale are strictly prohibited.
 *
 * Include file description:
 *	%$(description)$%
 *
 * Original Author: %$(author)$%	Created on: %$(created-on)$%
 *
 * Revision Control Information:
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/inc/na/RCS/na.h,v 1.2 1996/10/04 12:04:35 cwilson Exp $
 *
 * This file created by RCS from:
 *
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/inc/na/RCS/na.h,v $
 *
 * Revision History:
 *
 * $Log: na.h,v $
 * Revision 1.2  1996/10/04 12:04:35  cwilson
 * latest rev
 *
 * Revision 3.66.102.4  1995/02/23  14:53:21  carlson
 * Added patches for supporting 64 bit systems.
 *
 * Revision 3.66.102.3  1995/01/11  15:54:37  carlson
 * SPR 3342 -- Removed TMUX (not supported until R9.3).
 *
 * Revision 3.66.102.2  1994/10/10  16:23:42  russ
 * Increase MAXVALUE to 1024 to allow long lat group_values to be output.
 *
 * Revision 3.66.102.1  1994/09/21  10:22:33  bullock
 * spr.3537 - Change V_7_1_DEC mask so it includes version 7.1 and 9.0 only
 * Reviewed by Russ L.
 *
 * Revision 3.66  1994/08/26  10:19:24  geiser
 * Turn on NDEC
 *
 * Revision 3.65  1994/07/13  10:38:24  bullock
 * spr.2951 - changed MAXVALUE - maximum length of string needed to be
 * alot bigger than 32.  It's now 150 which takes care of rip_adv &
 * rip_acc max. lengths
 *
 * Revision 3.64  1994/07/11  13:50:07  geiser
 * mark CNV_DUIFC as unused. This value can be resued when a new conversion is needed.
 *
 * Revision 3.63  1994/05/23  14:58:25  russ
 * Bigbird na/adm merge.
 *
 * Revision 3.62  1994/05/18  17:03:44  russ
 * disable NPRONET_FOUR forever and remove bogus RESET defines.
 *
 * Revision 3.61  1994/02/22  17:34:29  sasson
 * Added R8.1 version.
 *
 * Revision 3.60  1993/09/20  14:20:15  reeve
 * Aded flags to mark option key settings.  Reviewed by David Wang. [SPR 1738]
 *
 * Revision 3.59  1993/09/03  15:20:36  grant
 * Added new convert routine CNV_ZONE
 *
 * Revision 3.58  1993/07/30  10:52:23  carlson
 * SPR 1923 -- ripped out silly outp variable completely.
 *
 * Revision 3.57  93/07/27  20:23:48  wang
 * na/admin interface redesign changes.
 * 
 * Revision 3.56  1993/05/20  16:29:27  wang
 * Removed some defines to new file. Reviewed by Jim Carlson.
 *
 * Revision 3.55  93/05/12  17:39:00  carlson
 * Added arap and clitn3270 as normal selectable modules.
 * 
 * Revision 3.54  93/05/12  16:17:55  carlson
 * R7.1 -- Added hardware type number for ELS, and added dec.h and
 * cmusnmp.h as configurable modules.
 * 
 * Revision 3.53  93/04/19  17:27:04  raison
 * added R7.1 version
 * 
 * Revision 3.52  93/03/30  11:05:42  raison
 * reserve parm ids for decserver and add them, turned off
 * 
 * Revision 3.51  93/03/24  15:08:18  wang
 * Added in some new defines.
 * 
 * Revision 3.50  93/03/17  15:23:49  wang
 * Added encode define for AppleTalk default_zone_list param.
 * 
 * Revision 3.49  93/03/01  18:44:04  wang
 * Support "show port serial" group params.
 * 
 * Revision 3.48  93/02/26  07:33:05  barnes
 * add new defines for processing rip address list parameters
 * 
 * Revision 3.47  93/02/24  12:58:59  carlson
 * Added pager definitions for new init and stop functions and
 * integrated "outp" pointer.
 * 
 * Revision 3.46  93/02/20  13:45:33  wang
 * Added interface parameters related defines.
 * 
 * Revision 3.45  93/02/18  21:51:18  raison
 * added DECserver Annex parameters
 * 
 * Revision 3.44  93/02/18  15:57:11  barnes
 * move some defines and structure typedefs from na/conv.c 
 * 
 * Revision 3.43  93/02/09  13:10:51  wang
 * Created a new type for box rip_routers.
 * 
 * Revision 3.42  93/01/22  17:47:16  wang
 * New parameters support for Rel8.0
 * 
 * Revision 3.41  92/12/04  15:15:24  russ
 * Added conversion for forwarding timer.
 * 
 * Revision 3.40  92/10/30  14:34:00  russ
 * updated all V_N_N revision range defines for 8.0
 * 
 * Revision 3.39  92/10/15  14:05:52  wang
 * 
 * 
 * Added new NA parameters for ARAP support.
 * 
 * Revision 3.38  92/08/14  15:19:45  reeve
 * Made Annex IIE platform flag backward compatible for operational images.
 * 
 * Revision 3.37  92/08/03  15:31:59  reeve
 * Put DPTG back to 1.  Version mask will biff post(sic) 6.2 images.
 * 
 * Revision 3.36  92/08/03  14:10:54  reeve
 * Turned off DPTG parameters for NA.
 * 
 * Revision 3.35  92/07/26  21:27:49  raison
 * allow verbose-help in disabled_modules list for non-AOCK,non-ELS images
 * 
 * Revision 3.34  92/07/21  12:15:56  reeve
 * Incorporated the item NTFTP_PROTO in order to determine if
 * TFTP params belong.
 * 
 * Revision 3.33  92/05/09  17:18:34  raison
 * make all lat groups the same conversion type
 * 
 * Revision 3.32  92/04/17  16:37:59  carlson
 * Removed warnings from link editors that don't like to see
 * benign redefinition.
 * 
 * Revision 3.31  92/04/01  22:22:26  russ
 * Add new slave buffering param.
 * 
 * Revision 3.30  92/03/09  13:58:53  raison
 * added conversion types for timezonewest, network_turnaround, slip_metric,
 * and input_buffer_size.
 * 
 * Revision 3.29  92/03/07  13:05:16  raison
 * added CNV_PORT, changed syslog_port to CNV_PORT type
 * 
 * Revision 3.28  92/03/03  15:13:07  reeve
 * Added 802.5 annex type.
 * 
 * Revision 3.27  92/02/18  17:46:54  sasson
 *  Added support for 3 LAT parameters(authorized_group, vcli_group, lat_q_max).
 * 
 * Revision 3.26  92/02/13  10:58:30  raison
 * added check for self boot option
 * 
 * Revision 3.25  92/02/07  16:01:19  carlson
 * Included slip.h and lat.h.
 * 
 * Revision 3.24  92/02/03  10:45:26  carlson
 * Added LONG_FILENAME error message -- keep it close to its constant.
 * 
 * Revision 3.23  92/01/31  22:53:32  raison
 * added printer_set and printer_speed parm
 *
 * Revision 3.22  92/01/31  13:35:28  reeve
 * Added extra field to ANNEX_ID struct to further diffentiate 
 * 
 * Revision 3.21  92/01/27  19:00:20  barnes
 * changes for selectable modules
 * 
 * Revision 3.20  91/12/20  15:42:50  raison
 * added flash as a new type of load_dump_seq
 *
 * Revision 3.19  91/12/13  13:20:46  raison
 * added new conversion type for attn_char (string)
 * 
 * Revision 3.18  91/12/11  17:54:48  carlson
 * Added & merged define/include code.
 * 
 * Revision 3.17  91/11/22  12:34:10  russ
 * Add version 6.2 defines.
 * 
 * Revision 3.16  91/11/05  10:30:26  russ
 * Add slip_mtu_size to allow booting out of cslip ports.
 * 
 * Revision 3.15  91/10/15  11:32:34  russ
 * added a real s/w version 7.0.
 * 
 * Revision 3.14  91/10/14  15:54:12  russ
 * Added much shortened defines for version fields which will make it much
 * easier to add new s/w revisions in the future (you only need to change
 * the define values rather than add a new define all over the place.
 * 	Also, added a define for using stdout as the output file if the
 * pager code is not enabled.
 * 
 * Revision 3.13  91/10/04  15:02:05  reeve
 * Added definitions of convert routines for PPP parameters.
 * 
 * Revision 3.12  91/09/24  15:41:56  emond
 * Changed u_char and u_long to unsigned char and unsigned long. Got
 * errors on Sequent DYNIX.
 * 
 * Revision 3.11  91/08/01  10:22:00  dajer
 * Added status variable for exiting na.
 * 
 * Revision 3.10  91/06/13  12:19:20  raison
 * ifdef'ed out unneccessary structs, strings, and globals for cli admin
 * 
 * Revision 3.9  91/04/02  09:15:50  russ
 * Added Micro annex support.
 * 
 * Revision 3.8  91/03/07  16:20:44  pjc
 * Added CNV_DPTG conversion type for dptg_settings param.
 * 
 * Revision 3.7  91/02/12  14:04:49  raison
 * added define for NBBY for those users that donot have NBBY (later we may have
 * to change to CHAR_BIT to conform to X/OPEN).
 * 
 * Revision 3.6  91/01/24  13:29:27  raison
 * fixed na write command for group_value.
 * 
 * Revision 3.5  91/01/23  21:51:48  raison
 * added lat_value na parameter and GROUP_CODE parameter type.
 * 
 * Revision 3.4  90/12/14  14:48:58  raison
 * add LAT defines for CNV functions.
 * 
 * Revision 3.3  90/12/04  12:29:00  sasson
 * The image_name was increased to 100 characters on annex3.
 * 
 * Revision 3.2  90/08/16  11:26:13  grant
 * 64 port support
 * 
 * Revision 3.1  90/04/11  14:51:27  grant
 * Added Annex3 hw id and VERS_6.
 * 
 * Revision 2.34  90/04/03  15:26:46  emond
 * Added some defines that used to be in conv.c but since conv.c had to
 * be split the defines had to go into a common include file which both
 * (conv.c conv2.c) could use.
 * 
 * Revision 2.33  89/11/20  16:17:35  grant
 * added broadcast_dir and printer type conversion.
 * 
 * Revision 2.32  89/11/01  15:43:37  grant
 * Added both option to AnnexIIe control_lines param.
 * 
 * Revision 2.31  89/11/01  13:09:52  russ
 * reset macro code.
 * 
 * Revision 2.30  89/08/21  15:07:21  grant
 * Looks for new host table size and reset name server parameter.
 * 
 * Revision 2.29  89/06/15  16:36:41  townsend
 * Added support for trunks
 * 
 * Revision 2.28  89/05/18  10:03:40  grant
 * Added defines to support boot shutdown options.
 * 
 * Revision 2.27  89/05/11  14:10:45  maron
 * Modified this module to support the new security parameters, vcli_password,
 * vlci_sec_ena, and port_password.
 * 
 * Revision 2.26  89/04/05  12:42:06  loverso
 * Changed copyright notice
 * 
 * Revision 2.25  89/01/12  16:41:43  loverso
 * allocate new conv routine
 * 
 * Revision 2.24  89/01/12  14:43:28  loverso
 * Allocate new conversion routine CNV_INACTCLI
 * 
 * Revision 2.23  88/09/20  09:08:01  townsend
 * Added CNV_RNGPRI and VERS_4_1 defines
 * 
 * Revision 2.22  88/05/16  12:13:51  mattes
 * Added dedicated-port conversion
 * 
 * Revision 2.21  88/05/04  23:02:00  harris
 * ALL_PORTS as an unsigned long is more lint-worthy.
 * 
 * Revision 2.20  88/04/07  16:11:13  emond
 * Accommodate for new bit parameter, IP encapsulation type
 * 
 * Revision 2.19  88/03/15  14:37:36  mattes
 * Added constants for new port_list scheme
 * 
 * Revision 2.18  88/03/14  16:52:46  parker
 * Added params for activity and idle timers
 * 
 * Revision 2.17  88/03/07  12:28:22  mattes
 * Added VCLI conversion
 * 
 * Revision 2.16  88/03/02  14:32:17  parker
 * added syslog and time parameters
 * 
 * Revision 2.15  88/01/19  09:56:39  mattes
 * Added boot server parameter type
 * 
 * Revision 2.14  87/12/29  11:45:28  mattes
 * Added 4.0 SL/IP parameters
 * 
 * Revision 2.13  87/09/28  11:52:09  mattes
 * Generalized box name
 * 
 * Revision 2.12  87/08/20  10:43:05  mattes
 * Added 32 port support
 * 
 * Revision 2.11  87/08/12  15:34:03  mattes
 * Added new products (Annex II, TIU) and a "machine type" dimension to the support bitmask array
 * 
 * Revision 2.10  87/07/31  11:47:57  parker
 * added support for r3.0 s/w
 * 
 * Revision 2.9  87/06/10  18:11:43  parker
 * Added support for Xenix/Excelan PC.
 * 
 * Revision 2.8  86/12/22  17:12:34  parker
 * added new name server parms
 * 
 * Revision 2.7  86/12/08  12:17:38  parker
 * R2.1 NA parameter changes
 * 
 * Revision 2.6  86/07/03  15:10:25  harris
 * Added definitions for new tables which describe commands, parameters, and
 * help entries.  These tables are initialized in cmd.c.
 * 
 * Revision 2.5  86/06/16  09:36:18  parker
 * added do_leap and allow_broadcast parmamters
 * 
 * Revision 2.4  86/05/07  11:00:06  goodmon
 * Added broadcast command.
 * 
 * Revision 2.3  86/03/07  14:25:05  goodmon
 * Made # a PUNCTUATION character so that it doesn't have to be followed by
 * a separator to indicate a comment.
 * 
 * Revision 2.2  86/02/27  19:07:55  goodmon
 * Standardized string lengths.
 * 
 * Revision 2.1  86/02/26  11:47:51  goodmon
 * Changed configuration files to script files.
 * 
 * Revision 1.10  86/02/18  14:22:13  goodmon
 * Removed case-insensitive changes.
 * 
 * Revision 1.9  86/02/05  17:46:43  goodmon
 * Made na input case-insensitive.
 * 
 * Revision 1.8  86/01/21  11:18:49  goodmon
 * Fixed problem with copying or reading serial port parameters "password"
 * and "term_var".
 * 
 * Revision 1.7  86/01/14  11:53:19  goodmon
 * Modified to extract erpc port number from /etc/services.
 * 
 * Revision 1.6  85/12/18  11:19:33  goodmon
 * Modifications to autobaud support.
 * 
 * Revision 1.5  85/12/18  10:16:52  goodmon
 * Modifications suggested by market analyst types.
 * 
 * Revision 1.4  85/12/12  12:05:24  goodmon
 * *** empty log message ***
 * 
 * Revision 1.3  85/12/12  11:55:47  goodmon
 * Modified to allow hyphens in annex names.
 * 
 * Revision 1.2  85/11/20  13:18:55  parker
 * added definition of hmux parameter, also changed erpc port back to 530
 * for now.
 * 
 * Revision 1.1  85/11/01  17:41:49  palmer
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *  DATE:	$Date: 1996/10/04 12:04:35 $
 *  REVISION:	$Revision: 1.2 $
 *
 ****************************************************************************
 */


#ifndef UINT32
#include "port/port.h"
#endif

/* selectable modules */

#ifdef NA

#define NDPTG	1
#define NHMUX	0	/* Never! */
#define NPPP	1
#define NSLIP	1
#define NLAT	1
#define NDEC	1
#define NPRONET_FOUR	0	/* Never! */
#define NTFTP_PROTO	1
#define NRDRP		1
#define NCMUSNMP	1
#define NARAP		1
#define NCLITN3270	1

#else

#include "dptg.h"
#include "hmux.h"
#include "ppp.h"
#include "slip.h"
#include "lat.h"
#include "dec.h"
#include "pronet_four.h"
#include "tftp_proto.h"
#include "rdrp.h"
#include "cmusnmp.h"
#include "arap.h"
#include "clitn3270.h"

#endif

/* misc defines */

#ifndef NULL
#define NULL 0
#endif

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
#define WHITE_SPACE " \t\n"
#define PUNCTUATION ",;@=#-"
#define SEPARATORS  "\\ \t\n,;@=-"
#define TERMINATORS " \t\n,;"

#define ALL_PORTS	72
#define	ALL_SYNC_PORTS	4 
#define ALL_PRINTERS	2
#define ALL_INTERFACES	(1 + ALL_PORTS + ALL_SYNC_PORTS)

#define ALL_TRUNKS	(unsigned long)0xffffffff

#ifndef	NBBY
#define	NBBY	8
#endif

#define SETBIT(a,i)	((a)[(i)/NBBY] |= 1<<((i)%NBBY))
#define CLRBIT(a,i)	((a)[(i)/NBBY] &= ~(1<<((i)%NBBY)))

#define SETPORTBIT(a,i)	((a)[(i-1)/NBBY] |= 1<<((i-1)%NBBY))
#define PORTBITSET(a,i)	((a)[(i-1)/NBBY] & (1<<((i-1)%NBBY)))
#define CLRPORTBIT(a,i)	((a)[(i-1)/NBBY] &= ~(1<<((i-1)%NBBY)))
#define PORTBITCLR(a,i)	(((a)[(i-1)/NBBY] & (1<<((i-1)%NBBY))) == 0)

#define SETPRINTERBIT(a,i)	((a)[(i-1)/NBBY] |= 1<<((i-1)%NBBY))
#define PRINTERBITSET(a,i)	((a)[(i-1)/NBBY] & (1<<((i-1)%NBBY)))
#define CLRPRINTERBIT(a,i)	((a)[(i-1)/NBBY] &= ~(1<<((i-1)%NBBY)))
#define PRINTERBITCLR(a,i)	(((a)[(i-1)/NBBY] & (1<<((i-1)%NBBY))) == 0)

#define SETINTERFACEBIT(a,i)	((a)[(i-1)/NBBY] |= 1<<((i-1)%NBBY))
#define INTERFACEBITSET(a,i)	((a)[(i-1)/NBBY] & (1<<((i-1)%NBBY)))
#define CLRINTERFACEBIT(a,i)	((a)[(i-1)/NBBY] &= ~(1<<((i-1)%NBBY)))
#define INTERFACEBITCLR(a,i)	(((a)[(i-1)/NBBY] & (1<<((i-1)%NBBY))) == 0)

#define SETGROUPBIT(ptr,bit)	((ptr)[(bit)/NBBY] |= 1<<((bit)%NBBY))
#define CLRGROUPBIT(ptr,bit)	((ptr)[(bit)/NBBY] &= ~(1<<((bit)%NBBY)))

#define PG_ALL		0x0001		/* context-sensitive "all" */
#define PG_VIRTUAL	0x0002		/* explicit "virtual" */
#define PG_SERIAL	0x0004		/* explicit "serial */
#define PG_PRINTER	0x0008		/* explicit "printer" */

#define PRINTER_OK	TRUE
#define PRINTER_NOT_OK	FALSE
#define VIRTUAL_OK	TRUE
#define VIRTUAL_NOT_OK	FALSE

#define NULLSP	((char *)0)

/* names for things */

#include "names.h"

/* sizes */

#ifdef NA
#define LINE_LENGTH 1024	/* length of input line */
#define WARNING_LENGTH 256	/* length of warning matches cli limit */
#define HOSTNAME_LENGTH 32      /* length of hostname, match with cli limit */
#define USERNAME_LENGTH 32      /* length of username. matches with cli limit */
#define FILENAME_LENGTH 80	/* length of filenames for commands */
/* This message should match the above define */
#define LONG_FILENAME	"maximum filename length is 80 characters"
#endif

#define MAX_STRING_100  100	/* length of image_name on ANNEX3.Also defined*/
				/* in netadm/netadm.h and inc/rom/e2rom.h */
#define	MAX_ADM_STRING	32	/* for lat strings, also in inc/rom/e2rom.h */
				/* length of image_name and term_var: */

#define MAX_KERB_EXT_STRING	67
#define MAX_KERB_STRING		17
#define MAX_KERB_INT_STRING	MAX_KERB_STRING  /* Max length of 4 ip addresses plus length */

#define MAX_IPX_STRING	48	/* Yet another unreasonable assumption */

#define	MAX_RIP_STRING	33	/* for rip network list strings */
#define	MAX_RIP_INT_STRING MAX_RIP_STRING	/* rip network list strings 
							(internal format)*/
#define	MAX_RIP_EXT_STRING	138	/* rip network list strings 
					   (external human readable format)*/
#define RIP_LEN_MASK	0x7f	/* mask off include/exclude bit from rip
					string length byte */

#define	MAX_BIT_STRING	(ALL_INTERFACES + (NBBY - 1 ))/NBBY /* max length of
							bit string in byte */

#define SHORTPARAM_LENGTH 8	/*   Annex I */
#define LONGPARAM_LENGTH 16	/*   Annex II, TIU */
#define MIN_HW_FOR_LONG	ANX_II	/* first hardware to have long parameters */
#define MAXVALUE  1024		/* size of string values, > UNIXPARAM_LENGTH */
#define MSG_LENGTH 1024		/* length of broadcast messages */

/* return codes */

#define LEX_OK     0	/* for lex() */
#define LEX_EOS    1
#define LEX_EOSW   2   /* for lex_switch() */

/* boot command switch values */
#define SWABORT   1  /* abort option  (-a) */
#define SWDELAY   2  /* delayed option (-t) */
#define SWDIAG    4  /* diag option (-h) */
#define SWQUIET   8  /* quiet option (-q) */
#define SWDUMP   16  /* dump option -d */
#define SWFLASH  32  /* dump option -l */

#define BADTIME  -1  /* returned by delay_time when an error occurs */

/*
*   defines for 5.0 host_tbl_size parameters  passed as u_short to oper
*   code and then put into e2
*/

#define HTAB_NONE	       254 
#define HTAB_UNLIMITED	       255

/* special categories used by na in parameter tables */

#define GRP_CAT		99
#define ALL_CAT		9999
#define VOID_CAT	-1

/* special categories used by na/admin for displaying group category  */
#define B_GENERIC_CAT		981
#define B_VCLI_CAT		982
#define B_NAMESERVER_CAT 	983
#define B_SECURITY_CAT		984
#define B_TIME_CAT		985
#define B_SYSLOG_CAT		986
#define B_LAT_CAT 		987
#define B_ATALK_CAT		988
#define B_ROUTER_CAT		989

#define P_GENERIC_CAT		990
#define P_FLOW_CAT		991
#define P_SECURITY_CAT		992
#define P_EDITING_CAT		993
#define P_SERIAL_CAT		994
#define P_SLIP_CAT		995
#define P_PPP_CAT		996
#define P_LAT_CAT		997
#define P_TIMERS_CAT		998
#define P_ATALK_CAT		999
#define P_TN3270_CAT		1000

/* New BIG_BIRD display categories */
#define B_KERB_CAT       	1001
#define B_MOP_CAT        	1002
#define B_IPX_CAT        	1003
#define P_LOGIN_CAT      	1005
#define S_GENERIC_CAT    	1006
#define S_SECURITY_CAT		1007
#define S_NET_CAT        	1008
#define S_PPP_CAT        	1009


/* conversion techniques */

#define CNV_STRING	0
#define CNV_INT		1	/* unsigned integer */
#define CNV_DFT_N	2	/* Boolean, default N */
#define CNV_DFT_Y	3	/* Boolean, default Y */
#define CNV_PS		4	/* port speed */
#define CNV_BPC		5	/* bits per character */
#define CNV_SB		6	/* stop bits */
#define CNV_P		7	/* parity */
#define CNV_MC		8	/* modem control */
#define CNV_PT		9	/* port type */
#define CNV_PM		10	/* port mode */
#define CNV_FC		11	/* flow control type */
#define CNV_PRINT	12	/* expanded control character "^X" */
#define CNV_NET		13	/* Internet address, zero illegal */
#define CNV_NET_Z	14	/* Internet address, zzero legal */
#define CNV_NS		15	/* name server type */
#define CNV_HT		16	/* host table size (stored divided by 2) */
#define CNV_MS		17	/* maximum sessions (0 == 16) */
#define CNV_SEQ		18	/* load/dump sequence */
#define	CNV_IPENCAP	19	/* IP encapsulation type */
#define CNV_SCAP	20	/* server capability */
#define CNV_SYSLOG	21	/* syslog priority */
#define CNV_SYSFAC	22	/* syslog facility code */
#define CNV_VCLILIM	23	/* VCLI limit */
#define CNV_DLST	24	/* Daylight savings */
#define	CNV_ZERO_OK	25	/* integar value that could be zero */
#define CNV_RESET_IDLE	26	/* Reset Idle direction */
#define CNV_PROMPT	27	/* encoded CLI prompt */
#define CNV_DPORT	28	/* convert a dedicated port # */
#define CNV_RNGPRI	29	/* token ring priority */
#define CNV_INT0OFF	30	/* unsigned int, or "off"=0 */
#define CNV_INACTCLI	31	/* INACTCLI value, INT0FF+"immediate"=255 */
#define CNV_FS		32	/* packet size */
#define CNV_T1T		33	/* T1 timer */
#define CNV_T2T		34 	/* T2 timer */
#define CNV_N2N		35	/* N2 Number */
#define CNV_TIT		36	/* Trunk Interface Type */
#define CNV_X25A	37	/* trunk address */
#define CNV_TCT		38	/* trunk connector type */
#define CNV_PL		39	/* trunk lowest PVC */
#define CNV_PH		40	/* trunk highest PVC */
#define CNV_SL		41	/* trunk lowest SVC */
#define CNV_SH		42	/* trunk highest SVC */
#define CNV_TS		43	/* trunk speed conversion */
#define CNV_WS		44	/* window size */
#define CNV_RBCAST	45	/* reverse broadcast {net, port} */
#define CNV_PTYPE	46 	/* printer interface type */

/* LAT fields */
#define CNV_HOST_NUMBER		47 	/* LAT host number type */
#define CNV_SERVICE_LIMIT	48 	/* LAT service limit type */
#define CNV_KA_TIMER		49 	/* LAT keep alive timer type */
#define CNV_CIRCUIT_TIMER	50 	/* LAT circuit timer type */
#define CNV_RETRANS_LIMIT	51 	/* LAT retrans limit type */
#define CNV_ADM_STRING		52 	/* LAT string size type */
#define CNV_GROUP_CODE		53 	/* LAT group code type */
#define CNV_QUEUE_MAX		54 	/* LAT HIC max queue depth */

#define CNV_DPTG		55	/* DPTG settings string */
#define CNV_BML			56	/* Async Control Bit Mask (Longword) */ 
#define CNV_SEC			57	/* Port security mode */ 
#define CNV_MRU			58	/* Maximum receive unit size */
#define CNV_LG_SML		59	/* boolean settings str large/small */

#define CNV_ATTN		60	/* strip ^ from string */

#define CNV_SELECTEDMODS	61	/* selectable software modules */

#define CNV_PSPEED		62 	/* printer interface speed */
#define CNV_PORT		63 	/* port range conversion */
#define CNV_TZ_MIN		64 	/* tz_minutes */
#define CNV_NET_TURN		65 	/* net turnaround */
#define CNV_BYTE		66 	/* 1 - 255 is valid */
#define CNV_BYTE_ZERO_OK	67 	/* 0 - 255 is valid */
#define CNV_HIST_BUFF		68 	/* 0 - 32767 is valid */

/* ARAP fields */
#define CNV_PPP_NCP		69 	/* ipcp, atcp or all */
#define CNV_A_BYTE		70 	/* 0 - 253 is valid */
#define CNV_ARAP_AUTH		71 	/* des or none */
#define CNV_DEF_ZONE_LIST	72 	/* 100 chars MAX */
#define CNV_THIS_NET_RANGE	73 	/* 0x0 - 0xfefe is valid */

#define CNV_INT5OFF		74	/*  FWDTIMER value, 5OFF */
#define CNV_RIP_ROUTERS		75 	/* a list of up to eight IP addr */
#define CNV_RIP_SEND_VERSION	76 	/* 1, 2 or compatibility */
#define CNV_RIP_RECV_VERSION	77 	/* 1, 2 or both */
#define CNV_RIP_HORIZON		78 	/* off, split or poison */
#define CNV_RIP_DEFAULT_ROUTE	79 	/* 0 .. 15 or off (off same as 0) */
#define CNV_RIP_OVERRIDE_DEF	80 	/* 0 .. 15 or none or all */
#define CNV_RIP_DOMAIN		81 	/* 0 .. 65535 */
#define CNV_BOX_RIP_ROUTERS	82 	/* a list of up to eight IP addr */

#define CNV_ENET_ADDR           83      /* ethernet address in hex */
#define CNV_MULTI_TIMER         84      /* multicast timer */
#define CNV_PASSLIM             85      /* password retry limit */
#define CNV_SESS_MODE           86      /* default session mode for lat */
#define CNV_USER_INTF           87      /* user interface type */
#define CNV_DUIFC               88      /* UNUSED */
#define CNV_ZONE		89	/* Annex AppleTalk Zone */

/* New conversion types for BIG_BIRD ...*/

#define CNV_SESS_LIM   		90   /* 1 to <ports>*16 or 0 */
#define CNV_PASS_LIM          	91   /* 1 to 10          */
#define CNV_KERB_HOST    	92   /* 1 to 4 net addresses */
#define CNV_TIMER        	93   /* 1 to 60 (minutes)     */
#define CNV_IPX_FMTY     	94   /* raw802_3, ethernetII, */
				     /* 802_2 or 802_2snap */
#define CNV_CLI_IF       	98   /* uci or vci       */
#define CNV_IPSO_CLASS 		99   /* topsecret, secret, confidential, */
				     /* unclassified, or none */
#define CNV_SYNC_MODE     	100  /* unused or ppp */
#define CNV_SYNC_CLK     	101  /* 56, 64, 112, 128 or external */
#define CNV_SMETRIC      	102  /* 1 to 15 hops */
#define CNV_CRC_TYPE     	103  /* crc16 or ccitt */
#define CNV_IPX_STRING		104  /* length == MAX_IPX_STRING */
#define CNV_MOP_PASSWD          105  /* MOP Password */
#define CNV_SESSION_LIMIT       106  /* Annex session limit */
#define CNV_INACTDUI            107   /* DEC Inactivity Timer (1-255) */


/* annex version defintions */

#define VERS_1		0x0001
#define VERS_2		0x0002
#define VERS_3		0x0004
#define VERS_4		0x0008
#define VERS_4_1	0x0010
#define VERS_5          0x0020
#define VERS_6          0x0040
#define VERS_6_1        0x0080
#define VERS_6_2        0x0100
#define VERS_7		0x0200
#define VERS_7_1	0x0400
#define VERS_7_1_DEC	0x0800
#define VERS_8		0x1000
#define VERS_8_1	0x2000
#define VERS_BIG_BIRD	0x4000
#define VERS_POST_BB	0x8000


    /*
     * The following defines are used to specify all versions staring at 
     * version x, where x == a version number. 
     *
     *  V_2_N == (VERS_2 + VERS_3 + VERS_4 + VERS_4_1 + VERS_5.....)
     *
     *  These defines must be updated for each new revision!  It was done this
     *  way to avoid imbedded define problems on some hosts, and lots of typing
     *  each time a new sw revision or hw platform is added.
     *   
     *   Example: When VERS_6_1 was created;
     *		V_1_N		changed from 0x007f to 0x00ff
     *		V_2_N		changed from 0x007e to 0x00fe
     *		....
     * 		V_5_N		changed from 0x0060 to 0x00e0
     * 		V_6_N		changed from 0x0040 to 0x00c0
     *	    and V_6_1_N		was created.
     *
     * OP.
     * The above description is a 'little' vague. The defines below are a
     * set of masks that are used in cmd.h to mark whether a feature is
     * supported for a specific revision and not previous revisions...
     * Why do these defines start at 7fff for all revisions and not
     * ffff...
     */

#define V_1_N		0x7fff
#define V_2_N		0x7ffe
#define V_3_N		0x7ffc
#define V_4_N		0x7ff8
#define V_4_1_N		0x7ff0
#define V_5_N		0x7fe0
#define V_6_N		0x7fc0
#define V_6_1_N		0x7f80
#define V_6_2_N		0x7f00
#define V_7_N		0x7e00
#define V_7_1_N		0x7c00
#define V_7_1_DEC	0x4800
#define V_8_N		0x7000
#define V_8_1_N		0x6000
#define V_BIG_BIRD_N	0x4000
#define V_POST_BB_N	0x8000


/* annex hardware types */

#define ANX_I		0
#define ANX_II		1
#define ANX_II_EIB	2
#define ANX3		3
#define ANX_MICRO	4
#define X25		5
#define ANX_MICRO_ELS	6
#define N_HW_TYPES	7

/* Codes used by get_port_eib(). */
#define ANX_IIE		0x0100 		/* For historical reasons. */
#define ANX_MICRO_V11	0x0002 
#define ANX_802_5	0x0004
#define ANX_RDRP	0x0008

#define ANX_DIALOUT_ENA	0x0001
#define ANX_APPTALK_ENA	0x0020
#define ANX_TN3270_ENA	0x0040

/* type definitions */

typedef struct
    {
    struct sockaddr_in  addr;
    UINT32		version;
    UINT32		sw_id;
    u_short		flag;
    UINT32		hw_id;
    short		port_count;
    short		sync_count;
    short		trunk_count;
    short		printer_count;
    unsigned char	interface[MAX_BIT_STRING];
    short		lat;
    short		self_boot;
    short		vhelp;
    }			ANNEX_ID;

#ifdef NA

typedef struct annex_list
    {
    char                name[FILENAME_LENGTH + 2];
    ANNEX_ID		annex_id;
    struct annex_list  *next;
    }                   ANNEX_LIST;

typedef struct port_group
    {
    int			pg_bits;
    unsigned char	serial_ports[ALL_PORTS/NBBY];
    }			PORT_GROUP;

typedef struct port_set
    {
    PORT_GROUP		ports;
    char                name[FILENAME_LENGTH + 2];
    ANNEX_ID		annex_id;
    struct port_set    *next;
    }                   PORT_SET;

typedef struct printer_group
    {
    int			pg_bits;
    unsigned char	ports[(ALL_PRINTERS + (NBBY - 1))/NBBY];
    }			PRINTER_GROUP;

typedef struct printer_set
    {
    PRINTER_GROUP	printers;
    char                name[FILENAME_LENGTH + 2];
    ANNEX_ID		annex_id;
    struct printer_set  *next;
    }                   PRINTER_SET;

typedef struct sync_group
    {
     int       pg_bits;
     unsigned char  sync_ports[(ALL_SYNC_PORTS +(NBBY - 1))/NBBY];
    } SYNC_GROUP;

typedef struct sync_set
    {
     SYNC_GROUP     syncs;
     char      name[FILENAME_LENGTH + 2];
     ANNEX_ID  annex_id;
     struct sync_set     *next;
    } SYNC_SET;

typedef struct interface_group
    {
    int			pg_bits;
    unsigned char	interface_ports[(ALL_INTERFACES + (NBBY - 1))/NBBY];
    }			INTERFACE_GROUP;

typedef struct interface_set
    {
    INTERFACE_GROUP		interfaces;
    char                name[FILENAME_LENGTH + 2];
    ANNEX_ID		annex_id;
    struct interface_set    *next;
    }                   INTERFACE_SET;

typedef struct trunk_group
    {
    int			pg_bits;
    UINT32		serial_trunks;
    }			TRUNK_GROUP;

typedef struct trunk_set
    {
    TRUNK_GROUP		trunks;
    char                name[FILENAME_LENGTH + 2];
    ANNEX_ID		annex_id;
    struct trunk_set    *next;
    }                   TRUNK_SET;

#define ALLTRUNK(Pgrp) \
    (((Pgrp)->trunks.pg_bits & (PG_ALL | PG_SERIAL)) ? \
     ALL_TRUNKS : \
     (Pgrp)->trunks.serial_trunks)

typedef struct show_list
    {
    int               param_num;
    struct show_list *next;
    }                 SHOW_LIST;


typedef struct set_list
    {
    int              param_num;
    char             value[LINE_LENGTH + 2];
    struct set_list *next;
    }                SET_LIST;

#endif

/* define structures to contain parameter descriptions for na */

typedef struct
{
	char   *d_key;
	short	d_usage;
	short	d_index;
#ifdef NA
	char   *d_text;
#endif

}	definition;

typedef struct
{
	short	pt_index;
	short	pt_category;
	short	pt_catid;
	short	pt_displaycat;
	short	pt_type;
	short	pt_convert;
#ifdef NA
	UINT32	pt_version[N_HW_TYPES];
#endif

}	parameter_table;

/*	Use these defines to reference entries in the help dictionary	    */

#define D_key(x)	(dictionary[x].d_key)
#define D_usage(x)	(dictionary[x].d_usage)
#define D_index(x)	(dictionary[x].d_index)
#ifdef NA
#define D_text(x)	(dictionary[x].d_text)
#endif

/*	Use these defines to reference in annex parameter table entries     */

#define Ap_index(x)	(annexp_table[x].pt_index)
#define Ap_category(x)	(annexp_table[x].pt_category)
#define Ap_catid(x)	(annexp_table[x].pt_catid)
#define Ap_displaycat(x) (annexp_table[x].pt_displaycat)
#define Ap_type(x)	(annexp_table[x].pt_type)
#define Ap_convert(x)	(annexp_table[x].pt_convert)

#ifdef NA
#define Ap_version(x, h)	(annexp_table[x].pt_version[h])
#define Ap_support(id,param) \
	((id)->version & annexp_table[param].pt_version[(id)->hw_id])
#endif


/*	Use these defines to reference the serial port parameter table	    */

#define Sp_index(x)	(portp_table[x].pt_index)
#define Sp_category(x)	(portp_table[x].pt_category)
#define Sp_catid(x)	(portp_table[x].pt_catid)
#define Sp_displaycat(x) (portp_table[x].pt_displaycat)
#define Sp_type(x)	(portp_table[x].pt_type)
#define Sp_convert(x)	(portp_table[x].pt_convert)
#ifdef NA
#define Sp_version(x, h)	(portp_table[x].pt_version[h])
#define Sp_support(id,param) \
	((id)->version & portp_table[param].pt_version[(id)->hw_id])
#endif


/*	Use these defines to reference the serial trunk parameter table	    */

#define St_index(x)	(trunkp_table[x].pt_index)
#define St_category(x)	(trunkp_table[x].pt_category)
#define St_catid(x)	(trunkp_table[x].pt_catid)
#define St_displaycat(x) (trunkp_table[x].pt_displaycat)
#define St_type(x)	(trunkp_table[x].pt_type)
#define St_convert(x)	(trunkp_table[x].pt_convert)
#ifdef NA
#define St_version(x, h)	(trunkp_table[x].pt_version[h])
#define St_support(id,param) \
	((id)->version & trunkp_table[param].pt_version[(id)->hw_id])
#endif


/*	Use these defines to reference the centronics port parameter table  */

#define Cp_index(x)	(printp_table[x].pt_index)
#define Cp_category(x)	(printp_table[x].pt_category)
#define Cp_catid(x)	(printp_table[x].pt_catid)
#define Cp_displaycat(x) (printp_table[x].pt_displaycat)
#define Cp_type(x)	(printp_table[x].pt_type)
#define Cp_convert(x)	(printp_table[x].pt_convert)
#ifdef NA
#define Cp_version(x, h)	(printp_table[x].pt_version[h])
#define Cp_support(id,param) \
	((id)->version & printp_table[param].pt_version[(id)->hw_id])
#endif

/*	Use these defines to reference the interface parameter table  */

#define Ip_index(x)	(interfacep_table[x].pt_index)
#define Ip_category(x)	(interfacep_table[x].pt_category)
#define Ip_catid(x)	(interfacep_table[x].pt_catid)
#define Ip_displaycat(x) (interfacep_table[x].pt_displaycat)
#define Ip_type(x)	(interfacep_table[x].pt_type)
#define Ip_convert(x)	(interfacep_table[x].pt_convert)
#ifdef NA
#define Ip_version(x, h)	(interfacep_table[x].pt_version[h])
#define Ip_support(id,param) \
	((id)->version & interfacep_table[param].pt_version[(id)->hw_id])
#endif

/* defines for referencing sync parameter table entries...*/

#define Syncp_index(x)     (syncp_table[x].pt_index)
#define Syncp_category(x)  (syncp_table[x].pt_category)
#define Syncp_catid(x)     (syncp_table[x].pt_catid)
#define Syncp_displaycat(x) (syncp_table[x].pt_displaycat)
#define Syncp_type(x)      (syncp_table[x].pt_type)
#define Syncp_convert(x)   (syncp_table[x].pt_convert)
#ifdef NA
#define Syncp_version(x, h)        (syncp_table[x].pt_version[h])
#define Syncp_support(id,param) \
        ((id)->version & syncp_table[param].pt_version[(id)->hw_id])
#endif

#ifdef NA

#ifdef IN_MAIN
#define EXTERN	/* */
#else
#define EXTERN	extern
#endif

/* global variables */

EXTERN
FILE *cmd_file;		/* pointer to file descriptor of command file. */

EXTERN
short erpc_port;	/* port number of erpc server */

EXTERN
int status;		/* exit status: 0 no errors, 1 something went wrong */

EXTERN
int done,		/* TRUE when quit command has been recognized */
    eos,		/* TRUE when command line is empty */
    prompt_mode,	/* TRUE when the human is to be prompted for args */
    symbol_length,	/* length of current symbol */
    script_input,	/* TRUE iff input is from a script file */
    inswitch,           /* TRUE if in a switch ie -abcd, abcd are in a switch */
    is_super;		/* TRUE iff userid is root */

EXTERN
char command_line[LINE_LENGTH + 1],	/* last line typed by human */
     *Psymbol,			/* lex pointer; points to next unlexed char */
     symbol[LINE_LENGTH + 1];	/* nul-delimited copy of current symbol */

EXTERN
ANNEX_LIST *Pdef_annex_list,
           *Pdef_annex_tail,
	   *Pspec_annex_list,
	   *Pspec_annex_tail;

EXTERN
PORT_SET   *Pdef_port_set,
           *Pdef_port_tail,
	   *Pspec_port_set,
	   *Pspec_port_tail;

EXTERN
PRINTER_SET *Pdef_printer_set,
            *Pdef_printer_tail,
	    *Pspec_printer_set,
	    *Pspec_printer_tail;

EXTERN
INTERFACE_SET   *Pdef_interface_set,
           *Pdef_interface_tail,
	   *Pspec_interface_set,
	   *Pspec_interface_tail;

EXTERN
TRUNK_SET  *Pdef_trunk_set,
           *Pdef_trunk_tail,
	   *Pspec_trunk_set,
	   *Pspec_trunk_tail;

EXTERN
SYNC_SET  *Pdef_sync_set,
          *Pdef_sync_tail,
          *Pspec_sync_set,
          *Pspec_sync_tail;

EXTERN
SHOW_LIST  *Pshow_list,
           *Pshow_tail;

EXTERN
SET_LIST   *Pset_list,
           *Pset_tail;

EXTERN
jmp_buf cmd_env;
EXTERN
jmp_buf prompt_env;

#endif /* NA */

/* for lat specific conversion */

struct mask_options {
    UINT32 mask;
    char *name;
    };

#define	ALL_LAT_GROUPS	"0-255"
#define	ALL_STR		"all"
#define	NONE_STR	"none"

#ifndef NA
#define punt(x,y)		return(-1)
#define match(x,y,z)		matchit(x,y)
#define match_flag(v,w,x,y,z)	matchflag(v,w,x,z)
#endif

#ifdef NA
EXTERN void open_pager(),close_pager(),initialize_pager(),stop_pager();
#endif /* NA */

/*
 *      Structure Definitions for conversion routines
 */
   
typedef struct
{
	unsigned short  C_length;
	char            C_string[MAX_STRING_100];
		 
}       COURIER_STRING;
		  
typedef union
{
	UINT32		*I_long;
	unsigned short  *I_short;
	char            *I_char;
	struct in_addr  *I_inet;
        COURIER_STRING  *I_courier;
	 
}       INTERN;
	  
#define Linternal       (Internal.I_long)
#define Sinternal       (Internal.I_short)
#define Cinternal       (Internal.I_char)
#define Ninternal       (Internal.I_inet)
#define CS_length       (Internal.I_courier)->C_length
#define CS_string       (Internal.I_courier)->C_string
 
#define THISNET_LEN     4
#define THIS_NET_MAX    0xfeff  /* this_net parameter maximum */
