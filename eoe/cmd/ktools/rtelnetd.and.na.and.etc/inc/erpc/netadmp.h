/******************************************************************************
 *
 *        Copyright 1989,1990 Xylogics, Inc.  ALL RIGHTS RESERVED.
 *
 * ALL RIGHTS RESERVED. Licensed Material - Property of Xylogics, Inc.
 * This software is made available solely pursuant to the terms of a
 * software license agreement which governs its use.
 * Unauthorized duplication, distribution or sale are strictly prohibited.
 *
 * Include file description:
 *  %$(description)$%
 *
 * Original Author: %$(author)$%    Created on: %$(created-on)$%
 *
 * Revision Control Information:
 *
 * $Id: netadmp.h,v 1.2 1996/10/04 12:03:26 cwilson Exp $
 *
 * This file created by RCS from:
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/inc/erpc/RCS/netadmp.h,v $
 *
 * Revision History:
 *
 * $Log: netadmp.h,v $
 * Revision 1.2  1996/10/04 12:03:26  cwilson
 * latest rev
 *
 * Revision 2.95  1994/05/27  16:13:09  townsend
 * NA/Admin merge.
 *
 * Revision 2.94  1994/05/24  09:20:21  russ
 * Bigbird na/adm merge.
 *
 * Revision 2.93  1994/05/18  16:51:34  russ
 * Add RESET_ANNEX_SYSLOG.
 *
 * Revision 2.92  1994/03/17  07:01:13  booth
 * Put in new IPX dump_server param definition
 *
 * Revision 2.91  94/03/01  16:18:14  carlson
 * SPR 2424 -- repaired DLA_NDEVS definition and added comments for
 * device types (must match device_table in net_line.c).
 * 
 * Revision 2.90  1994/02/17  12:56:24  townsend
 * Littleboy merge.
 *
 * Revision 2.89  1994/01/20  14:11:59  booth
 * Add in DFE_MODEM_ADD_ENTRIES and DFE_SEG_JUMPER_5390
 * to be consistent with David Wangs changes since the ROM
 * guys have there own netadmp.h file.
 *
 * Revision 2.88  94/01/20  14:09:00  booth
 * Put in define for IPX_DO_CHECKSUM field
 * 
 * Revision 2.87  93/12/12  12:53:03  booth
 * Put in new defines for EEPROM IPX file server, frame type, dmp user name,
 * dmp password and dmp pathname.
 * 
 * Revision 2.86.1.1  1994/01/26  14:00:01  geiser
 * Added new parameters for modem management and 5390 cmb jumper
 * (code by David Wang; check-in by Maryann Geiser)
 *
 * Revision 2.86  1993/05/07  17:17:22  raison
 * set holding place for DecServer parm, session_limit
 *
 * Revision 2.85  93/05/05  16:19:59  carlson
 * Added a new error code -- INTERNAL_ERROR -- for those cases where
 * something's just plain wrong on the Annex.
 * 
 * Revision 2.84  93/03/30  11:06:45  raison
 * added DECserver parms, turned off
 * 
 * Revision 2.83  93/03/25  13:43:52  wang
 * Added in option_key param for AppleTalk and Tn3270 and removed appletalk_key.
 * 
 * Revision 2.82  93/03/24  15:09:29  wang
 * Added in new remote procedure define.
 * 
 * Revision 2.81  93/03/03  15:16:34  wang
 * Removed THIS_NET param and changed NODE_ID param size.
 * 
 * Revision 2.80  93/02/25  12:45:26  carlson
 * SPR 1319 -- Added DFE_TIMESERVE -- time server host address.
 * 
 * Revision 2.79  93/02/19  13:57:24  carlson
 * Added three TCP keepalive timers -- box, serial and parallel.
 * 
 * Revision 2.78  93/02/18  21:48:00  raison
 * added DECserver Annex parameters
 * 
 * Revision 2.77  93/02/18  13:23:43  wang
 * Removed un-needed appletalk and rip parameters.
 * 
 * Revision 2.76  93/02/06  21:36:16  sasson
 * Added the ability to reset the modem information table (dialback).
 * 
 * Revision 2.75  93/02/01  17:20:50  carlson
 * Added Annex parameter -- IP broadcast forwarding.
 * 
 * Revision 2.74  93/01/27  15:51:45  wang
 * Fixed can't set annex rip_auth parameter problem.
 * 
 * Revision 2.73  93/01/26  16:01:32  reeve
 * Put the new Toll Saver params at the bottom of the Serial Protocol params.
 * 
 * Revision 2.72  93/01/22  17:42:51  wang
 * New parameters support for Rel8.0
 * 
 * Revision 2.71  92/11/02  15:52:58  russ
 * Added new printer auto linefeed param.
 * 
 * Revision 2.70  92/10/30  14:31:15  russ
 * Added loose source route annex parameter.
 * 
 * Revision 2.69  92/10/15  14:03:22  wang
 * Added new NA parameters for ARAP support.
 * 
 * Revision 2.68  92/07/21  11:51:01  reeve
 * Removed "#if" for DLA_TFTP_DIR and DLA_TFTP_DUMP.
 * 
 * Revision 2.67  92/04/28  13:08:47  bullock
 * spr.756 - added banner parameter
 * 
 * Revision 2.66  92/04/01  22:41:10  russ
 * Add new slave buffering command.
 * 
 * Revision 2.65  92/03/24  17:54:59  sasson
 * Fixed local admin on Annex-II's: could not set any annex parameter past
 * syslog_host. Added VOID_CAT for syslog_port.
 * 
 * Revision 2.64  92/02/28  16:26:47  wang
 * Removed syslog_port support for Annex-II.
 * 
 * Revision 2.63  92/02/20  17:02:47  wang
 * Added support for new NA parameter syslog_port
 * 
 * Revision 2.62  92/02/18  17:41:27  sasson
 * Added support for 3 LAT parameters (authorized_group, vcli_group, lat_q_max).
 * 
 * Revision 2.61  92/02/06  11:23:38  carlson
 * SPR 543 - added latb_enable flag so LAT data-b packets can be ignored.
 * 
 * Revision 2.60  92/01/31  22:52:02  raison
 * added printer cmd and printer_speed
 * 
 * Revision 2.59  92/01/31  13:13:18  wang
 * Add new NA parameter for config.annex file
 * 
 * Revision 2.58  92/01/27  19:01:18  barnes
 * add define for selectable modules, suppress DFE_MACRO
 * 
 * Revision 2.57  92/01/11  17:27:40  reeve
 * Re-aligned reset params for ELS due to removal of motd.
 *
 * Revision 2.56  92/01/09  13:51:35  carlson
 * Inserted definition of slip security flag.
 *
 * Revision 2.55  91/11/22  12:36:14  reeve
 * Added definition of offset into NET category for
 * dialup_address.
 * 
 * Revision 2.54  91/11/15  13:30:15  russ
 * Added the define for macro_file param.
 * 
 * Revision 2.53  91/11/05  10:27:35  russ
 * 
 * Add slip_mtu_size to allow booting out of cslip ports.
 * 
 * Revision 2.52  91/10/30  15:11:58  barnes
 * add new NA parameter to enable/disable SNMP sets
 * 
 * Revision 2.51  91/10/04  14:39:35  reeve
 * Made NET_CAT and SLIP_CAT synonomous.
 * 
 * Revision 2.50  91/10/04  14:20:58  reeve
 * Added parameters for PPP fields.
 * 
 * Revision 2.49  91/07/31  20:46:43  raison
 * added telnet_crlf port parameter
 * 
 * Revision 2.48  91/07/26  15:58:27  raison
 * unsupport tftp NA parameter fields for Annex-II
 * 
 * Revision 2.47  91/07/02  13:19:45  raison
 * added tftp_dir_name and tftp_dump_name paramters
 * 
 * Revision 2.46  91/03/07  16:24:43  pjc
 * Added defines for need_dsr and dptg_settings port parameters.
 * 
 * Revision 2.45  91/02/06  14:13:26  townsend
 * Added forwarding count
 * 
 * Revision 2.44  91/01/23  21:52:59  raison
 * added lat_value na parameter and GROUP_CODE parameter type.
 * 
 * Revision 2.43  91/01/10  13:24:27  grant
 * Added new param cli_imask7
 * 
 * Revision 2.42  91/01/03  15:20:30  grant
 * Drop num_dla_cats to 2.
 * 
 * Revision 2.41  90/12/14  15:13:05  raison
 * added lat and sys_location defines
 * 
 * Revision 2.40  90/12/04  13:29:31  sasson
 * The image_name was increased to 100 characters on annex3.
 * 
 * Revision 2.39  90/10/08  14:09:47  raison
 * for spr.273 - added defines for new na field, authoritative agent as
 * specified in RFC 1122.
 * 
 * Revision 2.38  90/07/29  22:18:00  loverso
 * new flags for compressed slip
 * 
 * Revision 2.37  89/12/14  15:46:53  russ
 * Added new NA parameters.
 * 
 * Revision 2.36  89/11/20  16:50:12  grant
 * Added defines for printer type and broadcast_direction.
 * 
 * Revision 2.35  89/11/16  14:27:54  russ
 * Added a define RESET_ANNEX_MAX which specifies upper range of the na reset
 * commands
 * 
 * Revision 2.34  89/11/01  15:48:14  grant
 * Added RPROC_GET_EIB for annexIIe.
 * 
 * Revision 2.33  89/11/01  13:29:16  russ
 * added na reset macro define.
 * 
 * 
 * Revision 2.32  89/10/01  16:48:02  loverso
 * Add SLIP_TCPHDRCOMP
 * 
 * Revision 2.31  89/08/21  14:14:51  grant
 * Added RESET_NAME_SERVER define for na reset command.
 * 
 * Revision 2.30  89/08/02  15:18:54  parker
 * Added define for ACT_SERIAL_DEV.
 * 
 * Revision 2.29  89/05/23  16:38:58  grant
 * Added defines for rproc shutdown.
 * 
 * Revision 2.28  89/05/11  13:49:55  maron
 * Modified this module to support the new security parameters, vcli_password,
 * vcli_sec_ena, and port_password.
 * 
 * Revision 2.27  89/04/05  12:39:40  loverso
 * Changed copyright notice
 * 
 * Revision 2.26  88/12/29  15:58:01  townsend
 * Added remote procedure to get trunk count and added trunk parameters
 * 
 * Revision 2.25  88/12/15  17:02:23  loverso
 * CLI "no jobs" inactivity timer
 * 
 * Revision 2.24  88/12/13  17:05:56  parker
 * Added defines for MOTD
 * 
 * Revision 2.23  88/11/17  15:53:47  loverso
 * New routed parameter
 * 
 * Revision 2.22  88/09/20  08:59:02  townsend
 * Added DLA_RING_PRIORITY definition for token ring annex
 * 
 * Revision 2.21  88/07/08  14:08:33  harris
 * New abort type TOO_MANY_SESSIONS.  Annex can only handle 12 requests at once.
 * 
 * Revision 2.20  88/06/30  15:27:34  mattes
 * Added 7-bit masking flag
 * 
 * Revision 2.19  88/06/09  12:31:42  mattes
 * Added two per-Annex Booleans
 * 
 * Revision 2.18  88/06/07  15:23:25  harris
 * DFE_ACP_KEY parameter - ACP encryption key - string.
 * 
 * Revision 2.17  88/06/01  16:31:31  mattes
 * Added RESET_ANNEX remote procedure call and parameter values
 * 
 * Revision 2.16  88/05/16  12:10:50  mattes
 * Added dedicated-port parameters
 * 
 * Revision 2.15  88/05/04  23:35:29  harris
 * Added RPROC_SRPC_OPEN procedure, and data type RAW_BLOCK_P.
 * 
 * Revision 2.14  88/04/07  16:14:29  emond
 * Accommodate addition of IP encapsulation type paramether to DLA parameters.
 * 
 * Revision 2.13  88/03/17  15:23:31  mattes
 * Added per-port prompt and telnet escape char
 * 
 * Revision 2.12  88/03/15  14:24:05  mattes
 * Added virtual CLI device type
 * 
 * Revision 2.11  88/03/14  16:48:58  parker
 * Added params for activity and idle timers.
 * 
 * Revision 2.10  88/03/04  17:09:45  mattes
 * Added VCLI limit
 * 
 * Revision 2.9  88/03/02  14:26:51  parker
 * Added some new parameters.
 * 
 * Revision 2.8  88/01/19  09:59:38  mattes
 * Added boot server parameter
 * 
 * Revision 2.7  87/12/29  11:43:00  mattes
 * Added some 4.0 parameters
 * 
 * Revision 2.6  87/08/20  10:45:12  mattes
 * 32 port support, HPCL parameter added
 * 
 * Revision 2.5  87/08/12  15:37:59  mattes
 * Added 3.0 parameters
 * 
 * Revision 2.4  86/12/22  17:16:58  parker
 * added new name server parameters.
 * 
 * Revision 2.3  86/12/09  09:31:25  parker
 * added new r2.1 parameters.
 * 
 * Revision 2.2  86/06/16  09:56:42  parker
 * Added EDIT_DOLEAP.
 * 
 * Revision 2.1  86/05/01  13:51:03  goodmon
 * Modified for annex broadcast command.
 * 
 * Revision 2.0  86/02/20  16:00:26  parker
 * First development revision for Release 2
 * 
 * Revision 1.16  85/11/20  13:30:18  parker
 * Added define of hmux parameter.
 * 
 * Revision 1.15  85/10/28  10:00:41  brennan
 * Carrier override, printer tab expansion parameter definitions.
 * 
 * Revision 1.14  85/08/26  19:14:33  parker
 * Base level 2 merge.
 * 
 * Revision 1.13.1.2  85/08/20  13:05:19  goodmon
 * Added warning about coordinating additions with parm.doc.
 * 
 * Revision 1.13.1.1  85/08/19  14:52:36  goodmon
 * Added #declare directives for parameter group operations.
 * Changed names of parameter types to be more similar to courier.
 * 
 * Revision 1.13  85/07/01  10:55:30  parker
 * Put device types back in.
 * 
 * Revision 1.12  85/07/01  10:48:37  parker
 * Changed names of line printer parameters.
 * 
 * Revision 1.11  85/06/29  16:59:27  goodmon
 * added centronics parameter numbers
 * 
 * Revision 1.10  85/06/28  17:53:30  brennan
 * Really. This time for sure.
 * 
 * Revision 1.9  85/06/28  17:33:29  brennan
 * Really fix th e1 origin thing, this time.
 * 
 * Revision 1.8  85/06/28  17:28:59  parker
 * Remove line device type defines and put in ../h/devtypes.h
 * 
 * Revision 1.7  85/06/28  15:30:19  brennan
 * Modified for "1 origin" interface.
 * 
 * Revision 1.6  85/06/27  15:32:14  goodmon
 * fixed DLA_IMAGE, added DLA_INETADDR
 * 
 * Revision 1.5  85/06/25  14:41:35  goodmon
 * Added dla parameter numbers.
 * 
 * Revision 1.4  85/06/25  14:34:43  goodmon
 * Added interface, device, and editing parameter numbers.
 * 
 * Revision 1.3  85/06/25  14:31:27  goodmon
 * added interface, device, and editing parameter numbers.
 * 
 * Revision 1.2  85/06/25  14:28:04  parker
 * Added catagory defines.
 * 
 * Revision 1.1  85/06/21  09:28:24  parker
 * Initial revision
 * 
 * Revision 1.1  85/06/19  13:45:49  parker
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *****************************************************************************/


