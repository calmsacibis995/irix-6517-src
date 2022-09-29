/*
 * xlv_mgr.c
 *
 *          Main program for the xlv_mgr(1m) command.
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Revision: 1.15 $"

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/uuid.h>
#include <sys/syssgi.h>
#include <unistd.h>
#include <string.h>
#include <pfmt.h>
#include <locale.h>

#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <sys/xlv_attr.h>
#include <xlv_utils.h>
#include <xlv_cap.h>

#include "xlv_mgr.h"

uid_t	mypid;
char	*ME;
int	Tclflag = 0;				/* command file specified */
int	has_plexing_license;			/* licensed for mirroring */
int	has_plexing_support;			/* mirroring kernel software */

/*
 * Command table.
 */
typedef struct {
#define	PRIV_ROOT	1
#define	PRIV_NONROOT	0
	int		command_priv;		/* PRIV_* */
	char		*command_name;
	Tcl_CmdProc	*command_procedure;
} cmd_entry_type;

cmd_entry_type		xlv_mgr_cmds [] = {
	{ PRIV_ROOT,	"attach",	tclcmd_attach },
	{ PRIV_ROOT,	"detach",	tclcmd_detach },
	{ PRIV_ROOT,	"delete",	tclcmd_delete },
	{ PRIV_ROOT,	"change", 	tclcmd_change }, /* node and objects */
	{ PRIV_ROOT,	"insert",	tclcmd_insert },
#ifdef REMOVE_COMMAND
	{ PRIV_ROOT,	"remove",	tclcmd_remove },
#endif
	{ PRIV_ROOT,	"script",	tclcmd_script },
	{ PRIV_NONROOT,	"show",		tclcmd_show },
	{ PRIV_NONROOT,	"quit", 	tclcmd_quit },
	{ PRIV_NONROOT,	"help",		tclcmd_help },
	{ PRIV_NONROOT,	"?",		tclcmd_help },
	{ PRIV_NONROOT,	"sh",		tclcmd_shell },
	{ PRIV_ROOT,	"reset",	tclcmd_reset },
	{ PRIV_ROOT,	NULL,		NULL }
};


static void
usage(void)
{
	fprintf(stderr, "usage: %s [-Rv] [-c cmdstring] [-r rootpath] [input_file]\n", ME);
	fprintf(stderr, "-R           Do not read XLV disk labels.\n");
	fprintf(stderr, "-ccmdstring  Use \"cmdstring\" as command input.\n");
	fprintf(stderr, "-v           Print verbose messages.\n");
	fprintf(stderr, "-rrootpath   Run with \"rootpath\" as the xlv root directory.\n");
	exit(1);
}


/*
 * Register all the XLV make commands so that they can be invoked
 * by the Tcl interpreter.
 */
static void
register_commands (Tcl_Interp *interp, int priv) {

	cmd_entry_type		*cmd_p;

	for (cmd_p = xlv_mgr_cmds; cmd_p->command_name != NULL; cmd_p++) {
		if (priv < cmd_p->command_priv)
			continue;
		Tcl_CreateCommand (interp, cmd_p->command_name,
			cmd_p->command_procedure, NULL, NULL);
	}
}


/*
 * Collect input until a complete command is read, then returns
 * the result of evaluating the command.
 */

static int 
do_one_command (Tcl_Interp *interp)
{
	char		line[200],*rc;
	Tcl_DString	cmd;
	int		result;

#define ENTIRE_STRING -1

	Tcl_DStringInit (&cmd);
	/*CONSTCOND*/
	for (;;) {
		if ((rc = fgets(line, 200, stdin)) == NULL) {
			break;
		}
		/*
		 * Append all of [line], up to the NULL character.
		 */
		Tcl_DStringAppend (&cmd, line, ENTIRE_STRING);
		if (Tcl_CommandComplete (cmd.string)) {
			break;
		}
	}

	/*
	 * Record the command in the history list and then eval it.
	 */

	if (rc != NULL)
		result = Tcl_RecordAndEval (interp, cmd.string, 0);
	else {
		if (!feof(stdin))
			perror("fgets");
		result = -1;
	}
	Tcl_DStringFree (&cmd);
	return result;

} /* end of do_one_command() */



/*
 * Determine if the running kernel has XLV plexing support.
 * Return: -1	unknown
 *	    0	no plexing code
 *	    1	plexing code
 */
static int
check_plexing_support(void)
{
	xlv_attr_req_t		req;
	xlv_attr_cursor_t	cursor;

	req.attr = XLV_ATTR_SUPPORT;

	if (0 > syssgi(SGI_XLV_ATTR_CURSOR, &cursor)) {
		printf("Failed to get a XLV cursor\n");
		return(-1);

	} else if (0 > syssgi(SGI_XLV_ATTR_GET, &cursor, &req)) {
		return(-1);	/* unknown */

	} else {
		return(req.ar_supp_plexing);
	}
}



/*
 * Main().
 */

