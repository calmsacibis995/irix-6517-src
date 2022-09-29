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
 * Module Function:
 * 	%$(Description)$%
 *
 * Original Author: %$(author)$%	Created on: %$(created-on)$%
 *
 * Revision Control Information:
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/na/RCS/do.c,v 1.3 1996/10/04 12:10:31 cwilson Exp $
 *
 * This file created by RCS from:
 *
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/na/RCS/do.c,v $
 *
 * Revision History:
 *
 * $Log: do.c,v $
 * Revision 1.3  1996/10/04 12:10:31  cwilson
 * latest rev
 *
 * Revision 2.95  1994/09/21  17:01:59  sasson
 * SPR 3529: fixed alignment problem in all the routines
 * that use union INTERN.
 *
 * Revision 2.94  1994/05/25  10:29:54  russ
 * Bigbird na/adm merge.
 *
 * Revision 2.93  1994/05/19  11:03:14  russ
 * removed references to duplicate/incorrect defines.
 *
 * Revision 2.92  1994/04/10  07:27:14  raison
 * do not include VOID_CAT parameters in write command
 *
 * Revision 2.91  1994/04/07  09:51:36  russ
 * allow write to handle all parameters. write of keys and passwords are
 * written as comment lines.
 *
 * Revision 2.90  1994/02/24  11:10:40  sasson
 * Changed write_annex_script() not to write the option_key to a file.
 *
 * Revision 2.89  1994/02/01  16:35:03  russ
 * Added another argument to get_internal_vers...  This is a True or
 * False to ask the user for prompting.  FALSE from the boot code.
 *
 * Revision 2.88  1993/12/30  14:04:04  carlson
 * SPR 2084 -- made compatible with 64 bit machines.
 *
 * Revision 2.87  1993/12/16  00:32:42  russ
 * don't imply to the user an immediate boot if delay booting.
 *
 * Revision 2.86  1993/09/24  10:15:29  bullock
 * spr.2047 - make error messages more sensible when trying to reset
 * non-existent printer or interfaces via reset [interface|printer] all
 *
 * Revision 2.85  93/09/13  13:31:04  wang
 * Fixed NA write command does not copy per annex arap and rip params problem.
 * Reviewed by Migual Sasson.
 * 
 * Revision 2.84  93/07/30  10:38:31  carlson
 * SPR 1923 -- rewrote pager to use file descriptor instead of file pointer.
 * 
 * Revision 2.83  1993/07/27  20:26:59  wang
 * na/admin interface redesign changes.
 *
 * Revision 2.82  1993/06/28  16:12:12  wang
 * Fixed spr1834, don't copy annex option_key param in "copy annex" command.
 *
 * Revision 2.81  93/06/10  11:44:57  wang
 * Added support for "copy interface" command and fix Spr1715.
 * Reviewed by Thuan Tran.
 * 
 * Revision 2.80  93/05/20  18:00:15  carlson
 * Rewrote split_string to make it more generally useful and remove
 * global variables.  (Part of SPR 1167)
 * Reviewed by David Wang.
 * 
 * Revision 2.79  93/05/20  16:24:07  wang
 * Fixed interface number out of sync problem. Reviewed by Jim Carlson.
 * 
 * Revision 2.78  93/05/12  16:06:49  carlson
 * R7.1 -- Motd not supported on ELS -- check for it.
 * 
 * Revision 2.77  93/02/24  13:24:55  carlson
 * Removed pager conditionals -- now taken care of in inc/na.h.
 * 
 * Revision 2.76  93/02/22  17:48:38  wang
 * Added reset interface command for NA.
 * 
 * Revision 2.75  93/02/20  13:42:03  wang
 * Added reading and writing file support for interface parameters.
 * 
 * Revision 2.74  93/01/22  18:18:25  wang
 * New parameters support for Rel8.0
 * 
 * Revision 2.73  92/08/24  12:58:11  bullock
 * spr1055 -  on sun machines comparing an unsigned short to -1 always
 * evaluates to true, which was causing core dump when doing na port copy,
 * the fix is just to cast the -1 to an unsigned short  
 * 
 * Revision 2.72  92/08/14  17:27:20  sasson
 * clean up after code review.
 * 
 * Revision 2.71  92/08/14  15:26:31  reeve
 * Call get_internal_vers with flag for RDRP.
 * 
 * Revision 2.70  92/08/12  13:41:01  sasson
 * changed copy_port(), copy_annex(), and copy_printer() to read in all
 * the parameters into the host's memory and then write them all out to
 * the target annex. This way rpc() opens only one connection to each
 * annex because the requests will no longer come with alternating addresses.
 * Currently rpc() is opening one connection per request, and on the
 * annex side na_listener() is exceeding any reasonable limit for
 * the number of processes.
 * 
 * Revision 2.69  92/08/04  14:15:43  sasson
 * spr 969: fixed the "write": don't write the LAT port pameters to a
 * file if LAT is turned off.
 * 
 * Revision 2.68  92/06/15  14:58:56  raison
 * separate attn_char_string so that Annex II's know it as attn_char and
 * Annex 3's and Micro's know it as attn_string.
 * 
 * Revision 2.67  92/05/27  17:02:42  wang
 * Fixed spr825, the NA write and read command problem for authorized_group port
 * parameter.
 * 
 * Revision 2.66  92/05/12  14:41:46  raison
 * do not copy or write PPP_password_remote
 * 
 * Revision 2.65  92/05/09  17:16:39  raison
 * make all lat groups the same conversion type
 * 
 * Revision 2.64  92/03/03  15:14:28  reeve
 * All calls to Ap_support() now also call Ap_support_check()
 * 
 * Revision 2.63  92/02/19  15:14:01  reeve
 * jFor call to get_ln_param(), use CARDINAL_P type when getting ATTN_CHAR
 * param on a rev >= 6.2.
 * 
 * Revision 2.62  92/02/13  11:06:18  raison
 * added support for requesting self boot option
 * 
 * Revision 2.61  92/02/12  17:13:04  reeve
 * Changed call of Sp_support_check to use Pannex_id instead of local
 * var, id.
 * 
 * Revision 2.60  92/02/03  14:03:08  raison
 * fixed printer_set bug, sorry
 * 
 * Revision 2.59  92/01/31  22:49:38  raison
 * change printer to have printer_set and added printer_speed
 * 
 * Revision 2.58  92/01/31  12:20:51  reeve
 * Changed references of Sp_support to also check Sp_support_check.
 * 
 * Revision 2.57  91/12/13  15:14:37  raison
 * changed attn_char checks for write script and copy port.  CAN NOT COPY
 * BETWEEN HETEROGENEOUS attn_char types.
 * 
 * Revision 2.56  91/11/25  09:20:06  carlson
 * Minor oops -- fixed wrongthinking when I wrote the SPR 369 stuff.
 * 
 * Revision 2.55  91/11/05  14:41:05  carlson
 * SPR 369 -- if change_dir is set, then don't allow relative names on write.
 * 
 * Revision 2.54  91/10/28  09:49:43  carlson
 * For SPR 361 -- forward/backward attn_char compatibility.
 * 
 * Revision 2.53  91/10/15  13:51:41  russ
 * Remember to close a file after opening and reading.
 * 
 * Revision 2.52  91/10/14  17:05:45  russ
 * Added pager.
 * 
 * Revision 2.51  91/07/29  12:55:37  raison
 * fix ICL SVR4 Sparc-based DRS6000 compiler error
 * 
 * Revision 2.50  91/06/13  12:13:26  raison
 * fixed bug for string parms when annex-ii
 * 
 * Revision 2.49  91/04/10  16:22:47  raison
 * fixed annex copy for the annex group_value.
 * 
 * Revision 2.48  91/04/08  23:32:32  emond
 * Fix some compile errors on SysV and accommodate generic API I/F
 * 
 * Revision 2.47  91/03/21  17:17:06  dajer
 * Changed copy_port() so that it doesn't change the copied to port's password.
 * 
 * Revision 2.46  91/01/24  13:27:14  raison
 * fixed na write command for group_value.
 * 
 * Revision 2.45  91/01/23  21:51:27  raison
 * added lat_value na parameter and GROUP_CODE parameter type.
 * 
 * Revision 2.44  91/01/15  13:51:15  raison
 * added check to exclude writing LAT parms (via write command) for non-lat
 * annex.
 * 
 * Revision 2.43  90/12/14  14:46:58  raison
 * add in LAT category.
 * 
 * Revision 2.42  90/12/04  13:07:41  sasson
 * The image_name was increased to 100 characters on annex3.
 * 
 * Revision 2.41  90/08/16  11:24:55  grant
 * 64 port support
 * 
 * Revision 2.40  90/03/27  09:08:17  russ
 * The hardware revision test for writing printer parameters was using the
 * Ap_support macro instead of Cp_support. It has been fixed.
 * 
 * Revision 2.39  90/03/06  22:43:36  loverso
 * in do_boot(): check to make sure getlogin() didn't return a NULL and that
 * gethostname returns a NUL terminated string.
 * 
 * Revision 2.38  90/01/24  14:38:58  russ
 * Added compile time switches to turn off X25 trunk functionality.
 * 
 * Revision 2.37  90/01/23  16:13:57  russ
 * Parse all internal parameter strings for separators.
 * 
 * Revision 2.36  89/11/20  16:55:47  grant
 * Fixed reset annex {nameserver,macros} from looping when talking to 
 * a pre 5.0 annex.
 * 
 * Revision 2.35  89/11/20  16:15:32  grant
 * Made reset printer message more generic.
 * 
 * Revision 2.34  89/11/01  13:09:23  russ
 * reset macro code.
 * 
 * Revision 2.33  89/09/11  11:21:25  grant
 * Fixed write command to show short_break, newline_terminal and slip_allow_dumo
 * 
 * Revision 2.32  89/08/21  15:05:37  grant
 * New reset option (reset name server)
 * 
 * Revision 2.31  89/06/15  16:28:22  townsend
 * Added support for X.25 (trunks)
 * 
 * Revision 2.30  89/05/18  09:37:29  grant
 * Added shutdown features to do_boot().
 * 
 * Revision 2.29  89/05/11  14:13:57  maron
 * Modified this module to support the new security parameters, vcli_password,
 * vcli_sec_ena, and port_password.
 * 
 * Revision 2.28  89/04/05  12:41:57  loverso
 * Changed copyright notice
 * 
 * Revision 2.27  89/01/25  14:48:17  harris
 * Fixed "reset annex" bug - did not work correctly with annex lists.
 * 
 * Revision 2.26  88/07/08  13:59:27  harris
 * Use netadm_error() to report errors and punt().  Remove local error table.
 * 
 * Revision 2.25  88/06/07  15:19:31  harris
 * Add DFE_ACP_KEY to the list of annex parameters not to "write".
 * 
 * Revision 2.24  88/06/02  14:18:34  harris
 * Fix broadcast and reset commands to not reset R3.0 virtual ports.
 * 
 * Revision 2.23  88/06/01  19:06:43  harris
 * Extend reset command - reset BOX subsystem_list.
 * 
 * Revision 2.22  88/05/25  09:48:07  harris
 * Fix do_copy_port bug - couldn't copy from port of 16 port Annex to a port
 * greater than 16 on an Annex II.
 * 
 * Revision 2.21  88/05/24  18:37:55  parker
 * Changes for new install-annex script
 * 
 * Revision 2.20  88/05/04  23:01:15  harris
 * Lint fix - unreferenced variables due to previous bug fix.
 * 
 * Revision 2.19  88/04/20  14:49:49  harris
 * Changed boot code to not ask for image name from EEPROM if none was given
 * with the boot command.  The Annex automatically selects EEPROM value.
 * Code was buggy anyway - Annexes with no EEPROM would boot the file name
 * returned by the previous Annex in the Annex list that had an EEPROM value.
 * 
 * Revision 2.18  88/04/15  12:43:52  mattes
 * SL/IP integration
 * 
 * Revision 2.17  88/03/16  12:52:41  mattes
 * Don't reset or broadcast to vcli ports on annexes
 * what ain't got none
 * 
 * Revision 2.16  88/03/15  14:36:06  mattes
 * Understand "virtual", "serial", and different "all"
 * 
 * Revision 2.15  87/12/29  11:44:29  mattes
 * Added 4.0 SL/IP parameters
 * 
 * Revision 2.14  87/09/29  13:24:08  mattes
 * was Message garbled port reset
 * 
 * Revision 2.13  87/09/28  11:49:35  mattes
 * Generalized box name
 * 
 * Revision 2.12  87/08/20  10:42:36  mattes
 * Added 32 port support
 * 
 * Revision 2.11  87/06/10  18:11:25  parker
 * Added support for Xenix/Excelan PC.
 * 
 * Revision 2.10  86/12/08  12:16:11  parker
 * R2.1 NA parameter changes
 * 
 * Revision 2.9  86/11/14  17:04:42  harris
 * Include files for XENIX and UMAX V.
 * 
 * Revision 2.8  86/07/14  14:54:07  harris
 * Remove references to old conversion routines.  Change write command to
 * use decode routine instead.
 * 
 * Revision 2.7  86/07/03  14:41:46  harris
 * Reorganized to use tables describing parameters and commands.
 * 
 * Revision 2.6  86/06/16  09:36:50  parker
 * added do_leap and allow_broadcast parmamters
 * 
 * Revision 2.5  86/05/07  10:58:37  goodmon
 * Added broadcast command.
 * 
 * Revision 2.4  86/04/18  16:50:38  goodmon
 * Simplified grammar by removing meaningless optional symbols.
 * 
 * Revision 2.3  86/03/20  18:31:42  goodmon
 * Fixed bug in read command -- forgot to check value returned by fopen().
 * 
 * Revision 2.2  86/02/27  19:03:44  goodmon
 * Standardized string lengths.
 * 
 * Revision 2.1  86/02/26  11:50:07  goodmon
 * Changed configuration files to script files.
 * 
 * Revision 1.4  86/01/02  14:20:33  goodmon
 * Fixed the error message given when the write of a configuration file fails.
 * 
 * Revision 1.3  85/12/17  17:12:31  goodmon
 * Changes suggested by the market analyst types.
 * 
 * Revision 1.2  85/12/05  18:21:58  goodmon
 * Added ^c handling and fixed port_multiplexing bug.
 * 
 * Revision 1.1  85/11/01  17:40:28  palmer
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *****************************************************************************
 */