/* Defines constants used in the NETwork ADMinistration Protocol. */

/* When adding remote procedure or parameter numbers, be sure to coordinate
   with the file /annex/doc/parm.doc. */
  

#define NETADM_VERSION 1

/* remote procedure numbers */
#define RPROC_BOOT           0
#define RPROC_DUMPBOOT       1
#define RPROC_RESET_LINE     2
#define RPROC_RESET_ALL      3
#define RPROC_READ_MEMORY    4
#define RPROC_SET_INET_ADDR  5
#define RPROC_GET_DLA_PARAM  6
#define RPROC_GET_LINE_PARAM 7
#define RPROC_SET_DLA_PARAM  8
#define RPROC_SET_LINE_PARAM 9
#define RPROC_GET_REV	     10
#define RPROC_GET_PORTS	     11
#define RPROC_BCAST_TO_PORT  12
#define RPROC_SRPC_OPEN	     13
#define RPROC_RESET_ANNEX    14
#define RPROC_GET_TRUNKS     15
#define RPROC_SHUTDOWN	     16 
#define RPROC_GET_EIB	     17
#define RPROC_GET_OPTS	     18
#define RPROC_GET_PRINTERS   19
#define RPROC_GET_IF_PARAM   20
#define RPROC_SET_IF_PARAM   21 
#define RPROC_GET_IFS        22 
#define RPROC_SET_SYNC_PARAM 23
#define RPROC_GET_SYNC_PARAM 24
#define RPROC_GET_SYNCS      25

