#define _RPCGEN_SVC
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <sys/signal.h>
#include <rpc/rpc.h>
#include "lockprocess.h"
#include "nprocess.h"

static proclist_t *ProcessList = NULL;
static char *Starting_directory = NULL;

int Verbose = 0;
char *Progname = "nprocess_svc";

int *
nprocessproc_servopts_1(servopts *sop, struct svc_req *rqstp)
{
	static int status = 0;
	char *curdir = NULL;

	if ( Verbose > 1 ) {
		printf( "%s: nprocessproc_servopts_1\n", Progname );
	}
	assert( valid_addresses( (caddr_t)sop, sizeof(struct servopts) ) );
	Verbose = sop->so_verbose ? 1 : 0;
	if ( !Starting_directory &&
		!(Starting_directory = getcwd( NULL, MAXPATHLEN )) ) {
			status = -1;
			if ( Verbose ) {
				(void)fprintf( stderr, "%s: unable to get current directory: ",
					Progname );
				perror( "getcwd" );
			}
	} else if ( sop->so_directory && (strcmp( sop->so_directory, "." ) != 0) ) {
		if ( (status = chdir( sop->so_directory )) && Verbose ) {
			(void)fprintf( stderr, "%s: unable to change directory: ",
				Progname );
			perror( sop->so_directory );
		} else if ( Verbose ) {
			printf( "%s: nprocessproc_servopts_1:\n", Progname );
			printf( "\t          Verbose = %d\n", Verbose );
			printf( "\t              dir = %s\n", sop->so_directory );
		}
	} else if ( Verbose ) {
		curdir = getcwd( NULL, MAXPATHLEN );
		printf( "%s: nprocessproc_servopts_1:\n", Progname );
		printf( "\t          Verbose = %d\n", Verbose );
		printf( "\t              dir = %s\n", curdir );
		free( curdir );
	}
	if ( Verbose > 1 ) {
		printf( "%s: nprocessproc_servopts_1: return %d\n", Progname, status );
	}
	return( &status );
}

static void
sigchld_handler(int sig, int code, struct sigcontext *sc)
{
	int status;

	status = reap_processes(ProcessList);
	if (status != 0) {
		perror("reaping process");
	}
}

int *
nprocessproc_runprocs_1(int *procsp, struct svc_req *rqstp)
{
	proclist_t *plp;
	static int status = 0;
	pid_t pid;

	if (Verbose) {
		printf( "%s: nprocessproc_runprocs_1: starting %d processes\n",
			Progname, *procsp );
	}
	(void)signal(SIGCHLD, sigchld_handler);
	while ((*procsp)--) {
		if ( (pid = newprocess(0, &ProcessList)) == -1 ) {
			status = errno;
			for (plp = ProcessList; plp; plp = plp->pl_next) {
				if (kill(plp->pl_pid, SIGINT) == -1) {
					fprintf(stderr, "%s: kill(%d, SIGINT): %s\n", Progname,
						(int)plp->pl_pid, strerror(errno));
				}
			}
			break;
		}
	}
	return(&status);
}

int *
nprocessproc_killprocs_1(void *argp, struct svc_req *rqstp)
{
	proclist_t *plp;
	static int status = 0;
	int waitstat;
	pid_t pid;

	if (Verbose) {
		printf( "%s: nprocessproc_killprocs_1: killing processes\n", Progname );
	}
	if (ProcessList) {
		/*
		 * kill all of the processes
		 */
		for (plp = ProcessList; plp; plp = plp->pl_next) {
			if (kill(plp->pl_pid, SIGINT) == -1) {
				fprintf(stderr, "%s: kill(%d, SIGINT): %s\n", Progname,
					(int)plp->pl_pid, strerror(errno));
			}
		}
		/*
		 * collect process status and messages
		 */
		while ((status = reap_processes(ProcessList)) == 0) ;
		if (status == ECHILD) {
			status = 0;
		}
	}
	return(&status);
}

/* ARGSUSED */
int *
nprocessproc_reset_1( void *argp, struct svc_req *rqstp )
{
	proclist_t *plp;
	static int status = 0;

	if ( !Starting_directory ) {
		if ( !(Starting_directory = getcwd( NULL, MAXPATHLEN )) ) {
			status = -1;
			if ( Verbose ) {
				(void)fprintf( stderr, "%s: unable to get current directory: ",
					Progname );
				perror( "getcwd" );
			}
		} else if ( Verbose ) {
			printf( "%s: nprocessproc_reset_1:\n", Progname );
			printf( "\tdir = %s\n", Starting_directory );
		}
	} else if ( status = chdir( Starting_directory ) ) {
		if ( Verbose ) {
			(void)fprintf( stderr, "%s: unable to chdir back to %s: ",
				Progname, Starting_directory );
			perror( "chdir"  );
		}
		if ( errno ) {
			status = errno;
		} else {
			status = -1;
		}
	} else if ( Verbose ) {
		printf( "%s: nprocessproc_reset_1:\n", Progname );
		printf( "\tdir = %s\n", Starting_directory );
	}
	/*
	 * free the process list
	 */
	while (plp = ProcessList) {
		ProcessList = plp->pl_next;
		close(plp->pl_pipefd);
		free(plp);
	}
	ProcessList = NULL;
	Verbose = 0;
	return( &status );
}
