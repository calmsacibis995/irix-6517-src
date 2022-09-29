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
 * (Include file) OR (Module) description:
 *	%$(description)$%
 *
 * Original Author: %$(author)$%	Created on: %$(created-on)$%
 *
 * Revision Control Information:
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/inc/na/RCS/cmd.h,v 1.2 1996/10/04 12:03:45 cwilson Exp $
 *
 * This file created by RCS from $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/inc/na/RCS/cmd.h,v $
 *
 * Revision History:
 *
 * $Log: cmd.h,v $
 * Revision 1.2  1996/10/04 12:03:45  cwilson
 * latest rev
 *
 * Revision 1.107.102.4  1995/01/11  15:54:02  carlson
 * SPR 3342 -- Removed TMUX (not supported until R9.3).
 *
 * Revision 1.107.102.3  1994/12/22  12:00:18  bpf
 * #define's modified to reduce nesting - spr.3915
 *
 * Revision 1.107.102.2  1994/12/06  09:34:35  carlson
 * Disabled LAT groups and multicast timer when LAT is aconfiged out.
 *
 * Revision 1.107.102.1  1994/09/21  10:24:09  bullock
 * spr.3534 - Changed chap_auth_name Annex3 version to V_BIG_BIRD_N instead
 * of V_7_1_DEC - it was never in 7.1         Reviewed by Russ L.
 *
 * Revision 1.107  1994/09/10  13:01:53  bullock
 * spr.2512 - Make printer_speed an Annex3 parameter for use in choosing
 * between postscript operation or high speed operation for parallel
 * printer.  Reviewed by Russ L.
 *
 * Revision 1.106  1994/06/20  17:34:27  russ
 * cannot use BYTE_P in conversion type definitions.
 *
 * Revision 1.105  1994/06/20  16:12:49  russ
 * IPX_DUMP_PWD is an ipx string now! use IPX_STRING_P conversion stuff.
 *
 * Revision 1.104  1994/06/20  15:15:37  russ
 * changed IPX_FRAME_TYPE defined type to BYTE_P.
 *
 * Revision 1.103  1994/06/20  12:22:04  russ
 * added multicast_timer as a lat subsystem parameter.
 *
 * Revision 1.102  1994/06/20  12:13:36  russ
 * added lock_enable, passwd_limit, and chap_auth_name to the list of
 * security subsystem parameters.
 *
 * Revision 1.101  1994/06/14  10:42:03  bullock
 * spr.2802 - MOP_PASSWD type is MOP_PASSWD_P, not STRING_P
 *
 * Revision 1.100  1994/05/23  17:30:07  russ
 * Bigbird na/adm merge.
 *
 * Revision 1.97.2.12  1994/05/09  18:01:04  reeve
 * Added dialout keyword for 'adm reset annex'.
 *
 * Revision 1.97.2.11  1994/05/04  19:49:18  couto
 * Updated sync parameter related defines.
 *
 * Revision 1.97.2.10  1994/04/12  20:51:18  geiser
 * DEC user interface code
 *
 * Revision 1.97.2.9  1994/03/07  13:27:26  russ
 * removed all X25 trunk code.
 *
 * Revision 1.97.2.8  1994/02/25  15:26:06  russ
 * ipx_dump_password has been changed to 16 bytes max.
 *
 * Revision 1.97.2.7  1994/02/22  11:18:37  russ
 * changed all admin commands to return errno_t instead of void. This
 * was a large signal black-hole.
 *
 * Revision 1.97.2.6  1994/02/14  09:11:49  russ
 * Add IPX params; Add chap_auth_name param; hide the kerberos cmds
 * and categories; default_modem_hangup removed from post-8.0
 * as was modem_acc_entries.
 *
 * Revision 1.97.2.5  1994/02/05  08:49:17  russ
 * seg_jumper_5390 is never displayed or settable by na/adm
 *
 * Revision 1.97.2.4  1994/02/04  16:30:09  russ
 * Added annex, asynchronous port, and synchronous port categories for
 * na definitions. Changed DEC to MOP. Disabled all kerberos code.
 * Disable display or setting of jumper_5390.
 *
 * Revision 1.97.2.3  1994/01/15  15:52:12  reeve
 * Removed "dialout" as a keyword used with "reset annex".
 *
 * Revision 1.97.2.2  1994/01/06  15:31:02  wang
 * Bigbird NA/Admin params support.
 *
 * Revision 1.97.2.1  1993/12/12  12:50:25  reeve
 * Additions for "reset ann dialout" within admin and na.
 *
 * Revision 1.99  1994/05/18  17:04:09  russ
 * Add reset annex syslog.
 *
 * Revision 1.98  1994/02/17  13:11:45  defina
 * *** empty log message ***
 *
 * Revision 1.97.1.2  1994/02/11  14:40:20  sasson
 * Added debug switch for the 5390 cmb jumper.
 *
 * Revision 1.97.1.1  1994/01/26  13:57:40  geiser
 * Added new parameters for modem management and 5390 cmb jumper
 * (code by David Wang; check-in by Maryann Geiser)
 *
 * Revision 1.97  1993/10/06  18:51:33  reeve
 * Fixed glitch in removal of params due to option key setting
 *
 * Revision 1.96  1993/09/17  12:16:52  russ
 * added allow_compression for ppp re: spr.2038!
 *
 * Revision 1.95  1993/09/03  15:21:04  grant
 * Made the ARAP_ZONE param use CNV_ZONE instead of CNV_ADM_STRING
 *
 * Revision 1.94  1993/07/27  20:25:17  wang
 * na/admin interface redesign changes.
 *
 * Revision 1.93  1993/07/14  22:52:54  raison
 * take out acp_key for Micro-ELS
 *
 * Revision 1.92  93/06/30  18:01:22  wang
 * Removed DPTG support.
 * 
 * Revision 1.91  93/06/08  15:51:06  bullock
 * spr.1713 - Change the pt_version for timeserver host to make it
 * an 8.0 parameter, not a 7.1 parameter
 * 
 * Revision 1.90  93/05/20  18:02:25  carlson
 * SPR 1167 -- fixed some misleading na progress messages.
 * SPR 1580 -- added in SLIP and DNS to the 7.1 ELS.
 * Reviewed by David Wang.
 * 
 * Revision 1.89  93/05/12  17:43:28  carlson
 * Fixed lots of random consistency errors and removed bogus NARAP.
 * 
 * Revision 1.88  93/05/12  16:24:50  carlson
 * R7.1 -- Added new index for Micro Annex ELS hardware type and updated
 * tables, added SNMP check, and fixed bracket matching.
 * 
 * Revision 1.87  93/05/12  08:38:32  raison
 * remove multicast_timer with void_cat
 * 
 * Revision 1.86  93/05/10  14:41:04  wang
 * Fixed Spr1533, display error for "show port interface".
 * 
 * Revision 1.85  93/04/16  09:51:30  sasson
 * Missing comma.
 * 
 * Revision 1.84  93/03/30  11:04:25  raison
 * reserve parm ids for decserver and add them, turned off
 * 
 * Revision 1.83  93/03/25  13:45:26  wang
 * Added option_key param for AppleTalk and TN3270 and removed appletalk_key.
 * 
 * Revision 1.82  93/03/17  15:22:10  wang
 * Added encode define for AppleTalk default_zone_list param.
 * 
 * Revision 1.81  93/03/15  17:10:55  wang
 * Added in interface parameters help text.
 * 
 * Revision 1.80  93/03/03  15:12:52  wang
 * Removed AppleTalk THIS_NET param and changed NODE_ID param size.
 * 
 * Revision 1.79  93/03/03  13:16:10  wang
 * Changes per port AppleTalk at_nodeid param from BYTE to LONG_CARDINAL_P
 * to hold net and node value.
 * 
 * Revision 1.78  93/03/01  18:42:14  wang
 * Support "show port serial" group params.
 * 
 * Revision 1.77  93/03/01  17:41:06  wang
 * Changed per port AppleTalk params to use Boolean instead of BYTE.
 * 
 * Revision 1.76  93/03/01  15:30:59  wang
 * Fixed set rip_sub_accept param problem.
 * 
 * Revision 1.75  93/02/27  15:03:36  wang
 * Fixed show lat category parameters problem. Also, don't try to display DEC
 * interface related parameters in Rel8.0
 * 
 * Revision 1.74  93/02/26  20:32:25  wang
 * Changed a_router parameter into ethernet addresss format.
 * 
 * Revision 1.73  93/02/25  17:44:05  wang
 * Fixed the NA break problem.
 * 
 * Revision 1.72  93/02/25  12:50:49  carlson
 * SPR 1319 -- Added TIMESERVER_HOST in time group.
 * 
 * Revision 1.71  93/02/19  15:14:42  carlson
 * Added three TCP keepalive timers -- box, serial and parallel.
 * 
 * Revision 1.70  93/02/18  21:51:34  raison
 * added DECserver Annex parameters
 * 
 * Revision 1.69  93/02/18  13:21:22  wang
 * Removed un-needed appletalk and rip parameters.
 * 
 * Revision 1.68  93/02/09  13:11:53  wang
 * Use different type for encode and decode for box rip_routers.
 * 
 * Revision 1.67  93/02/06  21:42:22  sasson
 * Changes reset_parms[]: added the ability to reload the modem info table.
 * 
 * Revision 1.66  93/02/02  11:32:42  carlson
 * Looks like that data structure changed on me.
 * 
 * Revision 1.65  93/02/01  17:58:21  carlson
 * Added Annex parameter -- IP broadcast forwarding.
 * 
 * Revision 1.64  93/01/29  13:07:24  wang
 * Fixed show annex router category problem.
 * 
 * Revision 1.63  93/01/22  17:47:52  wang
 * New parameters support for Rel8.0
 * 
 * Revision 1.62  92/12/04  15:16:04  russ
 * CNV_INT5OFF used for FORWARDING_TIMER.
 * 
 * Revision 1.61  92/11/02  15:49:06  russ
 * Added new printer auto linefeed param.
 * 
 * Revision 1.60  92/10/30  14:37:33  russ
 * Added loose source route annex parameter. fixed some defines.
 * 
 * Revision 1.59  92/10/15  14:04:26  wang
 * Added new NA parameters for ARAP support.
 * 
 * Revision 1.58  92/08/24  15:15:33  russ
 * fix define for PORT_LAT_GROUP per annex2.  Disable NEED_DSR for annex2.
 * reviewed by M. Bullock.
 * 
 * Revision 1.57  92/08/06  11:30:58  reeve
 * Small fixes for Sun compiler.
 * 
 * Revision 1.56  92/08/03  15:34:31  reeve
 * Have version mask biff dptg_settings and port_multiplex for post 6.2 images.
 * 
 * Revision 1.55  92/08/03  14:11:43  reeve
 * Made default_modem_hangup parameter dependent on conf. option, RDRP.
 * 
 * Revision 1.54  92/07/21  12:17:11  reeve
 * Made TFTP_PROTO macro consistent with other "parameter inclusion"
 * macros.
 * 
 * Revision 1.53  92/07/21  11:54:10  reeve
 * Removed #if's surrounding TFTP_DIR_NAME and TFTP_DUMP_NAME params.
 * 
 * Revision 1.52  92/06/25  15:02:38  barnes
 * change definition for LAT_NO_A2
 * 
 * Revision 1.51  92/06/15  11:39:10  barnes
 * correct problem for ANNEX-II builds introduced by changes for new
 * NA displays
 * 
 * Revision 1.50  92/06/12  14:10:05  barnes
 * many changes to regroup parameters for display by NA and CLI admin
 * 
 * Revision 1.49  92/05/09  17:18:46  raison
 * make all lat groups the same conversion type
 * 
 * Revision 1.48  92/05/09  13:56:59  russ
 * Added a VOID_CAT for the ps_history_buffer for non-i386's.
 * 
 * Revision 1.47  92/05/07  18:28:17  reeve
 * Turned off PPP_ACTOPEN.
 * 
 * Revision 1.46  92/04/28  10:06:19  bullock
 * spr.756 - Add banner parameter
 * 
 * Revision 1.45  92/04/10  15:05:36  reeve
 * Enabled PPP params for Micro platform.
 * 
 * Revision 1.44  92/04/01  22:24:45  russ
 * Add new slave buffering command.
 * 
 * Revision 1.43  92/03/24  17:51:02  sasson
 * Fixed local admin on Annex-II's: could not set any annex parameter past
 * syslog_host. Added VOID_CAT for syslog_port.
 * 
 * Revision 1.42  92/03/18  15:17:56  bullock
 * remove login_timer by changing DEV_CAT to VOID_CAT
 * (spr.662)
 * 
 * Revision 1.25.1.7  92/03/18  14:56:49  bullock
 * remove login_timer by changing DEV_CAT to VOID_CAT
 * 
 * Revision 1.25.1.6  92/02/28  16:54:20  reeve
 * For admin, made RING_PRIORITY dependent on PRONET_FOUR being
 * defined > 0.  Also made cmd.h "multiply-includable" for defines
 * of box params.
 *
 * Revision 1.41  92/03/09  13:58:06  raison
 * added conversion types for timezonewest, network_turnaround, slip_metric,
 * and input_buffer_size.
 *
 * Revision 1.40  92/03/07  13:05:29  raison
 * added CNV_PORT, changed syslog_port to CNV_PORT type
 * 
 * Revision 1.39  92/03/03  15:13:33  reeve
 * Changed ring_priority param to be dependent on PRONET_FOUR being defined.
 * 
 * Revision 1.38  92/02/28  16:24:12  wang
 * Removed syslog_port support for Annex-II.
 * 
 * Revision 1.37  92/02/20  17:04:05  wang
 * Added support for new NA parameter syslog_port
 * 
 * Revision 1.36  92/02/18  17:44:41  sasson
 * Added support for 3 LAT parameters(authorized_group, vcli_group, lat_q_max).
 * 
 * Revision 1.35  92/02/07  16:00:39  carlson
 * Made DPTG and LAT selectable in local admin.
 * 
 * Revision 1.34  92/02/06  17:04:31  carlson
 * Fixed multidefines -- blast that rcsmerge!
 *
 * Revision 1.33  92/02/06  11:28:03  carlson
 * SPR 543 - added latb_enable flag so LAT data-b packets can be ignored.
 * 
 * Revision 1.32  92/02/05  10:40:12  reeve
 * Fixed up version mask and default display for DIALUP_ADDR param.
 *
 * Revision 1.31  92/02/01  20:49:07  raison
 * enabled printer parms for micros.
 *
 * Revision 1.30  92/01/31  22:52:48  raison
 * added printer_set and printer_speed parm
 * 
 * Revision 1.29  92/01/31  13:34:34  reeve
 * Enabled more than one file to acces port param defines.
 * 
 * Revision 1.28  92/01/31  12:44:42  wang
 * Add new NA parameter for config.annex file
 * 
 * Revision 1.27  92/01/27  18:59:26  barnes
 * changes for selectable modules
 * 
 * Revision 1.26  92/01/21  09:34:09  grant
 * Removed NPPP.  Made PPP conditional via the version mask or the
 * VOID_CAT switching for local adm.
 *
 * Revision 1.25  92/01/16  17:11:47  reeve
 * Removed RING_PRIORITY parameter for ELS.
 *
 * Revision 1.24  92/01/16  15:27:32  carlson
 * Removed macro file name parameter.
 *
 * Revision 1.23  92/01/10  13:32:49  reeve
 * For ELS, removed CLI_SECURITY, CONNECT_SECURITY, and PORT_SERVER_SECURITY
 * parameters.
 *
 * Revision 1.22  92/01/10  11:56:31  russ
 * changed slip security and slip_mtu_size to be R6.2 based.
 *
 * Revision 1.21  92/01/09  13:53:48  carlson
 * Added slip security flag.
 *
 * Revision 1.20  91/12/30  15:22:30  reeve
 * For Micro Annex ELS, disabled use of certain Annex and port params.
 * Also (for ELS), took out 'motd' from reset_usage string.
 * 
 * Revision 1.19  91/12/13  13:22:22  raison
 * change attn_char fields to use new converion id
 * 
 * Revision 1.18  91/12/11  17:55:35  carlson
 * Fixed NPPP usage.
 * 
 * Revision 1.17  91/11/22  14:05:22  reeve
 * Added new net parameter, dialup_address.
 * 
 * Revision 1.16  91/11/22  12:32:50  russ
 * Add micro name
 * oops, Add macro file name.
 * 
 * Revision 1.15  91/11/05  10:29:52  russ
 * Add slip_mtu_size to allow booting out of cslip ports.
 * 
 * Revision 1.14  91/11/04  13:14:59  reeve
 * Enabled use of "slip", "net", or "PPP" for network port parameters.
 * 
 * Revision 1.13  91/10/30  15:09:50  barnes
 * add new NA parameter to enable/disable SNMP sets
 * 
 * Revision 1.12  91/10/28  09:59:08  carlson
 * Changed ATTN_CHAR to string type for SPR 361.
 * 
 * Revision 1.11  91/10/16  10:17:39  russ
 * Added off as a settable value for forwarding_timer.
 * 
 * Revision 1.10  91/10/15  11:31:51  russ
 * Added a real s/w version 7.0.
 * 
 * Revision 1.9  91/10/14  16:21:56  russ
 * Use the new defines for base_version to most recent!  These will save a
 * lot of typing when a new version is added.
 * 
 * Revision 1.8  91/10/09  19:23:53  reeve
 * Added categories for SLIP, NET, and PPP.
 * 
 * Revision 1.7  91/10/04  14:53:17  reeve
 * Added PPP command definitions.
 * 
 * Revision 1.6  91/07/31  20:44:09  raison
 * added telnet_crlf port parameter
 * 
 * Revision 1.5  91/07/30  09:34:09  raison
 * fixed errors reported by other compiler - per A. Barnett
 * 
 * Revision 1.4  91/07/26  15:35:47  raison
 * unsupport tftp NA parameter fields for Annex-II
 * 
 * Revision 1.3  91/07/12  15:56:57  sasson
 * Obsoleted leap_protocol_on (DO_LEAP_PROTOCOL).
 * 
 * Revision 1.2  91/07/02  13:18:14  raison
 * added tftp_dir_name and tftp_dump_name parameters
 * 
 * Revision 1.1  91/06/13  12:22:39  raison
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *  DATE: 	$Date: 1996/10/04 12:03:45 $
 *  REVISION:	$Revision: 1.2 $
 *
 ****************************************************************************
 */