#define NETADM_NPROCS	     26

/*
 * Annex line device types
 * (Order must match entries in oper/adm/net_line.c device_table[].)
 */
#define SERIAL_DEV	1
#define P_PRINT_DEV	2
#define SBX_DEV		3
#define ACT_SERIAL_DEV	4
#define INTERFACE_DEV	5
#define PSEUDO_DEV	6		/* Used by SNMP */
/* #define ANNEX_DEV	10 */		/* Not used */

#define DLA_NDEVS	6		/* Should match last type */


/*
 * Annex Synchronous device types
 * Only the one generic to start with???
 */
#define SYNC_DEV	1

#define SYNC_NDEVS	1

/* annex device type categories */
#define DLA_CAT 1
#define DFE_CAT 2
#define LAT_CAT 3
#define ARAP_CAT 4
#define RIP_CAT 5

#define DLA_PARM_NCATS   5

/* serial line parameter categories */
#define DEV_CAT		1
#define EDIT_CAT	2
#define INTF_CAT	3
#define NET_CAT	        4
#define SLIP_CAT        4
#define DEV2_CAT        5

#define LINE_PARM_NCATS   5

/* Synchronous parameter categories */
#define SYNC_CAT	1

#define SYNC_PARM_NCATS	1

/* centronics port categories */
#define LP_CAT		1
#define CX_CAT		2