#define RCSDATE $Date: 1996/10/04 12:10:31 $
#define RCSREV	$Revision: 1.3 $
#define RCSID   "$Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/na/RCS/do.c,v 1.3 1996/10/04 12:10:31 cwilson Exp $"

#ifndef lint
static char rcsid[] = RCSID;
#endif

/*
 *	Include Files
 */
#include "../inc/config.h"

#include "port/port.h"
#include <sys/types.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <strings.h>

#include <setjmp.h>
#include <stdio.h>
#include "../netadm/netadm_err.h"
#include "../inc/erpc/netadmp.h"
#include "../inc/na/interface.h"
#include "na.h"

/*
 *	External Definitions
 */

extern UINT32		 masks[];	    /* initialized in parse.c */
extern char		*reset_params[];
extern char		*annex_params[];
extern char		*port_params[];
extern char		*sync_params[];
extern char		*interface_params[];

extern char		*printer_params[];
extern parameter_table	annexp_table[];
extern parameter_table	portp_table[];
extern parameter_table	syncp_table[];
extern parameter_table	interfacep_table[];

extern parameter_table	printp_table[];

extern char *inet_ntoa();
extern void decode();
extern char *getlogin();
extern int  get_internal_vers();

/*
 *	Defines and Macros
 */

#define OWNER_RW  0600

/*
 *	Structure Definitions
 */


/*
 *	Forward Routine Definitions
 */

char		*split_string();

/*
 *	Global Data Declarations
 */


/*
 *	Static Declarations
 */

char	defalt[] = {0, 0, 0, 0};

static	unsigned char	disable_groups[LAT_GROUP_SZ + 1] = {
				0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
				0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
				0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
				0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
				0x00 };

/*
 *****************************************************************************
 *
 * Function Name:
 *	do_boot()
 *
 * Functional Description:
 *	Attempt to boot all the annexez in the Annex list.  Annexes with
 *      Annexes are dropped from the list if the uses any options other
 *      then a file name or dump.
 *
 * Parameters:
 * 	switches - commands switches used.
 *      filename - image name to be booted.
 *      warning  - warning message.
 * 	Pannex_list - list of annexes to be booted.
 * 	boot_time - time offset in seconds to boot.
 *
 *****************************************************************************
 */


do_boot(filename, Pannex_list, boot_time, switches, warning)

	short int  switches;
	char       filename[],
		   warning[];
	ANNEX_LIST *Pannex_list;
	time_t	   boot_time;

{
	int 
		boot_count = 0,
		maj_vers,
		hw,
	    	error_code;
	char 
		*username,
	         hostname[HOSTNAME_LENGTH+1];

	if ((username = getlogin()) == NULL)
		username = "unknown";
		
	(void)gethostname(hostname,HOSTNAME_LENGTH);
	hostname[HOSTNAME_LENGTH] = '\0';
	
	while (Pannex_list) {
	    get_internal_vers(Pannex_list->annex_id.sw_id, &maj_vers,&hw,
			     &Pannex_list->annex_id.flag, FALSE);	

	    if (maj_vers < VERS_5 && 
	        ((switches & SWDELAY) || (switches & SWDIAG) ||
		 (switches & SWFLASH) || (switches & SWABORT) ||
		 (warning[0] != '\0'))) {

	        printf("Shutdown not supported by pre-5.0 software\n");
		printf("Dropping %s from the list of %s\n",
				Pannex_list->name, BOXES);
	    } else {
		if ((switches & SWFLASH)
			 && (Pannex_list->annex_id.self_boot == 0)) {
		    printf("%s does not have self booting capability\n",
				     Pannex_list->name);
		    printf("Dropping %s from the list of %s\n",
				Pannex_list->name, BOXES);
		} else {

		    printf("%s %s %s\n",
			   switches&SWABORT ? "aborting boot" :
			   switches&SWDELAY ? "delay booting" : "booting", 
			       BOX, Pannex_list->name);

		    error_code = boot(&Pannex_list->annex_id.addr,
			filename, warning, switches, boot_time, username,
			hostname, (switches & SWDUMP), (maj_vers >= VERS_5));

	    	    if (error_code != NAE_SUCC)
		        netadm_error(error_code);

	    	    boot_count++;
		}
	    }
	    Pannex_list = Pannex_list->next;
	}
	if (!(switches & SWABORT) && (boot_count) && !(switches & SWDELAY)) {
		printf("The %s %s performing self-diagnostics, and will not respond\n",
	 	boot_count > 1 ? BOXES : BOX,
	 	boot_count > 1 ? "are" : "is");
		printf("to administration operations for a short period.\n");
	}
}	/* do_boot() */