void
main (int argc, char *argv[])
{
	Tcl_Interp	*interp;
	int		code=0;
	int		opt;
	int		vflag=0;
	int		xflag=0;
	int		Rflag=0;
	char		*rootname = NULL;
	int		priv;
	int		stdin_isatty = 0;
	FILE		*cfile = NULL;
	char		*cfilename = NULL;

	/*
	 *	Set up international items
	 */
	setlocale(LC_ALL, "C");
	setcat("xlv_mgr"); 

	ME = argv[0];

	if (cap_envl(0, CAP_DEVICE_MGT, 0) == -1) {
		mypid = 1;
		priv = PRIV_NONROOT;
		Rflag = 1;
		pfmt(stderr, MM_NOSTD,
		     "::%s: not started by super-user; limited functions\n",
		     ME);
	} else {
		mypid = 0;
		priv = PRIV_ROOT;
	}

	/*
	 * Command line parsing.
	 */
	opterr = 0;
	while ((opt = getopt(argc, argv, "c:r:vxR")) != EOF) {
		switch (opt) {
		case 'c':		/* command line */
			if (cfile == NULL) {
				cfilename = tmpnam(NULL);
				cfile = fopen(cfilename, "wb");
				if (cfile == NULL) {
					char msg[500];

					sprintf (msg,
					"%s cannot create tmp file \"%s\"",
						 ME, cfilename);
					perror(msg);
					exit(1);
				}
#ifdef DEBUG
				else {
					printf("DBG: tmpfile is \"%s\"\n",
						cfilename);
				}
#endif
			}
			fprintf(cfile, "%s\n", optarg);
			break;
		case 'r':		/* root directory */
			rootname = optarg;
			if (!xlv_setrootname(rootname)) {
				char msg[500];
				sprintf (msg,
					 "%s cannot set \"%s\" as root dir",
					 ME, rootname);
				perror(msg);
				exit(1);
			}
			break;
		case 'v':		/* verbose mode */
			vflag++;
			break;
		case 'x':		/* expert mode */
			xflag++;
			break;
		case 'R':		/* Don't read xlv labels off of disks */
			Rflag = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (cfile) {
		fprintf(cfile, "quit\n");
		fclose(cfile);		/* write it out */
	}

	if (argc > 1)
		usage();

	/*
	 * Determine whether or not the plex commands should be
	 * available by looking for the plexing license and software.
	 */
	has_plexing_license = check_plexing_license(rootname);
	has_plexing_support = check_plexing_support();

	interp = Tcl_CreateInterp();

	/*
	 * Register the XLV make commands.
	 */
	register_commands (interp, priv);

	/*
	 * Initialize the state in the backend of this program.
	 */
	tclcmd_init (!Rflag);

	if (vflag) {
		if (!Tcl_SetVar(interp, XLV_MGR_VAR_VERBOSE, "0xFFFF", 0)) {
			printf("%s: Cannot start up in verbose mode.\n", ME);
			exit(1);
		};
	}

	if (xflag) {
		if (!Tcl_SetVar(interp, XLV_MGR_VAR_SUSER, "1", 0)) {
			printf("%s: Cannot start up in expert mode.\n", ME);
			exit(1);
		};
	}

	if (argc > 0) {
		/*
		 * If an argument was specified, we assume that
		 * it's a tcl script and we will just interpret
		 * its contents.
		 */
		if (cfilename == NULL) {
			cfilename = argv[0];
		} else {
			 fprintf (stderr, "Ignoring input file %s\n", argv[0]);
		}
	}
	if (cfilename != NULL) {
		/*
		 * Interpret the command file.
		 */
		Tclflag = 1;
		code = Tcl_EvalFile (interp, cfilename);
		if (strcmp(interp->result, "end")) {
			fprintf (stderr, "%s\n", interp->result);
		}

		if (cfile)
			unlink(cfilename);
		
		if (code != TCL_OK) {
			exit (1);
		}
		
		exit (0);
	}

	/*
	 * If we reached this part, then we want to interactively
	 * administer the pieces.  We will go into an infinite interp
	 * loop, processing each command.
	 */
	stdin_isatty = isatty(STDIN_FILENO);
	if (!stdin_isatty) {
		/*
		 * Treat non-tty as TCL script -- don't ask for confirmations.
		 */
		Tclflag = 1;
	}
	for (;;) {
		if (stdin_isatty) {
			printf ("xlv_mgr> ");
			fflush(stdout);
		}
		code = do_one_command (interp);
		if (strcmp (interp->result, "end") == 0 || code == -1) 
			break;
		printf ("%s\n", interp->result);
	};

	printf("\n");
	/*
	 * Check for abnormal end. End-Of-File implies an "exit" command.
	 */
	if (code == -1 && (stdin_isatty && !feof(stdin))) {
		printf(
	"Unexpected end of file encountered. Abnormal termination\n");
		exit(1);
	}

	exit (0);

} /* end of main() */