#define LP_PARM_NCATS	2

/* SBX port categories */
#define SBX_CAT		1

#define SBX_PARM_NCATS	1

/* interface parameter categories */
#define IF_CAT		1

#define IF_PARM_NCATS   1

/* per annex statistics categories */
#define TCP_IP_CAT    1
#define HW_CONFIG_CAT 2
#define MXL_CAT	      3

#define DLA_STAT_NCATS 3

/* per annex line statistics categories */
#define SERIAL_LINE_CAT  1

#define LINE_STAT_NCATS  1

/* parameter types */
#define NULL_P          	0
#define BYTE_P          	1
#define STRING_P        	2
#define CARDINAL_P      	3
#define LONG_CARDINAL_P 	4
#define FLOAT_P         	5
#define BOOLEAN_P       	6
#define LONG_UNSPEC_P   	7
#define RAW_BLOCK_P		8
#define ARBL_STRING_P        	9
#define STRING_P_100        	10
#define ADM_STRING_P        	11
#define LAT_GROUP_P        	12
#define RIP_ROUTERS_P		13
#define ENET_ADDR_P		14
#define FILLER_P		15		/* basically a void */
#define KERB_HOST_P		16
#define IPX_STRING_P		17
#define MOP_PASSWD_P		18

/* interface parameter numbers */
#define INTER_REV		1
#define INTER_IBAUD		3
#define INTER_OBAUD		4
#define INTER_BCHAR		5
#define INTER_STOPB		6
#define INTER_PCHECK		7
#define INTER_PGEN		8
#define INTER_MODEM		12
#define INTER_IMASK7		13
#define INTER_ABAUD		14

