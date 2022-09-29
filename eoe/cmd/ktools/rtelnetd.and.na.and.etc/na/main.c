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
 * 	Annex Network Administrator program "main" function
 *
 * Original Author: %$(author)$%	Created on: %$(created-on)$%
 *
 * Revision Control Information:
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/na/RCS/main.c,v 1.3 1996/10/04 12:10:39 cwilson Exp $
 *
 * This file created by RCS from:
 *
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/na/RCS/main.c,v $
 *
 * Revision History:
 *
 * $Log: main.c,v $
 * Revision 1.3  1996/10/04 12:10:39  cwilson
 * latest rev
 *
 * Revision 2.37  1994/08/04  11:07:19  sasson
 * SPR 3211: moved declaration of "debug" to this module.
 *
 * Revision 2.36  1994/05/24  11:07:25  russ
 * Bigbird na/adm merge.
 *
 * Revision 2.35  1993/07/30  10:38:37  carlson
 * SPR 1923 -- rewrote pager to use file descriptor instead of file pointer.
 *
 * Revision 2.34  1993/07/27  20:26:39  wang
 * na/admin interface redesign changes.
 *
 * Revision 2.33  1993/07/15  10:50:41  carlson
 * Removed bogus externs which reduced portability.
 *
 * Revision 2.32  93/02/24  13:07:58  carlson
 * Removed pager conditionals and set to call init/stop routines.
 * 
 * Revision 2.31  92/11/23  14:28:35  carlson
 * Check for bogus port numbers because "-d" flag for port number looks
 * like a debug flag.
 * 
 * Revision 2.30  92/04/17  16:34:46  carlson
 * Removed warnings from link editors that don't like to see
 * benign redefinition.
 * 
 * Revision 2.29  92/01/31  22:50:00  raison
 * change printer to have printer_set and added printer_speed
 * 
 * Revision 2.28  91/11/05  14:42:44  carlson
 * SPR 369 -- if change_dir is set, then go to that directory on startup.
 * 
 * Revision 2.27  91/10/18  11:49:34  carlson
 * Removed my mistaken change to the PAGER and updated for SysV compilers.
 * 
 * Revision 2.26  91/10/17  16:50:24  carlson
 * Fixed PAGER switch and added argument to interrupt().  (Signal passes arg!0
 * 
 * Revision 2.25  91/10/14  16:52:13  russ
 * Added pager.
 * 
 * Revision 2.24  91/08/01  10:17:59  dajer
 * Changed main() to exit with a status.
 * 
 * Revision 2.23  91/04/24  19:19:35  emond
 * Explicitly state "debug" is an extern. Paranoid about diff compilers.
 * 
 * Revision 2.22  91/04/08  23:32:56  emond
 * Fix some compile errors on SysV and accommodate generic API I/F
 * 
 * Revision 2.21  90/08/16  11:25:19  grant
 * 64 port support.
 * 
 * Revision 2.20  90/04/17  17:52:00  emond
 * Made "version[]" an extern so it will compile on Ultrix & SGI.
 * 
 * Revision 2.19  90/02/15  15:45:52  loverso
 * erradicate #else/#endif TAG
 * 
 * Revision 2.18  89/06/15  16:31:12  townsend
 * Added support for trunks
 * 
 * Revision 2.17  89/04/28  15:20:02  loverso
 * Allow change of port # from command line
 * 
 * Revision 2.16  89/04/11  01:08:39  loverso
 * use getservbyname not #ifdef XENIX. use stderr for error messages
 * 
 * Revision 2.15  89/04/05  12:42:04  loverso
 * Changed copyright notice
 * 
 * Revision 2.14  88/05/24  18:38:03  parker
 * Changes for new install-annex script
 * 
 * Revision 2.13  88/05/10  17:24:38  harris
 * Restore echo on SIGINT, in case a ^C is entered at Password prompt.
 * 
 * Revision 2.12  88/04/12  19:13:43  parker
 * Fixes for Xenix
 * 
 * Revision 2.11  88/03/15  14:36:36  mattes
 * Different port_list structure
 * 
 * Revision 2.10  88/01/19  09:56:18  mattes
 * Added -D debug switch; fixed Sys V SIGINT problem
 * 
 * Revision 2.9  87/09/28  11:51:17  mattes
 * Fixed default port problem; generalized box name
 * 
 * Revision 2.8  87/06/10  18:11:32  parker
 * Added support for Xenix/Excelan PC.
 * 
 * Revision 2.7  86/12/08  12:17:29  parker
 * R2.1 NA parameter changes
 * 
 * Revision 2.6  86/11/14  17:05:08  harris
 * Include file for XENIX and UMAX V.
 * 
 * Revision 2.5  86/07/03  15:05:48  harris
 * Reorganized to use tables describing parameters and commands.
 * Added init_tables function to validate tables an extract keys.
 * 
 * Revision 2.4  86/05/30  17:23:19  goodmon
 * Re-enabled the superuser checking.
 * 
 * Revision 2.3  86/05/16  15:59:07  goodmon
 * Fixed exit() values.
 * 
 * Revision 2.2  86/05/07  10:59:37  goodmon
 * Added broadcast command.
 * 
 * Revision 2.1  86/02/26  11:49:21  goodmon
 * Changed configuration files to script files.
 * 
 * Revision 1.6  86/01/21  18:14:06  goodmon
 * Added version banner.
 * 
 * Revision 1.5  86/01/14  11:54:09  goodmon
 * Modified to extract erpc port number from /etc/services.
 * 
 * Revision 1.4  86/01/10  12:13:52  goodmon
 * New improved error messages.
 * 
 * Revision 1.3  85/12/06  16:14:00  goodmon
 * Added port_mode parameter value 'adaptive' and character parameter
 * value minimum uniqueness.
 * 
 * Revision 1.2  85/12/05  18:22:23  goodmon
 * Added ^c handling and fixed port_multiplexing bug.
 * 
 * Revision 1.1  85/11/01  17:41:02  palmer
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *****************************************************************************
 */

#define RCSDATE $Date: 1996/10/04 12:10:39 $
#define RCSREV	$Revision: 1.3 $
#define RCSID   "$Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/na/RCS/main.c,v 1.3 1996/10/04 12:10:39 cwilson Exp $"
#ifndef lint
static char rcsid[] = RCSID;
#endif

/*
 *	Include Files
 */
#include "../inc/config.h"

#include <ctype.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <strings.h>
#include <sys/uio.h>

#include <setjmp.h>
#include <stdio.h>
#include <signal.h>

/* Tell include file to declare globals */
#define IN_MAIN		1
#include "na.h"

/*
 *	External Definitions
 */

extern char version[];
extern char *cmd_spellings[];
extern int (*cmd_actions[])();
extern void interrupt();

/*
 *	Forward Routine Declarations
 */

int debug = 0;
int init_tables();

/*
 *	Defines and Macros
 */

#define STDIN 0
#define ROOT 0


show_usage()
{
	fprintf(stderr,
		"usage: na [-d<UDP-port>] [-D[<debug-level>]]\n");
	exit(1);
}

int
main(argc, argv)
int argc;
char *argv[];
{

	struct servent *Pserv;
	int port = -1, i;

#ifdef CHANGE_DIR
/*
 * Note that we cannot use chroot here because of a bug in
 * gethostbyname -- it uses /etc/hosts and it gets lost if a new
 * root directory is set.
 */
	if (chdir(CHANGE_DIR) != 0) {
		perror("Cannot change to main directory");
		exit(2);
		}
#endif

	debug = 0;
	status = 0;

	/* process arguments */
	while (argc-- > 1) {
	    if (**++argv == '-') {
		switch ((*argv)[1]) {
		case 'd':
			if (isdigit((*argv)[2])) {
				(*argv)+=2;
			} else {
				argv++;
				argc--;
				if (*argv == NULL || !isdigit(**argv)) {
					fprintf(stderr,
						"na: port number required!\n");
					show_usage();
				}
			}
	    	        port = atoi(*argv);
	                break;
	    
		case 'D':
			if (isdigit((*argv)[2]))
				debug = (*argv)[2] - '0';
			else
				debug = 1;
			fprintf(stderr, "na: debug level %d\n", debug);
			break;

		default:
			show_usage();
	        }	/* switch ((*argv)[1]) */
	    } else
		show_usage();
	}	/* while (argc) */


	/* Print banner. */
	printf("%s network administrator %s\n", Box, version);

	/* decide what port number to use */

	if (port != -1)
		erpc_port = htons((u_short)port);
	else {
		Pserv = getservbyname("erpc", "udp");
		if (Pserv == 0) {
			fprintf(stderr, "na: udp/erpc: unknown service\n");
			exit(1);
		}
		erpc_port = Pserv->s_port;
	}
	if (debug)
		fprintf(stderr, "port=%d; using udp port %d\n",
			port, (int)ntohs(erpc_port));

	/* Set up to get input from pre-opened standard input. */
	cmd_file = stdin;

	/* Determine whether input is coming from a script file. */
	script_input = !isatty(STDIN);

	/* Determine whether user is super. */
/*	is_super = (getuid() == ROOT || geteuid() == ROOT); */
	/* when anyone can be root locally, not much point... */
	is_super = 1;

	/* Assign the initial default port set. */

	Pdef_port_set = (PORT_SET *)malloc(sizeof(PORT_SET));
	Pdef_port_tail = Pdef_port_set;
	Pdef_port_set->next = NULL;
	Pdef_port_set->ports.pg_bits = PG_ALL;
	for (i=1;i<=ALL_PORTS;i++)
#ifdef DEFAULT_ALL_SET
	    SETPORTBIT(Pdef_port_set->ports.serial_ports,i);
#else
	    CLRPORTBIT(Pdef_port_set->ports.serial_ports,i);
#endif
	Pdef_port_set->annex_id.addr.sin_addr.s_addr = 0;
	Pdef_port_set->name[0] = '\0';

	/* Assign the initial default port set. */

	Pdef_printer_set = (PRINTER_SET *)malloc(sizeof(PRINTER_SET));
	Pdef_printer_tail = Pdef_printer_set;
	Pdef_printer_set->next = NULL;
	Pdef_printer_set->printers.pg_bits = PG_ALL;
	for (i=1;i<=ALL_PRINTERS;i++)
#ifdef DEFAULT_ALL_SET
	    SETPRINTERBIT(Pdef_printer_set->printers.ports,i);
#else
	    CLRPRINTERBIT(Pdef_printer_set->printers.ports,i);
#endif
	Pdef_printer_set->annex_id.addr.sin_addr.s_addr = 0;
	Pdef_printer_set->name[0] = '\0';

	/* Assign the initial default interface set. */

	Pdef_interface_set = (INTERFACE_SET *)malloc(sizeof(INTERFACE_SET));
	Pdef_interface_tail = Pdef_interface_set;
	Pdef_interface_set->next = NULL;
	Pdef_interface_set->interfaces.pg_bits = PG_ALL;
	for (i=1;i<=ALL_INTERFACES;i++)
#ifdef DEFAULT_ALL_SET
	    SETINTERFACEBIT(Pdef_interface_set->interfaces.interface_ports,i);
#else
	    CLRINTERFACEBIT(Pdef_interface_set->interfaces.interface_ports,i);
#endif
	Pdef_interface_set->annex_id.addr.sin_addr.s_addr = 0;
	Pdef_interface_set->name[0] = '\0';

	/* Assign the initial default trunk set. */

	Pdef_trunk_set = (TRUNK_SET *)malloc(sizeof(TRUNK_SET));
	Pdef_trunk_tail = Pdef_trunk_set;
	Pdef_trunk_set->next = NULL;
	Pdef_trunk_set->trunks.pg_bits = PG_ALL;
	Pdef_trunk_set->trunks.serial_trunks = 0L;
	Pdef_trunk_set->annex_id.addr.sin_addr.s_addr = 0;
	Pdef_trunk_set->name[0] = '\0';

	/* Initialize table from help dictionary, check indices */

	(void) init_tables();

	/* Set up a signal handler to catch control-c's and go back
	   to the main command prompt. */

	(void)signal(SIGINT, interrupt);

	initialize_pager();

	/* Get and execute commands. */

	(void) cmd_sub();
	exit(status);

}	/* main() */
	   


cmd_sub()

{
	int cmd_number;

	done = FALSE;	/* set to TRUE in quit_cmd() */

	while(!done)
	    {

	    /* Come back here for another command prompt after an error. */
	    if (setjmp(cmd_env))
		printf("\n");

	    (void)setjmp(prompt_env);

	    close_pager();
	    prompt("command", NULLSP, FALSE);

	    /* If end of a script read by a read command, quit. */
	    if (eos)
		return;

	    /* Execute the specified command. */
	    cmd_number = match(symbol, cmd_spellings, "command");
	        (*cmd_actions[cmd_number])();

	    /* Warn human about any extraneous arguments. */
	    while (!eos)
		{
		printf("extra symbol '%s' ignored\n", symbol);
		(void)lex();
		}
	    }

}	/* cmd_sub() */




void interrupt(arg)
int arg;
{
#ifdef SYS_V
	signal(SIGINT, interrupt);
#endif
	devttyecho();
	stop_pager();
	longjmp(cmd_env, TRUE);
}	/* interrupt() */