#ifndef CMD_H_PARAMS_ONLY
#ifdef NA
char *changes_will = "\tChanges will take effect ";
char *immediately = "immediately.\n";
char *imm_local = "immediately for local sessions, and, for na\n\
\tsessions, at next %s boot";
char *next_boot = "at next %s boot";
char *or_passwd_cmd = "at next CLI passwd change or\n\t";
char *annex_reset_secureserver = ", reset %s security,";
char *annex_reset_nameserver = ", reset %s nameserver,";
char *annex_reset_motd = ", reset %s motd,";
char *annex_reset_lat = ", reset %s lat,";
char *cr_reset_all = "\n\tor reset %s all.\n";
char *or_reset_all = " or reset %s all.\n";

char *annex_msg   = ".\n";
char *port_msg    = " or port reset.\n";
char *sync_msg    = " or sync reset.\n";
char *printer_msg = " or printer reset.\n";
char *interface_msg = " or interface reset.\n";

#ifndef isdigit
#define isdigit(x) (x >= '0' && x <= '9')
#endif

#endif

extern time_t time();

char	*get_password();

#define A_COMMAND		0
#define PARAM_CLASS		1
#define BOX_PARAM		2
#define PORT_PARAM		3
#define PRINTER_PARAM		4
#define HELP_ENTRY		5
#define INTERFACE_PARAM		6
#define SYNC_PARAM		7
#define BOX_CATEGORY		8
#define PORT_CATEGORY		9
#define SYNC_CATEGORY		10

#ifdef NA
char *usage_table[] =
{
	"command",
	"parameter class",
	BOX_PARAMETER,
	"asynchronous port parameter",
	"printer parameter",
	"help entry",
	"interface parameter",
	"synchronous port parameter",
	"annex parameter category",
	"asynchronous port category",
	"synchronous port category",
};

#define BOX_CMD			0
#define BOOT_CMD		(BOX_CMD + 1)
#define COMMENT_CMD		(BOX_CMD + 2)
#define COPY_CMD		(BOX_CMD + 3)
#define DUMPBOOT_CMD		(BOX_CMD + 4)
#define ECHO_CMD		(BOX_CMD + 5)
#define PASSWORD_CMD		(BOX_CMD + 6)
#define READ_CMD		(BOX_CMD + 7)
#define WRITE_CMD		(BOX_CMD + 8)
#define BROADCAST_CMD		(BOX_CMD + 9)
#else
#define BROADCAST_CMD		0
#endif
#define HELP_CMD		(BROADCAST_CMD + 1)
#define PORT_CMD		(BROADCAST_CMD + 2)
#define QUIT_CMD 		(BROADCAST_CMD + 3)
#define RESET_CMD		(BROADCAST_CMD + 4)
#define SET_CMD			(BROADCAST_CMD + 5)
#define SHOW_CMD		(BROADCAST_CMD + 6)
#define QUEST_CMD		(BROADCAST_CMD + 7)
#define PRINTER_CMD		(BROADCAST_CMD + 8)
#define INTERFACE_CMD		(BROADCAST_CMD + 9)
#define	SYNC_CMD		(BROADCAST_CMD + 10)

#define NCOMMANDS		(BROADCAST_CMD + 11)

#ifdef NA
int	adm_box_cmd();
int	adm_boot_cmd();
int	adm_comment_cmd();
int	adm_copy_cmd();
int	adm_dumpboot_cmd();
int	adm_echo_cmd();
int	adm_password_cmd();
int	adm_read_cmd();
int	adm_write_cmd();
int	adm_broadcast_cmd();
int	adm_help_cmd();
int	adm_port_cmd();
int	adm_quit_cmd();
int	adm_reset_cmd();
int	adm_set_cmd();
int	adm_show_cmd();
int	adm_printer_cmd();
int	adm_interface_cmd();
int	adm_sync_cmd();

int (*cmd_actions[])() =
{
	adm_box_cmd,
	adm_boot_cmd,
	adm_comment_cmd,
	adm_copy_cmd,
	adm_dumpboot_cmd,
	adm_echo_cmd,
	adm_password_cmd,
	adm_read_cmd,
	adm_write_cmd,
	adm_broadcast_cmd,
	adm_help_cmd,
	adm_port_cmd,
	adm_quit_cmd,
	adm_reset_cmd,
	adm_set_cmd,
	adm_show_cmd,
	adm_help_cmd,
	adm_printer_cmd,
	adm_interface_cmd,
	adm_sync_cmd
};
#else	/* ! NA */
errno_t	adm_broadcast_cmd();
errno_t	adm_help_cmd();
errno_t	adm_port_cmd();
errno_t	adm_quit_cmd();
errno_t	adm_reset_cmd();
errno_t	adm_set_cmd();
errno_t	adm_show_cmd();
errno_t	adm_printer_cmd();
errno_t	adm_interface_cmd();
errno_t	adm_sync_cmd();

errno_t (*cmd_actions[])() =
{
	adm_broadcast_cmd,
	adm_help_cmd,
	adm_port_cmd,
	adm_quit_cmd,
	adm_reset_cmd,
	adm_set_cmd,
	adm_show_cmd,
	adm_help_cmd,
	adm_printer_cmd,
	adm_interface_cmd,
	adm_sync_cmd
};
#endif

char *cmd_spellings[NCOMMANDS + 1];

#define BOX_CLASS		0
#define PORT_CLASS		(BOX_CLASS + 1)
#define PRINTER_CLASS		(PORT_CLASS + 1)	
#define INTERFACE_CLASS		(PRINTER_CLASS + 1)	
#define SYNC_CLASS		(INTERFACE_CLASS + 1)

#define NCLASSES		(SYNC_CLASS + 1)

char *param_classes[NCLASSES + 1];

#endif /* ifndef CMD_H_PARAMS_ONLY */


/********************************************************************
 ********************************************************************
 **   NOTE:
 **
 **	IF YOU ALTER THE ORDER OF THESE PARAMETERS YOU MUST CHANGE
 **	THE ORDER OF THE ENTRIES IN THE TABLE annexp_table  
 **
 ********************************************************************
 */

/*
 * The following are display groupings which define the way
 * of paramters in the various menus as display grouping is not
 * related to E2ROM paramter offset!!!
 */

#define BOX_GENERIC_GROUP	0

#define INET_ADDR		BOX_GENERIC_GROUP
#define SUBNET_MASK		BOX_GENERIC_GROUP + 1
#define PREF_LOAD		BOX_GENERIC_GROUP + 2
#define PREF_DUMP		BOX_GENERIC_GROUP + 3
#define LOADSERVER_BCAST        BOX_GENERIC_GROUP + 4
#define BROAD_ADDR		BOX_GENERIC_GROUP + 5
#define LOADUMP_GATEWAY		BOX_GENERIC_GROUP + 6
#define LOADUMP_SEQUENCE	BOX_GENERIC_GROUP + 7
#define IMAGE_NAME		BOX_GENERIC_GROUP + 8
#define MOTD			BOX_GENERIC_GROUP + 9
#define CONFIG_FILE		BOX_GENERIC_GROUP + 10
#define AUTH_AGENT		BOX_GENERIC_GROUP + 11
#define NROUTED			BOX_GENERIC_GROUP + 12
#define SERVER_CAP		BOX_GENERIC_GROUP + 13
#define SELECTED_MODULES	BOX_GENERIC_GROUP + 14
#define TFTP_DIR_NAME		BOX_GENERIC_GROUP + 15
#define TFTP_DUMP_NAME		BOX_GENERIC_GROUP + 16
#define IPENCAP_TYPE		BOX_GENERIC_GROUP + 17
#define RING_PRIORITY 		BOX_GENERIC_GROUP + 18
#define IP_FWD_BCAST		BOX_GENERIC_GROUP + 19
#define TCP_KEEPALIVE           BOX_GENERIC_GROUP + 20
#define OPTION_KEY		BOX_GENERIC_GROUP + 21
#define ACC_ENTRIES            	BOX_GENERIC_GROUP + 22
#define JUMPER_5390            	BOX_GENERIC_GROUP + 23
#define SESSION_LIMIT         	BOX_GENERIC_GROUP + 24
#define OUTPUT_TTL            	BOX_GENERIC_GROUP + 25

#define BOX_VCLI_GROUP        	BOX_GENERIC_GROUP + 26

#define VCLI_LIMIT		BOX_VCLI_GROUP
#define CLI_PROMPT_STR		BOX_VCLI_GROUP + 1
#define VCLI_SEC_ENA            BOX_VCLI_GROUP + 2
#define VCLI_PASSWORD           BOX_VCLI_GROUP + 3

#define	BOX_NAMESERVER_GROUP	BOX_VCLI_GROUP + 4

#define NAMESERVER_BCAST        BOX_NAMESERVER_GROUP 
#define NRWHOD			BOX_NAMESERVER_GROUP + 1
#define PRIMARY_NS_ADDR		BOX_NAMESERVER_GROUP + 2
#define PRIMARY_NS		BOX_NAMESERVER_GROUP + 3
#define SECONDARY_NS_ADDR	BOX_NAMESERVER_GROUP + 4
#define SECONDARY_NS		BOX_NAMESERVER_GROUP + 5
#define HTABLE_SZ		BOX_NAMESERVER_GROUP + 6
#define NMIN_UNIQUE		BOX_NAMESERVER_GROUP + 7

#define BOX_SECURITY_GROUP	BOX_NAMESERVER_GROUP + 8

#define ENABLE_SECURITY		BOX_SECURITY_GROUP
#define SECURSERVER_BCAST       BOX_SECURITY_GROUP + 1
#define PREF_SECURE_1		BOX_SECURITY_GROUP + 2
#define PREF_SECURE_2		BOX_SECURITY_GROUP + 3
#define NET_TURNAROUND		BOX_SECURITY_GROUP + 4
#define LOOSE_SOURCE_RT		BOX_SECURITY_GROUP + 5
#define ACP_KEY			BOX_SECURITY_GROUP + 6
#define	BOX_PASSWORD		BOX_SECURITY_GROUP + 7
#define ALLOW_SNMP_SETS		BOX_SECURITY_GROUP + 8
#define LOCK_ENABLE           	BOX_SECURITY_GROUP + 9
#define PASSWD_LIMIT          	BOX_SECURITY_GROUP + 10
#define CHAP_AUTH_NAME		BOX_SECURITY_GROUP + 11

#ifdef POST_BIGBIRD
/* Sorry no other way to remove this from N/A right now */
#define BOX_KERBEROS_GROUP   	BOX_SECURITY_GROUP + 12
#endif

#define KERB_SECURITY_ENA	BOX_SECURITY_GROUP + 12
#define KERB_HOST             	BOX_SECURITY_GROUP + 13
#define TGS_HOST              	BOX_SECURITY_GROUP + 14
#define TELNETD_KEY           	BOX_SECURITY_GROUP + 15
#define KERBCLK_SKEW          	BOX_SECURITY_GROUP + 16

#define BOX_TIME_GROUP        	BOX_SECURITY_GROUP + 17