#define INTER_LPVC		15
#define INTER_HPVC		16
#define INTER_LSVC		17
#define INTER_HSVC		18
#define INTER_TRADD		19
/* overlapping params for X.25 */
#define INTER_WNDSZ		5
#define INTER_FRMSZ		6
#define INTER_TRSPEED		7
#define INTER_X25T2		8
#define INTER_TRCNCTR		9
#define INTER_X25T1		10
#define INTER_X25N2		11
#define INTER_TRNK		12

/* device parameter numbers */
#define DEV_REV			1
#define DEV_NSBRK		2
#define DEV_NLBRK		3
#define DEV_NAUTOBAUD		4
#define DEV_LOGINT		5
#define DEV_HMUX		6
#define DEV_IFLOW		7
#define DEV_ISTOPC		8
#define DEV_ISTARTC		9
#define DEV_OFLOW		10
#define DEV_OSTOPC		11
#define DEV_OSTARTC		12
#define DEV_ATTN		13
#define DEV_ISIZE		14
#define DEV_TIMOUT		15
#define DEV_INACTIVE		16
#define DEV_LTYPE		17
#define DEV_NAME		18
#define DEV_TERM		19
#define DEV_MODE		20
#define DEV_CARRIER_OVERRIDE	21
#define DEV_NBROADCAST		22
#define DEV_CLI_SECURITY	23
#define DEV_CONNECT_SECURITY	24
#define DEV_PORT_SECURITY	25
#define DEV_SESSIONS		26
#define DEV_IXANY		27
#define DEV_DEFAULT_HPCL	29
#define DEV_LOCATION		30
#define DEV_INPUT_ACT		31
#define DEV_OUTPUT_ACT		32
#define DEV_RESET_IDLE		33
#define DEV_DEDICATED_ADDR	34
#define DEV_DEDICATED_PORT	35
#define DEV_INACTCLI		36
#define DEV_RBCAST		37    /* maps to na BROADCAST_DIR */
#define DEV_CLI_IMASK7		38 
#define DEV_FORWARD_COUNT	39 
#define DEV_NEED_DSR		40
#define DEV_TELNET_CRLF		41
#define DEV_LATB_ENABLE		42
#define LAT_AUTHORIZED_GROUPS	43
#define DEV_PS_HISTORY_BUFF	44
#define DEV_BANNER		45
#define DEV_KEEPALIVE		46
#define DEV_MODEM_VAR		47
#define TN3270_PRINTER_HOST    	48
#define TN3270_PRINTER_NAME	49
#define DEV_DFLOW               50
#define DEV_DIFLOW              51
#define DEV_DOFLOW              52
#define DEV_SESS_MODE           53
#define DEV_DUI_TIMEOUT         54
#define DEV_DUI_PASSWD          55
#define DEV_IPSO_CLASS		56
#define DEV_IPX_SECURE		57