do_broadcast(Pport_set, Ptext)

	PORT_SET       *Pport_set;
	char	       *Ptext;

{
	ANNEX_LIST *Ptemp_annex_list;

	/* Send the message to each port in the given port set. */

	while (Pport_set)
	    {
	    /* If an annex id was specified, use it; otherwise, use the
	       default annex list. */
	    if (Pport_set->annex_id.addr.sin_addr.s_addr)
		broadcast_sub(&Pport_set->annex_id, Pport_set->name,
		 &Pport_set->ports, Ptext);
	    else
		if (Pdef_annex_list)
		    for (Ptemp_annex_list = Pdef_annex_list; Ptemp_annex_list;
		     Ptemp_annex_list = Ptemp_annex_list->next)
			broadcast_sub(&Ptemp_annex_list->annex_id,
			 Ptemp_annex_list->name, &Pport_set->ports, Ptext);
		else
		    punt(NO_BOXES, (char *)NULL);

	    Pport_set = Pport_set->next;
	    }

}	/* do_broadcast() */



broadcast_sub(Pannex_id, name, Pports, Ptext)

	ANNEX_ID	   *Pannex_id;
	char                name[];
	PORT_GROUP	   *Pports;
	char               *Ptext;

{
	int                error_code,
			   loop, loop_limit;

	/* Send the message to each port whose bit is set in the port mask. */
	if(Pports->pg_bits && (PG_ALL|PG_SERIAL))
	    loop_limit = (int)Pannex_id->port_count;
        else 
	    loop_limit = ALL_PORTS;

	for (loop = 1; loop <= loop_limit; loop++)
	    if (PORTBITSET(Pports->serial_ports,loop))
	    {
		if (loop > (int)Pannex_id->port_count)
		{
		printf("\n%s %s does not have a port %d\n",
		       BOX, name, loop);
		    continue;
		}

	        if ((error_code = broadcast(&Pannex_id->addr, 
			(u_short)SERIAL_DEV, (u_short)loop,Ptext)) != NAE_SUCC)
		    if (error_code == NAE_PROC)
			{
			printf("%s '%s' does not support broadcast\n",
			 BOX, name);
			return;
			}
		    else
		        netadm_error(error_code);
	    }

	if (Pports->pg_bits & (PG_VIRTUAL | PG_ALL))
	    {
	     if ((Pannex_id->version < VERS_4) && (Pannex_id->hw_id != X25))
		{
		if (Pports->pg_bits & PG_VIRTUAL)
		    printf("%s '%s' does not support virtual ports\n",
			   BOX, name);
		return;
		}

	    if ((error_code = broadcast(&Pannex_id->addr, 
		    (u_short)PSEUDO_DEV, (u_short)0, Ptext)) != NAE_SUCC)
		if (error_code == NAE_PROC)
		    {
		    printf("%s '%s' does not support broadcast\n",
		     BOX, name);
		    return;
		    }
		else
		    netadm_error(error_code);
	    }

}	/* broadcast_sub() */



do_copy(source_port_number, Psource_annex_id, Pport_set)

	unsigned short     source_port_number;
	ANNEX_ID	   *Psource_annex_id;
	PORT_SET           *Pport_set;

{
	ANNEX_LIST      *Ptemp_annex_list;


	/* Copy the parameters to each port in the port set. */


	while (Pport_set)
	    {
	    /* If an annex id was specified, use it; otherwise, use the
	       default annex list. */

	    if (Pport_set->annex_id.addr.sin_addr.s_addr)

		    do_copy_port(Psource_annex_id, source_port_number,
			         &Pport_set->annex_id, 
				 Pport_set->ports.serial_ports,
		 	         Pport_set->name);

	    else if (Pdef_annex_list)

		for(Ptemp_annex_list = Pdef_annex_list; Ptemp_annex_list;
		    Ptemp_annex_list = Ptemp_annex_list->next)

		    do_copy_port(Psource_annex_id, source_port_number,
			         &Ptemp_annex_list->annex_id,
				 Pport_set->ports.serial_ports,
				 Ptemp_annex_list->name);
	    else
		punt(NO_BOXES, (char *)NULL);

	    Pport_set = Pport_set->next;
	    }

}	/* do_copy() */

do_copy_sync(source_sync_number, Psource_annex_id, Psync_set)

	unsigned short     source_sync_number;
	ANNEX_ID	   *Psource_annex_id;
	SYNC_SET           *Psync_set;

{
	ANNEX_LIST      *Ptemp_annex_list;


	/* Copy the parameters to each sync port in the sync port set. */


	while (Psync_set)
	    {
	    /* If an annex id was specified, use it; otherwise, use the
	       default annex list. */

	    if (Psync_set->annex_id.addr.sin_addr.s_addr)

		    do_copy_syncport(Psource_annex_id, source_sync_number,
			         &Psync_set->annex_id, 
				 Psync_set->syncs.sync_ports,
		 	         Psync_set->name);

	    else if (Pdef_annex_list)

		for(Ptemp_annex_list = Pdef_annex_list; Ptemp_annex_list;
		    Ptemp_annex_list = Ptemp_annex_list->next)

		    do_copy_syncport(Psource_annex_id, source_sync_number,
			         &Ptemp_annex_list->annex_id,
				 Psync_set->syncs.sync_ports,
				 Ptemp_annex_list->name);
	    else
		punt(NO_BOXES, (char *)NULL);

	    Psync_set = Psync_set->next;
	    }

}	/* do_copy_sync() */

printer_copy(source_printer_number, Psource_annex_id, Pprinter_set)

	unsigned short     source_printer_number;
	ANNEX_ID	   *Psource_annex_id;
	PRINTER_SET        *Pprinter_set;

{
	ANNEX_LIST      *Ptemp_annex_list;


	/* Copy the parameters to each printer in the printer set. */


	while (Pprinter_set)
	    {
	    /* If an annex id was specified, use it; otherwise, use the
	       default annex list. */

	    if (Pprinter_set->annex_id.addr.sin_addr.s_addr)

		    do_copy_print(Psource_annex_id, source_printer_number,
			         &Pprinter_set->annex_id, 
				 Pprinter_set->printers.ports,
		 	         Pprinter_set->name);

	    else if (Pdef_annex_list)

		for(Ptemp_annex_list = Pdef_annex_list; Ptemp_annex_list;
		    Ptemp_annex_list = Ptemp_annex_list->next)

		    do_copy_print(Psource_annex_id, source_printer_number,
			         &Ptemp_annex_list->annex_id,
				 Pprinter_set->printers.ports,
				 Ptemp_annex_list->name);
	    else
		punt(NO_BOXES, (char *)NULL);

	    Pprinter_set = Pprinter_set->next;
	    }

}	/* printer_copy() */



do_copy_print(Pannex_from, printer, Pannex_to, printer_mask, name)

ANNEX_ID	  	*Pannex_from;
unsigned short		printer;
ANNEX_ID	  	*Pannex_to;
unsigned char		*printer_mask;
char			name[];
{
    int	loop;

    /* Copy parameters to each printer whose bit is set in the printer mask. */

    if (printer > (u_short)Pannex_from->printer_count)
	printf("\nsource %s does not have a printer %d\n", BOX, printer);
    else 
	for (loop = 1; loop <= ALL_PRINTERS; loop++)
	    if (PRINTERBITSET(printer_mask,loop))
	    {
		if (loop > (int)Pannex_to->printer_count)
		{
		    continue;
		}
		printf(
		"\tcopying eeprom serial printer parameters to %s %s printer %d\n",
		BOX, name, loop);

		copy_printer(Pannex_from, printer, Pannex_to, (u_short)loop, name);
	    }

}	/* do_copy_print() */


copy_printer(Pannex_from, printer_from, Pannex_to, printer_to, name)

