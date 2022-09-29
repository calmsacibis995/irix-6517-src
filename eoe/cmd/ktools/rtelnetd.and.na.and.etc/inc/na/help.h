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
 * Include file description:
 *
 *	Help messages and command dictionary for Xylogics Annex version of NA
 *
 * Original Author: %$(author)$%	Created on: %$(created-on)$%
 *
 * Revision Control Information:
 *
 * $Id: help.h,v 1.2 1996/10/04 12:04:11 cwilson Exp $
 *
 * This file created by RCS from:
 *
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/inc/na/RCS/help.h,v $
 *
 * Revision History:
 *
 * $Log: help.h,v $
 * Revision 1.2  1996/10/04 12:04:11  cwilson
 * latest rev
 *
 * Revision 1.77.102.7  1995/01/11  16:55:55  russ
 * Fixed some typos.
 *
 * Revision 1.77.102.6  1995/01/11  16:30:43  russ
 * Added support for all keyed modules in disabled modules.
 *
 * Revision 1.77.102.5  1995/01/11  15:54:25  carlson
 * SPR 3342 -- Removed TMUX (not supported until R9.3).
 *
 * Revision 1.77.102.4  1995/01/06  16:05:05  bullock
 * spr.3715 - Update help for authorized_groups, group_value and vcli_groups
 * Reviewed by Russ L.
 *
 * Revision 1.77.102.3  1995/01/03  14:40:42  russ
 * add vci as a disabled module.
 *
 * Revision 1.77.102.2  1994/11/03  10:27:17  barnes
 * SPR 2904: correct misspellings of asynchronous and synchronous
 *
 * Revision 1.77.102.1  1994/10/27  09:33:38  geiser
 * Correct lower limit of lat_queue_max to 1, correct default value of
 * session_limit to 1152
 *
 * Revision 1.77  1994/05/23  17:54:44  russ
 * Bigbird na/adm merge.
 *
 * Revision 1.69.2.9  1994/05/09  09:28:05  barnes
 * add missing escape before newline so na can compile
 *
 * Revision 1.69.2.8  1994/05/04  19:47:32  couto
 * Updated sync parameter information.
 *
 * Revision 1.69.2.7  1994/03/07  13:26:39  russ
 * removed all X25 trunk code.
 *
 * Revision 1.69.2.6  1994/03/01  15:22:36  russ
 * Lots'a fixes to the help text.
 *
 * Revision 1.69.2.5  1994/02/23  14:43:43  russ
 * Added sync command to generic help list for adm.
 *
 * Revision 1.69.2.4  1994/02/22  17:35:49  russ
 * added auto_adapt port mode.
 *
 * Revision 1.69.2.3  1994/02/14  09:20:26  russ
 * Add chap_auth_name; Add IPX parameters; Fix lots of incorrect help
 * text; Add missing help text;
 *
 * Revision 1.69.2.2  1994/02/04  16:36:25  russ
 * Fixed spelling errors, ordering. added port group "vci", annex group
 * "mop", banner from ports to async ports. and other various
 * misinformation in the help text.
 *
 * Revision 1.69.2.1  1994/01/06  15:34:59  wang
 * Bigbird NA/Admin params support.
 *
 * Revision 1.76  1994/05/18  17:04:20  russ
 * Add reset annex syslog.
 *
 * Revision 1.75  1994/04/14  18:48:41  raison
 * added show port arap and show annex arap
 *
 * Revision 1.74  1994/03/25  10:27:08  carlson
 * Added ftpd as a disabled module.
 *
 * Revision 1.73  1994/03/15  13:19:15  russ
 * fixed multiple na help complaints from spr.2433:
 * interface and ann lat not listed in reset help text.
 * boot -h flag works but not documented
 * port_keyword out of date (replaced by keyword which covers the annex
 * keywords also).
 *
 * Revision 1.72  1994/02/17  13:21:07  townsend
 * Littleboy merge.
 *
 * Revision 1.71  1994/01/25  13:34:23  russ
 * added fingerd as a disabled_modules option.
 *
 * Revision 1.70  1993/12/16  00:01:08  russ
 * Added interface to the help for set/show/reset helps.
 *
 * Revision 1.69.1.1  1994/01/26  13:59:08  geiser
 * Added new parameters for modem management and 5390 cmb jumper
 * (code by David Wang; check-in by Maryann Geiser)
 *
 * Revision 1.69  1993/09/17  12:16:04  russ
 * added allow_compression for ppp re: spr.2038!   also fixed help text for
 * all serial interface parameters which were once slip only.
 *
 * Revision 1.68  1993/09/11  23:03:04  gmalkin
 * Closed SPR 1535 - cleaned up admin help format
 *
 * Revision 1.67  1993/09/10  10:37:01  sasson
 * Updated reset annex usage string to include the modem subsystem.
 *
 * Revision 1.66  1993/08/27  15:25:07  carlson
 * SPR 1122 -- Added text for "output_flow_control=both" and removed
 * useless "bell" (input-only) option.
 * Reviewed by P. Couto, M. Sasson and D. Wang.
 *
 * Revision 1.65  1993/07/27  20:24:57  wang
 * na/admin interface redesign changes.
 *
 * Revision 1.64  1993/06/30  18:02:04  wang
 * Removed DPTG support.
 *
 * Revision 1.63  93/06/30  17:20:53  wang
 * Minor spr1758 fix.
 * 
 * Revision 1.62  93/06/11  10:58:47  wang
 * Fixed Spr1736, AppleTalk nodeid range problem. Reviewed by Scott Reeve.
 * 
 * Revision 1.61  93/05/20  18:04:07  carlson
 * SPR 1580 -- ripped out ELS code that disables name_server_2 and
 * added in selective code for ppp-group on port parameters.
 * Reviewed by David Wang.
 * 
 * Revision 1.60  93/05/12  16:44:08  wang
 * Removed atcp of PPP_NCP from help text.
 * 
 * Revision 1.59  93/05/12  16:26:10  carlson
 * Added "magic" index for annoying ELS name_server parameter, fixed
 * text for sys_location parameter (not used by SNMP) and repaired
 * bracket matching.
 * 
 * Revision 1.58  93/05/10  14:36:50  wang
 * Fixed Spr1533, display error for "show port interface".
 * 
 * Revision 1.57  93/04/28  17:31:16  russ
 * fixed the help text for config_file and alphabetized the help.
 * 
 * Revision 1.56  93/04/16  15:18:00  wang
 * Changed user_interface to usr_interface for Rel8.0 to avoid minimum
 * uniqueness problem and fixed help node_id and at_nodeid range (255->253).
 * 
 * Revision 1.55  93/03/30  11:05:06  raison
 * reserve parm ids for decserver and add them, turned off
 * 
 * Revision 1.54  93/03/25  13:46:37  wang
 * Added option_key param for AppleTalk and TN3270 and removed appletalk_key.
 * 
 * Revision 1.53  93/03/22  13:03:05  wang
 * Changed some params name to please the use of minimum uniqueness.
 * 
 * Revision 1.52  93/03/18  11:42:58  wang
 * Fixed "show interface all" problem.
 * 
 * Revision 1.51  93/03/15  17:12:11  wang
 * Added in meaningful help text for AppleTalk and interface parameters.
 * 
 * Revision 1.50  93/03/03  15:14:52  wang
 * Removed Appletalk THIS_NET param and changed NODE_ID param size.
 * 
 * Revision 1.49  93/03/01  18:43:21  wang
 * Support "show port serial" group params.
 * 
 * Revision 1.48  93/03/01  11:38:39  wang
 * Changed param names to demand_dial and phone_number.
 * 
 * Revision 1.47  93/02/27  15:06:32  wang
 * Fixed show lat category problem.
 * 
 * Revision 1.46  93/02/25  17:47:37  wang
 * Added 57600 baudrate support.
 * 
 * Revision 1.45  93/02/25  12:52:22  carlson
 * SPR 1319 -- Added time_server parameter for directed time service.
 * 
 * Revision 1.44  93/02/19  15:15:58  carlson
 * Added three TCP keepalive timers -- box, serial and parallel.
 * 
 * Revision 1.43  93/02/18  21:50:55  raison
 * added DECserver Annex parameters
 * 
 * Revision 1.42  93/02/18  13:22:16  wang
 * Removed un-needed appletalk and rip parameters.
 * 
 * Revision 1.41  93/02/01  17:59:21  carlson
 * Added Annex parameter -- IP broadcast forwarding.
 * 
 * Revision 1.40  93/01/29  13:05:29  wang
 * Fixed the typo causing "show port appletalk" problem.
 * 
 * Revision 1.39  93/01/22  17:49:10  wang
 * New parameters support for Rel8.0
 * 
 * Revision 1.38  92/11/02  15:50:30  russ
 * made Y the default for new printer auto linefeed param.
 * 
 * Revision 1.37  92/11/02  15:41:29  russ
 * Added new printer auto linefeed param.
 * 
 * Revision 1.36  92/10/30  14:33:07  russ
 * Added loose source route annex parameter.
 * 
 * Revision 1.35  92/10/15  14:23:01  wang
 * Added new NA parameters for ARAP support.
 * 
 * Revision 1.34  92/09/17  15:17:58  emond
 * Added -l to boot help message. -l switch is for loading flash.
 * 
 * Revision 1.33  92/08/13  09:37:14  bullock
 * spr.940 - options on boot and dumpboot modified.  Verified by R. Dumond
 * 
 * Revision 1.32  92/07/21  11:53:12  reeve
 * Removed #if's surrounding tftp_dir_name and tftp_dump_name params.
 * 
 * Revision 1.31  92/06/15  15:01:23  raison
 * separate attn_char_string so that Annex II's know it as attn_char and
 * Annex 3's and Micro's know it as attn_string.
 * 
 * Revision 1.30  92/06/12  14:11:38  barnes
 * changes for new NA and CLI admin display
 * 
 * Revision 1.29  92/05/28  16:03:01  reeve
 * Changed some net (i.e. slip or ppp) parameter spellings.
 * 
 * Revision 1.28  92/05/19  13:46:09  wang
 * Added ppp option for the "help mode" command in NA.
 * 
 * Revision 1.27  92/05/08  01:55:46  raison
 * changed name of cli-edit selectable mod to edit
 * 
 * Revision 1.26  92/04/30  10:06:56  raison
 * fixed typo that pervents na from building on non-xenna hosts
 * 
 * Revision 1.25  92/04/28  10:07:49  bullock
 * spr.756 - Add banner parameter
 * 
 * Revision 1.24  92/04/23  18:20:33  raison
 * take dptf out of select_mod help, add lat
 * 
 * Revision 1.23  92/04/16  13:42:11  barnes
 * make lat silently disabled when lat_key is invalid, don't put lat
 * or verbose-help in list of disabled modules
 * 
 * Revision 1.22  92/04/15  15:31:38  raison
 * changed PPP_security to PPP_security_protocol, changed security help text.
 * 
 * Revision 1.21  92/04/01  22:38:16  russ
 * Add new slave buffering command.
 * 
 * Revision 1.20  92/04/01  19:20:05  barnes
 * add cli-edit to help text for disabled_modules
 * 
 * Revision 1.19  92/03/24  17:53:23  sasson
 * Fixed local admin on Annex-II's: could not set any annex parameter past
 * syslog_host. Added VOID_CAT for syslog_port.
 * 
 * Revision 1.18  92/03/07  09:41:55  raison
 * correct help text
 * 
 * Revision 1.17  92/02/28  16:25:31  wang
 * Removed syslog_port support for Annex-II.
 * 
 * Revision 1.16  92/02/27  11:13:12  reeve
 * Took out #ifdef checks for NDPTG and NHMUX in table entries.  They
 * are now "VOID_CAT"'ed.
 * 
 * Revision 1.15  92/02/20  17:05:03  wang
 * Added support for new NA parameter syslog_port
 * 
 * Revision 1.14  92/02/18  17:45:58  sasson
 *  Added support for 3 LAT parameters(authorized_group, vcli_group, lat_q_max).
 * 
 * Revision 1.13  92/02/11  11:14:03  raison
 * change load_dump_sequence name from flash to self
 * 
 * Revision 1.12  92/02/06  11:28:28  carlson
 * SPR 543 - added latb_enable flag so LAT data-b packets can be ignored.
 * 
 * Revision 1.11  92/02/05  10:40:58  reeve
 * Made name_server an alias for name_server_1 on an ELS.
 * 
 * Revision 1.10  92/02/03  14:03:42  raison
 * fixed help msg
 * 
 * Revision 1.9  92/01/31  22:53:14  raison
 * added printer_set and printer_speed parm
 * 
 * Revision 1.8  92/01/31  12:53:02  wang
 * Add new NA parameter for config.annex file
 * 
 * Revision 1.7  92/01/27  18:59:53  barnes
 * changes for selectable modules
 * 
 * Revision 1.6  92/01/21  09:35:18  grant
 * Removed NPPP.  Local adm uses VOID_CAT now to deselect stuff.
 *
 * Revision 1.5  92/01/16  15:30:34  carlson
 * Removed macro file name parameter.
 *
 * Revision 1.4  92/01/12  08:12:54  raison
 * added flash info
 *
 * Revision 1.3  92/01/09  13:54:25  carlson
 * Added slip security flag and fixed compiler warning.
 *
 * Revision 1.2  91/12/30  16:48:26  reeve
 * Added new string, void_entry_string, used by init_tables() when
 * a table entry has VOID_CAT as its category.
 * 
 * Revision 1.1  91/12/12  14:04:49  russ
 * Initial revision
 * 
 * Revision 1.78  91/12/11  17:55:55  carlson
 * Fixed NPPP usage.
 * 
 * Revision 1.77  91/12/11  08:23:07  russ
 * Changed help text for slip_mtu_size, small is now the default.
 * 
 * Revision 1.76  91/11/22  14:06:14  reeve
 * Added help text for dialup_addresses.
 * 
 * Revision 1.75  91/11/22  12:46:44  russ
 * Add macro file parameter...  turned off PPP in 6.2 cli_adm stuff.
 * 
 * Revision 1.74  91/11/05  10:28:37  russ
 * Add slip_mtu_size to allow booting out of cslip ports.
 * 
 * Revision 1.73  91/11/04  13:14:00  reeve
 * Enabled use of " slip", "net", or "PPP" for network port parameters.
 * 
 * Revision 1.72  91/10/30  15:10:32  barnes
 * add new NA parameter to enable/disable SNMP sets
 * 
 * Revision 1.71  91/10/28  09:59:35  carlson
 * Updated help text for ATTN_CHAR for SPR 361.
 * 
 * Revision 1.70  91/10/25  17:37:06  russ
 * made local/remote_address change backwards compatable by adding the
 * "new_name,old_name" construct in the name field!
 * 
 * Revision 1.69  91/10/16  12:35:02  russ
 * Added off as a settable value for forwarding_timer.
 * 
 * Revision 1.68  91/10/15  17:21:36  carlson
 * Added help text for new autobaud options.
 * 
 * Revision 1.67  91/10/15  14:49:05  russ
 * Added more help text. port_keyword.
 * 
 * Revision 1.66  91/10/09  19:22:30  reeve
 * Added all of the PPP parameter entries.
 * 
 * Revision 1.65  91/09/30  10:12:44  carlson
 * Merged in spelling and grammar fixes, along with Russ's term_var change.
 * 
 * Revision 1.63.1.1  91/09/30  10:03:07  carlson
 * Fixed multiple spelling and grammar errors.
 * 
 * Revision 1.63  91/08/27  15:21:25  wang
 * Add in the warning message for boot and dumpboot command
 * 
 * Revision 1.62  91/08/14  09:56:23  raison
 * changed help typo.
 * 
 * Revision 1.61  91/07/31  20:42:31  raison
 * added telnet_crlf port parameter.  Also fixed up tftp_dir_name (now called
 * tftp_load_dir) and tftp_dump_name.
 * 
 * Revision 1.60  91/07/26  15:36:17  raison
 * unsupport tftp NA parameter fields for Annex-II
 * 
 * Revision 1.59  91/07/23  15:13:18  raison
 * fix typo, ok on xenna, but not on others
 * 
 * Revision 1.58  91/07/12  16:02:48  sasson
 * Removed non-implemented options in the "type" parameter (3270 and pc).
 * 
 * Revision 1.57  91/07/02  13:18:34  raison
 * added tftp_dir_name and tftp_dump_name parameters
 * 
 * Revision 1.56  91/06/13  12:19:05  raison
 * ifdef'ed out unneccessary structs, strings, and globals for cli admin
 * 
 * Revision 1.55  91/06/12  15:06:59  pjc
 * Improved help message for dptg_settings
 * 
 * Revision 1.54  91/04/05  13:38:07  raison
 * fixed lat parm name conflicts (i.e. server_name vs host_name).
 * 
 * Revision 1.53  91/03/07  16:23:32  pjc
 * Added help for new need_dsr and dptg_settings port parameters
 * 
 * Revision 1.52  91/03/01  13:33:15  pjc
 * Changed help string for port_multiplex,
 * added NDPTG > 0 ti NHMUX #if's
 * 
 * Revision 1.51  91/02/07  12:12:57  townsend
 * Forgot a line continuation mark on prev. check-in.
 * 
 * Revision 1.50  91/02/06  14:22:08  townsend
 * Added forwarding count
 * 
 * Revision 1.49  91/01/24  09:34:42  raison
 * added lat_value na parameter and GROUP_CODE parameter type.
 * 
 * Revision 1.48  91/01/10  13:40:37  grant
 * Added help for cli_imask7.
 * 
 * Revision 1.47  90/12/04  12:50:39  sasson
 * The image_name was increased to 100 characters on annex3.
 * 
 * Revision 1.46  90/10/08  13:55:19  raison
 * for spr.273 - added authoritative agent field as specified in RFC 1122.
 * 
 * Revision 1.45  90/10/03  15:14:15  russ
 * Fixed slip_tos help verbage.
 * 
 * Revision 1.44  90/09/04  16:23:20  raison
 * Removed (via "#if NHMUX > 0") port_multiplexing (for spr.230).
 * 
 * Revision 1.43  90/08/16  11:26:35  grant
 * 64 ports on port command help
 * 
 * Revision 1.42  90/07/29  22:17:37  loverso
 * new flags for compressed slip
 * 
 * Revision 1.41  90/02/20  16:07:15  russ
 * removed the references to keyword "virtual" in the port command.
 * 
 * Revision 1.40  90/02/05  17:38:11  russ
 * Added Great Britian daylight savings time.
 * 
 * Revision 1.39  90/01/24  14:44:03  russ
 * Added compile time switches to turn off X25 trunk functionality.
 * 
 * Revision 1.38  90/01/24  14:10:15  russ
 * Fixed the help syntax for "all: set port" and "copy: annex, port, printer,
 * and trunk".
 * 
 * Revision 1.37  89/12/19  16:02:48  russ
 * Added some reverse compatability code for annex parameters.
 * 
 * Revision 1.36  89/12/14  15:37:57  russ
 * New NA parameters for broadcast and others.
 * 
 * Revision 1.35  89/11/20  16:36:50  grant
 * Added help for broadcast direction, printer type and control lines both.
 * 
 * Revision 1.34  89/11/01  15:55:06  grant
 * Fixed some reset help typos
 * 
 * Revision 1.33  89/09/26  18:16:45  loverso
 * Fix typos in trunk parameters
 * 
 * Revision 1.32  89/09/24  22:32:03  loverso
 * SL/IP --> SLIP
 * 
 * Revision 1.31  89/09/18  14:07:25  grant
 * Help text for boot command.
 * 
 * Revision 1.30  89/08/21  15:06:05  grant
 * Dictionary entry for reset name server and new host table size.
 * 
 * Revision 1.29  89/06/15  16:20:01  townsend
 * Added dictionary entries for X.25
 * 
 * Revision 1.28  89/05/11  14:11:14  maron
 * Modified this module to support the new security parameters, vcli_password,
 * vcli_sec_ena, and port_password.
 * 
 * Revision 1.27  89/04/05  12:46:58  loverso
 * New copyright message
 * 
 * Revision 1.26  89/01/12  16:41:18  loverso
 * New help text/conv routines for inactivity timers
 * 
 * Revision 1.25  88/12/15  16:59:47  loverso
 * CLI inactivity timer
 * 
 * Revision 1.24  88/12/13  17:10:15  parker
 * added help message for motd_file parameter.
 * 
 * Revision 1.23  88/11/17  15:52:45  loverso
 * New routed parameter
 * 
 * Revision 1.22  88/10/17  10:40:51  mattes
 * Expanded dedicated_port help for rlogin and call
 * 
 * Revision 1.21  88/09/20  12:34:32  townsend
 * Fix boo boo in prev. help message
 * 
 * Revision 1.20  88/09/20  09:07:04  townsend
 * Added help message for RING_PRIORITY selection
 * 
 * Revision 1.19  88/07/08  14:33:43  harris
 * Modify help for imask_7bits, min_unique_hostnames, and reset.
 * 
 * Revision 1.18  88/06/30  15:28:37  mattes
 * Added 7-bit masking flag
 * 
 * Revision 1.17  88/06/09  12:23:25  mattes
 * Two more parameters
 * 
 * Revision 1.16  88/06/07  15:17:40  harris
 * Add ACP_KEY for encrypting ACP (security) transactions.
 * 
 * Revision 1.15  88/06/01  19:05:05  harris
 * Added Annex parameter, password.  Extend reset command to annex subsystems.
 * 
 * Revision 1.14  88/05/25  11:21:47  harris
 * Moved password command to after annex command.
 * 
 * Revision 1.13  88/05/16  12:14:02  mattes
 * Added dedicated-port parameters
 * 
 * Revision 1.12  88/05/04  23:07:19  harris
 * Turns out that "newline_terminal" was in the wrong plac, alphabetically.
 * 
 * Revision 1.11  88/05/04  22:22:31  harris
 * Fixed syntax shown in all, serial, virtual entries.  Added password command.
 * General beautification - shorten long lines, change a few descriptions.
 * 
 * Revision 1.10  88/04/07  16:10:57  emond
 * Accommodate for new bit parameter, IP encapsulation type
 * 
 * Revision 1.9  88/03/31  12:32:15  parker
 * added missing "," in help message for "serial".
 * 
 * Revision 1.8  88/03/17  15:25:30  mattes
 * Added per-port prompt and telnet esc char
 * 
 * Revision 1.7  88/03/15  14:37:55  mattes
 * Added help for all, serial, and virtual
 * 
 * Revision 1.6  88/03/14  16:52:31  parker
 * Added params for activity and idle timers
 * 
 * Revision 1.5  88/03/07  12:27:58  mattes
 * Added VCLI limit
 * 
 * Revision 1.4  88/03/02  14:32:03  parker
 * added syslog and time parameters
 * 
 * Revision 1.3  88/01/19  09:55:41  mattes
 * Added boot server parameter
 * 
 * Revision 1.2  87/12/29  11:44:45  mattes
 * Added 4.0 SLIP parameters
 *
 * Revision 1.1  87/09/28  15:46:58  mattes
 * Initial revision
 *
 * This file is currently under revision by:
 *
 *
 ****************************************************************************
 */