/* device parameter numbers */
#define DEV2_REV		 1
#define DEV2_PORT_PASSWD         2
#define DEV2_DPTG_SETTINGS       3

/* editing parameter numbers */
#define EDIT_REV		1
#define EDIT_NEWLIN		2
#define EDIT_INECHO		3
#define EDIT_IUCLC		4
#define EDIT_OLCUC		5
#define EDIT_OCRTCERA		6
#define EDIT_OCRTLERA		7
#define EDIT_OTABS		8
#define EDIT_CERA		9
#define EDIT_LERA		10
#define EDIT_WERA		11
#define EDIT_LDISP		12
#define EDIT_FLUSH		13
#define EDIT_DOLEAP		14
#define EDIT_PROMPT		15
#define EDIT_TESC		16
#define EDIT_USER_INTF          17

/* NET parameter numbers */
#define SLIP_REV		1
#define SLIP_NODUMP		2
#define SLIP_LOCALADDR		3
#define SLIP_REMOTEADDR		4
#define SLIP_NETMASK		5
#define SLIP_LOADUMPADDR	6
#define SLIP_METRIC		7
#define SLIP_DO_COMP		8
#define SLIP_EN_COMP		9
#define SLIP_NO_ICMP		10
#define SLIP_FASTQ		11
#define SLIP_LGMTU		12
#define SLIP_SECURE             13
#define PPP_DIALUP_ADDR		14 /* Should be renamed: SLIP_DIALUP_ADDR */

#define FILLER			15 /* R7.0 bug left PPP_ACTOPEN field */
#define PPP_MRU			16
#define PPP_ACM			17
#define PPP_SECURITY		18
#define PPP_UNAMERMT		19
#define PPP_PWORDRMT		20
#define PPP_NCP			21

#define ARAP_AT_GUEST     	22
#define ARAP_AT_NODEID     	23
#define ARAP_AT_SECURITY     	24
#define ARAP_V42BIS    		25

#define SLIP_DEMAND_DIAL        26
#define SLIP_NET_INACTIVITY     27
#define SLIP_PHONE              28