ANNEX_ID	  	*Pannex_from;
unsigned short		printer_from;
ANNEX_ID	  	*Pannex_to;
unsigned short		printer_to;
char			name[];
{
	int		parm;
	u_short cat,id,type;

	struct	p_param	{
		u_short	cat;
		u_short	id;
		u_short	type;
		char	buf[MAXVALUE+2];
		};
	struct	p_param	*pp_param;
	int	mem_pool;
	int	i, n_param = 0;

	/* allocate memory to read the port parameters in */
	for(parm = 0; Cp_index(parm) != -1; parm++) n_param++;
	mem_pool = malloc((n_param + 1) * sizeof(struct p_param));
	if (!mem_pool) {
		printf("\tcopy printer: could not allocate memory.\n");
		return;
		}
	bzero(mem_pool, (n_param + 1) * sizeof(struct p_param));
	pp_param = (struct p_param *)mem_pool;

	/* read printer parameters */
	for(parm = 0; Cp_index(parm) != -1; parm++)
	    {
	    if(!Cp_support(Pannex_from,parm))
		continue;  /* skip parms not support on source annex */
	    cat = (u_short)Cp_category(parm);
	    id = (u_short)Cp_catid(parm);

	    if (cat == LP_CAT) {

		type = (u_short)Cp_type(parm);

		if(get_ln_param(&Pannex_from->addr, (u_short)P_PRINT_DEV, 
			(u_short)printer_from, cat, id,type,pp_param[parm].buf))
		    {
		    printf("\tusing default for %s\n", printer_params[parm]);
		    (void)strncpy(pp_param[parm].buf, defalt, sizeof defalt);
		    }
		pp_param[parm].cat = cat;
		pp_param[parm].id = id;
		pp_param[parm].type = type;
		}
	    }   /* for(parm ... */
	    pp_param[parm].cat = (u_short)-1;

	/* write printer parameters */
	for(parm = 0; pp_param[parm].cat != (u_short)-1; parm++)
	    {
		if ((cat = pp_param[parm].cat) == 0)
			continue;
		id = pp_param[parm].id;
		type = pp_param[parm].type;

		if(Cp_support(Pannex_to,parm))
		    {
		    if(set_ln_param(&Pannex_to->addr, (u_short)P_PRINT_DEV,
			(u_short)printer_to, cat, id, type, pp_param[parm].buf))

			    printf("\tcould not set %s\n", printer_params[parm]);
		    }
		else
		    printf("\t%s  does not support %s\n",name,
						printer_params[parm]);
	    }   /* for(parm ... */

	/* return memory */
	free(mem_pool);

}	/* copy_printer() */



interface_copy(source_interface_number, Psource_annex_id, Pinterface_set)

	unsigned short     source_interface_number;
	ANNEX_ID	   *Psource_annex_id;
	INTERFACE_SET      *Pinterface_set;

{
	ANNEX_LIST      *Ptemp_annex_list;


	/* Copy the parameters to each interface in the interface set. */


	while (Pinterface_set)
	    {
	    /* If an annex id was specified, use it; otherwise, use the
	       default annex list. */

	    if (Pinterface_set->annex_id.addr.sin_addr.s_addr)

		    do_copy_interface(Psource_annex_id, source_interface_number,
			         &Pinterface_set->annex_id, 
				 Pinterface_set->interfaces.interface_ports,
		 	         Pinterface_set->name);

	    else if (Pdef_annex_list)

		for(Ptemp_annex_list = Pdef_annex_list; Ptemp_annex_list;
		    Ptemp_annex_list = Ptemp_annex_list->next)

		    do_copy_interface(Psource_annex_id, source_interface_number,
			         &Ptemp_annex_list->annex_id,
				 Pinterface_set->interfaces.interface_ports,
				 Ptemp_annex_list->name);
	    else
		punt(NO_BOXES, (char *)NULL);

	    Pinterface_set = Pinterface_set->next;
	    }

}	/* interface_copy() */



do_copy_interface(Pannex_from, interface, Pannex_to, interface_mask, name)

ANNEX_ID	  	*Pannex_from;
unsigned short		interface;
ANNEX_ID	  	*Pannex_to;
unsigned char		*interface_mask;
char			name[];
{
 	int		loop, asy_end, syn_end;


    /* en0 plus asy */
    	asy_end = (int)Pannex_to->port_count + 1;
	syn_end = ALL_PORTS + (int)Pannex_to->sync_count + 1;

    /*
     * Copy parameters to each interface whose bit is set in the
     * interface mask.
     */

	for (loop = 1; loop <= ALL_INTERFACES; loop++)
	  if ((loop <= asy_end) || ((loop > ALL_PORTS) && (loop <= syn_end))) {
	    if (INTERFACEBITSET(interface_mask,loop))
	    {
		/* convert the interface logical index into human-eye form */
		if (loop == M_ETHERNET)
			printf("\tcopying eeprom interface parameters to %s %s interface en%d:\n", BOX, name, loop-M_ETHERNET);

		else 
		if( loop <= ALL_PORTS )
		        printf("\tcopying eeprom interface parameters to %s %s interface asy%d:\n", BOX, name, loop-M_ETHERNET);

		else
			printf("\tcopying eeprom interface parameters to %s %s interface syn%d:\n", BOX, name, loop-M_ETHERNET-ALL_PORTS);
		copy_interface(Pannex_from, interface, Pannex_to, (u_short)loop, name);
	    }
	}
}	/* do_copy_interface() */


copy_interface(Pannex_from, interface_from, Pannex_to, interface_to, name)

ANNEX_ID	  	*Pannex_from;
unsigned short		interface_from;
ANNEX_ID	  	*Pannex_to;
unsigned short		interface_to;
char			name[];
{
	int		parm;
	u_short cat,id,type;

	struct	p_param	{
		u_short	cat;
		u_short	id;
		u_short	type;
		char	buf[MAXVALUE+2];
		};
	struct	p_param	*pp_param;
	int	mem_pool;
	int	i, n_param = 0;

	/* allocate memory to read the interface parameters in */
	for(parm = 0; Ip_index(parm) != -1; parm++) n_param++;
	mem_pool = malloc((n_param + 1) * sizeof(struct p_param));
	if (!mem_pool) {
		printf("\tcopy interface: could not allocate memory.\n");
		return;
		}
	bzero(mem_pool, (n_param + 1) * sizeof(struct p_param));
	pp_param = (struct p_param *)mem_pool;

	/* read interface parameters */
	for(parm = 0; Ip_index(parm) != -1; parm++)
	    {
	    if(!Ip_support(Pannex_from,parm))
		continue;  /* skip parms not support on source annex */
	    cat = (u_short)Ip_category(parm);
	    id = (u_short)Ip_catid(parm);

	    if (cat == IF_CAT) {

		type = (u_short)Ip_type(parm);

		if(get_if_param(&Pannex_from->addr, (u_short)INTERFACE_DEV, 
			(u_short)interface_from, cat, id,type,pp_param[parm].buf))
		    {
		    printf("\tusing default for %s\n", interface_params[parm]);
		    (void)strncpy(pp_param[parm].buf, defalt, sizeof defalt);
		    }
		pp_param[parm].cat = cat;
		pp_param[parm].id = id;
		pp_param[parm].type = type;
		}
	    }   /* for(parm ... */
	    pp_param[parm].cat = (u_short)-1;

	/* write interface parameters */
	for(parm = 0; pp_param[parm].cat != (u_short)-1; parm++)
	    {
		if ((cat = pp_param[parm].cat) == 0)
			continue;
		id = pp_param[parm].id;
		type = pp_param[parm].type;

		if(Ip_support(Pannex_to,parm))
		    {
		    if(set_if_param(&Pannex_to->addr, (u_short)INTERFACE_DEV,
			(u_short)interface_to, cat, id, type, pp_param[parm].buf))

			    printf("\tcould not set %s\n", interface_params[parm]);
		    }
		else
		    printf("\t%s  does not support %s\n",name,
						interface_params[parm]);
	    }   /* for(parm ... */

	/* return memory */
	free(mem_pool);

}	/* copy_interface() */



do_copy_port(Pannex_from, port, Pannex_to, port_mask, name)

ANNEX_ID	  	*Pannex_from;
unsigned short		port;
ANNEX_ID	  	*Pannex_to;
unsigned char		*port_mask;
char			name[];
{
    int	loop;

    /* Copy parameters to each port whose bit is set in the port mask. */

    if (port > (u_short)Pannex_from->port_count)
	printf("\nsource %s does not have a port %d\n", BOX, port);
    else 
	for (loop = 1; loop <= ALL_PORTS; loop++)
	    if (PORTBITSET(port_mask,loop))
	    {
		if (loop > (int)Pannex_to->port_count)
		{
		    printf("\n%s %s does not have a port %d\n",
			       BOX, name, loop);
		    continue;
		}
		printf(
		"\tcopying eeprom serial port parameters to %s %s port %d\n",
		BOX, name, loop);

		copy_port(Pannex_from, port, Pannex_to, (u_short)loop, name);
	    }

}	/* do_copy_port() */



copy_port(Pannex_from, port_from, Pannex_to, port_to, name)

