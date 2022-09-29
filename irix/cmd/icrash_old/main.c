#ident  "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/RCS/main.c,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#define _KERNEL  1
#define _PAGESZ         16384
#include <sys/types.h>
#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/immu.h>
#include <sys/pcb.h>
#include <sys/utsname.h>
#include <sys/mbuf.h>
#include <stdio.h>
#include "icrash.h"
#include "extern.h"
#include "klib.h"

/* Global variables
 */
static FILE *errorfp = stderr;			/* error outfile used during startup */
FILE *ofp;				/* icrash output file		     */
char namelist[BUFSIZ];                  /* name list (normally /unix)        */
char corefile[BUFSIZ];                  /* core file (live = /dev/mem)       */
char indexname[BUFSIZ];                 /* name of index file                */
char outfile[256] = "stdout";           /* name of global output file        */
char icrashdef[256] = "";      		    /* name of file containing icrashdef */
char fromfile[256];                     /* name of global command file       */
char execute_cmd[256];                  /* name of command to execute        */
char *program;                          /* program name (argv[0])            */
int prog;								/* program type (icrash or fru)      */
int bounds_flag = 0;                    /* bounds flag option                */
int pager_flag = 0;						/* turns on use of page with output  */
int write_flag = 0;                     /* write option flag                 */
int full_flag = 0;                      /* full option flag                  */
int force_flag = 0;                     /* force FRU analysis flag           */
int all_flag = 0;                       /* all option flag (error dumpbuf)   */
int err_flag = 0;                       /* error flag                        */
int report_flag = 0;                    /* Generate report output            */
int report_level = 0;					/* Determines level of report output */
int availmon = 0;                       /* availability report flag          */
char availmonfile[256] = "stdout";      /* name of availmon output file      */
int cleardump = 0;                      /* flag to clear dump                */
int bounds = -1;                        /* bounds value with -n option       */
int version = 0;                        /* flag to print version number      */
int ignoreos = 0;                       /* ignore OS level in utsname struct */
int writeflag = 0;                      /* flag to determine if writing out  */
int icrashdef_flag = 0;                 /* flag to read in icrashdef info    */
int fromflag = 0;                       /* flag to specify from commands     */
int execute_flag = 0;				    /* next arg is a command to execute  */
int suppress_flag = 0;				    /* suppress opening text             */

/*
 * usage()
 */
void
usage()
{
	if (prog == FRU_PROGRAM) {
		fprintf(errorfp, "Usage: %s [-a] [-n bounds ] namelist corefile\n", 
			program);
	}
	else {
		fprintf(errorfp,
			"Usage: %s: [-f cmdfile] [-r] [-v] [-w outfile] [-n bounds] "
			"[-e cnd] [-S] namelist corefile\n", program);
	}
}

/*
 * bad_option()
 */
void
bad_option(char * p, char c)
{
	fprintf(errorfp, "%s: invalid command line option -- %c\n", p, c);
}

/*
 * main()
 */