/* user must include "../cda/hmux.h" before including this include file */

/* Command and help message definitions */

char *cmd_string[] =
{
#ifdef NA
	 "       annex:  enter default Annex list",
	 "        boot:  boot an Annex",
	 "        copy:  copy annex/port/printer parameters to other Annexes",
	 "    dumpboot:  boot an Annex and produce an upline dump",
	 "        echo:  echo the remainder of the line to standard output",
	 "           #:  indicate a comment line (useful in command files)",
	 "    password:  enter default password",
	 "        read:  read and execute a script file",
	 "       write:  write the current configuration to a script file",
	 "   help or ?:  get help; \"help <command>\" displays command syntax",
#else
	 "   help or ?:  display this help screen",
#endif

	 "   broadcast:  send a broadcast message to a port or ports",
	 "asynchronous:  enter default asynchronous port set",
	 "        port:  same as asynchronous",
	 " synchronous:  enter default synchronous port set",
	 "     printer:  enter default printer set",
	 "   interface:  enter default interface set",
	 "        quit:  terminate administrator",
	 "       reset:  reset a port , interface or subsystem",
	 "        show:  display the current value of an eeprom parameter",
	 "         set:  modify the value of an eeprom parameter",
	 (char *)NULL
};

static char void_entry_string[] = /*NOSTR*/"table_entry_void";