ANNEX_ID	  	*Pannex_from;
unsigned short		port_from;
ANNEX_ID	  	*Pannex_to;
unsigned short		port_to;
char			name[];
{
	int		parm;
	u_short cat,id,type;

	struct	p_param	{
		u_short	cat;
		u_short	id;
		u_short	type;
		char	buf[MAXVALUE+2];
		};
	struct	p_param	*pp_param;
	int	mem_pool;
	int	i, n_param = 0;

	/* allocate memory to read the port parameters in */
	for(parm = 0; Sp_index(parm) != -1; parm++) n_param++;
	mem_pool = malloc((n_param + 1) * sizeof(struct p_param));
	if (!mem_pool) {
		printf("\tcopy: could not allocate memory.\n");
		return;
		}
	bzero(mem_pool, (n_param + 1) * sizeof(struct p_param));
	pp_param = (struct p_param *)mem_pool;

	/* read in port parameters */
	for(parm = 0; Sp_index(parm) != -1; parm++)
	    {
	    if(!(Sp_support(Pannex_from,parm)))
		continue;  /* skip parms not supported on source annex */

	    cat = (u_short)Sp_category(parm);
	    id = (u_short)Sp_catid(parm);

	    if ((id == DEV2_PORT_PASSWD && cat == DEV2_CAT) ||
	     (id == PPP_PWORDRMT && cat == SLIP_CAT))
		continue;  			/* skip password */

	    if(cat == DEV_CAT ||
	       cat == EDIT_CAT ||
	       cat == INTF_CAT ||
               cat == DEV2_CAT  ||
	       cat == SLIP_CAT)

		{
		type = (u_short)Sp_type(parm);
#ifdef NA
                if (cat == DEV_CAT && id == DEV_ATTN) {
                    if ((Pannex_from->version < VERS_6_2)
			               || (Pannex_from->hw_id < ANX3)){
                        type = CARDINAL_P;
                    }
                }
#endif

		if(get_ln_param(&Pannex_from->addr, (u_short)SERIAL_DEV, 
			(u_short)port_from, cat, id, type, pp_param[parm].buf))
		    {
		    printf("\tusing default for %s\n", port_params[parm]);
		    (void)strncpy(pp_param[parm].buf, defalt, sizeof defalt);
		    }

		pp_param[parm].cat = cat;
		pp_param[parm].id = id;
		pp_param[parm].type = type;
		}
	    }
	    pp_param[parm].cat = (u_short)-1; /* end of table mark */

	/* write port parameters */
	for(parm = 0; pp_param[parm].cat != (u_short)-1; parm++)
	    {
		if ((cat = pp_param[parm].cat) == 0)
			continue;
		id = pp_param[parm].id;
		type = pp_param[parm].type;

		if(Sp_support(Pannex_to,parm))
		    {
	            if ( (id == LAT_AUTHORIZED_GROUPS) && (cat == DEV_CAT)) {

		      /* disable all group codes first, then copy them in */
	              if (set_ln_param(&Pannex_to->addr, (u_short)SERIAL_DEV,
			        (u_short)port_to,cat,id,type,
				(char *)disable_groups)) {
	 	          printf("\tcould not set param %s\n",
			        port_params[parm]);
			  continue;
		      }

		      /* now enable according to src annex */
		      pp_param[parm].buf[LAT_GROUP_SZ] = 1;
	            }
		    if(set_ln_param(&Pannex_to->addr, (u_short)SERIAL_DEV,
			(u_short)port_to, cat, id, type, pp_param[parm].buf))

			    printf("\tcould not set %s\n", port_params[parm]);
		    }
		else
		    printf("\t%s does not support %s\n",name,
						port_params[parm]);
	    }   /* for(parm ... */

	/* return memory */
	free(mem_pool);

}	/* copy_port() */

do_copy_syncport(Pannex_from, sync, Pannex_to, sync_mask, name)

ANNEX_ID	  	*Pannex_from;
unsigned short		sync;
ANNEX_ID	  	*Pannex_to;
unsigned char		*sync_mask;
char			name[];
{
    int	loop;

    /* Copy parameters to each sync port whose bit is set in the sync port mask. */

    if (sync > (u_short)Pannex_from->sync_count)
	printf("\nsource %s does not have a synchronous port %d\n", BOX, sync);
    else 
	for (loop = 1; loop <= ALL_SYNC_PORTS; loop++)
	    if (PORTBITSET(sync_mask,loop))
	    {
		if (loop > (int)Pannex_to->sync_count)
		{
		    printf("\n%s %s does not have a synchronous port %d\n",
			       BOX, name, loop);
		    continue;
		}
		printf(
		"\tcopying eeprom synchronous port parameters to %s %s port %d\n",
		BOX, name, loop);

		copy_sync(Pannex_from, sync, Pannex_to, (u_short)loop, name);
	    }

}	/* do_copy_syncport() */



copy_sync(Pannex_from, sync_from, Pannex_to, sync_to, name)

ANNEX_ID	  	*Pannex_from;
unsigned short		sync_from;
ANNEX_ID	  	*Pannex_to;
unsigned short		sync_to;
char			name[];
{
	int		parm;
	u_short cat,id,type;

	struct	p_param	{
		u_short	cat;
		u_short	id;
		u_short	type;
		char	buf[MAXVALUE+2];
		};
	struct	p_param	*pp_param;
	int	mem_pool;
	int	i, n_param = 0;

	/* allocate memory to read the synchronous port parameters in */
	for(parm = 0; Syncp_index(parm) != -1; parm++) n_param++;
	mem_pool = malloc((n_param + 1) * sizeof(struct p_param));
	if (!mem_pool) {
		printf("\tcopy: could not allocate memory.\n");
		return;
		}
	bzero(mem_pool, (n_param + 1) * sizeof(struct p_param));
	pp_param = (struct p_param *)mem_pool;

	/* read in port parameters */
	for(parm = 0; Syncp_index(parm) != -1; parm++)
	    {
	    if(!(Syncp_support(Pannex_from,parm)/* && 
	         Sp_support_check(Pannex_from,parm)*/))
		continue;  /* skip parms not supported on source annex */

	    cat = (u_short)Syncp_category(parm);
	    id = (u_short)Syncp_catid(parm);

	    if ((id == SYNC_PORT_PASSWD) || (id == SYNC_PPP_PASSWD_REMOTE)){
		continue;  			/* skip password */
	    }

	    if(cat == SYNC_CAT)

		{
		type = (u_short)Syncp_type(parm);

		if(get_sy_param(&Pannex_from->addr, (u_short)SYNC_DEV, 
			(u_short)sync_from, cat, id, type, pp_param[parm].buf))
		    {
		    printf("\tusing default for %s\n", sync_params[parm]);
		    (void)strncpy(pp_param[parm].buf, defalt, sizeof defalt);
		    }

		pp_param[parm].cat = cat;
		pp_param[parm].id = id;
		pp_param[parm].type = type;
		}
	    }
	    pp_param[parm].cat = (u_short)-1; /* end of table mark */

	/* write sync port parameters */
	for(parm = 0; pp_param[parm].cat != (u_short)-1; parm++)
	    {
		id = pp_param[parm].id;
		type = pp_param[parm].type;

	        if ( id == 0 ){ /* Should not allow this parameter to be copied */
		    continue;  		
	        }

		if(Syncp_support(Pannex_to,parm) /*&&
		   Syncp_support_check(Pannex_to,parm)*/)
		    {
		    if(set_sy_param(&Pannex_to->addr, (u_short)SYNC_DEV,
			(u_short)sync_to, cat, id, type, pp_param[parm].buf))
			   {
			    printf("\tcould not set %s\n", sync_params[parm]);
			   }
		    }
		else
		    {
		    printf("\t%s does not support %s\n",name,
						sync_params[parm]);
		    }
	    }   /* for(parm ... */

	/* return memory */
	free(mem_pool);

}	/* copy_sync() */


do_copy_annex(Pannex_id, Pannex_list)

ANNEX_ID 		*Pannex_id;
ANNEX_LIST		*Pannex_list;

{
	while(Pannex_list)
	   {
	     printf("\tcopying eeprom annex parameters to %s %s\n",
	    	    BOX, Pannex_list->name);
	     copy_annex(Pannex_id, &Pannex_list->annex_id, Pannex_list->name);
	     Pannex_list = Pannex_list->next;
	   }

} /* do_copy_annex */


copy_annex(Pannex_from, Pannex_to, name)