/* Synchronous parameter numbers */
#define SYNC_REV            1
#define SYNC_MODE           2
#define SYNC_LOCATION       3
#define SYNC_CLOCKING       4
#define SYNC_FORCE_CTS      5
#define SYNC_USRNAME        6
#define SYNC_PORT_PASSWD    7
#define SYNC_LOCAL_ADDR     8
#define SYNC_REMOTE_ADDR    9
#define SYNC_NETMASK        10
#define SYNC_METRIC         11
#define SYNC_ALW_COMP       12
#define SYNC_SECURE         13
#define SYNC_DIAL_ADDR      14
#define SYNC_PPP_MRU        15
#define SYNC_PPP_SECURE_PROTO   16
#define SYNC_PPP_USRNAME_REMOTE 17
#define SYNC_PPP_PASSWD_REMOTE  18
#define SYNC_PPP_NCP        19

/* dla parameter numbers */
#define DLA_REV			1
#define DLA_INETADDR		2
#define DLA_IMAGE		3
#define DLA_PREF_LOAD   	4
#define DLA_PREF_DUMP   	5
#define DLA_SUBNET		6
#define DLA_BROAD_ADDR  	7
#define DLA_LOADUMP_GATE	8
#define DLA_LOADUMP_SEQ		9
#define	DLA_IPENCAP		10
#define DLA_RING_PRIORITY	11
#define DLA_TFTP_DIR		12
#define DLA_TFTP_DUMP		13
#define DLA_MOP_HOST            14
#define DLA_IPX_FILE_SERVER	15
#define DLA_IPX_FRAME_TYPE	16
#define DLA_IPX_DMP_USER_NAME	17
#define DLA_IPX_DMP_PASSWD	18
#define DLA_IPX_DMP_PATH	19
#define DLA_IPX_DO_CHECKSUM     20
#define DLA_IPX_DMP_SERVER      21

/* dfe parameter numbers */
#define DFE_REV			1
#define DFE_1ST_NS		2
#define DFE_2ND_NS		3
#define DFE_HTABLE_SZ		4
#define DFE_PREF1_SECURE 	5
#define DFE_1ST_NS_ADDR		6
#define DFE_2ND_NS_ADDR		7
#define DFE_NET_TURNAROUND	8
#define DFE_SECURE		9
#define DFE_SERVER_CAP		10
#define DFE_SYSLOG_MASK		11
#define DFE_SYSLOG_FAC		12
#define DFE_SYSLOG_ADDR		13
#define DFE_PROMPT		14
#define DFE_TZ_MINUTES		15
#define	DFE_TZ_DLST		16
#define DFE_VCLI_LIMIT		17
#define DFE_PASSWORD		18
#define DFE_ACP_KEY		19
#define DFE_NRWHOD		20
#define DFE_NMIN_UNIQUE		21
#define DFE_NROUTED		22
#define DFE_MOTD		23
#define DFE_NAMESVR_BCAST       24
#define DFE_SECRSVR_BCAST       25
#define DFE_TIMESVR_BCAST       26
#define DFE_LOADSVR_BCAST       27
#define DFE_PREF2_SECURE 	28
#define DFE_VCLI_SEC_ENA        29
#define DFE_VCLI_PASSWD         30
#define DFE_AGENT         	31
#define LAT_HOST_ID        	32	/* sys_location used for lat and snmp */
#define LAT_KEY_VALUE        	33
#define	DFE_SNMPSET		34
#define DFE_SELECTED_MODULES	35
#define DFE_CONFIG		36
#define DFE_SYSLOG_PORT		37
#define DFE_LOOSE_SOURCE_RT	38
#define DFE_FWDBCAST		39
#define DFE_LOCK_ENABLE		40
#define DFE_PASSWD_LIMIT	41
#define DFE_KEEPALIVE		42
#define DFE_TIMESERVE		43
#define DFE_OPTION_KEY		44
#define DFE_LOGIN_PASSWD        45
#define DFE_LOGIN_PROMPT        46
#define DFE_DUI_TIMER           47
#define DFE_MOP_PASSWD          48
#define DFE_SESSION_LIMIT       49
#define DFE_MODEM_ACC_ENTRIES	50
#define DFE_SEG_JUMPER_5390	51
#define DFE_OUTPUT_TTL		52
#define	DFE_KERB_SECUREN 	53
#define	DFE_KERB_HOST		54
#define	DFE_TGS_HOST		55
#define	DFE_TELNETD_KEY		56
#define	DFE_KERBCLK_SKEW	57	
#define	DFE_TMUX_ENA		58
#define	DFE_TMUX_MAX_HOST	59
#define	DFE_TMUX_DELAY		60
#define	DFE_MAX_MPX		61
#define	DFE_CHAP_AUTH_NAME	62