static definition dictionary[] =
{
#ifdef NA
{"#",			A_COMMAND,	COMMENT_CMD
, "# <comments>"
},
#endif /* NA */
{"?",			A_COMMAND,	QUEST_CMD
#ifdef NA
, "?                               lists commands\n\t\
? *                             shows entire help dictionary\n\t\
? <command_name>                shows command syntax\n\t\
? <parameter_name>              explains parameter, shows valid values\n\t\
? <help_token>                  explains details of a sub-syntax\n\t\
? syntax                        explains help syntax"
#endif /* NA */
},
{"__dui_flow",          PORT_PARAM,     DUI_FLOW
#ifdef NA
, "For internal use only."
#endif /* NA */
},
{"__dui_iflow",         PORT_PARAM,     DUI_IFLOW
#ifdef NA
, "For internal use only."
#endif /* NA */
},
{"__dui_oflow",         PORT_PARAM,     DUI_OFLOW
#ifdef NA
, "For internal use only."
#endif /* NA */
},
{"a_router",		BOX_PARAM,	A_ROUTER
#ifdef NA
, "The node ID of the network's A_ROUTER, legal values are \n\t\
xx-xx-xx-xx-xx-xx"
#endif /* NA */
},
{"acp_key",		BOX_PARAM,	ACP_KEY
#ifdef NA
, "ACP encryption key: a string, maximum 15 characters"
#endif /* NA */
},
{"all",			BOX_PARAM,	ALL_BOX
#ifdef NA
, "show annex [= <annex_list>] all"
#endif /* NA */
},
{"all",			PORT_PARAM,	ALL_PORTP
#ifdef NA
, "show port [= <port_set>] all"
#endif /* NA */
},
{"all",			PRINTER_PARAM,	ALL_PRINTER
#ifdef NA
, "show printer [= <printer_set>] all"
#endif /* NA */
},
{"all",			INTERFACE_PARAM, ALL_INTERFACEP
#ifdef NA
, "show interface [= <interface_set>] all"
#endif /* NA */
},
{"all",			SYNC_PARAM, ALL_SYNCP
#ifdef NA
, "show synchronous [= <sync_set>] all"
#endif /* NA */
},
#ifdef NA
{"all",			HELP_ENTRY,	0
, "\
port all                set default asynchronous port list to all ports\n\t\
asynchronous all        set default asynchronous port list to all ports\n\t\
synchronous all         set default synchronous port lists to all ports\n\t\
broadcast = all         broadcast to all serial and virtual CLI ports\n\t\
reset all               reset all serial ports and all virtual CLI ports\n\t\
set port = all          set asynchronous port parameter on all ports\n\t\
set asynchronous = all  set asynchronous port parameter on all ports\n\t\
set interface = all     set interface parameter on all interfaces\n\t\
set synchronous = all   set synchronous port parameter on all ports\n\t\
show port = all         show asynchronous port parameter on all ports\n\t\
show asynchronous = all show asynchronous port parameter on all ports\n\t\
show interface = all    show interface parameter on all interface\n\t\
show synchronous = all  show synchronous port parameter on all ports"
},
#endif /* NA */
{"allow_broadcast",	PORT_PARAM,	BROADCAST_ON
#ifdef NA
, "allow NA broadcast to this port: Y or y to enable; N or n to disable"
#endif /* NA */
},
{"allow_compression",         SYNC_PARAM,    S_ALW_COMP
#ifdef NA
, "when enabled, the Annex will use TCP/IP header compression\n\t\
on this link only if the remote end initiates the compression.\n\t\
Y or y to enable; N or n to disable. By default it is disabled."
#endif /* NA */
},
{"allow_compression,slip_allow_compression",PORT_PARAM,	P_SLIP_EN_COMP
#ifdef NA
, "when enabled, the Annex will use TCP/IP header compression\n\t\
on this link only if the remote end initiates the compression.\n\t\
Y or y to enable; N or n to disable. By default it is disabled."
#endif /* NA */
},
{"allow_snmp_sets",	BOX_PARAM,	ALLOW_SNMP_SETS
#ifdef NA
, "allow SNMP set commands: Y or y to enable; N or n to disable"
#endif /* NA */
},
#ifdef NA
{"annex",		A_COMMAND,	BOX_CMD
, "annex <annex_list>"
},
#endif /* NA */
{"annex",		PARAM_CLASS,	BOX_CLASS
#ifdef NA
, "set/show annex [= <annex_list>] ..."
#endif /* NA */
},
#ifdef NA
{"annex_identifier",	HELP_ENTRY,	0
, "an Internet address (a.b/a.b.c/a.b.c.d) or a hostname (/etc/hosts)"
},
{"annex_list",		HELP_ENTRY,	0
, "<annex_identifier> [, <annex_identifier>]*"
},
#endif /* NA */
{"appletalk,arap",		BOX_CATEGORY,	BOX_APPLETALK
#ifdef NA
, "Show the AppleTalk subset of Annex parameters"
#endif /* NA */
},
{"appletalk,arap",		PORT_CATEGORY,	PORT_APPLETALK
#ifdef NA
, "Show the AppleTalk subset of port parameters"
#endif /* NA */
},
{"arap_v42bis",		PORT_PARAM,	P_ARAP_V42BIS
#ifdef NA
, "Allow the enabling of V.42bis compression\n\t\
Y or y to enable; N or n to disable"
#endif /* NA */
},
#ifdef SYNC
{"asd_forward",		BOX_PARAM,	ASD_FORWARD
#ifdef NA
, "to be defined"
#endif /* NA */
},
#endif
{"at_guest",		PORT_PARAM,	P_ARAP_AT_GUEST
#ifdef NA
, "Allow ARAP guest login service, Y or y to enable; N or n to disable"
#endif /* NA */
},
{"at_nodeid",		PORT_PARAM,	P_ARAP_AT_NODEID
#ifdef NA
, "the AppleTalk node ID hint the ANNEX will acquire and defined\n\t\
for the port, legal values are xxxx.xx, where xxxx range from 0..0xfeff\n\t\
and xx range from 0..0xfd"
#endif /* NA */
},
{"at_security",		PORT_PARAM,	P_ARAP_AT_SECURITY
#ifdef NA
, "enabling or disabling AppleTalk security: y or Y to enable;\n\t\
n or N to disable"
#endif /* NA */
},
{
#if defined(NA) || defined(i386)
"attn_string,attn_char"
#else
"attn_char,attn_string"
#endif
		,		PORT_PARAM,	ATTN_CHAR
#ifdef NA
, "CLI attention (interrupt) characters: a character or string (precede\n\t\
with \\ to force string interpretation)"
#endif /* NA */
},
{"authoritative_agent",		BOX_PARAM,	AUTH_AGENT
#ifdef NA
, "only authoritative agents may respond to ICMP subnet\n\t\
mask requests:  Y or y to enable; N or n to disable"
#endif /* NA */
},
{"authorized_groups",	PORT_PARAM,	AUTHORIZED_GROUPS
#ifdef NA
, "This Annex parameter will specify which remote group codes\n\t\
are accessible to a user on a particular Annex port. Each port\n\t\
has its own set of group codes. Syntax:\n\t\
set port authorized_groups <group range> enable | disable\n\t\
where <group range> is the set of groups ([similar to port set]\n\t\
between 0, and 255 inclusive) to affect (i.e. 1,2,3; 2; 5-10 are\n\t\
all valid group ranges).  A shortcut method can be used to enable or \n\t\
disable all group values.  To enable all groups, use:\n\t\
set port authorized_groups all \n\t\
To disable all groups, use:\n\t\
set port authorized_groups none"
#endif /* NA */
},
{"autobaud",    PORT_PARAM,     PORT_AUTOBAUD
#ifdef NA
, "This Port parameter will specify if autobauding is used:\n\t\
Y or y to enable; N or n to disable"
#endif /* NA */
},
{"banner", PORT_PARAM, BANNER
#ifdef NA
, "specifies whether to print banner \n\t\
Y or y to print, N or n to suppress"
#endif /*NA*/
},
{"bidirectional_modem",	PORT_PARAM,	BIDIREC_MODEM
#ifdef NA
, "bidirectional modem: Y or y to enable; N or n to disable"
#endif /* NA */
},
#ifdef NA
{"boot",		A_COMMAND,	BOOT_CMD
, "\
boot [-adhlq] <time> <annex_list> <filename> <warning>\n\t\
    a: abort a delay boot\n\t\
    d: create a code dump\n\t\
    h: cause a halt or reset diag\n\t\
    l: load the boot image into flash (for selfboot)\n\t\
    q: dumps quietly; send no warnings\n\t\
WARNING: booting the Annex with a non-existent image\n\t\
filename will cause the Annex to hang trying to find\n\t\
the image.  You must press the reset button to recover."
},
#endif /* NA */
{"broadcast",		A_COMMAND,	BROADCAST_CMD
#ifdef NA
, "broadcast [= <port_set>] <message>"
#endif /* NA */
},
{"broadcast_addr",	BOX_PARAM,	BROAD_ADDR
#ifdef NA
, "Internet address that the Annex uses for broadcasting: an inet address"
#endif /* NA */
},

{"broadcast_direction",	PORT_PARAM,	BROADCAST_DIR
#ifdef NA
, "Broadcast messages to network or port on slave ports"
#endif /* NA */
},
{"chap_auth_name",	BOX_PARAM,	CHAP_AUTH_NAME
#ifdef NA
, "Used in the name field in chap challenge messages. Maximim string\n\t\
length is 16 characters."
#endif /* NA */
},
{"char_erase",		PORT_PARAM,	CHAR_ERASING
#ifdef NA
, "destructive character erasing: Y or y to enable; N or n to disable"
#endif /* NA */
},
{"circuit_timer",		BOX_PARAM,	CIRCUIT_TIMER
#ifdef NA
, "the time interval in 10's of milliseconds between the transmission of\n\t\
LAT packets:  an integer, 1 - 100 inclusive.\n\t\
set annex circuit_timer <value>"
#endif /* NA */
},
{"cli_imask7",		PORT_PARAM,	CLI_IMASK7
#ifdef NA
, "Masks input at the CLI to 7 bits: Y or y to enable; N or n to disable"
#endif /* NA */
},
{"cli_inactivity",	PORT_PARAM,	INACTIVITY_CLI
#ifdef NA
, "CLI inactivity timer interval: 0 or \"off\" to disable, \"immediate\"\n\t\
to hangup after last session, or an integer (time in minutes)"
#endif /* NA */
},
{"cli_interface",              PORT_PARAM,     USER_INTERFACE
#ifdef NA
, "Specifies either a Unix or VMS command line interface.\n\t\
The default is uci, the alternative being vci."
#endif /* NA */
},
{"cli_prompt",		BOX_PARAM,	CLI_PROMPT_STR
#ifdef NA
, "Annex Command Line Interpreter prompt string: a prompt_string"
#endif /* NA */
},
{"cli_security",	PORT_PARAM,	CLI_SECURITY
#ifdef NA
, "ACP authorization required to use CLI: Y or y to enable; N or n to\n\t\
disable"
#endif /* NA */
},
{"clocking",             SYNC_PARAM,    S_CLOCKING
#ifdef NA
, "The clock rate supplied for this port. The allowable\n\t\
values are 56, 64, 112, 128 or external.\n\t\
The default is external."
#endif /* NA */
},
#ifdef NA
{"command_name",	HELP_ENTRY,	0
, "the name of one of the Network Administrator commands"
},
{"comments",		HELP_ENTRY,	0
, "any sequence of characters - used only for documentation"
},
#endif /* NA */
{"config_file",		BOX_PARAM,	CONFIG_FILE
#ifdef NA
, "Specifies the configuration file to access on the load host. This file\n\t\
contains information about gateways, rotaries, macros, and services"
#endif /* NA */
},
{"connect_security",	PORT_PARAM,	CONNECT_SECURITY
#ifdef NA
, "ACP authorization required to make host login connections:\n\t\
Y or y to enable; N or n to disable"
#endif /* NA */
},
{"control_lines",	PORT_PARAM,	CONTROL_LINE_USE
#ifdef NA
, "usage of control lines: none, flow_control, modem_control, both"
#endif /* NA */
},
#ifdef NA
{"copy",		A_COMMAND,	COPY_CMD
, "\
copy annex   [<annex_identifier>] [<annex_list>]\n\t\
copy printer [<printer_number>@<annex_identifier>] [<printer_set>]\n\t\
copy port    [<port_number>@<annex_id>] [<port_set>]\n\t\
copy asynchronous [<port_number>@<annex_id>] [<port_set>]\n\t\
copy interface    [<interface_number>@<annex_id>] [<interface_set>]\n\t\
copy synchronous  [<sync_port_number>@<annex_id>] [<sync_port_set>]"
},
#endif /* NA */
{"data_bits",		PORT_PARAM,	BITS_PER_CHAR
#ifdef NA
, "number of bits per character: 5, 6, 7, 8"
#endif /* NA */
},
{"daylight_savings",	BOX_PARAM,	TZ_DLST
#ifdef NA
, "type of Daylight Savings Time to use:\n\t\
us, australian, west_european, mid_european, east_european, british,\n\t\
canadian, or none"
#endif /* NA */
},
{"dedicated_address",	PORT_PARAM,	DEDICATED_ADDRESS
#ifdef NA
, "remote address to use when port is in \"dedicated\" mode:\n\t\
a host_identifier"
#endif /* NA */
},
{"dedicated_port",	PORT_PARAM,	DEDICATED_PORT
#ifdef NA
, "remote TCP port number to use when port is in \"dedicated\" mode:\n\t\
\"telnet\", \"rlogin\", \"call\" (Annex-MX only) or a number"
#endif /* NA */
},
{"default_modem_hangup",PORT_PARAM,	DEFAULT_HPCL
#ifdef NA
, "(4.2 only) always hang up the modem line after the last close:\n\t\
Y or y to enable; N or n to disable"
#endif /* NA */
},
{"default_session_mode",PORT_PARAM,     DEF_SESS_MODE
#ifdef NA
, "This is the initial session mode for LAT connections: \n\t\
\"interactive\", \"pasthru\", \"passall\", \"transparent\"."
#endif /* NA */
},
{"default_zone_list",		BOX_PARAM,	DEF_ZONE_LIST
#ifdef NA
, "the zone list sent to ARAP clients as the local backup to ACP failure\n\t\
a string, maximum 100 characters"
#endif /* NA */
},
{"demand_dial",	PORT_PARAM,	P_SLIP_NET_DEMAND_DIAL
#ifdef NA
, "dial on demand: y or Y to enable, n or N to disable"
#endif /* NA */
},
{"dialup_addresses",PORT_PARAM,	P_PPP_DIALUP_ADDR
#ifdef NA
, "Request dialup addresses from ACP.\n\t\
Y or y to enable; N or n to disable"
#endif /* NA */
},
{"dialup_addresses",SYNC_PARAM,	S_DIAL_ADDR
#ifdef NA
, "Request dialup addresses from ACP.\n\t\
Y or y to enable; N or n to disable"
#endif /* NA */
},
{"disabled_modules", 	BOX_PARAM,      SELECTED_MODULES
#ifdef NA
, "lists the software modules that are currently disabled. Valid \n\
\tmodule names are the constructs \"all\" or \"none\" or any combination\n\
\tof the following:\n\
\t  admin,atalk,dialout,edit,fingerd,ftpd,ipx,lat,nameserver,\n\
\t  ppp,slip,snmp,tn3270,vci\n\
\tThis parameter works in conjunction with lat_key and option_key"
#endif /* NA */
},
{"do_compression,slip_do_compression",	PORT_PARAM,	P_SLIP_DO_COMP
#ifdef NA
, "when enabled, the Annex will start TCP/IP header compression\n\t\
on this asynchronous link.\n\t\
Y or y to enable; N or n to disable. By default it is disabled."
#endif /* NA */
},
#ifdef NA
{"dumpboot",		A_COMMAND,	DUMPBOOT_CMD
, "dumpboot [-aq] <time> <annex_list> <filename> <warning>\n\n\
\tWARNING: booting the Annex with a non-existent image\n\
\tfilename will cause the Annex to hang, trying to find\n\
\tthe image.  You must press the reset button to recover."
},
{"echo",		A_COMMAND,	ECHO_CMD
, "echo [<message>]"
},
#endif /* NA */
{"echo",		PORT_PARAM,	INPUT_ECHO
#ifdef NA
, "perform input echoing: Y or y to enable; N or n to disable"
#endif /* NA */
},
{"editing",		PORT_CATEGORY,	PORT_EDITING
#ifdef NA
, "Show the port editing subset of port parameters"
#endif /* NA */
},
{"enable_security",	BOX_PARAM,	ENABLE_SECURITY
#ifdef NA
, "Selects if security (ACP or local security) is enabled or disabled for\n\t\
the entire Annex.  If this parameter is disabled, then all security is\n\t\
disabled.  Y or y to enable; N or n to disable"
#endif /* NA */
},
{"erase_char",		PORT_PARAM,	ERASE_CHAR
#ifdef NA
, "character used to erase a character: a character"
#endif /* NA */
},
{"erase_line",		PORT_PARAM,	ERASE_LINE
#ifdef NA
, "character used to erase a line: a character"
#endif /* NA */
},
{"erase_word",		PORT_PARAM,	ERASE_WORD
#ifdef NA
, "character used to erase a word: a character"
#endif /* NA */
},
{"facility_num",	BOX_PARAM,	HOST_NUMBER
#ifdef NA
, "an integer identifying the Annex's LAT facility number\n\t\
(0 - 32767):  set annex facility_num <value>"
#endif /* NA */
},
#ifdef NA
{"filename",		HELP_ENTRY,	0
, "a UNIX filename or pathname"
},
#endif /* NA */
{"flow",		PORT_CATEGORY,	PORT_FLOW
#ifdef NA
, "Show the flow control subset of Annex parameters"
#endif /* NA */
},
{"force_CTS",		SYNC_PARAM,	S_FORCE_CTS
#ifdef NA
, "Force CTS signal on a synchronous port to be active.  Default is to\n\t\
let the interface control CTS."
#endif /* NA */
},
{"forwarding_count",	PORT_PARAM,	FORWARD_COUNT
#ifdef NA
, "the minimum number of characters to be received by the port before\n\t\
the characters are forwarded: an integer"
#endif /* NA */
},
{"forwarding_timer",	PORT_PARAM,	FORWARDING_TIMER
#ifdef NA
, "forwarding timer interval: 0 or \"off\" to disable or an integer\n\t\
up to 255 (time in tens of milliseconds)"
#endif /* NA */
},
{"generic",		BOX_CATEGORY,	BOX_GENERIC
#ifdef NA
, "Show the generic subset of Annex parameters"
#endif /* NA */
},
{"generic",		PORT_CATEGORY,	PORT_GENERIC
#ifdef NA
, "Show the generic subset of port parameters"
#endif /* NA */
},
{"generic",		SYNC_CATEGORY,	S_GENERIC
#ifdef NA
, "Show the synchronous port subset of port parameters"
#endif /* NA */
},
{"group_value",		BOX_PARAM,	GROUP_CODE
#ifdef NA
, "Annex LAT group code for permitting access to LAT services.  To\n\t\
access a specific LAT services, the Annex must have at least one\n\t\
enabled group code match the service's set group codes.  In fact,\n\t\
the Annex will not maintain any information about unauthorized\n\t\
services :\n\t\t\
set annex group_value <group range> enable | disable\n\t\
where <group range> is the set of groups ([similar to port set]\n\t\
between 0, and 255 inclusive) to affect (i.e. 1,2,3; 2; 5-10 are\n\t\
all valid group ranges).  A shortcut method can be used to enable or \n\t\
disable all group values.  To enable all groups, use:\n\t\
set annex group_value all \n\t\
To disable all groups, use:\n\t\
set annex group_value none"
#endif /* NA */
},
{"hardware_tabs",	PORT_PARAM,	PORT_HARDWARE_TABS
#ifdef NA
, "hardware tab operation: Y or y to enable; N or n to disable"
#endif /* NA */
},
{"hardware_tabs",	PRINTER_PARAM,	PRINT_HARDWARE_TABS
#ifdef NA
, "hardware tab operation: Y or y to enable; N or n to disable"
#endif /* NA */
},
{"help",		A_COMMAND,	HELP_CMD
#ifdef NA
, "help                            lists commands\n\t\
help *                          shows entire help dictionary\n\t\
help <command_name>             shows command syntax\n\t\
help <parameter_name>           explains parameter, shows valid values\n\t\
help <help_token>               explains details of a sub-syntax\n\t\
help syntax                     explains help syntax"
#endif /* NA */
},
#ifdef NA
{"help_token",		HELP_ENTRY,	0
, "a metasymbol used in the text of help messages (this is one)"
},
{"host_identifier",	HELP_ENTRY,	0
, "an Internet address (a.b/a.b.c/a.b.c.d) or a hostname (/etc/hosts)"
},
#endif /* NA */
{"host_table_size",	BOX_PARAM,	HTABLE_SZ
#ifdef NA
, "the maximum number of host names that can be stored in the Annex\n\t\
host table: an integer, 1 - 250 or the keywords \"none\" for no host table\n\t\
or \"unlimited\" for no upper bounds for host table size"
#endif /* NA */
},
{"image_name",		BOX_PARAM,	IMAGE_NAME
#ifdef NA
, "the default boot image filename: a filename, maximum 100 characters"
#endif /* NA */
},
{"imask_7bits",		PORT_PARAM,	IMASK_7BITS
#ifdef NA
, "clear the 8th bit of received 8-bit characters:\n\t\
Y or y to enable; N or n to disable"
#endif /* NA */
},
{"inactivity_timer",	PORT_PARAM,	INACTIVITY_TIMER
#ifdef NA
, "serial port inactivity timer interval: 0 or \"off\" to disable or\n\t\
an integer (time in minutes)"
#endif /* NA */
},
{"inet_addr",		BOX_PARAM,	INET_ADDR
#ifdef NA
, "the Annex's Internet address: an annex_identifier"
#endif /* NA */
},
{"input_buffer_size",	PORT_PARAM,	INPUT_BUFFER_SIZE
#ifdef NA
, "number of 256 byte input buffers allocated to port: an integer"
#endif /* NA */
},
{"input_flow_control",	PORT_PARAM,	INPUT_FLOW_CONTROL
#ifdef NA
, "type of input flow control: none, eia, start/stop, bell"
#endif /* NA */
},
{"input_is_activity", PORT_PARAM,	INPUT_ACT
#ifdef NA
, "Port input should reset the port inactivity_timer:\n\t\
Y or y to enable; N or n to disable"
#endif /* NA */
},
{"input_start_char",	PORT_PARAM,	INPUT_START_CHAR
#ifdef NA
, "start character for input flow control: a character"
#endif /* NA */
},
{"input_stop_char",	PORT_PARAM,	INPUT_STOP_CHAR
#ifdef NA
, "stop character for input flow control: a character"
#endif /* NA */
},
{"interface",		A_COMMAND,	INTERFACE_CMD
#ifdef NA
, "interface <interface_set>"
#endif /* NA */
},
{"interface",		PARAM_CLASS,	INTERFACE_CLASS
#ifdef NA
, "set/show interface [= <interface_set>] ..."
#endif /* NA */
},
{"ip_forward_broadcast", BOX_PARAM,	IP_FWD_BCAST
#ifdef NA
, "forward broadcasted IP messages to all interfaces:\n\
\t\tY or y to enable; N or n to disable"
#endif /* NA */
},
#ifdef SYNC
{"ip_ttl",		BOX_PARAM,	IP_TTL
#ifdef NA
, "to be defined"
#endif /* NA */
},
#endif
{"ipencap_type",	BOX_PARAM,	IPENCAP_TYPE
#ifdef NA
, "type of IP encapsulation: ethernet, or ieee802 for IEEE 802.2/802.3"
#endif /* NA */
},
{"ipso_class",           PORT_PARAM,    IPSO_CLASS
#ifdef NA
, "Defines the IP security classification for packets sent\n\t\
and received on this port. Possible schemes are topsecret,\n\t\
secret, confidential, unclassified and none.\n\t\
The default value is none."
#endif /* NA */
},
{"ipx",            	BOX_CATEGORY,	BOX_IPX
#ifdef NA
, "Show the IPX subset of Annex parameters"
#endif /* NA */
},
{"ipx_do_checksum",	BOX_PARAM,	IPX_DO_CHKSUM
#ifdef NA
, "Allows the user to enable IPX checksum - the feature is\n\t\
only supported on Netware version 3.12 and 4.xx.\n\t\
Y or y to enable; N or n to disable. By default it is disabled."
#endif /* NA */
},
{"ipx_dump_password",	BOX_PARAM,	IPX_DUMP_PWD
#ifdef NA
,"This field defines the User's password that the Annex will\n\t\
use to log into the Novell File server it booted from. It is\n\t\
required to perform a dump of the Annex image to the file server.\n\t\
Maximim string length is 48 characters."
#endif /* NA */
},
{"ipx_dump_path",	BOX_PARAM,	IPX_DUMP_PATH
#ifdef NA
, "This field defines the full Novell path to store the dump\n\t\
image on the file server. Maximim string length is 100 characters."
#endif /* NA */
},
{"ipx_dump_username",	BOX_PARAM,	IPX_DUMP_UNAME
#ifdef NA
, "This field defines the User Name that the Annex will use\n\t\
to log into the Novell File server it booted from. It is\n\t\
required to perform a dump of the Annex image to the File server.\n\t\
Maximim string length is 48 characters."
#endif /* NA */
},
{"ipx_file_server",	BOX_PARAM,	IPX_FILE_SERVER
#ifdef NA
, "This field defines the name of the Novell File server from\n\t\
which the Annex is going to boot. Maximim string length is 48 \n\t\
characters."
#endif /* NA */
},
{"ipx_frame_type",	BOX_PARAM,	IPX_FRAME_TYPE
#ifdef NA
, "The framing used for IPX protocol packets. Legal values are raw802_3,\n\t\
ethernetII, 802_2 and 802_2snap. The default value is raw802_3."
#endif /* NA */
},
{"ipx_security",         PORT_PARAM,    IPX_SECURITY
#ifdef NA
, "Controls whether IPX security is enabled on this port.\n\t\
The default is disabled. Y or y to enable; N or n to disable"
#endif /* NA */
},
{"ixany_flow_control",	PORT_PARAM,	IXANY_FLOW_CONTROL
#ifdef NA
, "any character restarts output: Y or y to enable; N or n to disable"
#endif /* NA */
},
{"keep_alive_timer",		BOX_PARAM,	KA_TIMER
#ifdef NA
, "the time interval in seconds between LAT id packets during times\n\t\
of LAT network inactivity: an integer, 10 - 255 inclusive.\n\t\
set annex keep_alive_timer <value>"
#endif /* NA */
},
{"kerbclock_skew",       BOX_PARAM,     KERBCLK_SKEW
#ifdef NA
, "The value in minutes that the clocks of the various Kerberos\n\t\
servers and clients may differ. This is used to prevent replay\n\t\
attacks. The default value is 5 minutes."
#endif /* NA */
},
{"kerberos",            BOX_CATEGORY,      BOX_KERBEROS
#ifdef NA
, "Show the Kerberos subset of Annex parameters"
#endif /* NA */
},
{"kerberos_host",        BOX_PARAM,     KERB_HOST
#ifdef NA
, "A list of zero to four IP addresses (in dotted decimal form)\n\t\
or host names of the Kerberos authentication servers to be used\n\t\
when authenticating a new user. The names and addresses are \n\t\
separated by commas."
#endif /* NA */
},
{"kerberos_security",         BOX_PARAM,     KERB_SECURITY_ENA
#ifdef NA
, "Controls whether the Annex uses a Kerberos authentication\n\t\
server for user authentication.\n\t\
The default is no. Y or y to enable; N or n to disable"
#endif /* NA */
},
#ifdef NA
{"keyword",	HELP_ENTRY,	0
, "Specifies a logical group of parameters associated with the annex:\n\t\
    annex keywords: all/appletalk/generic/lat/nameserver/router/\n\t\
                    security/syslog/time/vcli\n\t\
    port keywords: appletalk/editing/flow/generic/lat/ppp/security/\n\t\
                   serial/slip/timers/tn3270"
},
#endif
{"lat",			BOX_CATEGORY,	BOX_LAT
#ifdef NA
, "Show the LAT subset of Annex parameters"
#endif /* NA */
},
{"lat",			PORT_CATEGORY,	PORT_LAT
#ifdef NA
, "Show the LAT subset of port parameters"
#endif /* NA */
},
{"lat_key",		BOX_PARAM,	KEY_VALUE
#ifdef NA
, "the lat_key is a security mechanism which restricts unauthorized\n\t\
activation of LAT in the Annex:\n\t\
set annex lat_key <value>\n\t\
This parameter works in conjunction with disabled_modules"
#endif /* NA */
},
{"lat_queue_max",	BOX_PARAM,	QUEUE_MAX
#ifdef NA
, "This parameter defines the maximum number of host requests (HIC's)\n\t\
that the Annex will save in its internal queue when the requested\n\t\
resource is not available (port busy). The syntax is:\n\t\
set annex lat_queue_max <number between 1 and 255>."
#endif /* NA */
},
{"latb_enable",		PORT_PARAM,	LATB_ENABLE
#ifdef NA
, "controls interpretation of LAT Data-B packets received from host:\n\t\
Y or y to enable; N or n to disable"
#endif /* NA */
},
#ifdef DO_LEAP_PROTOCOL
{"leap_protocol_on",	PORT_PARAM,	DO_LEAP_PROTOCOL
#ifdef NA
, "enable LEAP protocol: Y or y to enable; N or n to disable"
#endif /* NA */
},
#endif
{"line_erase",		PORT_PARAM,	LINE_ERASING
#ifdef NA
, "destructive line erasing: Y or y to enable; N or n to disable"
#endif /* NA */
},
{"load_broadcast",	 BOX_PARAM,	LOADSERVER_BCAST
#ifdef NA
, "broadcast for file loading server to use if none found:\n\t\
Y or y to enable; N or n to disable"
#endif /* NA */
},
{"load_dump_gateway",	BOX_PARAM,	LOADUMP_GATEWAY
#ifdef NA
, "if the preferred load or dump host is on a different network or subnet,\n\t\
the Internet address of a gateway to use: a host_identifier"
#endif /* NA */
},
{"load_dump_sequence",	BOX_PARAM,	LOADUMP_SEQUENCE
#ifdef NA
, "list of network interfaces to use when downloading or upline dumping.\n\n\t\
The load_dump_sequence, \"self\" indicates to boot the image and load\n\t\
the configuration files from the local media.  The Annex will not dump\n\t\
to itself, instead it will dump to the first non-local interface\n\t\
specified in the load_dump_sequence, or to the net interface by default:\n\t\
set annex load_dump_sequence <net_interface>[,<net_interface>]*"
#endif /* NA */
},
{"local_address",            SYNC_PARAM,     S_LOCAL_ADDR
#ifdef NA
, "the Internet address of the local endpoint of the interface\n\t\
associated with the port: a host_identifier\n\t\
The default value is 0.0.0.0"
#endif /* NA */
},
{"local_address,slip_local_address",	PORT_PARAM,	P_SLIP_LOCALADDR
#ifdef NA
, "the Internet address of the local endpoint of the interface\n\t\
associated with the port: a host_identifier\n\t\
The default value is 0.0.0.0"
#endif /* NA */
},
{"location",		PORT_PARAM,	LOCATION
#ifdef NA
, "Port Device location printed by \"who\" command:\n\t\
a string, maximum 16 characters"
#endif /* NA */
},
{"location",		SYNC_PARAM,	S_LOCATION
#ifdef NA
, "Port Device location printed by \"who\" command:\n\t\
a string, maximum 16 characters"
#endif /* NA */
},
{"lock_enable",         BOX_PARAM,      LOCK_ENABLE
#ifdef NA
, "Enables the lock command on ports"
#endif /* NA */
},
{"login_password",       BOX_PARAM,     LOGIN_PASSWD
#ifdef NA
, "The Password for all ports where the cli_interface is\n\t\
set to vci and the login_port_password is enabled.\n\t\
When defined this string is displayed as \"<set>\". The default\n\t\
value is \"<unset>\"."
#endif /* NA */
},
{"login_port_password",       PORT_PARAM,    DUI_PASSWD
#ifdef NA
, "Enables the port password if the port is configured as\n\t\
a DECserver interface port.\n\t\
The default is disabled. Y or y to enable; N or n to disable"
#endif /* NA */
},
{"login_prompt",                BOX_PARAM,      LOGIN_PROMPT
#ifdef NA
, "This is a string that specifies the port prompt for port with\n\t\
the user_interface_type set to \"vms\"."
#endif /* NA */
},
{"login_timeout",        PORT_PARAM,    DUI_INACT_TIMEOUT
#ifdef NA
, "Enables a login timer if the port is configured as a\n\t\
DECserver interface port.\n\t\
The default is disabled. Y or y to enable; N or n to disable"
#endif /* NA */
},
{"login_timer",          BOX_PARAM,     LOGIN_TIMER
#ifdef NA
, "the inactivity timer for all ports whose cli_interface parameter\n\t\
is set to vci. Legal values are in the range of 1-60 minutes.\n\t\
By default this is 30 minutes."
#endif /* NA */
},
{"long_break",		PORT_PARAM,	LONG_BREAK
#ifdef NA
, "accept long line break as CLI attention character:\n\t\
Y or y to enable; N or n to disable"
#endif /* NA */
},
{"loose_source_route",	BOX_PARAM,	LOOSE_SOURCE_RT
#ifdef NA
, "allow internet protocol loose source routing:\n\t\
Y or y to enable (default); N or n to disable"
#endif /* NA */
},
{"map_to_lower",	PORT_PARAM,	MAP_U_TO_L
#ifdef NA
, "upper to lower case mapping: Y or y to enable; N or n to disable"
#endif /* NA */
},
{"map_to_upper",	PORT_PARAM,	MAP_L_TO_U_PORT
#ifdef NA
, "lower to upper case mapping: Y or y to enable; N or n to disable"
#endif /* NA */
},
{"map_to_upper",	PRINTER_PARAM,	MAP_L_TO_U_PRINT
#ifdef NA
, "lower to upper case mapping: Y or y to enable; N or n to disable"
#endif /* NA */
},
{"max_session_count",	PORT_PARAM,	MAX_SESSIONS
#ifdef NA
, "maximum number of CLI sessions allowed: an integer, 1 - 16"
#endif /* NA */
},
{"max_vcli",		BOX_PARAM,	VCLI_LIMIT
#ifdef NA
, "maximum number of Virtual CLIs: an integer, 0 - 254, or \"unlimited\""
#endif /* NA */
},
#ifdef NA
{"message",		HELP_ENTRY,	0
, "the text of the message to be broadcast or echoed"
},
#endif /* NA */
{"metric",		PORT_PARAM,	P_SLIP_METRIC
#ifdef NA
, "the metric (cost) of using the serial interface associated with the\n\t\
port: an integer"
#endif /* NA */
},
{"metric",               SYNC_PARAM,     S_METRIC
#ifdef NA
, "this parameter describes the hop count to the remote end of\n\t\
the synchronous interface. Legal values are in the range 1-15.\n\t\
The default value is 1."
#endif /* NA */
},
{"min_unique_hostnames",BOX_PARAM,	NMIN_UNIQUE
#ifdef NA
, "accept abbreviated host names if they are unique:\n\t\
Y or y to enable; N or n to disable"
#endif /* NA */
},
{"mode",		PORT_PARAM,	PORT_MODE
#ifdef NA
, "the mode of an async port (valid options are):  cli, slave,\n\t\
adaptive, slip, ppp, dedicated, arap, printer, ndp, auto_detect,\n\t\
ipx and unused.  The default setting is cli.\n\t\
NOTE: some port mode options are keyed options\n\t\
    cli           Command Line Interface\n\t\
    slave         Available via the port server\n\t\
    adaptive      Adapts to cli or slave mode on a fcfs basis\n\t\
    slip          Serial Line Internet Protocol\n\t\
    ppp           Point-to-Point Protocol\n\t\
    dedicated     Virtual connection to a specific host\n\t\
    arap          Appletalk Reverse Access Protocol\n\t\
    printer       Currently equivelant to slave\n\t\
    auto_detect   Auto Detection of incoming IPX, SLIP, PPP, and CLI\n\t\
    auto_adapt    Adapts to auto_detect or slave mode on a fcfs basis\n\t\
    ndp           Novell LMMGR/LMUSER Utilities\n\t\
    ipx           incoming IPX only\n\t\
    unused        unused"
#endif /* NA */
},
{"mode",                 SYNC_PARAM,     S_MODE
#ifdef NA
, "the mode of the port;  ppp or unused. The default is unused."
#endif /* NA */
},
{"modem_acc_entries",	BOX_PARAM,	ACC_ENTRIES
#ifdef NA
, "modem accounting entries"
#endif /* NA */
},
{"mop",			BOX_CATEGORY,      BOX_MOP
#ifdef NA
, "Show the MOP subset of Annex parameters"
#endif /* NA */
},
{"mop_password",                BOX_PARAM,      MOP_PASSWD
#ifdef NA
, "MOP password for administrative net connection"
#endif /* NA */
},
{"motd_file",		BOX_PARAM,	MOTD
#ifdef NA
, "Name of host file that contains The Message-Of-The-Day"
#endif /* NA */
},
{"multicast_timer",             BOX_PARAM,      MULTI_TIMER
#ifdef NA
, "MOP Multicast timer for system id announcements"
#endif /* NA */
},
{
"name_server_1",	BOX_PARAM,	PRIMARY_NS
#ifdef NA
, "primary name server to use for host name translation:\n\t\
none, ien_116, dns"
#endif /* NA */
},
{"name_server_2",	BOX_PARAM,	SECONDARY_NS
#ifdef NA
, "secondary name server to use for host name translation:\n\t\
none, ien_116, dns"
#endif /* NA */
},
{"nameserver",		BOX_CATEGORY,	BOX_NAMESERVER
#ifdef NA
, "Show the nameserver subset of Annex parameters"
#endif /* NA */
},
{"nameserver_broadcast", BOX_PARAM,	NAMESERVER_BCAST
#ifdef NA
, "broadcast for name server to use for host name translation:\n\t\
Y or y to enable; N or n to disable"
#endif /* NA */
},
#ifdef SYNC
{"nd_forward",		BOX_PARAM,	ND_FORWARD
#ifdef NA
, "to be defined"
#endif /* NA */
},
#endif
{"need_dsr",		PORT_PARAM,	NEED_DSR
#ifdef NA
, "need the DSR signal to be asserted when connecting to slave port:\n\t\
Y or y to enable; N or n to disable"
#endif /* NA */
},
{"net_inactivity",	PORT_PARAM,	P_SLIP_NET_INACTIVITY
#ifdef NA
, "SLIP/PPP inactivity timer interval: 0 or\n\t\
to disable or time(in minutes) maximum is 255"
#endif /* NA */
},
{"net_interface",	HELP_ENTRY,	0
#ifdef NA
, "the name of a network interface: net (Ethernet) or sl<n> (SLIP\n\t\
interface <n>), where <n> is an integer, 2 - 64"
#endif /* NA */
},
{"network_turnaround",	BOX_PARAM,	NET_TURNAROUND
#ifdef NA
, "turnaround timeout for network (seconds): an integer, 1 - 10"
#endif /* NA */
},
{"newline_terminal",	PORT_PARAM,	NEWLINE_TERMINAL
#ifdef NA
, "newline operation: Y or y to enable; N or n to disable"
#endif /* NA */
},
{"node_id",		BOX_PARAM,	NODE_ID
#ifdef NA
, "the ANNEX AppleTalk node ID hint, legal values are xxxx.xx where \n\t\
xxxx range from 0..0xfeff and xx range from 0..0xfd"
#endif /* NA */
},
{"option_key",		BOX_PARAM,	OPTION_KEY
#ifdef NA
, "the option_key is a security mechanism which restricts unauthorized\n\t\
activation of keyed features in the Annex:\n\t\
set annex option_key <value>\n\t\
This parameter works in conjunction with disabled_modules"
#endif /* NA */
},
{"output_flow_control",	PORT_PARAM,	OUTPUT_FLOW_CONTROL
#ifdef NA
, "type of output flow control: none, eia, start/stop, both"
#endif /* NA */
},
{"output_is_activity",	PORT_PARAM,	OUTPUT_ACT
#ifdef NA
, "Port output should reset the port inactivity_timer:\n\t\
Y or y to enable; N or n to disable"
#endif /* NA */
},
{"output_start_char",	PORT_PARAM,	OUTPUT_START_CHAR
#ifdef NA
, "start character for output flow control: a character"
#endif /* NA */
},
{"output_stop_char",	PORT_PARAM,	OUTPUT_STOP_CHAR
#ifdef NA
, "stop character for output flow control: a character"
#endif /* NA */
},
{"output_ttl",           BOX_PARAM,     OUTPUT_TTL
#ifdef NA
, "The value that is placed in the ttl field of all locally\n\t\
generated IP packets. The default value is 64."
#endif /* NA */
},
#ifdef NA
{"parameter_name",	HELP_ENTRY,	0
, "the name of one of the annex/port/printer eeprom parameters"
},
#endif /* NA */
{"parity",		PORT_PARAM,	PARITY
#ifdef NA
, "type of parity: even, odd, none"
#endif /* NA */
},
{"passwd_limit",                BOX_PARAM,      PASSWD_LIMIT
#ifdef NA
, "Number of times that password is prompted before logging out the user"
#endif /* NA */
},
{"password",		BOX_PARAM,	BOX_PASSWORD
#ifdef NA
, "Annex administration password: a string, maximum 15 characters"
#endif /* NA */
},
#ifdef NA
{"password",		A_COMMAND,	PASSWORD_CMD
, "password [<password>]"
},
#endif /* NA */
{"phone_number",	PORT_PARAM,	P_SLIP_NET_PHONE
#ifdef NA
, "phone number for demand dialing: a string, maximum 32 characters"
#endif /* NA */
},
{"port,asynchronous",		A_COMMAND,	PORT_CMD
#ifdef NA
, "port <port_set>"
#endif /* NA */
},
{"port,asynchronous",		PARAM_CLASS,	PORT_CLASS
#ifdef NA
, "set/show port [= <port_set>] ..."
#endif /* NA */
},
#ifdef NA
{"port_list",		HELP_ENTRY,	0
, "<port_range> [, <port_range>]*  /  all / virtual / serial"
},
#endif /* NA */
#ifdef NA
{"port_number",		HELP_ENTRY,	0
, "an integer from 1 to 64"
},
#endif /* NA */
{"port_password",	PORT_PARAM,	PORT_PASSWORD
#ifdef NA
, "Port password for local security: a string, maximum 15 characters\n\t\
When defined this string is displayed as \"<set>\". The default\n\t\
value is \"<unset>\"."
#endif /* NA */
},
{"port_password",        SYNC_PARAM,     S_PORT_PASSWD
#ifdef NA
, "Port password for local security: a string, maximum 16 characters\n\t\
When defined this string is displayed as \"<set>\". The default\n\t\
value is \"<unset>\"."
#endif /* NA */
},
#ifdef NA
{"port_range",		HELP_ENTRY,	0
, "<port_number> [- <port_number>]"
},
#endif /* NA */
{"port_server_security",PORT_PARAM,	PORT_SERVER_SECURITY
#ifdef NA
, "ACP authorization required to access port via port server feature:\n\t\
Y or y to enable; N or n to disable"
#endif /* NA */
},
#ifdef NA
{"port_set",		HELP_ENTRY,	0
, "<port_list> [@ <annex_list>] [; <port_list> [@ <annex_list>]]*"
},
#endif /* NA */
{"ppp",			PORT_CATEGORY,	PORT_PPP
#ifdef NA
, "Show the PPP subset of port parameters"
#endif /* NA */
},
{"ppp",			SYNC_CATEGORY,	S_PPP
#ifdef NA
, "Show the PPP subset of synchronous port parameters"
#endif /* NA */
},
{"ppp_acm",		PORT_PARAM,	P_PPP_ACM
#ifdef NA
, "mask used by peer to avoid sending unwanted\n\t\
characters: a four octet bit mask, entered as 8 hex\n\t\
characters. (0x00000000)"
#endif /* NA */
},
#ifdef P_PPP_ACTOPEN
{"ppp_active_open",	PORT_PARAM,	P_PPP_ACTOPEN
#ifdef NA
, "have ANNEX initiate LCP negoatiation or wait for\n\t\
peer to do so.  Y or y to enable; N or n to disable"
#endif /* NA */
},
#endif
{"ppp_mru",		PORT_PARAM,	P_PPP_MRU
#ifdef NA
, "This parameter describes the maximum receive unit in bytes\n\t\
in use. Legal values are in the range 64-1500.\n\t\
The default value is 1500."
#endif /* NA */
},
{"ppp_mru",              SYNC_PARAM,     S_PPP_MRU
#ifdef NA
, "This parameter describes the maximum receive unit in bytes\n\t\
in use. Legal values are in the range 64-1500.\n\t\
The default value is 1500."
#endif /* NA */
},
{"ppp_ncp",		PORT_PARAM,	P_PPP_NCP
#ifdef NA
, "the protocol running on this interface. ipcp, atcp or all.\n\t\
The default is all."
#endif /* NA */
},
{"ppp_ncp",              SYNC_PARAM,     S_PPP_NCP
#ifdef NA
, "the protocol running on this interface. ipcp, atcp or all.\n\t\
The default is all."
#endif /* NA */
},
{"ppp_password_remote",	PORT_PARAM,	P_PPP_PWORDRMT
#ifdef NA
, "The password used by the Annex to identify itself if the\n\t\
remote peer asks for authentication. When defined this string\n\t\
is displayed as \"<set>\". The default value is \"<unset>\"."
#endif /* NA */
},
{"ppp_password_remote",  SYNC_PARAM,     S_PPP_PASSWD_REMOTE
#ifdef NA
, "The password used by the Annex to identify itself if the\n\t\
remote peer asks for authentication. When defined this string\n\t\
is displayed as \"<set>\". The default value is \"<unset>\"."
#endif /* NA */
},
{"ppp_security",       SYNC_PARAM,     S_SECURE
#ifdef NA
, "ACP authorization required to use ppp on a synchronous port not\n\t\
secured by ACP : Y or y to enable; N or n to disable"
#endif
},
{"ppp_security_protocol",	PORT_PARAM,	P_PPP_SECURITY
#ifdef NA
, "type of authentication to be used for protocol level security\n\t\
check if Annex enable_security is enabled. Legal values are \n\t\
none, pap, chap or chap-pap. The default is none."
#endif /* NA */
},
{"ppp_security_protocol",       SYNC_PARAM,     S_PPP_SECURE_PROTO
#ifdef NA
, "type of authentication to be used for protocol level security\n\t\
check if Annex enable_security is enabled. Legal values are \n\t\
none, pap, chap or chap-pap. The default is none."
#endif /* NA */
},
{"ppp_username_remote",	PORT_PARAM,	P_PPP_UNAMERMT
#ifdef NA
, "The Username used by the Annex to identify itself if the\n\t\
remote peer asks for authentication. The default value is empty."
#endif /* NA */
},
{"ppp_username_remote",  SYNC_PARAM,     S_PPP_USRNAME_REMOTE
#ifdef NA
, "The Username used by the Annex to identify itself if the\n\t\
remote peer asks for authentication. The default value is empty."
#endif /* NA */
},
{"pref_dump_addr",	BOX_PARAM,	PREF_DUMP
#ifdef NA
, "the Internet address of the preferred dump host: a host_identifier"
#endif /* NA */
},
{"pref_load_addr",	BOX_PARAM,	PREF_LOAD
#ifdef NA
, "the Internet address of the preferred load host: a host_identifier"
#endif /* NA */
},
{"pref_mop_host",       BOX_PARAM,      MOP_PREF_HOST
#ifdef NA
, "The ethernet address of the preferred load MOP host"
#endif /* NA */
},
{"pref_name1_addr",	BOX_PARAM,	PRIMARY_NS_ADDR
#ifdef NA
, "the Internet address of the preferred primary name server:\n\t\
a host_identifier"
#endif /* NA */
},
{"pref_name2_addr",	BOX_PARAM,	SECONDARY_NS_ADDR
#ifdef NA
, "the Internet address of the preferred secondary name server:\n\t\
a host_identifier"
#endif /* NA */
},
{"pref_secure1_host,pref_secure_host", BOX_PARAM, PREF_SECURE_1
#ifdef NA
, "the Internet address of the preferred primary security host:\n\t\
a host_identifier"
#endif /* NA */
},
{"pref_secure2_host",	BOX_PARAM,	PREF_SECURE_2
#ifdef NA
, "the Internet address of the preferred secondary security host:\n\t\
a host_identifier"
#endif /* NA */
},
{"printer",		A_COMMAND,	PRINTER_CMD
#ifdef NA
, "printer <printer_set>"
#endif /* NA */
},
{"printer",		PARAM_CLASS,	PRINTER_CLASS
#ifdef NA
, "set/show printer [= <printer_set>] ..."
#endif /* NA */
},
{"printer_crlf",	PRINTER_PARAM,	PRINTER_CR_CRLF
#ifdef NA
, "convert <CR> to <CR><LF>: Y or y to enable (default); N or n to disable"
#endif /* NA */
},
{"printer_host",	PORT_PARAM,	P_TN3270_PRINTER_HOST
#ifdef NA
, "the IP address of a host running a printer spooler"
#endif /* NA */
},
{"printer_name",	PORT_PARAM,	P_TN3270_PRINTER_NAME
#ifdef NA
, "the printer name"
#endif /* NA */
},
{"printer_speed",	PRINTER_PARAM,	PRINTER_SPD
#ifdef NA
, "printer speed on Micro-Annex: \"normal\", \"high_speed\""
#endif /* NA */
},
{"printer_width",	PRINTER_PARAM,	PRINTER_WIDTH
#ifdef NA
, "printer width (columns per line): an integer"
#endif /* NA */
},
{"prompt",		PORT_PARAM,	PORT_PROMPT
#ifdef NA
, "CLI prompt for this port: a prompt_string"
#endif /* NA */
},
#ifdef NA
{"prompt_string",	HELP_ENTRY,	0
, "a string with embedded format sequences, used as a prompt:\n\t\
   %a  the string \"annex\"     %c  the string \": \"\n\t\
   %d  the date and time      %i  the Annex's Internet address\n\t\
   %j  a newline character    %l  port location, or \"port n\"\n\t\
   %n  the Annex's name       %p  the port number\n\t\
   %r  the string \"port\"      %s  a space\n\t\
   %t  the time hh:mm:ss      %u  the user name of the port\n\t\
   %%  the string \"%\""
},
#endif /* NA */
{"ps_history_buffer",	PORT_PARAM,	PS_HISTORY_BUFF
#ifdef NA
, "specifies how much data to buffer on a slave port\n\t\
(0 - 32767): 0 to disable or an integer (number of characters)"
#endif /* NA */
},
{"quit",		A_COMMAND,	QUIT_CMD
#ifdef NA
, "quit"
#endif /* NA */
},
#ifdef NA
{"read",		A_COMMAND,	READ_CMD
, "read <filename>"
},
#endif /* NA */
{"redisplay_line",	PORT_PARAM,	REDISPLAY_LINE
#ifdef NA
, "character used to redisplay input line: a character"
#endif /* NA */
},
{"remote_address",       SYNC_PARAM,    S_REMOTE_ADDR
#ifdef NA
, "the Internet address of the remote endpoint of the interface\n\t\
associated with the port: a host_identifier\n\t\
The default value is 0.0.0.0"
#endif /* NA */
},
{"remote_address,slip_remote_address",	PORT_PARAM,	P_SLIP_REMOTEADDR
#ifdef NA
, "the Internet address of the remote endpoint of the interface\n\t\
associated with the port: a host_identifier\n\t\
The default value is 0.0.0.0"
#endif /* NA */
},
{"reset",		A_COMMAND,	RESET_CMD
#ifdef NA
, "\
reset annex [<subsystem>]*\n\t\
reset printer <= printer_set>\n\t\
reset <port_set>\n\t\
reset port [= <port_set>]\n\t\
reset asynchronous [= <port_set>]\n\t\
reset interface [= <interface_set>]\n\t\
reset synchronous [= <sync_port_set>]\n\n\t\
Subsystems are: all, dialout, lat, macros, modem, motd, nameserver,\n\t\
security, syslog"
#endif /* NA */
},
{"reset_idle_time_on", PORT_PARAM,	RESET_IDLE
#ifdef NA
, "Data direction that should reset the idle time displayed by \"who\":\n\t\
input or output"
#endif /* NA */
},
{"retrans_limit",		BOX_PARAM,	RETRANS_LIMIT
#ifdef NA
, "the number of times to retransmit a packet before notifying user of\n\t\
network failure:  an integer, 4 - 120 inclusive.\n\t\
set annex retrans_limit <value>"
#endif /* NA */
},
{"ring_priority",	BOX_PARAM,	RING_PRIORITY
#ifdef NA
, "access priority for IEEE 802.5 Token Ring: an integer, 0-3,\n\t\
where: 0 is lowest priority and 3 is highest priority"
#endif /* NA */
},
{"rip_accept",		INTERFACE_PARAM,	RIP_ACCEPT
#ifdef NA
, "Control which networks are accepted from RIP update. The legal values\n\t\
are none , all or up to eight inclusive or exclusive list of networks"
#endif /* NA */
},
{"rip_advertise",	INTERFACE_PARAM,	RIP_ADVERTISE
#ifdef NA
, "Control which networks are advertised. The legal values are none, all\n\t\
or up to eight inclusive or exclusive list of networks"
#endif /* NA */
},
{"rip_auth",		BOX_PARAM,	RIP_AUTH
#ifdef NA
, "Control RIP packets authentication. The legal value is the clear-text\n\t\
password to be used to authenticate the packets"
#endif /* NA */
},
{"rip_default_route",	INTERFACE_PARAM,	RIP_DEFAULT_ROUTE
#ifdef NA
, "Control override of configured default route. If a default route update\n\t\
is received with a metric less than or equal to the current setting\n\t\
the current default route will be overridden. The legal value is\n\t\
between 1 and 15 (inclusive)"
#endif /* NA */
},
/* rip_domain is a void interface parameter */
{"rip_domain",		INTERFACE_PARAM,	RIP_DOMAIN
#ifdef NA
, "Control to which RIP Routing Domain this interface belongs\n\t\
Any value between 1 and 65535 (inclusive) is valid"
#endif /* NA */
},
{"rip_horizon",		INTERFACE_PARAM,	RIP_HORIZON
#ifdef NA
, "Set the split horizon algorithm, the legal values are split, off\n\t\
or poison"
#endif /* NA */
},
#ifdef SYNC
{"rip_override_default",	INTERFACE_PARAM,	RIP_OVERRIDE_DEFAULT
#ifdef NA
, "to be defined"
#endif /* NA */
},
#endif
{"rip_recv_version",		INTERFACE_PARAM,	RIP_RECV_VERSION
#ifdef NA
, "Set the RIP version will be accepted, the legal values are 1, 2 or both"
#endif /* NA */
},
{"rip_routers",		BOX_PARAM,	RIP_ROUTERS
#ifdef NA
, "Control periodic RIP responses to be directed to a list of routers\n\t\
or broadcast. The legal values are all or a list of up to eight routers'\n\t\
IP addresses"
#endif /* NA */
},
{"rip_send_version",		INTERFACE_PARAM,	RIP_SEND_VERSION
#ifdef NA
, "Set the RIP version will be sent, the legal values are 1, 2 or \n\t\
compatibility"
#endif /* NA */
},
{"rip_sub_accept",		INTERFACE_PARAM,	RIP_SUB_ACCEPT
#ifdef NA
, "Control acceptance of subnet routes: y or Y to enable, n or N to disable"
#endif /* NA */
},
{"rip_sub_advertise",		INTERFACE_PARAM,	RIP_SUB_ADVERTISE
#ifdef NA
, "Control advertising of subnet routes: y or Y to enable, \n\t\
n or N to disable"
#endif /* NA */
},
{"routed",		BOX_PARAM,	NROUTED
#ifdef NA
, "Listen to routed broadcasts to fill the Annex routing table:\n\t\
Y or y to enable; N or n to disable"
#endif /* NA */
},
{"router",		BOX_CATEGORY,	BOX_ROUTER
#ifdef NA
, "Show the router subset of Annex parameters"
#endif /* NA */
},
{"rwhod",		BOX_PARAM,	NRWHOD
#ifdef NA
, "Listen to rwho broadcasts to fill the Annex host table:\n\t\
Y or y to enable; N or n to disable"
#endif /* NA */
},
#ifdef SYNC
{"sd_forward",		BOX_PARAM,	SD_FORWARD
#ifdef NA
, "to be defined"
#endif /* NA */
},
#endif
{"security",		BOX_CATEGORY,	BOX_SECURITY
#ifdef NA
, "Show the security subset of Annex parameters"
#endif /* NA */
},
{"security",		PORT_CATEGORY,	PORT_SECURITY
#ifdef NA
, "Show the security subset of port parameters"
#endif /* NA */
},
{"security_broadcast", BOX_PARAM,	SECURSERVER_BCAST
#ifdef NA
, "broadcast for security server to use if none found:\n\t\
Y or y to enable; N or n to disable"
#endif /* NA */
},
{"seg_jumper_5390",	BOX_PARAM,	JUMPER_5390
#ifdef NA
, "cmb 5390 jumper"
#endif /* NA */
},
{"serial",		PORT_CATEGORY,	PORT_SERIAL
#ifdef NA
, "Show the generic serial interface subset of port parameters"
#endif /* NA */
},
{"serial",		SYNC_CATEGORY,	S_NET
#ifdef NA
, "Show the generic serial interface subset of port parameters"
#endif /* NA */
},
#ifdef NA
{"serial",		HELP_ENTRY,	0
, "port serial          set default port list to all serial ports\n\t\
broadcast = serial   broadcast to all serial ports\n\t\
reset serial         reset all serial ports"
},
#endif /* NA */
{"server_capability",	BOX_PARAM,	SERVER_CAP
#ifdef NA
, "list of files that the Annex can provide to other Annexes:\n\t\
image, config, motd, all or none"
#endif /* NA */
},
{"server_name",		BOX_PARAM,	HOST_NAME
#ifdef NA
, "A string identifying the Annex's LAT host name (maximum\n\t\
of 16 characters): set annex server_name <string>"
#endif /* NA */
},
{"service_limit",		BOX_PARAM,	SERVICE_LIMIT
#ifdef NA
, "the maximum number of LAT service names that can be stored in the\n\t\
Annex service table: an integer, 16 - 2048 inclusive.\n\t\
set annex service_limit <value>"
#endif /* NA */
},
{"session_limit",             BOX_PARAM,      SESSION_LIMIT
#ifdef NA
, "the session_limit is the maximum number of sessions allowed on the Annex.\n\t\
The default value is 1152, setting a value of 0 sets no limit; the maximum\n\t\
value is normally 16 times the number of ports on the Annex.\n\t\
set annex session_limit <value>"
#endif /* NA */
},
{"set",			A_COMMAND,	SET_CMD
#ifdef NA
, "\
set annex   [= <annex_list>] [<parameter_name> <value>]*\n\t\
set printer [= <printer_set>] [<parameter_name> <value>]*\n\t\
set port    [= <port_set>] [<parameter_name> <value>]*\n\t\
set asynchronous [= <port_set>] [<parameter_name> <value>]*\n\t\
set interface    [= <interface_set>] [<parameter_name> <value>]*\n\t\
set synchronous  [= <sync_port_set>] [<parameter_name> <value>]*"
#endif /* NA */
},
{"short_break",		PORT_PARAM,	SHORT_BREAK
#ifdef NA
, "short line break: Y or y to enable; N or n to disable"
#endif /* NA */
},
{"show",		A_COMMAND,	SHOW_CMD
#ifdef NA
, "\
show annex   [= <annex_list>] [<parameter_name>]*\n\t\
show printer [= <printer_set>] [<parameter_name>]*\n\t\
show port    [= <port_set>] [<parameter_name>]* [<keyword>]*\n\t\
show asynchronous [= <port_set>] [<parameter_name>]* [<keyword>]*\n\t\
show interface    [= <interface_set>] [<parameter_name>]*\n\t\
show synchronous  [= <sync_port_set>] [<parameter_name>]* [<keyword>]*"
#endif /* NA */
},
{"slip",		PORT_CATEGORY,	PORT_SLIP
#ifdef NA
, "Show the SLIP subset of port parameters"
#endif /* NA */
},
{"slip_allow_dump",	PORT_PARAM,	P_SLIP_ALLOW_DUMP
#ifdef NA
, "allow upline memory dump over the SLIP interface associated with\n\t\
the port: Y or y to enable; N or n to disable"
#endif /* NA */
},
{"slip_load_dump_host",	PORT_PARAM,	P_SLIP_LOADUMP_HOST
#ifdef NA
, "the address of the host to load from or dump to over the SLIP\n\t\
interface associated with the port: a host_identifier"
#endif /* NA */
},
{"slip_mtu_size",	PORT_PARAM,	P_SLIP_LARGE_MTU
#ifdef NA
, "force CSLIP interface to use Large \"1006\" or Small \"256\"\n\t\
MTU (Maximum transmission unit): large or small (default)"
#endif /* NA */
},
{"slip_no_icmp",	PORT_PARAM,	P_SLIP_NO_ICMP
#ifdef NA
, "silently discard all ICMP packets destined to traverse this SLIP\n\t\
interface: Y or y to enable; N or n to disable"
#endif /* NA */
},
{"slip_ppp_security,slip_security",       PORT_PARAM,     P_SLIP_SECURE
#ifdef NA
, "ACP authorization required to use slip or ppp command from a CLI not\n\t\
secured by ACP : Y or y to enable; N or n to disable"
#endif
},
{"slip_tos",		PORT_PARAM,	P_SLIP_FASTQ
#ifdef NA
, "transmit interactive traffic before any other traffic over this SLIP\n\t\
interface for cheap type-of-service queuing:\n\t\
Y or y to enable; N or n to disable"
#endif /* NA */
},
{"speed",		PORT_PARAM,	PORT_SPEED
#ifdef NA
, "the speed of the port: 50, 75, 110, 134.5, 150, 200, 300, 600, 1200,\n\t\
1800, 2000, 2400, 3600, 4800, 7200, 9600, 19200, 38400, 57600, autobaud\n\t\
or autobaud with a default output speed specified as autobaud/(speed)."
#endif /* NA */
},
{"stop_bits",		PORT_PARAM,	STOP_BITS
#ifdef NA
, "number of stop bits: 1, 1.5, 2"
#endif /* NA */
},
{"subnet_mask",		BOX_PARAM,	SUBNET_MASK
#ifdef NA
, "the Annex network subnet mask: an Internet address mask"
#endif /* NA */
},
{"subnet_mask",	PORT_PARAM,	P_SLIP_NETMASK
#ifdef NA
, "the subnet mask of the point-to-point network defined by the SLIP\n\t\
interface associated with the port: an Internet address mask"
#endif /* NA */
},
{"subnet_mask",	SYNC_PARAM,	S_NETMASK
#ifdef NA
, "the subnet mask of the point-to-point network defined by the PPP\n\t\
interface associated with the port: an Internet address mask"
#endif /* NA */
},
#ifdef NA
{"sync_port_list",            HELP_ENTRY,     0
, "<sync_port_range> [, <sync_port_range>]*  /  all / virtual / serial"
},
#endif /* NA */
#ifdef NA
{"sync_port_number",          HELP_ENTRY,     0
, "an integer from 1 to 4"
},
#endif
#ifdef NA
{"sync_port_range",           HELP_ENTRY,     0
, "<sync_port_number> [- <sync_port_number>]"
},
#endif /* NA */
#ifdef NA
{"sync_port_set",             HELP_ENTRY,     0
, "<sync_port_list> [@ <annex_list>] [; <sync_port_list> [@ <annex_list>]]*"
},
#endif /* NA */
{"synchronous",                    A_COMMAND,      SYNC_CMD
#ifdef NA
, "synchronous <sync_port_set>"
#endif /* NA */
},
{"synchronous",           PARAM_CLASS,    SYNC_CLASS
#ifdef NA
, "set/show synchronous [= <sync_port_set>] ..."
#endif /* NA */
},
#ifdef NA
{"syntax",		HELP_ENTRY,	0
, "\n\t\
When entering mnemonics, use minimum uniqueness principle.\n\t\
Strings may be enclosed in double-quote characters.\n\t\
\n\t\
[syntax]                optional (may be omitted)\n\t\
[syntax]*		may be omitted, or occur one or more times\n\t\
one_way / another	choice between one_way and another (not both)\n\t\
...			continue according to appropriate command\n\t\
<help_token>		a sub-syntax - help <help_token> gives details\n\t\
\n\t\
Other symbols are actual commands or parameters to be entered."
},
#endif /* NA */
{"sys_location",	BOX_PARAM,	HOST_ID
#ifdef NA
, "system location string (maximum of 32 characters).  LAT uses this\n\
\tstring for the host identification field."
#endif /* NA */
},
{"syslog",		BOX_CATEGORY,	BOX_SYSLOG
#ifdef NA
, "Show the syslog subset of Annex parameters"
#endif /* NA */
},
{"syslog_facility",	BOX_PARAM,	SYSLOG_FAC
#ifdef NA
, "Annex syslog facility code: log_local[0-7] or number"
#endif /* NA */
},
{"syslog_host",		BOX_PARAM,	SYSLOG_HOST
#ifdef NA
, "Host to which Annex syslog messages are logged: a host_identifier"
#endif /* NA */
},
{"syslog_mask",		BOX_PARAM,	SYSLOG_MASK
#ifdef NA
, "list of syslog severity levels the Annex should report: emergency,\n\t\
alert, critical, error, warning, notice, info, debug, none, and all"
#endif /* NA */
},
{"syslog_port",		BOX_PARAM,	SYSLOG_PORT
#ifdef NA
, "Port to which Annex syslog messages are logged: a port_identifier"
#endif /* NA */
},
{"tcp_keepalive",	BOX_PARAM,	TCP_KEEPALIVE
#ifdef NA
, "default TCP connection keepalive timer value in minutes:\n\
\t\tInteger in the range 0 to 255 -- default value if zero is\n\
\t\t120 minutes."
#endif /* NA */
},
{"tcp_keepalive",	PORT_PARAM,	TCPA_KEEPALIVE
#ifdef NA
, "default TCP keepalive timer for serial port connections in minutes:\n\
\t\tInteger in the range 0 to 255.  If set to zero, then the\n\
\t\tdefault value as set by the \"annex tcp_keepalive\" value\n\
\t\tis used."
#endif /* NA */
},
{"tcp_keepalive",	PRINTER_PARAM,	TCPP_KEEPALIVE
#ifdef NA
, "default TCP keepalive timer for print connections in minutes:\n\
\t\tInteger in the range 0 to 255.  If set to zero, then the\n\
\t\tdefault value as set by the \"annex tcp_keepalive\" value\n\
\t\tis used."
#endif /* NA */
},
{"telnet_crlf",		PORT_PARAM,	TELNET_CRLF
#ifdef NA
, "newline sequence to use for telnet: when enabled, telnet uses\n\t\
<CR><LF>; when disabled, telnet uses <CR><NULL>\n\t\
Y or y to enable; N or n to disable"
#endif /* NA */
},
{"telnet_escape",	PORT_PARAM,	TELNET_ESC
#ifdef NA
, "escape character to use with the telnet command: a character"
#endif /* NA */
},
{"telnetd_key",               BOX_PARAM,     TELNETD_KEY
#ifdef NA
, "The Kerberos service key used by the kerberised telnetd on\n\t\
this Annex. This service key is used to decrypt a Kerberos ticket\n\t\
received from the user when attempting to access the telnetd\n\t\
service on this Annex. When defined this string is displayed as\n\t\
\"<set>\". The default value is \"<unset>\"."
#endif /* NA */
},
{"term_var",		PORT_PARAM,	TERM_VAR
#ifdef NA
, "Terminal type variable: a string, maximum sixteen characters"
#endif /* NA */
},
{"tftp_dump_name",		BOX_PARAM,	TFTP_DUMP_NAME
#ifdef NA
, "the filename to which the Annex should dump in the event of a critical\n\t\
system error.  This filename including complete parent directory names\n\t\
(maximum 100 characters)"
#endif /* NA */
},
{"tftp_load_dir,tftp_dir_name",		BOX_PARAM,	TFTP_DIR_NAME
#ifdef NA
, "the tftp directory name which is prepended to Annex file names\n\t\
for tftp transfers during Annex loading.  Used for image file name and\n\t\
support files (i.e. gateways, rotaries, etc)"
#endif /* NA */
},
{"tgs_host",             BOX_PARAM,     TGS_HOST
#ifdef NA
, "A list of zero to four IP addresses (in dotted decimal form)\n\t\
or host names of the Kerberos ticket granting servers to be used\n\t\
when a user requests a Kerberos ticket. The names and addresses are \n\t\
separated by commas."
#endif /* NA */
},
{"time",		BOX_CATEGORY,	BOX_TIME
#ifdef NA
, "Show the time subset of Annex parameters"
#endif /* NA */
},
{"time_broadcast",	 BOX_PARAM,	TIMESERVER_BCAST
#ifdef NA
, "broadcast for time service if primary server doesn't respond:\n\t\
Y or y to enable; N or n to disable"
#endif /* NA */
},
{"time_server",	 BOX_PARAM,	TIMESERVER_HOST
#ifdef NA
, "IP address of host providing time service.  Boot host will be used\n\
\tif this field is 0.0.0.0.  No direct queries will be made if this\n\
\tfield is set to 127.0.0.1."
#endif /* NA */
},
{"timers",		PORT_CATEGORY,	PORT_TIMERS
#ifdef NA
, "Show the timer subset of port parameters"
#endif /* NA */
},
{"timezone_minuteswest",BOX_PARAM,	TZ_MINUTES
#ifdef NA
, "Minutes west of GMT: an integer"
#endif /* NA */
},
{"tn3270",		PORT_CATEGORY,	PORT_TN3270
#ifdef NA
, "Show the tn3270 subset of port parameters"
#endif /* NA */
},
{"toggle_output",	PORT_PARAM,	TOGGLE_OUTPUT
#ifdef NA
, "character used to toggle output: a character"
#endif /* NA */
},
{"type",		PORT_PARAM,	PORT_TYPE
#ifdef NA
, "the type of the port: hardwired, dial_in, terminal, modem, printer"
#endif /* NA */
},
{"type",	PRINTER_PARAM,	PRINTER_INTERFACE
#ifdef NA
, "printer interface style: (dataproducts or centronics)"
#endif /* NA */
},
{"type_of_modem",		PORT_PARAM, 	MODEM_VAR	
#ifdef NA
, "the modem type connected to the port: a string, maximum 16 characters"
#endif /* NA */
},
{"user_name",		PORT_PARAM,	PORT_NAME
#ifdef NA
, "Default username printed by \"who\" command and passed by \"rlogin\":\n\t\
a string, maximum 16 characters"
#endif /* NA */
},
{"user_name",      SYNC_PARAM,    S_USRNAME
#ifdef NA
, "A string specifying the current user name for the synchronous\n\t\
port (maximum of 16 characters): The default value is a null string."
#endif /* NA */
},
#ifdef NA
{"value",		HELP_ENTRY,	0
, "the value of an annex/port/printer eeprom parameter\n\t\
\n\t\
A value may be enclosed in double-quotes.  Integers may range \n\t\
from 0 to 255, 0 is converted to a default.  Characters may \n\t\
be entered in ^X notation, hexadecimal (0x58), octal (0130),\n\t\
or literally.  Note that '00' is the character NULL which\n\t\
converts to the default, while '0' is ASCII zero.\n\t\
\n\t\
the default value for a parameter which is a list of names/values can\n\t\
be set by the command:\n\t\
\n\t\
	set ... <parameter_name> default"
},
#endif /* NA */
{"vci",			PORT_CATEGORY,     PORT_LOGIN
#ifdef NA
, "Show the vci subset of port parameters"
#endif /* NA */
},
{"vcli",		BOX_CATEGORY,	BOX_VCLI
#ifdef NA
, "Show the vcli subset of Annex parameters"
#endif /* NA */
},
{"vcli_groups",		BOX_PARAM,	VCLI_GROUPS
#ifdef NA
, "This Annex parameter will specify which remote group codes\n\t\
are accessible to virtual cli users. All virtual cli users have\n\t\
the same group code. Syntax:\n\t\
set annex vcli_groups <group range> enable | disable\n\t\
where <group range> is the set of groups ([similar to port set]\n\t\
between 0, and 255 inclusive) to affect (i.e. 1,2,3; 2; 5-10 are\n\t\
all valid group ranges).  A shortcut method can be used to enable or \n\t\
disable all group values.  To enable all groups, use:\n\t\
set annex vcli_groups all \n\t\
To disable all groups, use:\n\t\
set annex vcli_groups none"
#endif /* NA */
},
{"vcli_password",	BOX_PARAM,	VCLI_PASSWORD
#ifdef NA
, "VCLI password: a string, maximum 15 characters"
#endif /* NA */
},
{"vcli_security",	BOX_PARAM,	VCLI_SEC_ENA
#ifdef NA
, "ACP authorization required to use VCLI: Y or y to enable; N or n to\n\t\
disable"
#endif /* NA */
},
#ifdef NA
{"virtual",		HELP_ENTRY,	0
, "broadcast = virtual  broadcast to all virtual CLI ports\n\t\
reset virtual        reset all virtual CLI ports"
},
{"write",		A_COMMAND,	WRITE_CMD
, "write <annex_identifier> <filename>"
},
#endif /* NA */
{"zone",		BOX_PARAM,	ZONE
#ifdef NA
, "the hint for the AppleTalk zone to be used at startup\n\t\
a string up to 32 bytes"
#endif /* NA */
},
{(char *)NULL,		0,		0
#ifdef NA
, "Beyond Table"
#endif
}
};