ANNEX_ID	  	*Pannex_from;
ANNEX_ID	  	*Pannex_to;
char			name[];
{
	int		parm;

	u_short cat,id,type;
	struct	p_param	{
		u_short	cat;
		u_short	id;
		u_short	type;
		char	buf[MAX_STRING_100 + 4];
		};
	struct	p_param	*pp_param;
	int	mem_pool;
	int	i, n_param = 0;

	/* allocate memory to read the port parameters in */
	for(parm = 0; Ap_index(parm) != -1; parm++) n_param++;
	mem_pool = malloc((n_param + 1) * sizeof(struct p_param));
	if (!mem_pool) {
		printf("\tcopy annex: could not allocate memory.\n");
		return;
		}
	bzero(mem_pool, (n_param + 1) * sizeof(struct p_param));
	pp_param = (struct p_param *)mem_pool;

	/* read in annex parameters */
	for (parm = 1; Ap_index(parm) != -1; parm++)	/* Skip zero (INET) */
	{

	  /* do not copy password, acp_key, option_key or lat_key */
	  if ( (Ap_category(parm) != DFE_CAT ||
	        (Ap_catid(parm) != DFE_PASSWORD &&
	         Ap_catid(parm) != DFE_ACP_KEY &&
	         Ap_catid(parm) != DFE_OPTION_KEY &&
		 Ap_catid(parm) != LAT_KEY_VALUE)) &&
		 (Ap_category(parm) == DLA_CAT || Ap_category(parm) == DFE_CAT
		        || Ap_category(parm) == ARAP_CAT
			|| Ap_category(parm) == RIP_CAT
			|| Ap_category(parm) == LAT_CAT) &&
		 Ap_support(Pannex_from,parm))
	    {

	    if(get_dla_param(&Pannex_from->addr, (u_short)Ap_category(parm),
		    (u_short)Ap_catid(parm), (u_short)Ap_type(parm),
			 pp_param[parm].buf))
	      {
	      printf("\tusing default for %s\n", annex_params[parm]);
	      (void)strncpy(pp_param[parm].buf, defalt, sizeof defalt);
	      }

	     pp_param[parm].cat = Ap_category(parm);
	     pp_param[parm].id = Ap_catid(parm);
	     pp_param[parm].type = Ap_type(parm);
	    }
	}
	pp_param[parm].cat = (u_short)-1; /* end of table mark */

	/* write annex parameters out */
	for(parm = 0; pp_param[parm].cat != (u_short)-1; parm++)
	{
	    if ((cat = pp_param[parm].cat) == 0)
		continue;
	    id = pp_param[parm].id;
	    type = pp_param[parm].type;

	    if(Ap_support(Pannex_to,parm))
	      {
	      if ((type == STRING_P_100)
	     		 && (Pannex_to->hw_id != ANX3)
			 && (Pannex_to->hw_id != ANX_MICRO))
		  {
		  /* use type STRING_P when writing to old annexes */
	          if(set_dla_param(&Pannex_to->addr, (u_short)cat,
                    (u_short)id, (u_short)STRING_P, pp_param[parm].buf))
	 	  printf("\tcould not set param %s\n", annex_params[parm]);
		  }
	       else
		  {
	          if ( ((id == LAT_GROUP_CODE) ||
			(id == LAT_VCLI_GROUPS)) &&
	     			 (cat == LAT_CAT)) {

		      /* disable all group codes first, then copy them in */
	              if (set_dla_param(&Pannex_to->addr,
		                (u_short)cat,
			        (u_short)id,(u_short)type,
				(char *)disable_groups)) {
	 	          printf("\tcould not set param %s\n",
			        annex_params[parm]);
			  continue;
		      }

		      /* now enable according to src annex */
		      pp_param[parm].buf[LAT_GROUP_SZ] = 1;
	          }
	          if(set_dla_param(&Pannex_to->addr, (u_short)cat,
                    (u_short)id, (u_short)type, pp_param[parm].buf))
	 	  printf("\tcould not set param %s\n", annex_params[parm]);
		  }
	      }
	    else
	      printf("%s does not support parameter: %s\n\n",name,
			    annex_params[parm]);

	}
	/* return memory */
	free(mem_pool);

}	/* copy_annex() */


do_read(filename)

	char filename[];

{
	FILE *save_cmd_file;
	int   save_script_input;

	save_cmd_file = cmd_file;
	save_script_input = script_input;

	if ((cmd_file = fopen(filename, "r")) == NULL)
	    {
	    cmd_file = save_cmd_file;
	    punt("couldn't open ", filename);
	    }

	script_input = TRUE;

	cmd_sub();
	fclose(cmd_file);
	script_input = save_script_input;
	cmd_file = save_cmd_file;

}	/* do_read() */



do_reset_printer(Pprinter_set)

	PRINTER_SET *Pprinter_set;

{
	ANNEX_LIST *Ptemp_annex_list;

	/* Reset each printer in the printer set. */

	while (Pprinter_set)
	    {
	    /* If an annex id was specified, use it; otherwise, use the
	       default annex list. */
	    if (Pprinter_set->annex_id.addr.sin_addr.s_addr)
		reset_printer_sub(&Pprinter_set->annex_id, Pprinter_set->name,
		 &Pprinter_set->printers);
	    else
		if (Pdef_annex_list)
		    for(Ptemp_annex_list = Pdef_annex_list; Ptemp_annex_list;
		     Ptemp_annex_list = Ptemp_annex_list->next)
			reset_printer_sub(&Ptemp_annex_list->annex_id,
			 Ptemp_annex_list->name, &Pprinter_set->printers);
		else
		    punt(NO_BOXES, (char *)NULL);

	    Pprinter_set = Pprinter_set->next;
	    }

}	/* do_reset_printer() */


do_reset_port(Pport_set)

	PORT_SET *Pport_set;

{
	ANNEX_LIST *Ptemp_annex_list;

	/* Reset each port in the port set. */

	while (Pport_set)
	    {
	    /* If an annex id was specified, use it; otherwise, use the
	       default annex list. */
	    if (Pport_set->annex_id.addr.sin_addr.s_addr)
		reset_sub(&Pport_set->annex_id, Pport_set->name,
		 &Pport_set->ports);
	    else
		if (Pdef_annex_list)
		    for(Ptemp_annex_list = Pdef_annex_list; Ptemp_annex_list;
		     Ptemp_annex_list = Ptemp_annex_list->next)
			reset_sub(&Ptemp_annex_list->annex_id,
			 Ptemp_annex_list->name, &Pport_set->ports);
		else
		    punt(NO_BOXES, (char *)NULL);

	    Pport_set = Pport_set->next;
	    }

}	/* do_reset_port() */



do_reset_interface(Pinterface_set)

	INTERFACE_SET *Pinterface_set;

{
	ANNEX_LIST *Ptemp_annex_list;

	/* Reset each interface in the interface set. */

	while (Pinterface_set)
	    {
	    /* If an annex id was specified, use it; otherwise, use the
	       default annex list. */
	    if (Pinterface_set->annex_id.addr.sin_addr.s_addr)
		reset_interface_sub(&Pinterface_set->annex_id, Pinterface_set->name, &Pinterface_set->interfaces);
	    else
		if (Pdef_annex_list)
		    for(Ptemp_annex_list = Pdef_annex_list; Ptemp_annex_list;
		     Ptemp_annex_list = Ptemp_annex_list->next)
			reset_interface_sub(&Ptemp_annex_list->annex_id,
			 Ptemp_annex_list->name, &Pinterface_set->interfaces);
		else
		    punt(NO_BOXES, (char *)NULL);

	    Pinterface_set = Pinterface_set->next;
	    }

}	/* do_reset_interface() */



do_reset_box(Pannex_list)
ANNEX_LIST *Pannex_list;
{
    int	param;
    ANNEX_LIST *Ptemp_annex_list;

	/* if no parameters, assume all subsystems reset */

    if (eos) {
	(void)strcpy(command_line, "all");
	Psymbol = command_line;
	eos = FALSE;
	(void)lex();
    }

	/* for each keyword in the reset annex list, for each annex */

    while (!eos) {
	param = match(symbol, reset_params, "reset parameter");
	(void)lex();
	Ptemp_annex_list = Pannex_list;

	for (;Ptemp_annex_list;Ptemp_annex_list=Ptemp_annex_list->next){
	    switch (param) {
	    case RESET_ANNEX_NAMESERVER:
		if (Ptemp_annex_list->annex_id.version < VERS_5) {
		    printf(
		"%s %s does not support resetting name servers\n",
			BOX, Ptemp_annex_list->name);
		    continue;
		}
		break;
	    case RESET_ANNEX_MACRO:
		if (Ptemp_annex_list->annex_id.version < VERS_5) {
		    printf("%s %s does not support macros\n",
			BOX, Ptemp_annex_list->name);
		    continue;
		}
		break;
	    case RESET_ANNEX_LAT:
		if (!Ptemp_annex_list->annex_id.lat) {
		    printf("%s %s does not support lat\n",
			BOX, Ptemp_annex_list->name);
		    continue;
		}
		break;
	    case RESET_ANNEX_MOTD:
		if (Ptemp_annex_list->annex_id.hw_id == ANX_MICRO_ELS) {
		    printf("%s %s does not support motd\n",
			BOX, Ptemp_annex_list->name);
		    continue;
		}
		break;
	    }
	    printf("resetting %s on %s %s\n",reset_params[param],
		BOX, Ptemp_annex_list->name);
	    reset_annex(&Ptemp_annex_list->annex_id.addr,
		(u_short)param);
	}
    }
}	/* do_reset_box() */



reset_sub(Pannex_id, name, Pports)

	ANNEX_ID	   *Pannex_id;
	char               name[];
	PORT_GROUP	   *Pports;

{
	int error_code,
	    loop;

	/* If all ports are to be reset, call reset_all().  Otherwise,
	   call reset() for each port whose bit is set in the port mask. */
	if (Pports->pg_bits & (PG_ALL | PG_SERIAL))
	    {
	    printf("resetting all serial ports of %s %s\n", BOX, name);
	    if ((error_code = reset_all(&Pannex_id->addr)) != NAE_SUCC)
	        netadm_error(error_code);
	    }
	else
	    for (loop = 1; loop <= ALL_PORTS; loop++)
		if (PORTBITSET(Pports->serial_ports,loop))
		    {
			if (loop > (int)Pannex_id->port_count)
			{
			printf("\n%s %s does not have a port %d\n",
				       BOX, name, loop);
			    continue;
			}

	            printf("resetting serial port %d of %s %s\n",
		     loop, BOX, name);
		    if ((error_code = reset_line(&Pannex_id->addr,
			(u_short)SERIAL_DEV, (u_short)loop)) != NAE_SUCC)
	                netadm_error(error_code);
		    }

	/* Reset the virtual CLI ports if requested. */

	if (Pports->pg_bits & (PG_VIRTUAL | PG_ALL))
	    {
	     if ((Pannex_id->version < VERS_4) && (Pannex_id->hw_id != X25))
		{
		if (Pports->pg_bits & PG_VIRTUAL)
		    printf("%s '%s' does not support virtual ports\n",
			   BOX, name);
		return;
		}

	    printf("resetting virtual CLI ports of %s %s\n", BOX, name);
	    if ((error_code = reset_line(&Pannex_id->addr,
		 (u_short)PSEUDO_DEV, (u_short)0)) != NAE_SUCC)
	        netadm_error(error_code);
	    }

}	/* reset_sub() */