/* LAT parameter numbers */
#define LAT_HOST_NAME        	1
#define LAT_HOST_NUMBER        	2
#define LAT_SERVICE_LIMIT      	3
#define LAT_KA_TIMER        	4
#define LAT_CIRCUIT_TIMER      	5
#define LAT_RETRANS_LIMIT      	6
#define LAT_GROUP_CODE      	7
#define LAT_QUEUE_MAX      	8
#define LAT_VCLI_GROUPS      	9
#define LAT_MULTI_TIMER		10

/* ARAP parameter numbers */
#define ARAP_A_ROUTER     	1
#define ARAP_DEF_ZONE_LIST     	2
#define ARAP_NODE_ID     	3
#define ARAP_ZONE   	  	4

/* Annex RIP parameter numbers */
#ifdef SYNC
#define RIP_IP_TTL	     	1
#define RIP_ND_FORWARD     	2
#define RIP_ASD_FORWARD     	3
#define RIP_SD_FORWARD     	4
#endif
#define RIP_RIP_ROUTERS     	1
#define RIP_RIP_AUTH    	2

/* centronics parameter numbers */
#define PRINTER_OLTOU		2
#define PRINTER_MCOL		3
#define	PRINTER_OTABS		4
#define PRINTER_TYPE		5
#define PRINTER_SPEED		6
#define PRINTER_CRLF		7
#define PRINTER_KEEPALIVE 	8

/* interface RIP parameter numbers */
#define IF_RIP_SEND_VERSION    	1
#define IF_RIP_RECV_VERSION    	2
#define IF_RIP_HORIZON    	3
#define IF_RIP_DEFAULT_ROUTE   	4
#define IF_RIP_DOMAIN    	5
#define IF_RIP_SUB_ADVERTISE   	6
#define IF_RIP_SUB_ACCEPT    	7
#define IF_RIP_ADVERTISE    	8
#define IF_RIP_ACCEPT    	9

/* abort codes */
#define BAD_TYPE		1
#define BAD_COUNT		2
#define BAD_PARAM		3
#define WRITE_FAILURE		4
#define	TOO_MANY_SESSIONS	5
#define BAD_DEVICE		6
#define INTERNAL_ERROR		7

/* miscellaneous */
#define READ_MEM_MAX 0x400

/* RESET_ANNEX parameter values */

#define RESET_ANNEX_ALL		0
#define RESET_ANNEX_SECURITY	1
#ifndef MICRO_ELS
#define RESET_ANNEX_MOTD	2
#define RESET_ANNEX_NAMESERVER	3
#define RESET_ANNEX_MACRO	4
#define RESET_ANNEX_LAT		5
#define RESET_ANNEX_MODEM_TAB	6
#define RESET_ANNEX_DIALOUT_TAB	7
#define RESET_ANNEX_SYSLOG	8
#define RESET_ANNEX_MAX		8    /* bump this count when adding a reset */
#else
#define RESET_ANNEX_NAMESERVER  2
#define RESET_ANNEX_MACRO       3
#define RESET_ANNEX_LAT         4
#define RESET_ANNEX_SYSLOG	5
#define RESET_ANNEX_MAX         5
#endif

#define ENET_ADDR_SZ		6
#define MOP_PASSWD_SZ		8

/* LAT defines shared by netadm and na files */

#define	LAT_GROUP_SZ	32	/* for lat groups, also in inc/rom/e2rom.h */

/* parser configuration file parameters */
#define READ_GATEWAYS			0x0001
#define READ_ROTARIES			0x0002
#define READ_MACROS			0x0004
#define RESET_MACROS			0x0008
#define READ_SERVICES			0x0010
#define RESET_SERVICES			0x0020
#define READ_MODEM_TABLE		0x0040
#define RESET_MODEM_TABLE		0x0080
#define READ_DIALOUT_TABLE              0x0100
#define RESET_DIALOUT_TABLE             0x0200

