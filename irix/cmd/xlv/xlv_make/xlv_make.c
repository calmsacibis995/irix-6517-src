/*
 * Main program for the xlv_make command.
 */
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <tcl.h>

#include <sys/uuid.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <sys/dvh.h>
#include <xlv_utils.h>	/* for xlv_setnodename() */
#include <xlv_cap.h>

#include "xlv_make_cmd.h"

#define	ME	"xlv_make"

int	Tclflag=0;  /* command file specified */

/*
 * Command table.
 */
typedef struct {
	char		*command_name;
	Tcl_CmdProc	*command_procedure;
} cmd_entry_type;

cmd_entry_type		xlv_make_cmds [] = {
	{ "vol",	xlv_make_vol },
	{ "log",	xlv_make_subvol_log },
	{ "data", 	xlv_make_subvol_data },
	{ "rt",		xlv_make_subvol_rt },
	{ "plex",	xlv_make_plex },
	{ "ve",		xlv_make_ve },
	{ "show",	xlv_make_show },
	{ "end",	xlv_make_end },
	{ "create",	xlv_make_create },
	{ "clear",	xlv_make_clear },
	{ "exit",	xlv_make_exit },
	{ "quit", 	xlv_make_quit },
	{ "help",	xlv_make_help },
	{ "?",		xlv_make_help },
	{ "sh",		xlv_make_shell },
	{ NULL,		NULL }
};


static void
usage(void)
{
	fprintf(stderr, "usage: %s [-Afv] [-c cmdstring] [-h nodename] [input_file]\n", ME);
	fprintf(stderr, "-A           Do not invoke xlv_assemble upon exit\n");
	fprintf(stderr, "-f           Run with force mode in effect.\n");
	fprintf(stderr, "-v           Print verbose messages.\n");
	fprintf(stderr, "-ccmdstring  Use \"cmdstring\" as command input.\n");
	fprintf(stderr, "-hnodename   Use \"nodename\" as the local nodename.\n");

	exit(1);
}


/*
 * Register all the XLV make commands so that they can be invoked
 * by the Tcl interpreter.
 */
static void
register_commands (Tcl_Interp *interp) {

	cmd_entry_type		*cmd_p;

	for (cmd_p = xlv_make_cmds; cmd_p->command_name != NULL; cmd_p++) {
		Tcl_CreateCommand (interp, cmd_p->command_name,
			cmd_p->command_procedure, NULL, NULL);
	}
}


/*
 * Collect input until a complete command is read, then returns
 * the result of evaluating the command.
 */

static int 
do_one_command (Tcl_Interp *interp) {
	char		line[200],*rc;
	Tcl_DString	cmd;
	int		result;

	Tcl_DStringInit (&cmd);
	for (;;) {
		if ((rc=fgets (line, 200, stdin)) == NULL) {
			break;
		}
		/*
		 * Append all of [line], up to the NULL character.
		 */
		#define ENTIRE_STRING -1
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
}


/*
 * Main().
 */

void
main (int argc, char *argv[])
{
	Tcl_Interp	*interp;
	int		code=0;
	int		retcode=0;	/* command return code */
	int		opt;
	int		vflag=0;
	int		xflag=0;
	int		fflag=0;
	int		no_Assemble_flag=0;
	int		stdin_isatty = 0;
	FILE		*cfile = NULL;
	char		*cfilename = NULL;
	char		*nodename = NULL;

	if (cap_envl(0, CAP_DEVICE_MGT, 0) == -1) {
		fprintf(stderr,
			"%s: must be started by super-user\n", 
			argv[0]);
		exit(1);
	}

	/*
	 * Command line parsing.
	 */
	opterr = 0;
	while ((opt = getopt(argc, argv, "c:fh:vxA")) != EOF) {
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
		case 'h':
			nodename = optarg;
			no_Assemble_flag++;	/* implied flag */
			break;
		case 'v':		/* verbose mode */
			vflag++;
			break;
		case 'f':		/* force mode */
			fflag++;
			break;
		case 'x':		/* expert mode */
			xflag++;
			break;
		case 'A':		/* no_xlv_assemble mode */
			no_Assemble_flag++;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (cfile) {
		fprintf(cfile, "exit\n");
		fclose(cfile);		/* write it out */
	}

	if (argc > 1)
		usage();

	if (nodename)
		xlv_setnodename(nodename);

	interp = Tcl_CreateInterp();

	/*
	 * Register the XLV make commands.
	 */
	register_commands (interp);

	/*
	 * Initialize the state in the backend of this program.
	 */

	xlv_make_init ();

	if (vflag) {
		if (!Tcl_SetVar(interp, XLV_MAKE_VAR_VERBOSE, "0xFFFF", 0)) {
			printf("%s: Cannot start up in verbose mode.\n", ME);
			exit(1);
		};
	}

	if (xflag) {
		if (!Tcl_SetVar(interp, XLV_MAKE_VAR_SUSER, "1", 0)) {
			printf("%s: Cannot start up in expert mode.\n", ME);
			exit(1);
		};
	}

	if (fflag) {
		if (!Tcl_SetVar(interp, XLV_MAKE_VAR_FORCE, "1", 0)) {
			printf("%s: Cannot start up in force mode.\n", ME);
			exit(1);
		};
	}

	if (no_Assemble_flag) {
		if (!Tcl_SetVar(interp, XLV_MAKE_VAR_NO_ASSEMBLE, "1", 0)) {
			printf("%s: Cannot start up in no xlv_assemble mode.\n", ME);
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
		Tclflag = 1;
		code = Tcl_EvalFile (interp, cfilename);
		if (strcmp(interp->result,"end")) {
			fprintf (stderr, "%s\n", interp->result);
		}

		if (cfile)
			unlink(cfilename);
		
		if (code != TCL_OK) {
			exit (1);
		}
		
		/*
		 * XXX Here is where we would commit the changes.
		 */
		exit (0);
	}

	/*
	 * If we reached this part, then we want to interactively
	 * create the pieces.  We will go into an infinite interp
	 * loop, processing each command.
	 */
	stdin_isatty = isatty(STDIN_FILENO);
	if (!stdin_isatty) {
		/*
		 * Treat non-tty as TCL script.
		 */
		Tclflag = 1;
	}
	for (;;) {
		if (stdin_isatty) {
			printf ("xlv_make> ");
			fflush(stdout);
		}
		code = do_one_command (interp);
		if (strcmp (interp->result, "end") == 0 || code == -1) 
			break;
		printf ("%s\n", interp->result);
	};

	printf("\n");
	/*
	 * Check for abnormal end. End-Of-File implies a "quit" command,
	 * and no changes make it out to disk.
	 */
	if (code == -1) {
		printf(
	"Unexpected end of file encountered. Abnormal termination\n");
		retcode = 1;
	}

	/* closeup */
	xlv_make_done();
	exit (retcode);
}