reset_printer_sub(Pannex_id, name, Pprinters)

	ANNEX_ID	   *Pannex_id;
	char               name[];
	PRINTER_GROUP	   *Pprinters;

{
	int error_code,
	    loop;
	u_short number_reset = 0;

	for (loop = 1; loop <= ALL_PRINTERS; loop++) {
	    if (PRINTERBITSET(Pprinters->ports,loop)) {
		if (loop > (int)Pannex_id->printer_count) {
			    continue;
		}

	        printf("resetting printer %d of %s %s\n",
		     loop, BOX, name);
		number_reset++;
		if ((error_code = reset_line(&Pannex_id->addr,
			(u_short)P_PRINT_DEV, (u_short)loop)) != NAE_SUCC)
	                netadm_error(error_code);
	    }

	}
	if (number_reset == 0)
		printf("no printers were reset\n");

}	/* reset_printer_sub() */



reset_interface_sub(Pannex_id, name, Pinterfaces)

	ANNEX_ID	   *Pannex_id;
	char               name[];
	INTERFACE_GROUP	   *Pinterfaces;

{
	int error_code,
	    if_num,
	    loop;
	u_short number_reset = 0;

	for (loop = 1; loop <= ALL_INTERFACES; loop++) {
	    if (INTERFACEBITSET(Pinterfaces->interface_ports,loop)) {

	    	if_num = loop;

		/*
		 * Not allow ethernet interface to reset for now
		 */
		if (if_num == M_ETHERNET) {
			printf("\n%s %s ethernet interface not allowed to be reset\n",
				       BOX, name);
			    continue;
		}

	    	/*
	     	 * Minus ethernet number.
	     	 */

	    	if_num = if_num - M_ETHERNET;


		if (if_num > (int)Pannex_id->port_count) {
			    continue;
		}

	        printf("resetting interface asy%d of %s %s\n",
		     if_num, BOX, name);
		/*
		 * Reseting the asy interface is identical to
		 * reset the serial port.
		 */
		number_reset++;
		if ((error_code = reset_line(&Pannex_id->addr,
			(u_short)SERIAL_DEV, (u_short)if_num)) != NAE_SUCC)
	                netadm_error(error_code);
	    }

	}
	if (number_reset == 0)
		printf("no interfaces were reset\n");

}	/* reset_interface_sub() */


do_write(filename, Pannex_id, name)
char                *filename,
		    name[];
ANNEX_ID	   *Pannex_id;
{
    int		loop, asy_end, syn_end;
    FILE        *fdesc;

#ifdef CHANGE_DIR
    if (index(filename,'/') || !strcmp(filename,".") || !strcmp(filename,".."))
	punt("illegal file name:  ",filename);
#endif
    /* Open the file. */

    if ((fdesc = fopen(filename, "w")) == NULL)
	punt("couldn't open ", filename);

    /* Write an annex configuration file. */
    printf("\twriting...\n");

    /* Write the file. */
    fprintf(fdesc, "# %s %s\n", BOX, name);

    fprintf(fdesc, "\necho setting %s parameters\n", BOX);
    write_annex_script(fdesc, Pannex_id);

    for (loop = 1; loop <= Pannex_id->printer_count; loop++) {
	fprintf(fdesc,"\necho setting parameters for printer %d\n", loop);
	write_printer_script(fdesc, Pannex_id, loop);
    }

    for (loop = 1; loop <= Pannex_id->port_count; loop++) {
	fprintf(fdesc, "\necho setting parameters for async port %d\n", loop);
	write_port_script(fdesc, Pannex_id, loop);
    }

    for (loop = 1; loop <= Pannex_id->sync_count; loop++) {
	fprintf(fdesc, "\necho setting parameters for sync port %d\n", loop);
	write_sync_script(fdesc, Pannex_id, loop);
    }

    /* example for micro-annex M_SLUS = 16
     *		calling SETINTERFACEBIT(xxx, 1)  sets en0
     *		calling SETINTERFACEBIT(xxx, 2)  sets asy1
     *		calling SETINTERFACEBIT(xxx, 18) sets syn1
     */
    asy_end = (int)Pannex_id->port_count + 1;
    syn_end = (int)Pannex_id->sync_count + 1 + ALL_PORTS;
    for (loop = 1; loop <= ALL_INTERFACES; loop++) {

	if ((loop <= asy_end) || ((loop > ALL_PORTS+1) && (loop <= syn_end))) {

	    if (loop == M_ETHERNET)
		fprintf(fdesc,"\necho setting parameters for interface en%d\n",
		       loop-M_ETHERNET);
	    else 
		if(loop <= (M_ETHERNET + ALL_PORTS))
		    fprintf(fdesc, "\necho setting parameters for interface asy%d\n",
			   loop-M_ETHERNET);
	        else
		    fprintf(fdesc, "\necho setting parameters for interface syn%d\n",
			    loop-M_ETHERNET-ALL_PORTS);

	    write_interface_script(fdesc, Pannex_id, loop);
	}
    }

        if (chmod(filename, OWNER_RW))
           (void)unlink(filename);

	(void)fclose(fdesc);

}	/* do_write() */



write_annex_script(file, Pannex_id)
FILE			*file;
ANNEX_ID	  	*Pannex_id;
{
    int		xx;
    static char	format[] = "%sset %s %s %s\n";
    long	align_internal[(MAX_STRING_100 + 4)/sizeof(long) + 1];
    char	*internal = (char *)align_internal;
    char	external[LINE_LENGTH + 4];
    char	*start_delim;
    char   	*comment;
    
    for (xx = 1; Ap_index(xx) != -1; xx++) {	/* Skip zero (INET) */

	/*
	 * write out passwords and keys as comments:
	 *
	 * keys:
	 * option_key			DFE_CAT		DFE_OPTION_KEY
	 * lat_key			DFE_CAT		LAT_KEY_VALUE
	 *
	 * passwords:
	 * vcli_password		DFE_CAT		DFE_VCLI_PASSWD
	 * acp_key			DFE_CAT		DFE_ACP_KEY
	 * password			DFE_CAT		DFE_PASSWORD
	 * mop_password			DFE_CAT		DFE_MOP_PASSWD
	 * login_password		DFE_CAT		DFE_LOGIN_PASSWD
	 * rip_auth			RIP_CAT		RIP_RIP_AUTH
	 * ipx_dump_password		DLA_CAT		DLA_IPX_DMP_PASSWD
	 */
	comment = "";

	if (Ap_category(xx) == DFE_CAT) {
	    switch (Ap_catid(xx)) {
		case DFE_OPTION_KEY:
		case LAT_KEY_VALUE:
		case DFE_VCLI_PASSWD:
		case DFE_ACP_KEY:
		case DFE_PASSWORD:
		case DFE_MOP_PASSWD:
		case DFE_LOGIN_PASSWD:
		    comment = "# ";
		    break;
		default:
		    break;
	    }
	}
	else
	    if ((Ap_category(xx) == RIP_CAT && Ap_catid(xx) == RIP_RIP_AUTH) ||
		(Ap_category(xx) == DLA_CAT &&
		 Ap_catid(xx) == DLA_IPX_DMP_PASSWD))
		comment = "# ";

	if ((!Ap_support(Pannex_id, xx)) || (Ap_category(xx) == VOID_CAT))
	    continue;

	if (!get_dla_param(&Pannex_id->addr, (u_short)Ap_category(xx),
			   (u_short)Ap_catid(xx), (u_short)Ap_type(xx),
			   internal)) {

	    decode(Ap_convert(xx), internal, external, Pannex_id);
	    if (Ap_convert(xx) == CNV_GROUP_CODE) {
		fprintf(file, format, comment, BOX, annex_params[xx],
			"all disable");
		if (strcmp(external, NONE_STR)) {
		    strcat(external, " enable");
		}
		else 
		    strcpy(external, "all disable");
	    }
	    if (start_delim = split_string(annex_params[xx],FALSE))
		fprintf(file, format, comment, BOX, start_delim, external);
	    else
		fprintf(file, format, comment, BOX, annex_params[xx],external);
	}
	else
	    printf("\tUnable to get %s parameter\n", annex_params[xx]);
    }
}	/* write_annex_script() */


write_printer_script(file, Pannex_id, printer)
FILE			*file;
ANNEX_ID	  	*Pannex_id;
int			printer;
{
    int		xx;
    static char	format[] = "%sset printer=%d %s %s\n";
    long	align_internal[MAXVALUE/sizeof(long) + 1];
    char	*internal = (char *)align_internal;
    char	external[MAXVALUE];
    char	*start_delim;
    char   	*comment = "";
    
    for(xx = 0; Cp_index(xx) != -1; xx++) {
	
	if(Cp_category(xx) == LP_CAT && Cp_support(Pannex_id,xx)) {

	    if(!get_ln_param(&Pannex_id->addr, (u_short)P_PRINT_DEV,
			     (u_short)printer, (u_short)LP_CAT,
			     (u_short)Cp_catid(xx),
			     (u_short)Cp_type(xx), internal)) {
		decode(Cp_convert(xx), internal, external, Pannex_id);
		if (start_delim = split_string(printer_params[xx],FALSE))
		    fprintf(file, format, comment, printer, start_delim, 
			    external);
		else
		    fprintf(file, format, comment, printer, printer_params[xx],
			    external);
	    }
	    else
		printf("\tUnable to get %s parameter\n", printer_params[xx]);
	}
    }
}	/* write_printer_script() */