main(int argc, char **argv)
{
	int i, c, errflg = 0;
	command_t cb;

	/* getopt() variables.
	 */
	extern char *optarg;
	extern int optind;
	extern int opterr;

	/* Set opterr to zero to suppress getopt() error messages
	 */
	opterr = 0;

	/* Initialize a few default variables.
	 */
	indexname[0] = '\0';
	sprintf(corefile, "/dev/mem");
	sprintf(namelist, "/unix");

	/* Get the name of the program (e.g., "icrash" or "fru")
	 */
	if (program = strstr(argv[0], "icrash")) {
		prog = ICRASH_PROGRAM;
	}
	else if (program = strstr(argv[0], "fru")) {
		prog = FRU_PROGRAM;
	}
	else {

		/* For now, if we don't know if the program is icrash or fru,
		 * assume it is icrash.
		 */
		program = argv[0];
		prog = ICRASH_PROGRAM;
	}

	/* Grab all of the command line arguments. 
	 */
	while ((c = getopt(argc, argv, "n:f:w:r:ca:vd:ie:I:S")) != -1) {

		switch (c) {

			case '?':

				/* If there was an error in an argument, see if it is a 
				 * situation that can be worked around. Or, check to see
				 * if it is an argument with a different meaning for icrash 
				 * and fru.
				 */
				switch (optopt) {

					case 'd':
						/* If no debug level was specified, then set it
						 * equal to one (1).
						 */
						klib_debug = 1;
						break;

					case 'f':
						if (prog == FRU_PROGRAM) {
							full_flag++;
						}
						else {
							fprintf(errorfp, "icrash: No from file was "
								"specified\n");
							errflg++;
						}
						break;
					
					case 'a':
						if (prog == FRU_PROGRAM) {
							all_flag++;
						}
						else {
							fprintf(errorfp, "icrash: Availmon file not "
								"specified\n");
							errflg++;
						}
						break;

					case 'r':
						if (prog == FRU_PROGRAM) {
							bad_option(program, optopt);
							errflg++;
							break;
						}
						/* If report level wasn't specified, just break. It
						 * is set to one by default.
						 */
						break;

					default:
						bad_option(program, optopt);
						errflg++;
						break;
				}
				break;

			case 'a':
				/* Availability information
				 * 
				 * This option has to be used in conjunction with 
				 * the -r option, and should normally specify the 
				 * file location.
				 *
				 * If the program is fru, then set all_flag if -a 
				 * is included on the command line. If the program
				 * is icrash, then it's an error.
				 */
				if (((char*)optarg)[0] == '-') {
					if (prog == FRU_PROGRAM) {
						all_flag++;
						optind--;
					}
					else {
						fprintf(errorfp, "icrash: Availmon file not "
							"specified\n");
						errflg++;
					}
				} 
				else {
					sprintf(availmonfile, "%s", optarg);
					availmon++;
				}
				break;

			case 'c':
				/* Clear dump out.  This only works in specific cases.
				 */
				if (prog == FRU_PROGRAM) {
					bad_option(program, c);
					errflg++;
				}
				else {
					cleardump++;
				}
				break;

			case 'd':
				/* This is a non-documented command line option. It 
				 * provides a way to set the internal debug message level.
				 * If no value has been included on the command line, then 
				 * set debug equal to one (1).
				 *
				 * Note that there is no easy way to tell if the next
				 * argument is actually a valid debug level (except 
				 * when the next argument starts with a '-,' which
				 * would be the case when it is the next command line 
				 * option. If atoi returns a value less than zero, the
				 * assume it's not a valid level and set debug equal
				 * to one (1).
				 */
				if (((char*)optarg)[0] == '-') {
					klib_debug = 1;
					optind--;
				}
				else {
					klib_debug = strtoull(optarg, (char **)NULL, 0);
					if (klib_debug < 1) {
						klib_debug = 1;
						optind--;
					}
				}
				break;

			case 'f':
				if (((char*)optarg)[0] == '-') {
					if (prog == FRU_PROGRAM) {
						full_flag++;
						optind--;
					}
					else {
						fprintf(errorfp, "icrash: From file was not "
							"specified\n");
						errflg++;
					}
				} 
				else {
					if (prog == FRU_PROGRAM) {
						bad_option(program, c);
						errflg++;
					}
					else {
						sprintf(fromfile, "%s", optarg);
						fromflag++;
					}
				}
				break;

			case 'i':
				/* Ingore OS flag (undocumented)
				 */
				ignoreos++;
				break;

			case 'n':
				/* unix/vmcore file extentison 
				 */
				if (((char*)optarg)[0] == '-') {
					fprintf(errorfp, "%s: Bounds was not specified\n", program);
					errflg++;
				}
				else {
					bounds = atoi(optarg);
					bounds_flag++;
				}
				break;

			case 'r':
				/* If a report level isn't specified, it defaults to 1.
				 */
				if (prog == FRU_PROGRAM) {
					bad_option(program, c);
					errflg++;
				}
				else {
					if (((char*)optarg)[0] == '-') {
						report_level = 0;
						report_flag++;
						optind--;
					}
					else {
						report_level = atoi(optarg);
						if (report_level < 1) {
							report_level = 1;
							optind--;
						}
						report_flag++;
					}
				}
				break;

			case 'e':
				if (prog == FRU_PROGRAM) {
					bad_option(program, c);
					errflg++;
				}
				else {
					sprintf(execute_cmd, "%s", optarg);
					execute_flag++;
				}
				break;

			case 'v':
				/* Version information
				 */
				version++;
				break;

			case 'w':
				/* Set the output file
				 */
				if (((char*)optarg)[0] == '-') {
					fprintf(errorfp, 
						"%s: Outfile was not specified\n", program);
					errflg++;
				}
				else {
					sprintf(outfile, "%s", optarg);
					writeflag++;
				}
				break;

			case 'I':
				/* Set the icrashdef file (undocumented)
				 */
				if (((char*)optarg)[0] == '-') {
					fprintf(errorfp, 
						"%s: icrashdef file was not specified\n", program);
					errflg++;
				}
				else {
					sprintf(icrashdef, "%s", optarg);
					icrashdef_flag++;
				}
				break;

			case 'S':
				suppress_flag++;
				break;

			default:
				bad_option(program, c);
				errflg++;
				break;
		}

		/* Check to see if there was an error.
		 */
		if (errflg) {
			usage();
			exit(1);
		}
	}

	if (!bounds_flag) {
		if (optind < argc) {
			if ((argc - optind) > 1) {
				strcpy(namelist, argv[optind]);
				strcpy(corefile, argv[optind+1]);
			} 
			else {
				fprintf(errorfp,
					"Need to specify both namelist and corefile!\n");
				errflg++;
			}
		}
	} 
	else if (bounds >= 0) {
		sprintf(namelist, "unix.%d", bounds);
		sprintf(corefile, "vmcore.%d.comp", bounds);
	} 
	else {
		fprintf(errorfp,
			"Bounds value for -n option needs to be >= 0!\n");
		errflg++;
	}

	if (errflg) {
		usage();
		exit(1);
	}

	if ((availmon) && (!report_flag)) {
		/* 
		 * Cannot use fatal here.. because KL_ERRORFP 
		 * is not yet defined. 
		 */
		fprintf(errorfp,"You must use the -r option with the -a option.\n");
		exit(1);
	}

	/* If we are trying to clear a vmcore image, we have to make sure
	 * that /unix and /dev/swap are being used, otherwise, it is an
	 * error.  Yea, yea, it's ugly, but we have to do this because the
	 * savecore script that runs on boot-up has no other way to clear
	 * a core dump when 'icrash' fails initially even though there might
	 * be a dump in /dev/swap to begin with.
	 */
	if ((cleardump) && (!strcmp(namelist, "/unix")) &&
		(!strcmp(corefile, "/dev/swap"))) {
			clear_dump();
			exit(0);
	} 
	else if (cleardump) {
		fprintf(errorfp,
			"icrash: the -c option is only valid with /unix and /dev/swap!\n");
		exit(1);
	}

	if (version) {
#ifdef VERSION
		fprintf(errorfp, "IRIX Crash, Version %s\n", VERSION);
#else
		fprintf(errorfp, "IRIX Crash, Version (Special)\n");
#endif
		exit(0);
	}

	if (writeflag) {
		if (!(ofp = fopen(outfile, "a"))) {
			fprintf(errorfp, "Cannot open file %s.  ", outfile);
			fprintf(errorfp, "Output going to stdout.\n");
			strcpy(outfile, "stdout");
			ofp = stdout;
			writeflag = 0;
		}
	} 
	else {
		ofp = stdout;
		writeflag = 0;
	}

	if ((prog == ICRASH_PROGRAM) && !report_flag && !suppress_flag) {
		fprintf(ofp, "corefile = %s, namelist = %s, outfile = %s\n", 
			corefile, namelist, outfile);
		fprintf(ofp, "\nPlease wait...");
		if (klib_debug) {
			fprintf(ofp, "\n");
		}
	}

	init(ofp);

#ifdef NOT
	/* Set up some signals to catch interrupts.
	 */
	signal((int)SIGINT,  (int (*)())sig_handler);
	signal((int)SIGPIPE, (int (*)())sig_handler);
#endif

	/* Set up signal handler for SEGV and BUSERR signals so that icrash
	 * doesn't dump core.
	 */
	sig_setup();

	/* If the program name is "fru," then run the FRU Analyzer. If the
	 * program name is "icrash," and  a report has been requested, dump 
	 * out the information from the core dump. Otherwise, start processing 
	 * commands.
	 */
	if (prog == FRU_PROGRAM) {

		/* XXX -- the fru code isn't working yet!
		 */
		fprintf(errorfp, "FRU not supported at this time\n");
		exit(1);

#ifdef XXX
		dofru((full_flag ? C_FULL : 0), ofp);
		fflush(ofp);
		if (all_flag) {
			kl_print_errorbuf(ofp);
			fflush(ofp);
		}
		if (write_flag) {
			fclose(ofp);
		}
		exit(0);
#endif
	}
	else if (execute_flag) {

		command_t cb;
		char *t;
		char *lasts = (char*)NULL;

#define _SGI_REENTRANT_FUNCTIONS
		t = strtok_r(execute_cmd, ";", &lasts);
		while (t) {

			t += strspn(t, " ");
			get_cmd(&cb, t);

			if (writeflag) {
				cb.ofp = ofp;
			} 
			else {
				cb.ofp = stdout;
			}
			if (!suppress_flag) {
				fprintf(ofp, "\n>> %s\n", t);
			}
			checkrun_cmd(cb, ofp);
			t = strtok_r((char*)NULL, ";", &lasts);
		}
#undef _SGI_REENTRANT_FUNCTIONS
		if (writeflag) {
			exit(fclose(ofp));
		}
		exit(0);
	}
	else if ((report_flag) || (fromflag)) {

		command_t cb;

		/* Make sure the command buffer is all NULL before we start
		 */
		bzero(&cb, sizeof(command_t));

		/* The following allows the report_cmd() function to be called 
		 * just like any other command function. This approach is 
		 * likely to change after the command interface gets reworked. 
		 */
		get_cmd(&cb, "report");
		if (writeflag) {
			cb.ofp = ofp;
		} 
		else {
			cb.ofp = stdout;
		}
		report_cmd(cb);
		if (writeflag) {
			exit(fclose(ofp));
		}
		exit(0);
	} 
	else {
		process_commands(ofp);
	}
}
