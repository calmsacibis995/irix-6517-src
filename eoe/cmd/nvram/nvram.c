/*
 * NAME
 *	nvram - read or write non-volatile ram.
 * SYNOPSIS
 *	nvram [-v] [name [value]]
 * DESCRIPTION
 *	Get or set a non-volatile RAM variable.
 */
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <sys/syssgi.h>
#include <sys/types.h>
#include <sys/systeminfo.h>
#include <sys/wait.h>
#include <sys/capability.h>

/*
 * Use this Auxilary command support to add a command that can be
 * used to access PROM environment variables that are outside of the
 * irix kernel.
 *
 * The Auxilary command must support the following command lines:
 * nvram:	auxcmd -N [-E] var [value]
 *	only one variable at a time to be retrieved, and if value
 *	is specified, then the command is a "set" operation.
 * 	if -E is specified, produce "var=value" output.
 *
 * if auxcmd is unable to access the auxilary device for any reason,
 * return -1 and print no output.  In other words, act like the
 * auxilary command doesn't exist.
 *
 * auxcmd must return a value from the following set:
 *	0 	success
 *	errno	errno failure value to display
 */

/*
 * IP30 has the ability to have non-volatile ram variables
 * in the IP30 system board PROM.
 */
#define	IP30AUXPATH		"/usr/sbin/flash"
#define	IP30AUX			"flash"
#define IP30AUXERROR		"IP30 Flash PROM device is not available"
#define	NCMDARGS		6

/*
 * ATTENTION:
 * this set of reserved variables needs to be kept synchronized
 * between the nvram command, this flash command, and the set
 * of variables defined to be resident in the flash PROM in the
 * file irix/kern/sys/RACER/IP30nvram.h
 *
 * these variables are specially defined to be allowed to be set
 * into the flash PROM, even if they did not exist in the past.
 */
char * ip30ReservedVars[] = 	{ "OSLoadPartition",	"OSLoader",
				  "OSLoadFilename",	"SystemPartition",
				  "OSLoadOptions",	"fastfan",
				  NULL
				};
/*
 * prototypes
 */
int auxcmd( const char *executable, char *const *args );
int is_reserved( const char *var, char *const *reserved);