#define TIMESERVER_BCAST	BOX_TIME_GROUP
#define TZ_DLST			BOX_TIME_GROUP + 1
#define	TZ_MINUTES		BOX_TIME_GROUP + 2
#define TIMESERVER_HOST		BOX_TIME_GROUP + 3

#define BOX_SYSLOG_GROUP	BOX_TIME_GROUP + 4

#define SYSLOG_MASK		BOX_SYSLOG_GROUP
#define SYSLOG_FAC		BOX_SYSLOG_GROUP + 1
#define SYSLOG_HOST		BOX_SYSLOG_GROUP + 2
#define SYSLOG_PORT		BOX_SYSLOG_GROUP + 3

#define BOX_VMS_GROUP           BOX_SYSLOG_GROUP + 4

#define MOP_PREF_HOST           BOX_VMS_GROUP
#define MOP_PASSWD              BOX_VMS_GROUP + 1
#define LOGIN_PASSWD            BOX_VMS_GROUP + 2
#define LOGIN_PROMPT            BOX_VMS_GROUP + 3
#define LOGIN_TIMER         	BOX_VMS_GROUP + 4

#define BOX_LAT_GROUP		BOX_VMS_GROUP + 5

#define KEY_VALUE		BOX_LAT_GROUP
#define HOST_NUMBER 		BOX_LAT_GROUP + 1
#define HOST_NAME 		BOX_LAT_GROUP + 2
#define HOST_ID 		BOX_LAT_GROUP + 3
#define QUEUE_MAX 		BOX_LAT_GROUP + 4
#define SERVICE_LIMIT 		BOX_LAT_GROUP + 5
#define KA_TIMER		BOX_LAT_GROUP + 6
#define CIRCUIT_TIMER 		BOX_LAT_GROUP + 7
#define RETRANS_LIMIT 		BOX_LAT_GROUP + 8
#define GROUP_CODE 		BOX_LAT_GROUP + 9
#define VCLI_GROUPS 		BOX_LAT_GROUP + 10
#define MULTI_TIMER             BOX_LAT_GROUP + 11

#define BOX_ARAP_GROUP		BOX_LAT_GROUP + 12

#define A_ROUTER 		BOX_ARAP_GROUP 
#define DEF_ZONE_LIST 		BOX_ARAP_GROUP + 1
#define NODE_ID 		BOX_ARAP_GROUP + 2
#define ZONE			BOX_ARAP_GROUP + 3

#define BOX_RIP_GROUP		BOX_ARAP_GROUP + 4

#ifdef SYNC
#define IP_TTL			BOX_RIP_GROUP
#define ND_FORWARD 		BOX_RIP_GROUP + 1
#define ASD_FORWARD 		BOX_RIP_GROUP + 2
#define SD_FORWARD 		BOX_RIP_GROUP + 3
#endif
#define RIP_AUTH 		BOX_RIP_GROUP

#define RIP_ROUTERS 		BOX_RIP_GROUP + 1

#define BOX_IPX_GROUP  		BOX_RIP_GROUP + 2

#define IPX_FILE_SERVER        	BOX_IPX_GROUP
#define IPX_FRAME_TYPE		BOX_IPX_GROUP + 1
#define IPX_DUMP_UNAME		BOX_IPX_GROUP + 2
#define IPX_DUMP_PWD		BOX_IPX_GROUP + 3
#define IPX_DUMP_PATH		BOX_IPX_GROUP + 4
#define IPX_DO_CHKSUM		BOX_IPX_GROUP + 5

#define BOX_TMUX_GROUP        	BOX_IPX_GROUP + 6

/* Removed -- TMUX isn't in 9.0 */

#define BOX_GROUPS		BOX_TMUX_GROUP
#define BOX_GENERIC		BOX_GROUPS
#define BOX_VCLI		BOX_GROUPS + 1
#define BOX_NAMESERVER		BOX_GROUPS + 2
#define BOX_SECURITY		BOX_GROUPS + 3
#define BOX_KERBEROS		BOX_GROUPS + 4
#define BOX_TIME		BOX_GROUPS + 5
#define BOX_SYSLOG		BOX_GROUPS + 6
#define BOX_MOP			BOX_GROUPS + 7
#define BOX_LAT			BOX_GROUPS + 8
#define BOX_APPLETALK		BOX_GROUPS + 9
#define BOX_ROUTER		BOX_GROUPS + 10
#define BOX_IPX			BOX_GROUPS + 11
#define ALL_BOX			BOX_GROUPS + 12
#define NBOXP			BOX_GROUPS + 13


#ifndef CMD_H_PARAMS_ONLY

char *annex_params[NBOXP + 1];

#endif /* ifndef CMD_H_PARAMS_ONLY */

/********************************************************************
 ********************************************************************
 **   NOTE:
 **
 **	IF YOU ALTER THE ORDER OF THESE PARAMETERS YOU MUST CHANGE
 **	THE ORDER OF THE ENTRIES IN THE TABLE portp_table  BELOW
 **
 ********************************************************************
 */

#define PORT_GENERIC_GROUP	0

#define PORT_MODE		PORT_GENERIC_GROUP
#define LOCATION		PORT_GENERIC_GROUP + 1
#define PORT_TYPE		PORT_GENERIC_GROUP + 2
#define TERM_VAR		PORT_GENERIC_GROUP + 3
#define PORT_PROMPT		PORT_GENERIC_GROUP + 4
#define USER_INTERFACE		PORT_GENERIC_GROUP + 5
#define PORT_SPEED		PORT_GENERIC_GROUP + 6
#define PORT_AUTOBAUD           PORT_GENERIC_GROUP + 7
#define BITS_PER_CHAR		PORT_GENERIC_GROUP + 8
#define STOP_BITS		PORT_GENERIC_GROUP + 9
#define PARITY			PORT_GENERIC_GROUP + 10
#define MAX_SESSIONS		PORT_GENERIC_GROUP + 11
#define BROADCAST_ON		PORT_GENERIC_GROUP + 12
#define BROADCAST_DIR		PORT_GENERIC_GROUP + 13
#define IMASK_7BITS		PORT_GENERIC_GROUP + 14
#define CLI_IMASK7		PORT_GENERIC_GROUP + 15
#define PS_HISTORY_BUFF		PORT_GENERIC_GROUP + 16
#define BANNER			PORT_GENERIC_GROUP + 17
#define TCPA_KEEPALIVE		PORT_GENERIC_GROUP + 18
#define DEDICATED_ADDRESS	PORT_GENERIC_GROUP + 19
#define DEDICATED_PORT		PORT_GENERIC_GROUP + 20
#define MODEM_VAR 		PORT_GENERIC_GROUP + 21
#define DEF_SESS_MODE           PORT_GENERIC_GROUP + 22

#define PORT_FLOWCONTROL_GROUP  PORT_GENERIC_GROUP + 23

#define CONTROL_LINE_USE	PORT_FLOWCONTROL_GROUP 
#define INPUT_FLOW_CONTROL	PORT_FLOWCONTROL_GROUP + 1
#define INPUT_START_CHAR	PORT_FLOWCONTROL_GROUP + 2
#define INPUT_STOP_CHAR		PORT_FLOWCONTROL_GROUP + 3
#define OUTPUT_FLOW_CONTROL     PORT_FLOWCONTROL_GROUP + 4
#define OUTPUT_START_CHAR       PORT_FLOWCONTROL_GROUP + 5
#define OUTPUT_STOP_CHAR        PORT_FLOWCONTROL_GROUP + 6
#define DUI_FLOW                PORT_FLOWCONTROL_GROUP + 7
#define DUI_IFLOW               PORT_FLOWCONTROL_GROUP + 8
#define DUI_OFLOW               PORT_FLOWCONTROL_GROUP + 9
#define INPUT_BUFFER_SIZE       PORT_FLOWCONTROL_GROUP + 10
#define BIDIREC_MODEM		PORT_FLOWCONTROL_GROUP + 11
#define IXANY_FLOW_CONTROL	PORT_FLOWCONTROL_GROUP + 12
#define NEED_DSR		PORT_FLOWCONTROL_GROUP + 13

#define PORT_TIMER_GROUP	PORT_FLOWCONTROL_GROUP + 14

#define FORWARDING_TIMER	PORT_TIMER_GROUP
#define FORWARD_COUNT	        PORT_TIMER_GROUP + 1
#define INACTIVITY_CLI		PORT_TIMER_GROUP + 2
#define INACTIVITY_TIMER	PORT_TIMER_GROUP + 3
#define INPUT_ACT		PORT_TIMER_GROUP + 4
#define OUTPUT_ACT		PORT_TIMER_GROUP + 5
#define RESET_IDLE              PORT_TIMER_GROUP + 6
#define LONG_BREAK		PORT_TIMER_GROUP + 7
#define SHORT_BREAK		PORT_TIMER_GROUP + 8

#define PORT_SECURITY_GROUP	PORT_TIMER_GROUP + 9

#define PORT_NAME		PORT_SECURITY_GROUP
#define CLI_SECURITY		PORT_SECURITY_GROUP + 1
#define CONNECT_SECURITY	PORT_SECURITY_GROUP + 2
#define PORT_SERVER_SECURITY	PORT_SECURITY_GROUP + 3
#define PORT_PASSWORD           PORT_SECURITY_GROUP + 4
#define IPSO_CLASS       	PORT_SECURITY_GROUP + 5
#define IPX_SECURITY          	PORT_SECURITY_GROUP + 6


#define PORT_LOGINUSR_GROUP   	PORT_SECURITY_GROUP + 7

#define DUI_PASSWD     		PORT_LOGINUSR_GROUP
#define DUI_INACT_TIMEOUT       PORT_LOGINUSR_GROUP + 1

#define PORT_CHAR_GROUP       	PORT_LOGINUSR_GROUP + 2

#define ATTN_CHAR		PORT_CHAR_GROUP
#define INPUT_ECHO		PORT_CHAR_GROUP + 1
#define TELNET_ESC		PORT_CHAR_GROUP + 2
#define TELNET_CRLF		PORT_CHAR_GROUP + 3
#define MAP_U_TO_L		PORT_CHAR_GROUP + 4
#define MAP_L_TO_U_PORT		PORT_CHAR_GROUP + 5
#define CHAR_ERASING		PORT_CHAR_GROUP + 6
#define LINE_ERASING		PORT_CHAR_GROUP + 7
#define PORT_HARDWARE_TABS	PORT_CHAR_GROUP + 8
#define ERASE_CHAR		PORT_CHAR_GROUP + 9
#define ERASE_WORD		PORT_CHAR_GROUP + 10
#define ERASE_LINE		PORT_CHAR_GROUP + 11
#define REDISPLAY_LINE		PORT_CHAR_GROUP + 12
#define TOGGLE_OUTPUT		PORT_CHAR_GROUP + 13
#define NEWLINE_TERMINAL	PORT_CHAR_GROUP + 14

#define PORT_NETADDR_GROUP	PORT_CHAR_GROUP + 15

#define P_SLIP_LOCALADDR	PORT_NETADDR_GROUP
#define P_SLIP_REMOTEADDR	PORT_NETADDR_GROUP + 1
#define P_PPP_DIALUP_ADDR	PORT_NETADDR_GROUP + 2
#define P_SLIP_METRIC		PORT_NETADDR_GROUP + 3
#define P_SLIP_SECURE		PORT_NETADDR_GROUP + 4
#define P_SLIP_NET_DEMAND_DIAL	PORT_NETADDR_GROUP + 5
#define P_SLIP_NET_INACTIVITY	PORT_NETADDR_GROUP + 6
#define P_SLIP_NET_PHONE	PORT_NETADDR_GROUP + 7
#define P_SLIP_DO_COMP		PORT_NETADDR_GROUP + 8
#define P_SLIP_EN_COMP		PORT_NETADDR_GROUP + 9

#define PORT_SLIP_GROUP		PORT_NETADDR_GROUP + 10

#define P_SLIP_NETMASK		PORT_SLIP_GROUP
#define P_SLIP_LOADUMP_HOST	PORT_SLIP_GROUP + 1
#define P_SLIP_ALLOW_DUMP	PORT_SLIP_GROUP + 2
#define P_SLIP_LARGE_MTU	PORT_SLIP_GROUP + 3
#define P_SLIP_NO_ICMP		PORT_SLIP_GROUP + 4
#define P_SLIP_FASTQ		PORT_SLIP_GROUP + 5

#define PORT_PPP_GROUP		PORT_SLIP_GROUP + 6

#define P_PPP_MRU		PORT_PPP_GROUP
#define P_PPP_ACM		PORT_PPP_GROUP + 1
#define P_PPP_SECURITY		PORT_PPP_GROUP + 2
#define P_PPP_UNAMERMT		PORT_PPP_GROUP + 3
#define P_PPP_PWORDRMT		PORT_PPP_GROUP + 4
#define P_PPP_NCP		PORT_PPP_GROUP + 5

#define PORT_ARAP_GROUP		PORT_PPP_GROUP + 6
#define P_ARAP_AT_GUEST		PORT_ARAP_GROUP
#define P_ARAP_AT_NODEID	PORT_ARAP_GROUP + 1
#define P_ARAP_AT_SECURITY	PORT_ARAP_GROUP + 2
#define P_ARAP_V42BIS		PORT_ARAP_GROUP + 3

#define PORT_TN3270_GROUP	PORT_ARAP_GROUP + 4
#define P_TN3270_PRINTER_HOST	PORT_TN3270_GROUP
#define P_TN3270_PRINTER_NAME	PORT_TN3270_GROUP + 1

#define AUTHORIZED_GROUPS	PORT_TN3270_GROUP + 2
#define LATB_ENABLE		AUTHORIZED_GROUPS + 1

#ifdef ns16000
#define PORT_LAT_GROUP		LATB_ENABLE
#else
#define PORT_LAT_GROUP		AUTHORIZED_GROUPS
#endif

#define PORT_MX_GROUP		AUTHORIZED_GROUPS + 2

#define DEFAULT_HPCL		PORT_MX_GROUP

#define PORT_GENERIC		PORT_MX_GROUP + 1
#define PORT_FLOW		PORT_MX_GROUP + 2
#define PORT_SECURITY		PORT_MX_GROUP + 3
#define PORT_LOGIN		PORT_MX_GROUP + 4
#define PORT_EDITING		PORT_MX_GROUP + 5
#define PORT_SERIAL		PORT_MX_GROUP + 6
#define PORT_SLIP		PORT_MX_GROUP + 7
#define PORT_PPP		PORT_MX_GROUP + 8
#define PORT_LAT		PORT_MX_GROUP + 9
#define PORT_TIMERS		PORT_MX_GROUP + 10
#define PORT_APPLETALK		PORT_MX_GROUP + 11
#define PORT_TN3270		PORT_MX_GROUP + 12