write_port_script(file, Pannex_id, port)
FILE			*file;
ANNEX_ID	  	*Pannex_id;
int			port;
{
    int		xx, error;
    static char	format[] = "%sset port=%d %s %s\n";
    long	align_internal[MAXVALUE/sizeof(long) + 1];
    char	*internal = (char *)align_internal;
    char	external[MAXVALUE];
    short	id, category, convert, type;
    char	latter = FALSE;
    char	*start_delim;
    char	*comment;

    for(xx = 0; Sp_index(xx) != -1; xx++) {

	comment = "";
	category = Sp_category(xx);
	id = Sp_catid(xx);

	/*
	 * write out passwords as comments:
	 *
	 * passwords:
	 * port_password		DEV2_CAT	DEV2_PORT_PASSWD
	 * ppp_password_remote		NET_CAT		PPP_PWORDRMT
	 */
	if ((category == DEV2_CAT && id == DEV2_PORT_PASSWD) ||
	    (category == NET_CAT && id == PPP_PWORDRMT))
	    comment = "# ";

	if ((!Sp_support(Pannex_id, xx)) || (category == VOID_CAT))
	    continue;

	type = (u_short)Sp_type(xx);
	convert = (u_short) Sp_convert(xx);
	if (category == DEV_CAT && id == DEV_ATTN) {
	    if((Pannex_id->version < VERS_6_2) || (Pannex_id->hw_id < ANX3)) {
		convert = CNV_PRINT;
		type = CARDINAL_P;
		latter = TRUE;
	    }
	}
	if(get_ln_param(&Pannex_id->addr, (u_short)SERIAL_DEV, (u_short)port,
			category, id, (u_short)type, internal)) {
	    printf("\tport %d:\n", port);
	    printf("\tUnable to get %s parameter\n", port_params[xx]);
	}
	else {
	    decode(convert, internal, external, Pannex_id);
	    if (convert == CNV_GROUP_CODE) {
		fprintf(file, format, comment, port, port_params[xx], "all disable");
		if (strcmp(external, NONE_STR))
		    strcat(external, " enable");
		else 
		    strcpy(external, "all disable");
	    }
	    if (start_delim = split_string(port_params[xx], latter))
		fprintf(file, format, comment, port, start_delim, external);
	    else
		fprintf(file,format, comment, port, port_params[xx], external);
	}
    }
}	/* write_port_script() */

write_sync_script(file, Pannex_id, sync)

FILE			*file;
ANNEX_ID	  	*Pannex_id;
int			sync;
{
    int		xx,error;
    static	char	format[] = "%sset sync=%d %s %s\n";
    long		align_internal[MAXVALUE/sizeof(long) + 1];
    char		*internal = (char *)align_internal;
    char		external[MAXVALUE];
    u_short id,category,convert,type;
    char		latter = FALSE;
    char *start_delim;
    char	*comment;
    
    for(xx = 0; Syncp_index(xx) != -1; xx++) {
	category = Syncp_category(xx);
	id = Syncp_catid(xx);
	
	if( (id == SYNC_PORT_PASSWD) || (id == SYNC_PPP_PASSWD_REMOTE)){
	    continue;
	}
	
	if (Syncp_support(Pannex_id,xx))
	    {
		type = (u_short)Syncp_type(xx);
		convert = (u_short) Syncp_convert(xx);
		if(get_sy_param(&Pannex_id->addr, (u_short)SYNC_DEV, 
				(u_short)sync, category,
				id, (u_short)type, internal))
		    {
			printf("\tsync %d:\n", sync);
			printf("\tUnable to get %s parameter\n", sync_params[xx]);
		    }
		else
		    {
			decode(convert,internal,external,Pannex_id);
			if (start_delim=split_string(sync_params[xx],latter))
			    fprintf(file, format, comment, sync, start_delim, external);
			else
			    fprintf(file, format, comment, sync, sync_params[xx], external);
		    }
	    }
    }
    
}	/* write_sync_script() */

write_interface_script(file, Pannex_id, interface)
FILE			*file;
ANNEX_ID	  	*Pannex_id;
int			interface;
{
    int		xx;
    static char	format_en[] = "%sset interface=en%d %s %s\n";
    static char	format_asy[] = "%sset interface=asy%d %s %s\n";
    static char	format_syn[] = "%sset interface=syn%d %s %s\n";
    long	align_internal[MAXVALUE/sizeof(long) + 1];
    char	*internal = (char *)align_internal;
    char	external[MAXVALUE];
    char	*start_delim;
    char	*comment = "";
    
    for(xx = 0; Ip_index(xx) != -1; xx++) {
	
	if(Ip_category(xx) == IF_CAT && Ip_support(Pannex_id,xx)) {

	    if(!get_if_param(&Pannex_id->addr, (u_short)INTERFACE_DEV,
			     (u_short)interface, (u_short)IF_CAT,
			     (u_short)Ip_catid(xx), (u_short)Ip_type(xx),
			     internal)) {
		decode(Ip_convert(xx), internal, external, Pannex_id);
		if (start_delim = split_string(interface_params[xx],FALSE)) {

		    /* convert the interface logical index into human-eye form */
		    if (interface == M_ETHERNET)
			fprintf(file, format_en, comment, interface-M_ETHERNET, start_delim, external);
		    else 
			if( interface <= (M_ETHERNET + ALL_PORTS) )
			    fprintf(file, format_asy, comment, interface-M_ETHERNET, start_delim, external);
		    	else
			    fprintf(file, format_syn, comment, interface-M_ETHERNET-ALL_PORTS, start_delim, external);
		}
		else {
		    if (interface == M_ETHERNET)
			fprintf(file, format_en, comment, interface-M_ETHERNET,
				interface_params[xx], external);
		    else
			if( interface <= (M_ETHERNET + ALL_PORTS))
			    fprintf(file, format_asy, comment,
				    interface-M_ETHERNET, interface_params[xx],
				    external);
		        else
			    fprintf(file, format_syn, comment,
				    interface-M_ETHERNET-ALL_PORTS,
				    interface_params[xx], external);
		}
	    }
	    else
		printf("\tUnable to get %s parameter\n", interface_params[xx]);
	}
    }
}	/* write_interface_script() */

free_port_set(PPport_set)

	PORT_SET **PPport_set;

{
	PORT_SET *Ptemp_port_set;

	while (*PPport_set)
	    {
	    Ptemp_port_set = *PPport_set;
	    *PPport_set = (*PPport_set)->next;
	    free((char *)Ptemp_port_set);
	    }

}	/* free_port_set() */


free_sync_set(PPsync_set)

	SYNC_SET **PPsync_set;

{
	SYNC_SET *Ptemp_sync_set;

	while (*PPsync_set)
	    {
	    Ptemp_sync_set = *PPsync_set;
	    *PPsync_set = (*PPsync_set)->next;
	    free((char *)Ptemp_sync_set);
	    }

}	/* free_sync_set() */


free_printer_set(PPprinter_set)

	PRINTER_SET **PPprinter_set;

{
	PRINTER_SET *Ptemp_printer_set;

	while (*PPprinter_set)
	    {
	    Ptemp_printer_set = *PPprinter_set;
	    *PPprinter_set = (*PPprinter_set)->next;
	    free((char *)Ptemp_printer_set);
	    }

}	/* free_printer_set() */


free_interface_set(PPinterface_set)

	INTERFACE_SET **PPinterface_set;

{
	INTERFACE_SET *Ptemp_interface_set;

	while (*PPinterface_set)
	    {
	    Ptemp_interface_set = *PPinterface_set;
	    *PPinterface_set = (*PPinterface_set)->next;
	    free((char *)Ptemp_interface_set);
	    }

}	/* free_interface_set() */


free_annex_list(PPannex_list)

	ANNEX_LIST **PPannex_list;

{
	ANNEX_LIST *Ptemp_annex_list;

	while (*PPannex_list)
	    {
	    Ptemp_annex_list = *PPannex_list;
	    *PPannex_list = (*PPannex_list)->next;
	    free((char *)Ptemp_annex_list);
	    }

}	/* free_annex_list() */

char *
split_string(string, latter)
char	*string;
int	latter;
{
    static char split_token[80];
    char *delim;
    int offs;

    /*
     * Split the annex_params with commas.
     */
    delim = index(string,',');
    if (delim == NULL)
	return NULL;
    while (latter > 0 && delim != NULL) {
	string = delim + 1;
	delim = index(string, ',');
	latter--;
    }
    if (latter > 0)
	return NULL;
    if (delim == NULL)
	strcpy(split_token,string);
    else {
	offs = delim - string;
	strncpy(split_token,string,offs);
	split_token[offs] = '\0';
    }
    return split_token;
}