main(int argc, char **argv)
{
	int verbose, i;
	char *progname;
	char *operation;
	char buf[SGI_NVSTRSIZE];
	char nam[SGI_NVSTRSIZE];
	extern int optind;
	int is_sgikopt;
	int ignored;			/* return value */
	char system_name[16];		/* holds system name */
	int use_aux = 0;		/* is this an IP30 system? */
	char * cmdargs[ NCMDARGS ];	/* arguments to auxilary exec */
	char * aux_prog = NULL;		/* auxilary program argv[0] */
	char * aux_error = NULL;	/* auxilary unavailable error */
	char * aux_pathname = NULL;	/* auxilary executable pathname */
	char *const * aux_reserve_vars = NULL; /*auxilary reserved variables */
	int rc;				/* return code */
	cap_t ocap;
	cap_value_t capv = CAP_SYSINFO_MGT;

	verbose = 0;
	progname = strrchr(*argv, '/');
	if (progname != NULL)
		progname++;
	else
		progname = *argv;

	/*
	 * check if we're running on an IP30 system
	 */
	ignored = sysinfo(SI_MACHINE, system_name, sizeof(system_name));
	if (!strcmp(system_name, "IP30")) {
		use_aux = 1;
		aux_prog = IP30AUX;
		aux_error = IP30AUXERROR;
		aux_pathname = IP30AUXPATH;	

		for (i=0; i<NCMDARGS; i++)	/* initialize */
			cmdargs[i] = NULL;
		aux_reserve_vars = ip30ReservedVars;
	}

	is_sgikopt = !strcmp(progname, "sgikopt");
	if (is_sgikopt && argc > 1) {
		i = 0;
		while (--argc > 0) {
			++argv;
			if (syssgi(SGI_GETNVRAM, *argv, buf) < 0) {
				if (use_aux) {
					cmdargs[0] = aux_prog;
					cmdargs[1] = "-N";
					cmdargs[2] = *argv;
					cmdargs[3] = NULL;

					/* fail even if silent */
					if (auxcmd(aux_pathname, cmdargs)) {
						i = 1;
					}
				}
				else {
					i = 1;
				}
				continue;
			}
			printf("%s\n", buf);
		}
		exit(i);
	}

	while ((i = getopt(argc, argv, "v")) != EOF) {
		if (i != 'v')
			goto usage;
		verbose = 1;
	}
	argv += optind;
	argc -= optind;

	switch (argc) {
	  case 0:
		for (i = 0;; i++) {
			sprintf(nam, "%d", i);
			if (syssgi(SGI_GETNVRAM, nam, buf) < 0)
				break;
			printf("%s=%s\n", nam, buf);
		}

		if (use_aux) {
			int j = 0;

			cmdargs[j++] = aux_prog;
			cmdargs[j++] = "-N";
			cmdargs[j++] = "-E";	/* always verbose */
			cmdargs[j++] = NULL;

			ignored = auxcmd( aux_pathname, cmdargs );
		}
		break;
	  case 1:
		if (syssgi(SGI_GETNVRAM, *argv, buf) < 0) {
			int auxfail = 0;

			if (use_aux) {
				int j = 0;
				int auxrc;

				cmdargs[j++] = aux_prog;
				cmdargs[j++] = "-N";
				if (verbose) cmdargs[j++] = "-E";
				cmdargs[j++] = *argv;
				cmdargs[j++] = NULL;

				auxrc = auxcmd(aux_pathname, cmdargs);
				if (auxrc != 0) {
					errno = auxrc;
					auxfail = 1;
				}
			}

			if ((!use_aux) || auxfail) {
				operation = "get";
				goto bad;
			}
		}
		else {
			if (verbose)
				printf("%s=%s\n", *argv, buf);
			else
				printf("%s\n", buf);
		}
		break;
	  case 2:
		ocap = cap_acquire(1, &capv);
		if ((syssgi(SGI_SETNVRAM, argv[0], argv[1]) < 0) || 
			is_reserved(argv[0], aux_reserve_vars)) {
			int auxfail = 0;

			if (use_aux) {
				int j = 0;
				int auxrc;

				cmdargs[j++] = aux_prog;
				cmdargs[j++] = "-N";
				cmdargs[j++] = argv[0];
				cmdargs[j++] = argv[1];
				cmdargs[j++] = NULL;

				auxrc = auxcmd(aux_pathname, cmdargs);
				if (auxrc != 0) {
					errno = auxrc;
					auxfail = 1;
				}
			}

			if ((!use_aux) || auxfail) {
				operation = "set";
				cap_surrender(ocap);
				goto bad;
			}
		}
		cap_surrender(ocap);
		if (verbose)
			printf("%s=%s\n", argv[0], argv[1]);
		break;
	  default:
usage:
		fprintf(stderr, "usage: %s [-v] [name [value]]\n", progname);
		exit(-1);
	}

	exit(0);

bad:
	if (use_aux && (errno == ENODEV)) {
		fprintf(stderr, "%s: %s.\n",progname, aux_error);
	} else if (errno == EINVAL) {
		fprintf(stderr, "%s: no variable named \"%s\".\n",
			progname, *argv);
	} else if (errno == ENXIO) {
		fprintf(stderr,"%s: operation not available on this machine.\n",
			progname);
	} else if ((errno < 0) || (errno > sys_nerr)) {
		fprintf(stderr, "%s: invalid error value %d\n",progname, errno);
	} else {
		fprintf(stderr, "%s: cannot %s \"%s\": %s.\n",
			progname, operation, *argv, sys_errlist[errno]);
	}
	exit(1);
}

/*
 * auxcmd
 *
 * handle the execution of the command line passed into us.
 * we'll fork and exec the child process, let the subprocess
 * write directly to stdout, and then reap the child process 
 * to get the exit return code.
 */
int
auxcmd( const char * path, char *const *args )
{
	int rc;				/* return code */
	int status;			/* exit status of child */
	pid_t child;			/* child process */

	fflush(stdout);

	child = fork();
	if (-1 == child) return -1;

	if (0 == child) {
		/* child processing */
		rc = execvp( path, args );
		_exit(-1);		/* NOTREACHED */
	}

	/* parent (nvram) processing */
	rc = waitpid(child, &status, 0);
	if (-1 != rc) {
		return WEXITSTATUS( status );
	}

	return -1;
}
/*
 * is_reserved
 *
 * check to see if the variable passed in as "var" is one of
 * special "reserved" set requires that we call the auxiliary
 * command, regardless of the success/failure of the syssgi()
 * call.
 *
 * return values:
 *	0	var is not in the set of strings in reserved
 *	1	var is in reserved
 */
int
is_reserved( const char *var, char *const *reserved) 
{
	if (!var || !reserved) return 0;

	while (*reserved) {
		if (!strcmp(var, *reserved)) {
			return 1;
		}
		reserved++;
	}

	return 0;
}