#define ALL_PORTP		PORT_MX_GROUP + 13
#define NPORTP			PORT_MX_GROUP + 14

/*
#define LOGIN_TIMER		11
#define DO_LEAP_PROTOCOL	43
#define P_PPP_ACTOPEN		77
*/

/********************************************************************
 ********************************************************************
 **   NOTE:
 **
 **     IF YOU ALTER THE ORDER OF THESE PARAMETERS YOU MUST CHANGE
 **     THE ORDER OF THE ENTRIES IN THE TABLE syncp_table  BELOW
 **
 ********************************************************************
 */

#define SYNC_GENERIC_GROUP         0

#define S_MODE                SYNC_GENERIC_GROUP
#define S_LOCATION            SYNC_GENERIC_GROUP + 1
#define S_CLOCKING            SYNC_GENERIC_GROUP + 2
#define S_FORCE_CTS           SYNC_GENERIC_GROUP + 3

#define SYNC_PORT_SECURITY_GROUP    SYNC_GENERIC_GROUP + 4

#define S_USRNAME             SYNC_PORT_SECURITY_GROUP
#define S_PORT_PASSWD         SYNC_PORT_SECURITY_GROUP + 1

#define SYNC_NETADDR_GROUP    SYNC_PORT_SECURITY_GROUP + 2

#define S_LOCAL_ADDR          SYNC_NETADDR_GROUP
#define S_REMOTE_ADDR         SYNC_NETADDR_GROUP + 1
#define S_NETMASK             SYNC_NETADDR_GROUP + 2
#define S_METRIC              SYNC_NETADDR_GROUP + 3
#define S_ALW_COMP            SYNC_NETADDR_GROUP + 4
#define S_SECURE              SYNC_NETADDR_GROUP + 5
#define S_DIAL_ADDR           SYNC_NETADDR_GROUP + 6

#define SYNC_PPP_GROUP        SYNC_NETADDR_GROUP + 7

#define S_PPP_MRU             SYNC_PPP_GROUP
#define S_PPP_SECURE_PROTO    SYNC_PPP_GROUP + 1
#define S_PPP_USRNAME_REMOTE  SYNC_PPP_GROUP + 2
#define S_PPP_PASSWD_REMOTE   SYNC_PPP_GROUP + 3
#define S_PPP_NCP             SYNC_PPP_GROUP + 4

#define S_GENERIC             SYNC_PPP_GROUP + 5
#define S_SECURITY            SYNC_PPP_GROUP + 6
#define S_NET		      SYNC_PPP_GROUP + 7
#define S_PPP		      SYNC_PPP_GROUP + 8

#define ALL_SYNCP             SYNC_PPP_GROUP + 9
#define NSYNCP                SYNC_PPP_GROUP + 10


#ifndef CMD_H_PARAMS_ONLY

char *sync_params[NSYNCP + 1];

#endif /* ifndef CMD_H_PARAMS_ONLY */

/********************************************************************
 ********************************************************************
 **   NOTE:
 **
 **	IF YOU ALTER THE ORDER OF THESE PARAMETERS YOU MUST CHANGE
 **	THE ORDER OF THE ENTRIES IN THE TABLE interfacep_table  BELOW
 **
 ********************************************************************
 */

/* RIP per interface parameter */
#define INTERFACE_RIP_GROUP	0

#define RIP_SEND_VERSION	INTERFACE_RIP_GROUP
#define RIP_RECV_VERSION	INTERFACE_RIP_GROUP + 1
#define RIP_HORIZON 		INTERFACE_RIP_GROUP + 2
#define RIP_DEFAULT_ROUTE 	INTERFACE_RIP_GROUP + 3
#define RIP_DOMAIN 		INTERFACE_RIP_GROUP + 4
#define RIP_SUB_ADVERTISE	INTERFACE_RIP_GROUP + 5
#define RIP_SUB_ACCEPT 		INTERFACE_RIP_GROUP + 6
#define RIP_ADVERTISE 		INTERFACE_RIP_GROUP + 7
#define RIP_ACCEPT 		INTERFACE_RIP_GROUP + 8
#define ALL_INTERFACEP		INTERFACE_RIP_GROUP + 9
#define NINTERFACEP		INTERFACE_RIP_GROUP + 10

#ifndef CMD_H_PARAMS_ONLY

char *port_params[NPORTP + 1];
char *interface_params[NINTERFACEP + 1];

#define MAP_L_TO_U_PRINT 	0
#define PRINTER_WIDTH		1
#define PRINT_HARDWARE_TABS	2
#define PRINTER_INTERFACE	3
#define PRINTER_SPD		4
#define PRINTER_CR_CRLF		5
#define TCPP_KEEPALIVE		6

#define ALL_PRINTER		7
#define NPRINTP			8

char *printer_params[NPRINTP + 1];

#define	NRESET			9

char *reset_params[NRESET + 1] =
{
	"all",
	"security",
	"motd",
	"nameserver",
	"macros",
	"lat",
	"modem_table",
	"dialout",
	"syslog",
	(char *)0
};


/*
 * These defines are used to exclude parameters from local adm.
 * To exclude a param have the MACHINE defines convert the
 * category into the VOID_CAT.  Use the machine version masks
 * to exclude the parameter from NA.
 */

#if (NRDRP > 0)
#define RDRP(x) (x)
#else
#define RDRP(x) (VOID_CAT)
#endif

#if (NPRONET_FOUR > 0)
#define PRONET_FOUR(x) (x)
#else
#define PRONET_FOUR(x) (VOID_CAT)
#endif

#if (NARAP > 0)
#define ARAP(x) (x)
#else
#define ARAP(x)	(VOID_CAT)
#endif

#if (NPPP > 0)
#define PPP(x) (x)
#else
#define PPP(x)	(VOID_CAT)
#endif

#if (NSLIP > 0)
#define SLIP(x) (x)
#else
#define SLIP(x) (VOID_CAT)
#endif

/* Used for things that are *NOT* in the ELS */
#ifdef MICRO_ELS
#define NO_ELS(x) (VOID_CAT)
#else
#define NO_ELS(x) (x)
#endif

#if NLAT > 0
#define LAT(x)	(x)

#if defined(NA) || defined(i386)
#define LAT_NO_A2(x)	(x)
#else
#define LAT_NO_A2(x)	(VOID_CAT)
#endif

#else
#define LAT(x)	(VOID_CAT)
#define LAT_NO_A2(x)	(VOID_CAT)
#endif /* NLAT > 0 */

#if NDEC > 0
#define DEC(x)  (x)
#else
#define DEC(x)  (VOID_CAT)
#endif

#if NDPTG > 0
#define DPTG(x)	(x)
#else
#define DPTG(x)	(VOID_CAT)
#endif

#if defined(NA) || defined(i386)
#define NO_A2(x) (x)
#else
#define NO_A2(x) (VOID_CAT)
#endif

#if defined(MICRO_ANNEX) || defined(NA)
#define MICRO_ONLY(x) (x)
#else
#define MICRO_ONLY(x) (VOID_CAT)
#endif

#if NTFTP_PROTO > 0
#define TFTP_PROTO(x) (x)
#else
#define TFTP_PROTO(x) (VOID_CAT)
#endif

#ifdef ANNEX_II
#define STRING_ANXII	STRING_P
#else
#define STRING_ANXII	STRING_P_100
#endif

#if NCMUSNMP > 0
#define SNMP(x)		(x)
#else
#define SNMP(x)		(VOID_CAT)
#endif

#if NKERB > 0
#define KERB(x)		(x)
#else
#define KERB(x)		(VOID_CAT)
#endif


/**************************************************************
 **************************************************************
 **  NOTE:
 **	THE ORDER OF THE ENTRIES IN THIS TABLE IS DEFINED BY
 **	THE NUMERIC ORDER OF THE PORT PARAMETER DEFINES ABOVE.
 **************************************************************
 */
parameter_table portp_table[] =
{
{PORT_MODE,		DEV_CAT,  DEV_MODE,		P_GENERIC_CAT,CARDINAL_P, CNV_PM
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{LOCATION,		DEV_CAT,  DEV_LOCATION,		P_GENERIC_CAT,STRING_P,   CNV_STRING
#ifdef NA
     , { V_4_N, V_4_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{PORT_TYPE,		DEV_CAT,  DEV_LTYPE,		P_GENERIC_CAT,CARDINAL_P, CNV_PT
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{TERM_VAR,		DEV_CAT,  DEV_TERM,		P_GENERIC_CAT,STRING_P,   CNV_STRING
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{PORT_PROMPT,		EDIT_CAT, EDIT_PROMPT,		P_GENERIC_CAT,STRING_P,   CNV_PROMPT
#ifdef NA
     , { 0, V_4_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{USER_INTERFACE, EDIT_CAT, EDIT_USER_INTF, P_GENERIC_CAT, CARDINAL_P, CNV_USER_INTF
#ifdef NA
     , { 0, 0, 0, V_7_1_DEC, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},
{PORT_SPEED,		INTF_CAT, INTER_IBAUD,		P_GENERIC_CAT,CARDINAL_P, CNV_PS
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{PORT_AUTOBAUD, INTF_CAT, INTER_ABAUD, P_GENERIC_CAT, BOOLEAN_P,  CNV_DFT_N
#ifdef NA
     , { 0, 0, 0, V_7_1_DEC, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},
{BITS_PER_CHAR,		INTF_CAT, INTER_BCHAR,		P_GENERIC_CAT,CARDINAL_P, CNV_BPC
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{STOP_BITS,		INTF_CAT, INTER_STOPB,		P_GENERIC_CAT,CARDINAL_P, CNV_SB
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{PARITY,		INTF_CAT, INTER_PCHECK,		P_GENERIC_CAT,CARDINAL_P, CNV_P
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{MAX_SESSIONS,		DEV_CAT,  DEV_SESSIONS,		P_GENERIC_CAT,CARDINAL_P, CNV_MS
#ifdef NA
     , { V_2_N, V_2_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{BROADCAST_ON,		DEV_CAT,  DEV_NBROADCAST,	P_GENERIC_CAT,BOOLEAN_P,  CNV_DFT_Y
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{BROADCAST_DIR,		DEV_CAT,  DEV_RBCAST, 	P_GENERIC_CAT,BOOLEAN_P, CNV_RBCAST
#ifdef NA
     , { V_5_N, V_5_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{IMASK_7BITS,		INTF_CAT, INTER_IMASK7,		P_GENERIC_CAT,BOOLEAN_P,  CNV_DFT_N
#ifdef NA
     , { V_4_N, V_4_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{CLI_IMASK7,		DEV_CAT, DEV_CLI_IMASK7,	P_GENERIC_CAT,BOOLEAN_P,  CNV_DFT_Y
#ifdef NA
     , { V_6_N, V_6_N, V_6_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{PS_HISTORY_BUFF, NO_A2(DEV_CAT),DEV_PS_HISTORY_BUFF, P_GENERIC_CAT,CARDINAL_P, CNV_HIST_BUFF
#ifdef NA
     , { 0, 0, 0, V_7_N, V_7_N, 0, V_7_1_N }
#endif
},
{BANNER, DEV_CAT, DEV_BANNER, P_GENERIC_CAT,BOOLEAN_P, CNV_DFT_Y
#ifdef NA
	, { 0, V_7_N, V_7_N, V_7_N, V_7_N, 0, V_7_1_N }
#endif
},
{TCPA_KEEPALIVE, NO_A2(DEV_CAT), DEV_KEEPALIVE, P_GENERIC_CAT, CARDINAL_P, CNV_BYTE_ZERO_OK
#ifdef NA
	, { 0, 0, 0, V_7_1_N, V_7_1_N, 0, V_7_1_N }
#endif
},
{DEDICATED_ADDRESS,	DEV_CAT,  DEV_DEDICATED_ADDR,  P_GENERIC_CAT,LONG_UNSPEC_P, CNV_NET_Z
#ifdef NA
     , { V_4_N, V_4_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{DEDICATED_PORT,	DEV_CAT,  DEV_DEDICATED_PORT,	P_GENERIC_CAT,CARDINAL_P, CNV_DPORT
#ifdef NA
     , { V_4_N, V_4_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{MODEM_VAR, NO_ELS(DEV_CAT), DEV_MODEM_VAR, P_GENERIC_CAT,STRING_P, CNV_STRING
#ifdef NA
     , { 0, 0, 0, V_8_N, V_8_N, 0, 0 }
#endif
},
{DEF_SESS_MODE, DEV_CAT,   DEV_SESS_MODE,  P_GENERIC_CAT, CARDINAL_P,     CNV_SESS_MODE
#ifdef NA
     , { 0, 0, 0, V_7_1_DEC, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},
{CONTROL_LINE_USE,	INTF_CAT, INTER_MODEM,		P_FLOW_CAT,CARDINAL_P, CNV_MC
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{INPUT_FLOW_CONTROL,	DEV_CAT,  DEV_IFLOW,		P_FLOW_CAT,CARDINAL_P, CNV_FC
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{INPUT_START_CHAR,	DEV_CAT,  DEV_ISTARTC,		P_FLOW_CAT,CARDINAL_P, CNV_PRINT
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{INPUT_STOP_CHAR,	DEV_CAT,  DEV_ISTOPC,		P_FLOW_CAT,CARDINAL_P, CNV_PRINT
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{OUTPUT_FLOW_CONTROL,	DEV_CAT,  DEV_OFLOW,		P_FLOW_CAT,CARDINAL_P, CNV_FC
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{OUTPUT_START_CHAR,	DEV_CAT,  DEV_OSTARTC,		P_FLOW_CAT,CARDINAL_P, CNV_PRINT
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{OUTPUT_STOP_CHAR,	DEV_CAT,  DEV_OSTOPC,		P_FLOW_CAT,CARDINAL_P, CNV_PRINT
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{DUI_FLOW,      VOID_CAT,  DEV_DFLOW,   0, CARDINAL_P, CNV_FC
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
{DUI_IFLOW,     VOID_CAT,  DEV_DIFLOW,  0, BOOLEAN_P,  CNV_DFT_Y
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
{DUI_OFLOW,     VOID_CAT,  DEV_DOFLOW,  0, BOOLEAN_P,  CNV_DFT_Y
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
{INPUT_BUFFER_SIZE,	DEV_CAT,  DEV_ISIZE,		P_FLOW_CAT,CARDINAL_P, CNV_BYTE
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{BIDIREC_MODEM,		DEV_CAT,  DEV_CARRIER_OVERRIDE,	P_FLOW_CAT,BOOLEAN_P,  CNV_DFT_N
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{IXANY_FLOW_CONTROL,	DEV_CAT,  DEV_IXANY,		P_FLOW_CAT,BOOLEAN_P,  CNV_DFT_N
#ifdef NA
     , { V_3_N, V_3_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{NEED_DSR,	NO_A2(DEV_CAT), DEV_NEED_DSR,   	P_FLOW_CAT,BOOLEAN_P, CNV_DFT_N
#ifdef NA
     , { 0, 0, 0, V_6_1_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{FORWARDING_TIMER,	DEV_CAT,  DEV_TIMOUT,		P_TIMERS_CAT,CARDINAL_P, CNV_INT5OFF
#ifdef NA
     , { VERS_1 + V_3_N, VERS_1 + V_3_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{FORWARD_COUNT,		DEV_CAT, DEV_FORWARD_COUNT,    	P_TIMERS_CAT,CARDINAL_P, CNV_INT
#ifdef NA
     , { V_6_N, V_6_N, V_6_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{INACTIVITY_CLI,	DEV_CAT,  DEV_INACTCLI,		P_TIMERS_CAT,CARDINAL_P,CNV_INACTCLI
#ifdef NA
     , { V_4_1_N, V_4_1_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{INACTIVITY_TIMER,	DEV_CAT,  DEV_INACTIVE,		P_TIMERS_CAT,CARDINAL_P, CNV_INT0OFF
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{INPUT_ACT,		DEV_CAT,  DEV_INPUT_ACT,	P_TIMERS_CAT,BOOLEAN_P,  CNV_DFT_Y
#ifdef NA
     , { V_4_N, V_4_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{OUTPUT_ACT,		DEV_CAT,  DEV_OUTPUT_ACT,	P_TIMERS_CAT,BOOLEAN_P,  CNV_DFT_N
#ifdef NA
     , { V_4_N, V_4_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{RESET_IDLE,		DEV_CAT,  DEV_RESET_IDLE,	P_TIMERS_CAT,BOOLEAN_P,  CNV_RESET_IDLE
#ifdef NA
     , { V_4_N, V_4_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{LONG_BREAK,		DEV_CAT,  DEV_NLBRK,		P_TIMERS_CAT,BOOLEAN_P,  CNV_DFT_Y
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{SHORT_BREAK,		DEV_CAT,  DEV_NSBRK,		P_TIMERS_CAT,BOOLEAN_P,  CNV_DFT_Y
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{PORT_NAME,		DEV_CAT,  DEV_NAME,		P_SECURITY_CAT,STRING_P,   CNV_STRING
#ifdef NA
     , { V_4_N, V_4_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{CLI_SECURITY,NO_ELS(DEV_CAT),  DEV_CLI_SECURITY,  P_SECURITY_CAT,BOOLEAN_P,  CNV_DFT_N
#ifdef NA
     , { V_2_N, V_2_N, V_5_N, V_6_N, V_6_1_N, 0, 0 }
#endif
},
{CONNECT_SECURITY,NO_ELS(DEV_CAT), DEV_CONNECT_SECURITY,  P_SECURITY_CAT,BOOLEAN_P,  CNV_DFT_N
#ifdef NA
     , { V_2_N, V_2_N, V_5_N, V_6_N, V_6_1_N, 0, 0 }
#endif
},
{PORT_SERVER_SECURITY,NO_ELS(DEV_CAT), DEV_PORT_SECURITY, P_SECURITY_CAT,BOOLEAN_P,  CNV_DFT_N
#ifdef NA
     , { V_2_N, V_2_N, V_5_N, V_6_N, V_6_1_N, 0, 0 }
#endif
},
{PORT_PASSWORD,		DEV2_CAT,  DEV2_PORT_PASSWD,	P_SECURITY_CAT,STRING_P,   CNV_STRING
#ifdef NA
     , { 0, V_5_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{IPSO_CLASS, DEV_CAT, DEV_IPSO_CLASS, P_SECURITY_CAT, CARDINAL_P, CNV_IPSO_CLASS
#ifdef NA
        , { 0, 0, 0, V_BIG_BIRD_N, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},
{IPX_SECURITY, DEV_CAT, DEV_IPX_SECURE, P_SECURITY_CAT, BOOLEAN_P, CNV_DFT_N
#ifdef NA
        , { 0, 0, 0, V_BIG_BIRD_N, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},

{DUI_PASSWD,    DEV_CAT,  DEV_DUI_PASSWD,  P_LOGIN_CAT,  BOOLEAN_P,  CNV_DFT_N
#ifdef NA
     , { 0, 0, 0, V_BIG_BIRD_N, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},
{DUI_INACT_TIMEOUT,     DEV_CAT,  DEV_DUI_TIMEOUT, P_LOGIN_CAT, BOOLEAN_P,  CNV_DFT_N
#ifdef NA
        , { 0, 0, 0, V_BIG_BIRD_N, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},
{
#if defined(NA) || defined(i386)
ATTN_CHAR,		DEV_CAT,  DEV_ATTN,		P_EDITING_CAT,STRING_P, CNV_ATTN
#else
ATTN_CHAR,		DEV_CAT,  DEV_ATTN,		P_EDITING_CAT,CARDINAL_P, CNV_PRINT
#endif
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif /* NA */
},
{INPUT_ECHO,		EDIT_CAT, EDIT_INECHO,		P_EDITING_CAT,BOOLEAN_P,  CNV_DFT_Y
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{TELNET_ESC,		EDIT_CAT, EDIT_TESC,		P_EDITING_CAT,CARDINAL_P, CNV_PRINT
#ifdef NA
     , { V_4_N, V_4_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{TELNET_CRLF,		DEV_CAT, DEV_TELNET_CRLF,	P_EDITING_CAT,BOOLEAN_P, CNV_DFT_N
#ifdef NA
     , { 0, V_6_1_N, V_6_1_N, V_6_1_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{MAP_U_TO_L,		EDIT_CAT, EDIT_IUCLC,		P_EDITING_CAT,BOOLEAN_P,  CNV_DFT_N
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{MAP_L_TO_U_PORT,	EDIT_CAT, EDIT_OLCUC,		P_EDITING_CAT,BOOLEAN_P,  CNV_DFT_N
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{CHAR_ERASING,		EDIT_CAT, EDIT_OCRTCERA,	P_EDITING_CAT,BOOLEAN_P,  CNV_DFT_Y
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{LINE_ERASING,		EDIT_CAT, EDIT_OCRTLERA,	P_EDITING_CAT,BOOLEAN_P,  CNV_DFT_Y
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{PORT_HARDWARE_TABS,	EDIT_CAT, EDIT_OTABS,		P_EDITING_CAT,BOOLEAN_P,  CNV_DFT_Y
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{ERASE_CHAR,		EDIT_CAT, EDIT_CERA,		P_EDITING_CAT,CARDINAL_P, CNV_PRINT
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{ERASE_WORD,		EDIT_CAT, EDIT_WERA,		P_EDITING_CAT,CARDINAL_P, CNV_PRINT
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{ERASE_LINE,		EDIT_CAT, EDIT_LERA,		P_EDITING_CAT,CARDINAL_P, CNV_PRINT
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{REDISPLAY_LINE,	EDIT_CAT, EDIT_LDISP,		P_EDITING_CAT,CARDINAL_P, CNV_PRINT
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{TOGGLE_OUTPUT,		EDIT_CAT, EDIT_FLUSH,		P_EDITING_CAT,CARDINAL_P, CNV_PRINT
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{NEWLINE_TERMINAL,	EDIT_CAT, EDIT_NEWLIN,		P_EDITING_CAT,BOOLEAN_P,  CNV_DFT_N
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{P_SLIP_LOCALADDR,SLIP(NET_CAT), SLIP_LOCALADDR,     P_SERIAL_CAT,LONG_UNSPEC_P,CNV_NET_Z
#ifdef NA
     , { 0, V_4_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{P_SLIP_REMOTEADDR,SLIP(NET_CAT), SLIP_REMOTEADDR,    P_SERIAL_CAT,LONG_UNSPEC_P,CNV_NET_Z
#ifdef NA
     , { 0, V_4_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{P_PPP_DIALUP_ADDR,NO_ELS(SLIP(NET_CAT)),PPP_DIALUP_ADDR,P_SERIAL_CAT,BOOLEAN_P,CNV_DFT_N
#ifdef NA
     , { 0, V_6_2_N, V_6_2_N, V_6_2_N, V_6_2_N, 0, 0 }
#endif	/* NA */
},
{P_SLIP_METRIC,	SLIP(NET_CAT), SLIP_METRIC,             P_SERIAL_CAT,CARDINAL_P, CNV_BYTE_ZERO_OK
#ifdef NA
     , { 0, V_4_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{P_SLIP_SECURE,NO_ELS(SLIP(NET_CAT)),SLIP_SECURE,P_SERIAL_CAT,BOOLEAN_P,CNV_DFT_N
#ifdef NA
     , { 0, V_6_2_N, V_6_2_N, V_6_2_N, V_6_2_N, 0, 0 }
#endif
},
{P_SLIP_NET_DEMAND_DIAL,NO_ELS(SLIP(NET_CAT)),SLIP_DEMAND_DIAL,P_SERIAL_CAT,BOOLEAN_P,CNV_DFT_N
#ifdef NA
     , { 0, 0, 0, V_8_N, V_8_N, 0, 0 }
#endif
},
{P_SLIP_NET_INACTIVITY,NO_ELS(SLIP(NET_CAT)),SLIP_NET_INACTIVITY,P_SERIAL_CAT,CARDINAL_P,CNV_INACTCLI
#ifdef NA
     , { 0, 0, 0, V_8_N, V_8_N, 0, 0 }
#endif
},
{P_SLIP_NET_PHONE,NO_ELS(SLIP(NET_CAT)), SLIP_PHONE,   P_SERIAL_CAT,ADM_STRING_P,  CNV_ADM_STRING
#ifdef NA
     , { 0, 0, 0, V_8_N, V_8_N, 0, 0 }
#endif
},
{P_SLIP_DO_COMP,SLIP(NET_CAT), SLIP_DO_COMP,	       	P_SERIAL_CAT,BOOLEAN_P,  CNV_DFT_N
#ifdef NA
     , { 0, V_6_N, V_6_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{P_SLIP_EN_COMP,SLIP(NET_CAT), SLIP_EN_COMP,	       	P_SERIAL_CAT,BOOLEAN_P,  CNV_DFT_N
#ifdef NA
     , { 0, V_6_N, V_6_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{P_SLIP_NETMASK,SLIP(NET_CAT), SLIP_NETMASK,	      P_SLIP_CAT,LONG_UNSPEC_P,CNV_NET_Z
#ifdef NA
     , { 0, V_4_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{P_SLIP_LOADUMP_HOST,SLIP(NET_CAT), SLIP_LOADUMPADDR,   P_SLIP_CAT,LONG_UNSPEC_P,CNV_NET_Z
#ifdef NA
     , { 0, V_4_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{P_SLIP_ALLOW_DUMP,SLIP(NET_CAT), SLIP_NODUMP,	       	P_SLIP_CAT,BOOLEAN_P,  CNV_DFT_Y
#ifdef NA
     , { 0, V_4_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{P_SLIP_LARGE_MTU,SLIP(SLIP_CAT), SLIP_LGMTU,	       	P_SLIP_CAT,BOOLEAN_P,  CNV_LG_SML
#ifdef NA
     , { 0, V_6_2_N, V_6_2_N, V_6_2_N, V_6_2_N, 0, V_7_1_N }
#endif	/* NA */
},
{P_SLIP_NO_ICMP,SLIP(NET_CAT), SLIP_NO_ICMP,	       	P_SLIP_CAT,BOOLEAN_P,  CNV_DFT_N
#ifdef NA
     , { 0, V_6_N, V_6_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{P_SLIP_FASTQ,	SLIP(NET_CAT), SLIP_FASTQ, P_SLIP_CAT,BOOLEAN_P,  CNV_DFT_N
#ifdef NA
     , { 0, V_6_N, V_6_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{P_PPP_MRU, PPP(NET_CAT),  PPP_MRU,         P_PPP_CAT,CARDINAL_P, CNV_MRU
#ifdef NA
	, { 0, 0, 0, V_7_N, V_7_N, 0, 0 }
#endif	/* NA */
},
{P_PPP_ACM, PPP(NET_CAT),  PPP_ACM,         P_PPP_CAT,LONG_CARDINAL_P, CNV_BML
#ifdef NA
	, { 0, 0, 0, V_7_N, V_7_N, 0, 0 }
#endif	/* NA */
},
{P_PPP_SECURITY, PPP(NET_CAT),  PPP_SECURITY,	P_PPP_CAT,CARDINAL_P, CNV_SEC
#ifdef NA
	, { 0, 0, 0, V_7_N, V_7_N, 0, 0 }
#endif	/* NA */
},
{P_PPP_UNAMERMT,        PPP(NET_CAT),  PPP_UNAMERMT,	P_PPP_CAT,STRING_P,   CNV_STRING
#ifdef NA
	, { 0, 0, 0, V_7_N, V_7_N, 0, 0 }
#endif	/* NA */
},
{P_PPP_PWORDRMT,        PPP(NET_CAT),  PPP_PWORDRMT,	P_PPP_CAT,STRING_P,   CNV_STRING
#ifdef NA
	, { 0, 0, 0, V_7_N, V_7_N, 0, 0 }
#endif	/* NA */
},
{P_PPP_NCP,        PPP(NET_CAT),  PPP_NCP,	P_PPP_CAT,CARDINAL_P,   CNV_PPP_NCP
#ifdef NA
	, { 0, 0, 0, V_8_N, V_8_N, 0, 0 }
#endif	/* NA */
},
{P_ARAP_AT_GUEST,        ARAP(NET_CAT),  ARAP_AT_GUEST,	P_ATALK_CAT,BOOLEAN_P,   CNV_DFT_N
#ifdef NA
	, { 0, 0, 0, V_8_N, V_8_N, 0, 0 }
#endif	/* NA */
},
{P_ARAP_AT_NODEID,        ARAP(NET_CAT),  ARAP_AT_NODEID,	P_ATALK_CAT,LONG_CARDINAL_P,   CNV_THIS_NET_RANGE
#ifdef NA
	, { 0, 0, 0, V_8_N, V_8_N, 0, 0 }
#endif	/* NA */
},
{P_ARAP_AT_SECURITY,        ARAP(NET_CAT),  ARAP_AT_SECURITY,	P_ATALK_CAT,BOOLEAN_P,   CNV_DFT_N
#ifdef NA
	, { 0, 0, 0, V_8_N, V_8_N, 0, 0 }
#endif	/* NA */
},
{P_ARAP_V42BIS,        ARAP(NET_CAT),  ARAP_V42BIS,	P_ATALK_CAT,BOOLEAN_P,   CNV_DFT_Y
#ifdef NA
	, { 0, 0, 0, V_8_N, V_8_N, 0, 0 }
#endif	/* NA */
},
{P_TN3270_PRINTER_HOST,        DEV_CAT,  TN3270_PRINTER_HOST,P_TN3270_CAT,LONG_UNSPEC_P,   CNV_NET_Z
#ifdef NA
	, { 0, 0, 0, V_8_N, V_8_N, 0, 0 }
#endif	/* NA */
},
{P_TN3270_PRINTER_NAME,        DEV_CAT,  TN3270_PRINTER_NAME,P_TN3270_CAT,STRING_P,   CNV_STRING
#ifdef NA
	, { 0, 0, 0, V_8_N, V_8_N, 0, 0 }
#endif	/* NA */
},
{AUTHORIZED_GROUPS,LAT_NO_A2(DEV_CAT),LAT_AUTHORIZED_GROUPS,P_LAT_CAT,LAT_GROUP_P, CNV_GROUP_CODE
#ifdef NA
     , { 0, 0, 0, V_7_N, V_7_N, 0, 0 }
#endif
},
{LATB_ENABLE,	LAT(DEV_CAT), DEV_LATB_ENABLE,	P_LAT_CAT,BOOLEAN_P, CNV_DFT_N
#ifdef NA
     , { 0, V_6_2_N, V_6_2_N, V_6_2_N, V_6_2_N, 0, 0 }
#endif
},
{DEFAULT_HPCL,	RDRP(DEV_CAT),  DEV_DEFAULT_HPCL,	0,BOOLEAN_P,  CNV_DFT_N
#ifdef NA
#define V_3_8		0x1ffc
#define V_5_8		0x1fe0
#define V_6_8		0x1fc0
#define V_6_1_8		0x1f80
     , { V_3_8, V_3_8, V_5_8, V_6_8, V_6_1_8, 0, 0 }
#endif
},

/*
{LOGIN_TIMER,		VOID_CAT,  DEV_LOGINT,		0,BOOLEAN_P,  CNV_DFT_N
#ifdef NA
     , { VERS_1, VERS_1, VERS_1, V_6_1_N, 0, 0, 0 }
#endif
},
*/


/*
{DO_LEAP_PROTOCOL,	VOID_CAT, EDIT_DOLEAP,		0,BOOLEAN_P,  CNV_DFT_N
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_6_1_N, 0, 0 }
#endif
},
*/


/*
{P_PPP_ACTOPEN,         VOID_CAT,  PPP_ACTOPEN,	0,BOOLEAN_P, CNV_DFT_Y
#ifdef NA
	, { 0, 0, 0, V_7_N, V_7_N, 0, 0 }
#endif
},
*/

/*
{INTERFACE,		GRP_CAT,  INTF_CAT,		0,0,          0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
*/

/*
{DEVICE,		GRP_CAT,  DEV_CAT,		0,0,          0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
*/

{PORT_GENERIC,		GRP_CAT,  P_GENERIC_CAT,	0,0,	    0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
{PORT_FLOW,		GRP_CAT,  P_FLOW_CAT,		0,0,	    0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
{PORT_SECURITY,		GRP_CAT,  P_SECURITY_CAT,	0,0,	    0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
{PORT_LOGIN,		GRP_CAT,  P_LOGIN_CAT,	0,0,	    0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
{PORT_EDITING,		GRP_CAT,  P_EDITING_CAT,	0,0,	    0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
{PORT_SERIAL,		GRP_CAT,  P_SERIAL_CAT,		0,0,	    0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
{PORT_SLIP,		GRP_CAT,  P_SLIP_CAT,		0,0,	    0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
{PORT_PPP,		GRP_CAT,  P_PPP_CAT,		0,0,	    0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
{PORT_LAT,		LAT(GRP_CAT),  P_LAT_CAT,	0,0,	    0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
{PORT_TIMERS,		GRP_CAT,  P_TIMERS_CAT,		0,0,	    0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
{PORT_APPLETALK,	GRP_CAT,  P_ATALK_CAT,	0,0,	    0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
{PORT_TN3270,		GRP_CAT,  P_TN3270_CAT,	0,0,	    0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
{ALL_PORTP,		GRP_CAT,  ALL_CAT,		0,0,          0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
{-1,			0,	  0,			0,0,	    0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
}
};


/**************************************************************
 **************************************************************
 **  NOTE:
 **	THE ORDER OF THE ENTRIES IN THIS TABLE IS DEFINED BY
 **	THE NUMERIC ORDER OF THE ANNEX PARAMETER DEFINES ABOVE.
 **************************************************************
 */

parameter_table annexp_table[] =
{
{INET_ADDR,	DLA_CAT,	DLA_INETADDR,	B_GENERIC_CAT, LONG_UNSPEC_P,	CNV_NET
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_6_1_N, VERS_1, V_7_1_N }
#endif
},
{SUBNET_MASK,	DLA_CAT,	DLA_SUBNET,	B_GENERIC_CAT, LONG_UNSPEC_P,	CNV_NET_Z
#ifdef NA
     , { V_2_N, V_2_N, V_5_N, V_6_N, V_6_1_N, VERS_1, V_7_1_N }
#endif
},
{PREF_LOAD,	DLA_CAT,	DLA_PREF_LOAD,	B_GENERIC_CAT, LONG_UNSPEC_P,	CNV_NET_Z
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_6_1_N, VERS_1, V_7_1_N }
#endif
},
{PREF_DUMP,	DLA_CAT,	DLA_PREF_DUMP,	B_GENERIC_CAT, LONG_UNSPEC_P,	CNV_NET_Z
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_6_1_N, VERS_1, V_7_1_N }
#endif
},
{LOADSERVER_BCAST,DFE_CAT,      DFE_LOADSVR_BCAST,B_GENERIC_CAT, BOOLEAN_P,    CNV_DFT_Y
#ifdef NA
     , { V_5_N, V_5_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{BROAD_ADDR,    DLA_CAT,        DLA_BROAD_ADDR, B_GENERIC_CAT, LONG_UNSPEC_P,  CNV_NET_Z
#ifdef NA
      , { V_2_N, V_2_N, V_5_N, V_6_N, V_6_1_N, VERS_1, V_7_1_N }
#endif
},
{LOADUMP_GATEWAY,DLA_CAT,       DLA_LOADUMP_GATE,B_GENERIC_CAT,LONG_UNSPEC_P, CNV_NET_Z
#ifdef NA
     , { 0, V_3_N, V_5_N, V_6_N, V_6_1_N, VERS_1, V_7_1_N }
#endif
},
{LOADUMP_SEQUENCE,DLA_CAT,      DLA_LOADUMP_SEQ,B_GENERIC_CAT,LONG_UNSPEC_P,  CNV_SEQ
#ifdef NA
     , { 0, V_4_N, V_5_N, V_6_N, V_6_1_N, VERS_1, V_7_1_N }
#endif
},
{IMAGE_NAME,	DLA_CAT,	DLA_IMAGE,	B_GENERIC_CAT, STRING_ANXII,	CNV_STRING
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_6_1_N, VERS_1, V_7_1_N }
#endif
},
{MOTD,		NO_ELS(DFE_CAT), DFE_MOTD,	B_GENERIC_CAT, STRING_P,	CNV_STRING
#ifdef NA
     , { V_4_1_N, V_4_1_N, V_5_N, V_6_N, V_6_1_N, VERS_1, 0 }
#endif
},
{CONFIG_FILE,	DFE_CAT,	DFE_CONFIG,	B_GENERIC_CAT, STRING_P,	CNV_STRING
#ifdef NA
     , { 0, V_7_N, V_7_N, V_7_N, V_7_N, 0, V_7_1_N }
#endif
},
{AUTH_AGENT,DFE_CAT,	DFE_AGENT,	B_GENERIC_CAT,BOOLEAN_P,	CNV_DFT_Y
#ifdef NA
     , { V_6_N, V_6_N, V_6_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{NROUTED,	DFE_CAT,	DFE_NROUTED,	B_GENERIC_CAT,BOOLEAN_P,	CNV_DFT_Y
#ifdef NA
     , { V_4_1_N, V_4_1_N, V_5_N, V_6_N, V_6_1_N, VERS_1, V_7_1_N }
#endif
},
{SERVER_CAP,	DFE_CAT,	DFE_SERVER_CAP,	B_GENERIC_CAT,CARDINAL_P,	CNV_SCAP
#ifdef NA
     , { V_4_N, V_4_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{SELECTED_MODULES, DFE_CAT, DFE_SELECTED_MODULES,   B_GENERIC_CAT,CARDINAL_P, CNV_SELECTEDMODS
#ifdef NA
     , { 0, V_7_N, V_7_N, V_7_N, V_7_N, 0, V_7_1_N }
#endif
},
{TFTP_DIR_NAME, TFTP_PROTO(DLA_CAT),DLA_TFTP_DIR,B_GENERIC_CAT,STRING_P_100,	CNV_STRING
#ifdef NA
     , { 0, 0, 0, V_6_1_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{TFTP_DUMP_NAME, TFTP_PROTO(DLA_CAT),	DLA_TFTP_DUMP,	B_GENERIC_CAT,STRING_P_100,	CNV_STRING
#ifdef NA
     , { 0, 0, 0, V_6_1_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{IPENCAP_TYPE,	DLA_CAT,	DLA_IPENCAP,	B_GENERIC_CAT,BOOLEAN_P,	CNV_IPENCAP
#ifdef NA
     , { 0, V_4_N, V_5_N, V_6_N, V_6_1_N, VERS_1, V_7_1_N }
#endif
},
{RING_PRIORITY,	PRONET_FOUR(DLA_CAT),DLA_RING_PRIORITY,	0,CARDINAL_P, CNV_RNGPRI
#ifdef NA
     , { 0, V_4_1_N, V_5_N, V_6_N, V_6_1_N, 0, 0 }
#endif
},
{IP_FWD_BCAST,NO_A2(DFE_CAT),DFE_FWDBCAST,B_GENERIC_CAT,BOOLEAN_P,CNV_DFT_N
#ifdef NA
     , { 0, 0, 0, V_7_1_N, V_7_1_N, 0, V_7_1_N }
#endif
},
{TCP_KEEPALIVE, NO_A2(DFE_CAT), DFE_KEEPALIVE, B_GENERIC_CAT, CARDINAL_P, CNV_BYTE_ZERO_OK
#ifdef NA
	, { 0, 0, 0, V_7_1_N, V_7_1_N, 0, V_7_1_N }
#endif
},
{OPTION_KEY, DFE_CAT,	DFE_OPTION_KEY,	B_GENERIC_CAT,STRING_P, CNV_STRING
#ifdef NA
     , { 0, 0, 0, V_8_N, V_8_N, 0, 0 }
#endif
},
{ACC_ENTRIES, VOID_CAT, DFE_MODEM_ACC_ENTRIES, B_GENERIC_CAT, CARDINAL_P,
CNV_BYTE
#ifdef NA
        , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
#if (defined(ANNEX3) && defined(DEBUG))
{JUMPER_5390, DFE_CAT,        DFE_SEG_JUMPER_5390,B_GENERIC_CAT,CARDINAL_P,
CNV_BYTE
#else
{JUMPER_5390, VOID_CAT, DFE_SEG_JUMPER_5390, B_GENERIC_CAT, CARDINAL_P,
CNV_BYTE
#endif
#ifdef NA
        , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
{SESSION_LIMIT,     DFE_CAT, DFE_SESSION_LIMIT, B_GENERIC_CAT,
CARDINAL_P, CNV_SESS_LIM
#ifdef NA
        , { 0, 0, 0, V_BIG_BIRD_N, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},
{OUTPUT_TTL, DFE_CAT, DFE_OUTPUT_TTL, B_GENERIC_CAT, CARDINAL_P,
CNV_BYTE
#ifdef NA
        , { 0, 0, 0, V_BIG_BIRD_N, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},
{VCLI_LIMIT,	DFE_CAT,	DFE_VCLI_LIMIT,	B_VCLI_CAT,CARDINAL_P,	CNV_VCLILIM
#ifdef NA
     , { V_4_N, V_4_N, V_5_N, V_6_N, V_6_1_N, VERS_1, V_7_1_N }
#endif
},
{CLI_PROMPT_STR, DFE_CAT, DFE_PROMPT,	B_VCLI_CAT,STRING_P,	CNV_PROMPT
#ifdef NA
     , { V_4_N, V_4_N, V_5_N, V_6_N, V_6_1_N, VERS_1, V_7_1_N }
#endif
},
{VCLI_SEC_ENA, NO_ELS(DFE_CAT), DFE_VCLI_SEC_ENA,B_VCLI_CAT,BOOLEAN_P,	CNV_DFT_N
#ifdef NA
     , { 0, V_5_N, V_5_N, V_6_N, V_6_1_N, 0, 0 }
#endif
},
{VCLI_PASSWORD,	DFE_CAT,	DFE_VCLI_PASSWD,B_VCLI_CAT,STRING_P,	CNV_STRING
#ifdef NA
     , { 0, V_5_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{NAMESERVER_BCAST,DFE_CAT,	DFE_NAMESVR_BCAST,B_NAMESERVER_CAT,BOOLEAN_P,	CNV_DFT_N
#ifdef NA
     , { V_5_N, V_5_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{NRWHOD,	DFE_CAT,	DFE_NRWHOD,	B_NAMESERVER_CAT,BOOLEAN_P,	CNV_DFT_Y
#ifdef NA
     , { V_4_N, V_4_N, V_5_N, V_6_N, V_6_1_N, VERS_1, V_7_1_N }
#endif
},
{PRIMARY_NS_ADDR,DFE_CAT,	DFE_1ST_NS_ADDR,B_NAMESERVER_CAT,LONG_UNSPEC_P,	CNV_NET_Z
#ifdef NA
     , { V_2_N, V_2_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{PRIMARY_NS,	DFE_CAT,	DFE_1ST_NS,	B_NAMESERVER_CAT,CARDINAL_P,	CNV_NS
#ifdef NA
     , { V_2_N, V_2_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{SECONDARY_NS_ADDR,DFE_CAT,	DFE_2ND_NS_ADDR,B_NAMESERVER_CAT,LONG_UNSPEC_P,	CNV_NET_Z
#ifdef NA
     , { V_2_N, V_2_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{SECONDARY_NS,DFE_CAT,DFE_2ND_NS,B_NAMESERVER_CAT,CARDINAL_P,CNV_NS
#ifdef NA
     , { V_2_N, V_2_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{HTABLE_SZ,	DFE_CAT,	DFE_HTABLE_SZ,	B_NAMESERVER_CAT,CARDINAL_P,	CNV_HT
#ifdef NA
     , { V_2_N, V_2_N, V_5_N, V_6_N, V_6_1_N, VERS_1, V_7_1_N }
#endif
},
{NMIN_UNIQUE,	DFE_CAT,	DFE_NMIN_UNIQUE,B_NAMESERVER_CAT,BOOLEAN_P,	CNV_DFT_Y
#ifdef NA
     , { V_4_N, V_4_N, V_5_N, V_6_N, V_6_1_N, VERS_1, V_7_1_N }
#endif
},
{ENABLE_SECURITY,DFE_CAT,	DFE_SECURE,	B_SECURITY_CAT,CARDINAL_P,	CNV_DFT_N
#ifdef NA
     , { V_3_N, V_3_N, V_5_N, V_6_N, V_6_1_N, VERS_1, V_7_1_N }
#endif
},
{SECURSERVER_BCAST,NO_ELS(DFE_CAT),DFE_SECRSVR_BCAST,B_SECURITY_CAT,BOOLEAN_P,	CNV_DFT_Y
#ifdef NA
     , { V_5_N, V_5_N, V_5_N, V_6_N, V_6_1_N, 0, 0 }
#endif
},
{PREF_SECURE_1,	NO_ELS(DFE_CAT), DFE_PREF1_SECURE,B_SECURITY_CAT,LONG_UNSPEC_P, CNV_NET_Z
#ifdef NA
     , { V_2_N, V_2_N, V_5_N, V_6_N, V_6_1_N, 0, 0 }
#endif
},
{PREF_SECURE_2,	NO_ELS(DFE_CAT), DFE_PREF2_SECURE,B_SECURITY_CAT,LONG_UNSPEC_P, CNV_NET_Z
#ifdef NA
     , { V_5_N, V_5_N, V_5_N, V_6_N, V_6_1_N, 0, 0 }
#endif
},
{NET_TURNAROUND, NO_ELS(DFE_CAT), DFE_NET_TURNAROUND,B_SECURITY_CAT,CARDINAL_P,	CNV_NET_TURN
#ifdef NA
     , { V_3_N, V_3_N, V_5_N, V_6_N, V_6_1_N, 0, 0 }
#endif
},
{LOOSE_SOURCE_RT, NO_A2(DFE_CAT), DFE_LOOSE_SOURCE_RT,B_SECURITY_CAT,BOOLEAN_P, CNV_DFT_Y
#ifdef NA
     , { 0, 0, 0, V_7_1_N, V_7_1_N, 0, V_7_1_N }
#endif
},
{ACP_KEY, NO_ELS(DFE_CAT), DFE_ACP_KEY,	B_SECURITY_CAT,STRING_P, CNV_STRING
#ifdef NA
     , { V_4_N, V_4_N, V_5_N, V_6_N, V_6_1_N, 0, 0 }
#endif
},
{BOX_PASSWORD,	DFE_CAT,	DFE_PASSWORD,	B_SECURITY_CAT,STRING_P,	CNV_STRING
#ifdef NA
     , { V_4_N, V_4_N, V_5_N, V_6_N, V_6_1_N, VERS_1, V_7_1_N }
#endif
},
{ALLOW_SNMP_SETS, SNMP(DFE_CAT), DFE_SNMPSET, B_SECURITY_CAT,BOOLEAN_P, CNV_DFT_N
#ifdef NA
     , { 0, V_7_N, V_7_N, V_7_N, V_7_N, 0, V_7_1_N }
#endif
},
{LOCK_ENABLE, DFE_CAT, DFE_LOCK_ENABLE, B_SECURITY_CAT, BOOLEAN_P, CNV_DFT_Y
#ifdef NA
     , { 0, 0, 0, V_7_1_DEC, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},
{PASSWD_LIMIT, DFE_CAT, DFE_PASSWD_LIMIT, B_SECURITY_CAT, CARDINAL_P, CNV_PASSLIM
#ifdef NA
     , { 0, 0, 0, V_7_1_DEC, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},
{CHAP_AUTH_NAME, DFE_CAT, DFE_CHAP_AUTH_NAME, B_SECURITY_CAT, STRING_P, CNV_STRING
#ifdef NA
     , { 0, 0, 0, V_BIG_BIRD_N, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},
{KERB_SECURITY_ENA, KERB(DFE_CAT), DFE_KERB_SECUREN, B_KERB_CAT,
BOOLEAN_P, CNV_DFT_N
#ifdef NA
        , { 0, 0, 0, V_POST_BB_N, V_POST_BB_N, 0, V_POST_BB_N }
#endif
},
{KERB_HOST, KERB(DFE_CAT), DFE_KERB_HOST, B_KERB_CAT, KERB_HOST_P,
CNV_KERB_HOST
#ifdef NA
        , { 0, 0, 0, V_POST_BB_N, V_POST_BB_N, 0, V_POST_BB_N }
#endif
},
{TGS_HOST, KERB(DFE_CAT), DFE_TGS_HOST, B_KERB_CAT, KERB_HOST_P,
CNV_KERB_HOST
#ifdef NA
        , { 0, 0, 0, V_POST_BB_N, V_POST_BB_N, 0, V_POST_BB_N }
#endif
},
{TELNETD_KEY, KERB(DFE_CAT), DFE_TELNETD_KEY, B_KERB_CAT, STRING_P,
CNV_STRING
#ifdef NA
        , { 0, 0, 0, V_POST_BB_N, V_POST_BB_N, 0, V_POST_BB_N }
#endif
},
{KERBCLK_SKEW, KERB(DFE_CAT), DFE_KERBCLK_SKEW, B_KERB_CAT,
CARDINAL_P, CNV_TIMER
#ifdef NA
        , { 0, 0, 0, V_POST_BB_N, V_POST_BB_N, 0, V_POST_BB_N }
#endif
},
{TIMESERVER_BCAST,DFE_CAT,	DFE_TIMESVR_BCAST,B_TIME_CAT,BOOLEAN_P,	CNV_DFT_N
#ifdef NA
     , { V_5_N, V_5_N, V_5_N, V_6_N, V_6_1_N, 0, V_7_1_N }
#endif
},
{TZ_DLST,	DFE_CAT,	DFE_TZ_DLST,	B_TIME_CAT,CARDINAL_P,	CNV_DLST
#ifdef NA
     , { V_4_N, V_4_N, V_5_N, V_6_N, V_6_1_N, VERS_1, V_7_1_N }
#endif
},
{TZ_MINUTES,	DFE_CAT,	DFE_TZ_MINUTES,	B_TIME_CAT,CARDINAL_P,	CNV_TZ_MIN
#ifdef NA
     , { V_4_N, V_4_N, V_5_N, V_6_N, V_6_1_N, VERS_1, V_7_1_N }
#endif
},
{TIMESERVER_HOST, NO_A2(DFE_CAT), DFE_TIMESERVE, B_TIME_CAT,LONG_UNSPEC_P, CNV_NET_Z
#ifdef NA
	, { 0, 0, 0, V_8_N, V_8_N, 0, 0 }
#endif
},
{SYSLOG_MASK,	DFE_CAT,	DFE_SYSLOG_MASK, B_SYSLOG_CAT,CARDINAL_P,	CNV_SYSLOG
#ifdef NA
     , { V_4_N, V_4_N, V_5_N, V_6_N, V_6_1_N, VERS_1, V_7_1_N }
#endif
},
{SYSLOG_FAC,	DFE_CAT,	DFE_SYSLOG_FAC,	B_SYSLOG_CAT,CARDINAL_P,	CNV_SYSFAC
#ifdef NA
     , { V_4_N, V_4_N, V_5_N, V_6_N, V_6_1_N, VERS_1, V_7_1_N }
#endif
},
{SYSLOG_HOST,	DFE_CAT,	DFE_SYSLOG_ADDR, B_SYSLOG_CAT,LONG_UNSPEC_P,	CNV_NET_Z
#ifdef NA
     , { V_4_N, V_4_N, V_5_N, V_6_N, V_6_1_N, VERS_1, V_7_1_N }
#endif
},
{SYSLOG_PORT,	NO_A2(DFE_CAT),	DFE_SYSLOG_PORT, B_SYSLOG_CAT,CARDINAL_P,	CNV_PORT
#ifdef NA
     , { 0, 0, 0, V_7_N, V_7_N, 0, V_7_1_N }
#endif
},
{MOP_PREF_HOST, DLA_CAT, DLA_MOP_HOST, B_MOP_CAT, ENET_ADDR_P, CNV_ENET_ADDR
#ifdef NA
     , { 0, 0, 0, V_7_1_DEC, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},
{MOP_PASSWD, DFE_CAT,      DFE_MOP_PASSWD, B_MOP_CAT, MOP_PASSWD_P, CNV_MOP_PASSWD
#ifdef NA
     , { 0, 0, 0, V_7_1_DEC, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},
{LOGIN_PASSWD, DFE_CAT, DFE_LOGIN_PASSWD, B_MOP_CAT, STRING_P, CNV_STRING
#ifdef NA
     , { 0, 0, 0, V_7_1_DEC, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},
{LOGIN_PROMPT,  DFE_CAT,   DFE_LOGIN_PROMPT, B_MOP_CAT, STRING_P,     CNV_STRING
#ifdef NA
     , { 0, 0, 0, V_7_1_DEC, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},
{LOGIN_TIMER, DFE_CAT,  DFE_DUI_TIMER, B_MOP_CAT, CARDINAL_P,  CNV_INT0OFF
#ifdef NA
     , { 0, 0, 0, V_7_1_DEC, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},
{KEY_VALUE, LAT(DFE_CAT),	LAT_KEY_VALUE,	B_LAT_CAT,STRING_P, CNV_STRING
#ifdef NA
     , { 0, V_6_N, V_6_N, V_6_N, V_6_1_N, 0, 0 }
#endif
},
{HOST_NUMBER, LAT(LAT_CAT), LAT_HOST_NUMBER, B_LAT_CAT,CARDINAL_P, CNV_HOST_NUMBER
#ifdef NA
     , { 0, V_6_N, V_6_N, V_6_N, V_6_1_N, 0, 0 }
#endif
},
{HOST_NAME, LAT(LAT_CAT),	LAT_HOST_NAME,	B_LAT_CAT,STRING_P, CNV_STRING
#ifdef NA
     , { 0, V_6_N, V_6_N, V_6_N, V_6_1_N, 0, 0 }
#endif
},
{HOST_ID, LAT(DFE_CAT),	LAT_HOST_ID,	B_LAT_CAT,ADM_STRING_P,	CNV_ADM_STRING
#ifdef NA
     , { 0, V_6_N, V_6_N, V_6_N, V_6_1_N, 0, 0 }
#endif
},
{QUEUE_MAX, LAT(LAT_CAT),	LAT_QUEUE_MAX,	B_LAT_CAT,CARDINAL_P, CNV_QUEUE_MAX
#ifdef NA
     , { 0, V_7_N, V_7_N, V_7_N, V_7_N, 0, 0 }
#endif
},
{SERVICE_LIMIT, LAT(LAT_CAT),	LAT_SERVICE_LIMIT,	B_LAT_CAT,CARDINAL_P, CNV_SERVICE_LIMIT
#ifdef NA
     , { 0, V_6_N, V_6_N, V_6_N, V_6_1_N, 0, 0 }
#endif
},
{KA_TIMER,LAT(LAT_CAT),	LAT_KA_TIMER,	B_LAT_CAT,CARDINAL_P,	CNV_KA_TIMER
#ifdef NA
     , { 0, V_6_N, V_6_N, V_6_N, V_6_1_N, 0, 0 }
#endif
},
{CIRCUIT_TIMER,LAT(LAT_CAT),	LAT_CIRCUIT_TIMER,B_LAT_CAT,CARDINAL_P, CNV_CIRCUIT_TIMER
#ifdef NA
     , { 0, V_6_N, V_6_N, V_6_N, V_6_1_N, 0, 0 }
#endif
},
{RETRANS_LIMIT,LAT(LAT_CAT),	LAT_RETRANS_LIMIT,	B_LAT_CAT,CARDINAL_P, CNV_RETRANS_LIMIT
#ifdef NA
     , { 0, V_6_N, V_6_N, V_6_N, V_6_1_N, 0, 0 }
#endif
},
{GROUP_CODE,LAT(LAT_CAT),	LAT_GROUP_CODE,	B_LAT_CAT,LAT_GROUP_P, CNV_GROUP_CODE
#ifdef NA
     , { 0, V_6_N, V_6_N, V_6_N, V_6_1_N, 0, 0 }
#endif
},
{VCLI_GROUPS,LAT_NO_A2(LAT_CAT),LAT_VCLI_GROUPS,B_LAT_CAT,LAT_GROUP_P, CNV_GROUP_CODE
#ifdef NA
     , { 0, 0, 0, V_7_N, V_7_N, 0, 0 }
#endif
},
{MULTI_TIMER,LAT(LAT_CAT), LAT_MULTI_TIMER, B_LAT_CAT, CARDINAL_P, CNV_MULTI_TIMER
#ifdef NA
     , { 0, 0, 0, V_7_1_DEC, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},
{A_ROUTER, ARAP_CAT,	ARAP_A_ROUTER,	B_ATALK_CAT,ENET_ADDR_P, CNV_ENET_ADDR
#ifdef NA
     , { 0, 0, 0, V_8_N, V_8_N, 0, 0 }
#endif
},
{DEF_ZONE_LIST, ARAP_CAT,	ARAP_DEF_ZONE_LIST, B_ATALK_CAT,STRING_P_100, CNV_DEF_ZONE_LIST
#ifdef NA
     , { 0, 0, 0, V_8_N, V_8_N, 0, 0 }
#endif
},
{NODE_ID, ARAP_CAT,	ARAP_NODE_ID,	B_ATALK_CAT,LONG_CARDINAL_P, CNV_THIS_NET_RANGE
#ifdef NA
     , { 0, 0, 0, V_8_N, V_8_N, 0, 0 }
#endif
},
{ZONE, ARAP_CAT,	ARAP_ZONE,	B_ATALK_CAT,ADM_STRING_P, CNV_ZONE
#ifdef NA
     , { 0, 0, 0, V_8_N, V_8_N, 0, 0 }
#endif
},
#ifdef SYNC
{IP_TTL, RIP_CAT,	RIP_IP_TTL,	B_ROUTER_CAT,CARDINAL_P, CNV_A_BYTE
#ifdef NA
     , { 0, 0, 0, V_8_N, V_8_N, 0, 0 }
#endif
},
{ND_FORWARD, RIP_CAT,	RIP_ND_FORWARD,	B_ROUTER_CAT,CARDINAL_P, CNV_DFT_Y
#ifdef NA
     , { 0, 0, 0, V_8_N, V_8_N, 0, 0 }
#endif
},
{ASD_FORWARD, RIP_CAT,	RIP_ASD_FORWARD,	B_ROUTER_CAT,CARDINAL_P, CNV_DFT_Y
#ifdef NA
     , { 0, 0, 0, V_8_N, V_8_N, 0, 0 }
#endif
},
{SD_FORWARD, RIP_CAT,	RIP_SD_FORWARD,		B_ROUTER_CAT,CARDINAL_P, CNV_DFT_Y
#ifdef NA
     , { 0, 0, 0, V_8_N, V_8_N, 0, 0 }
#endif
},
#endif
{RIP_AUTH, RIP_CAT,     RIP_RIP_AUTH,   B_ROUTER_CAT, STRING_P, CNV_STRING
#ifdef NA
     , { 0, 0, 0, V_8_N, V_8_N, 0, 0 }
#endif
},
{RIP_ROUTERS, RIP_CAT,	RIP_RIP_ROUTERS,	B_ROUTER_CAT,RIP_ROUTERS_P, CNV_BOX_RIP_ROUTERS
#ifdef NA
     , { 0, 0, 0, V_8_N, V_8_N, 0, 0 }
#endif
},
{IPX_FILE_SERVER, DLA_CAT, DLA_IPX_FILE_SERVER, B_IPX_CAT,IPX_STRING_P,CNV_IPX_STRING
#ifdef NA
        , { 0, 0, 0, V_BIG_BIRD_N, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},
{IPX_FRAME_TYPE, DLA_CAT, DLA_IPX_FRAME_TYPE, B_IPX_CAT, CARDINAL_P, CNV_IPX_FMTY
#ifdef NA
        , { 0, 0, 0, V_BIG_BIRD_N, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},
{IPX_DUMP_UNAME, DLA_CAT, DLA_IPX_DMP_USER_NAME, B_IPX_CAT,IPX_STRING_P,CNV_IPX_STRING
#ifdef NA
        , { 0, 0, 0, V_BIG_BIRD_N, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},
{IPX_DUMP_PWD, DLA_CAT, DLA_IPX_DMP_PASSWD, B_IPX_CAT, IPX_STRING_P,CNV_IPX_STRING
#ifdef NA
        , { 0, 0, 0, V_BIG_BIRD_N, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},
{IPX_DUMP_PATH, DLA_CAT, DLA_IPX_DMP_PATH, B_IPX_CAT, STRING_P_100 ,CNV_STRING
#ifdef NA
        , { 0, 0, 0, V_BIG_BIRD_N, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},
{IPX_DO_CHKSUM, DLA_CAT, DLA_IPX_DO_CHECKSUM, B_IPX_CAT,BOOLEAN_P,  CNV_DFT_N
#ifdef NA
        , { 0, 0, 0, V_BIG_BIRD_N, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},
{BOX_GENERIC,	GRP_CAT,	B_GENERIC_CAT, 	0,0,		0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
{BOX_VCLI,	GRP_CAT,	B_VCLI_CAT, 0,0,		0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
{BOX_NAMESERVER,	GRP_CAT,	B_NAMESERVER_CAT, 0,0,		0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
{BOX_SECURITY,	GRP_CAT,	B_SECURITY_CAT, 0,0,		0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
{BOX_KERBEROS,	KERB(GRP_CAT),	B_KERB_CAT, 0,0,		0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
{BOX_TIME,	GRP_CAT,	B_TIME_CAT, 0,0,		0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
{BOX_SYSLOG,	GRP_CAT,	B_SYSLOG_CAT, 0,0,		0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
{BOX_MOP,	GRP_CAT,	B_MOP_CAT, 0,0,		0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
{BOX_LAT,	LAT(GRP_CAT),  	B_LAT_CAT,	0,	    0,0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
{BOX_APPLETALK,	GRP_CAT,	B_ATALK_CAT, 0,0,		0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
{BOX_ROUTER,	GRP_CAT,	B_ROUTER_CAT,0,0,		0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
{BOX_IPX,	GRP_CAT,	B_IPX_CAT,0,0,		0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
{ALL_BOX,	GRP_CAT,	ALL_CAT,	0,0,		0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
{-1,		0,		0,		0,0,		0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
}
};

parameter_table printp_table[] =
{
{MAP_L_TO_U_PRINT,	LP_CAT,   PRINTER_OLTOU,	0,BOOLEAN_P,  CNV_DFT_N
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_7_N, 0, V_7_1_N }
#endif
},
{PRINTER_WIDTH,		LP_CAT,   PRINTER_MCOL,		0,CARDINAL_P, CNV_INT
#ifdef NA
     , { V_2_N, V_2_N, V_5_N, V_6_N, V_7_N, 0, V_7_1_N }
#endif
},
{PRINT_HARDWARE_TABS,	LP_CAT,   PRINTER_OTABS,	0,BOOLEAN_P,  CNV_DFT_N
#ifdef NA
     , { V_1_N, V_1_N, V_5_N, V_6_N, V_7_N, 0, V_7_1_N }
#endif
},
{PRINTER_INTERFACE,	LP_CAT,   PRINTER_TYPE,	0,BOOLEAN_P,  CNV_PTYPE
#ifdef NA
     , { 0, 0, V_5_N, V_6_N, V_7_N, 0, V_7_1_N }
#endif
},
{PRINTER_SPD,	LP_CAT,   PRINTER_SPEED,	0,BOOLEAN_P,  CNV_PSPEED
#ifdef NA
     , { 0, 0, 0, V_BIG_BIRD_N, V_7_N, 0, V_7_1_N }
#endif
},
{PRINTER_CR_CRLF,	LP_CAT,	PRINTER_CRLF,	0,BOOLEAN_P,	CNV_DFT_Y
#ifdef NA
     , { 0, 0, V_7_1_N, V_7_1_N, V_7_1_N, 0, V_7_1_N }
#endif
},
{TCPP_KEEPALIVE, NO_A2(LP_CAT), PRINTER_KEEPALIVE, 0, CARDINAL_P, CNV_BYTE_ZERO_OK
#ifdef NA
	, { 0, 0, 0, V_7_1_N, V_7_1_N, 0, V_7_1_N }
#endif
},
{ALL_PRINTER,		GRP_CAT,  ALL_CAT,		0,0,	    0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
{-1,			0,	  0,			0,0,	    0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
}
};


/**************************************************************
 **************************************************************
 **  NOTE:
 **	THE ORDER OF THE ENTRIES IN THIS TABLE IS DEFINED BY
 **	THE NUMERIC ORDER OF THE INTERFACE PARAMETER DEFINES ABOVE.
 **************************************************************
 */

parameter_table interfacep_table[] =
{
/* The followings are added to support RIP related parameters */
{RIP_SEND_VERSION, IF_CAT,	IF_RIP_SEND_VERSION,	0,CARDINAL_P, CNV_RIP_SEND_VERSION
#ifdef NA
     , { 0, 0, 0, V_8_N, V_8_N, 0, 0 }
#endif
},
{RIP_RECV_VERSION, IF_CAT, IF_RIP_RECV_VERSION,		0,CARDINAL_P, CNV_RIP_RECV_VERSION
#ifdef NA
     , { 0, 0, 0, V_8_N, V_8_N, 0, 0 }
#endif
},
{RIP_HORIZON, IF_CAT,	IF_RIP_HORIZON,	0,CARDINAL_P, CNV_RIP_HORIZON
#ifdef NA
     , { 0, 0, 0, V_8_N, V_8_N, 0, 0 }
#endif
},
{RIP_DEFAULT_ROUTE, IF_CAT,	IF_RIP_DEFAULT_ROUTE,	0,CARDINAL_P, 	CNV_RIP_DEFAULT_ROUTE
#ifdef NA
     , { 0, 0, 0, V_8_N, V_8_N, 0, 0 }
#endif
},
{RIP_DOMAIN, VOID_CAT,	IF_RIP_DOMAIN,	0,CARDINAL_P, CNV_RIP_DOMAIN
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
{RIP_SUB_ADVERTISE, IF_CAT,	IF_RIP_SUB_ADVERTISE,	0,BOOLEAN_P, CNV_DFT_N
#ifdef NA
     , { 0, 0, 0, V_8_N, V_8_N, 0, 0 }
#endif
},
{RIP_SUB_ACCEPT, IF_CAT,	IF_RIP_SUB_ACCEPT,	0,BOOLEAN_P, CNV_DFT_Y
#ifdef NA
     , { 0, 0, 0, V_8_N, V_8_N, 0, 0 }
#endif
},
{RIP_ADVERTISE, IF_CAT,	IF_RIP_ADVERTISE,0,RIP_ROUTERS_P, CNV_RIP_ROUTERS
#ifdef NA
     , { 0, 0, 0, V_8_N, V_8_N, 0, 0 }
#endif
},
{RIP_ACCEPT, IF_CAT,	IF_RIP_ACCEPT,	0,RIP_ROUTERS_P, CNV_RIP_ROUTERS
#ifdef NA
     , { 0, 0, 0, V_8_N, V_8_N, 0, 0 }
#endif
},
{ALL_INTERFACEP,	GRP_CAT,  ALL_CAT,		0,0,          0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
{-1,			0,	  0,			0,0,	    0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
}
};


/**************************************************************
 **************************************************************
 **  NOTE:
 **     THE ORDER OF THE ENTRIES IN THIS TABLE IS DEFINED BY
 **     THE NUMERIC ORDER OF THE SYNCHRONOUS PARAMETER DEFINES ABOVE.
 **************************************************************
 */
parameter_table syncp_table[] =
{

/* Synchronous Generic Grouping...*/

{ S_MODE, SYNC_CAT, SYNC_MODE, S_GENERIC_CAT, BYTE_P, CNV_SYNC_MODE
#ifdef NA
        , { 0, 0, 0, V_BIG_BIRD_N, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},
{ S_LOCATION, SYNC_CAT, SYNC_LOCATION, S_GENERIC_CAT, STRING_P, CNV_STRING
#ifdef NA
        , { 0, 0, 0, V_BIG_BIRD_N, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},
{ S_CLOCKING, SYNC_CAT, SYNC_CLOCKING, S_GENERIC_CAT, BYTE_P, CNV_SYNC_CLK
#ifdef NA
        , { 0, 0, 0, V_BIG_BIRD_N, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},
{ S_FORCE_CTS, SYNC_CAT, SYNC_FORCE_CTS, S_GENERIC_CAT, BOOLEAN_P, CNV_DFT_N
#ifdef NA
        , { 0, 0, 0, V_BIG_BIRD_N, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},

/*  Port Security Parameters */

{ S_USRNAME, SYNC_CAT, SYNC_USRNAME, S_SECURITY_CAT, STRING_P, CNV_STRING
#ifdef NA
        , { 0, 0, 0, V_BIG_BIRD_N, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},
{ S_PORT_PASSWD, SYNC_CAT, SYNC_PORT_PASSWD, S_SECURITY_CAT, STRING_P, CNV_STRING
#ifdef NA
        , { 0, 0, 0, V_BIG_BIRD_N, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},

/* Synchronous Serial Network grouping...*/

{ S_LOCAL_ADDR, SYNC_CAT, SYNC_LOCAL_ADDR, S_NET_CAT, LONG_UNSPEC_P, CNV_NET_Z
#ifdef NA
        , { 0, 0, 0, V_BIG_BIRD_N, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},
{ S_REMOTE_ADDR, SYNC_CAT, SYNC_REMOTE_ADDR, S_NET_CAT, LONG_UNSPEC_P, CNV_NET_Z
#ifdef NA
        , { 0, 0, 0, V_BIG_BIRD_N, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},
{ S_NETMASK, SYNC_CAT, SYNC_NETMASK, S_NET_CAT, LONG_UNSPEC_P, CNV_NET_Z
#ifdef NA
        , { 0, 0, 0, V_BIG_BIRD_N, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},
{ S_METRIC, SYNC_CAT, SYNC_METRIC, S_NET_CAT, BYTE_P, CNV_SMETRIC
#ifdef NA
        , { 0, 0, 0, V_BIG_BIRD_N, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},
{ S_ALW_COMP, SYNC_CAT, SYNC_ALW_COMP, S_NET_CAT, BOOLEAN_P, CNV_DFT_N
#ifdef NA
        , { 0, 0, 0, V_BIG_BIRD_N, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},
{ S_SECURE, SYNC_CAT, SYNC_SECURE, S_NET_CAT, BOOLEAN_P, CNV_DFT_N
#ifdef NA
        , { 0, 0, 0, V_BIG_BIRD_N, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},
{ S_DIAL_ADDR, SYNC_CAT, SYNC_DIAL_ADDR, S_NET_CAT, BOOLEAN_P, CNV_DFT_N
#ifdef NA
        , { 0, 0, 0, V_BIG_BIRD_N, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},

/* Synchronous PPP grouping...*/

{ S_PPP_MRU, SYNC_CAT, SYNC_PPP_MRU, S_PPP_CAT, CARDINAL_P, CNV_MRU
#ifdef NA
        , { 0, 0, 0, V_BIG_BIRD_N, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},
{ S_PPP_SECURE_PROTO, SYNC_CAT, SYNC_PPP_SECURE_PROTO, S_PPP_CAT, BYTE_P, CNV_SEC
#ifdef NA
        , { 0, 0, 0, V_BIG_BIRD_N, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},
{ S_PPP_USRNAME_REMOTE, SYNC_CAT, SYNC_PPP_USRNAME_REMOTE, S_PPP_CAT, STRING_P, CNV_STRING
#ifdef NA
        , { 0, 0, 0, V_BIG_BIRD_N, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},
{ S_PPP_PASSWD_REMOTE, SYNC_CAT, SYNC_PPP_PASSWD_REMOTE, S_PPP_CAT, STRING_P, CNV_STRING
#ifdef NA
        , { 0, 0, 0, V_BIG_BIRD_N, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},
{ S_PPP_NCP, SYNC_CAT, SYNC_PPP_NCP, S_PPP_CAT, BYTE_P, CNV_PPP_NCP
#ifdef NA
        , { 0, 0, 0, V_BIG_BIRD_N, V_BIG_BIRD_N, 0, V_BIG_BIRD_N }
#endif
},
{S_GENERIC,       GRP_CAT,        S_GENERIC_CAT,      0,          0,0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
{S_SECURITY,       GRP_CAT,        S_SECURITY_CAT,      0,          0,0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
{S_NET,       GRP_CAT,        S_NET_CAT,      0,          0,0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
{S_PPP,       GRP_CAT,        S_PPP_CAT,      0,          0,0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
{ALL_SYNCP,       GRP_CAT,        ALL_CAT,      0,          0,0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
},
{-1,		0,		0,		0,0,		0
#ifdef NA
     , { 0, 0, 0, 0, 0, 0, 0 }
#endif
}

};

#endif /* ifndef CMD_H_PARAMS_ONLY */
