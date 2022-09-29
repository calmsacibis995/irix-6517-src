#define _RPCGEN_CLNT
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/errno.h>
#include <string.h>
#if !SVR4
#include <strings.h>
#endif /* !SVR4 */
#include <stdlib.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <rpc/rpc.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/file.h>
#if _IRIX
#include <mntent.h>
#endif /* _IRIX */
#if _SUNOS
#include <sys/mnttab.h>
#endif /* _SUNOS */
#include "nprocess.h"
#include "util.h"
#include "lockprocess.h"

#define OPTSTR			"vo:"
#define DEFAULT_PROCS	3

char *Progname = NULL;
int Verbose = 0;
int Quit = 0;
int Alarm = 0;

char *Options[] = {
	"procs",
#define OPT_PROCS		0
	"time",
#define OPT_TIME		1
	NULL
};

extern int errno;
extern char *optarg;
extern int optind;

CLIENT *
server_init( char *hostname, char *dirname )
{
	struct servopts sops;
	CLIENT *clnt = NULL;
	int status;
	int *replyp;

	assert( valid_addresses( (caddr_t)hostname, 1 ) &&
		valid_addresses( (caddr_t)dirname, 1 ) );
	if ( clnt = clnt_create( hostname, NPROCESSPROG, NPROCESSVERS_ONE,
		"tcp" ) ) {
			if (replyp = nprocessproc_reset_1(NULL, clnt)) {
				if ( status = *replyp ) {
					fprintf( stderr,
						"%s: server_init: server reset failed, status = %d\n",
						Progname );
					clnt_destroy( clnt );
					clnt = NULL;
					if ( status > 0 ) {
						errno = status;
						perror( "set_server_opts" );
					}
				} else {
					sops.so_verbose = Verbose;
					sops.so_directory = dirname;
					if (replyp = nprocessproc_servopts_1(&sops, clnt)) {
						if ( status = *replyp ) {
							clnt_destroy( clnt );
							clnt = NULL;
							fprintf( stderr,
								"%s: server_init: status = %d\n",
								Progname, status );
							if ( status > 0 ) {
								errno = status;
								perror( "set_server_opts" );
							}
						}
					} else {
						fprintf(stderr, "%s: unable to set server options\n",
							Progname);
						status = -1;
					}
				}
			} else {
				fprintf(stderr, "%s: unable to reset server\n", Progname);
				status = -1;
			}
	} else {
		clnt = NULL;
		fprintf( stderr, "%s: server_init: %s\n", Progname,
			clnt_spcreateerror( hostname ) );
	}
	return( clnt );
}

static void
signal_handler(int sig, int code, struct sigcontext *sc)
{
	switch (sig) {
		case SIGALRM:
			Alarm = 1;
			break;
		default:
			printf("signal_handler: stray signal %d code %d\n", sig, code);
		case SIGINT:
			Quit = 1;
	}
}

void
usage(void)
{
	(void)fprintf( stderr, "usage: %s [-v] [-o options] [hostname]\n",
		Progname );
	(void)fprintf( stderr, "available options: hostdir=<pathname>\n");
	(void)fprintf( stderr, "                   dir=<pathname>\n");
	(void)fprintf( stderr, "                   procs=<process count>\n");
}

/*
 * starting point
 */

int
main( int argc, char **argv )
{
	time_t clock;
	proclist_t *plist = NULL;
	proclist_t *plp = NULL;
	int *resp;
	unsigned seconds = 0;
	char *token;
	int *replyp;
	pid_t pid;
	int procs = DEFAULT_PROCS;
	int status = 0;
	char *hostname = NULL;
	struct mntent *mnt;
	pathstr localpath = NULL;
	pathstr remotepath = NULL;
	int opt;
	CLIENT *clnt;
	char *optval;
	char *optstr;

	Progname = *argv;
	while ( (opt = getopt( argc, argv, OPTSTR )) != EOF ) {
		switch ( opt ) {
			case 'o':
				optstr = optarg;
				while (*optstr) {
					switch (getsubopt(&optstr, Options, &optval)) {
						case OPT_PROCS:
							procs = atoi(optval);
							break;
						case OPT_TIME:
							if (token = strrchr(optval, ':')) {
								*token = '\0';
								token++;
								seconds = (unsigned)atoi(token);
								if (token = strrchr(optval, ':')) {
									*token = '\0';
									token++;
									seconds += ((unsigned)atoi(token) * 60) +
										((unsigned)atoi(optval) * 3600);
								} else {
									seconds += (unsigned)atoi(optval) * 60;
								}
							} else {
								seconds = (unsigned)atoi(optval);
							}
							break;
						default:
							fprintf(stderr, "Illegal option %s\n", optval);
							usage();
							exit(-1);
					}
				}
				break;
			case 'v':
				Verbose++;
				break;
			default:
				usage();
				exit( -1 );
		}
	}
	/*
	 * if the directory name was specified, chdir to it
	 */
	if ( optind < argc ) {
		localpath = argv[optind];
		optind++;
		if (chdir( localpath )) {
			status = errno;
			(void)fprintf( stderr, "%s: unable to change directory to %s: ",
				Progname, localpath );
			perror( localpath );
			return( status );
		}
	}
	/*
	 * convert the local path name to an absolue path
	 */
	localpath = absolute_path(NULL);
	assert(localpath);
	/*
	 * locate the mount point for the given absolute path
	 */
	mnt = find_mount_point(localpath);
	assert(mnt);
	/*
	 * given the file system name for the mount point, determine the
	 * host name
	 */
	remotepath = localpath;
	if (optind < argc) {
		get_host_info(argv[optind], &hostname, &remotepath);
	} else {
		get_host_info(mnt->mnt_fsname, &hostname, &remotepath);
	}
	assert(hostname);
	assert(remotepath);
	assert(!isfile(remotepath));
	assert(!isfile(localpath));
	if ( Verbose ) {
		printf( "%s: hostname = %s, localpath = %s, remotepath = %s\n",
			Progname, hostname, localpath, remotepath );
		printf( "%s: %d test processes\n", Progname, procs );
	}
	if (seconds) {
		printf( "%s: test duration will be %d seconds\n", Progname,
			seconds );
	} else {
		printf( "%s: test duration is indefinate\n", Progname );
	}
	(void)signal(SIGINT, signal_handler);
	(void)signal(SIGALRM, signal_handler);
	/*
	 * connect to the server
	 */
	if ( clnt = server_init( hostname, remotepath ) ) {
		/*
		 * connection established, start the processes on the server
		 */
		if (replyp = nprocessproc_runprocs_1(&procs, clnt)) {
			if (!(status = *replyp)) {
				/*
				 * processes successfully started on the server, now
				 * start them here
				 */
				if (procs == 1) {
					status = lockprocess(stdout, seconds);
				} else {
					while (procs--) {
						if ( (pid = newprocess(seconds, &plist)) == -1 ) {
							/*
							 * process startup error, kill any already started
							 */
							status = errno;
							while (plp = plist) {
								plist = plp->pl_next;
								if (kill(plp->pl_pid, SIGINT) == -1) {
									fprintf(stderr, "%s: kill(%d, SIGINT): %s",
										Progname, (int)plp->pl_pid,
										strerror(errno));
								}
								free(plp);
							}
							plist = NULL;
							/*
							 * kill the processes on the server
							 */
							resp = nprocessproc_killprocs_1(NULL, clnt);
							if (!resp) {
								fprintf( stderr,
									"%s: nprocessproc_killprocs_1 returned "
									"NULL\n", Progname );
							} else if (*resp) {
								fprintf(stderr,
									"%s: nprocessproc_killprocs_1: %s\n",
									Progname, strerror(*resp));
							}
							return(status);
						}
					}
					/*
					 * wait for a signal or a process to terminate
					 */
					if (seconds) {
						(void)alarm(seconds/4);
					}
					status = 0;
					while (!Quit && !status) {
						status = reap_processes(plist);
						if (Alarm) {
							Alarm = 0;
							clock = time(NULL);
							printf("%s\n", ctime(&clock));
							(void)signal(SIGALRM, signal_handler);
							(void)alarm(seconds/4);
						}
					}
					/*
					 * kill the server processes and the local processes
					 */
					resp = nprocessproc_killprocs_1(NULL, clnt);
					if (!resp) {
						status = -1;
						fprintf( stderr, "%s: nprocessproc_killprocs_1 "
							"returned NULL\n", Progname );
					} else if (status = *resp) {
						fprintf(stderr, "%s: nprocessproc_killprocs_1: %s\n",
							Progname, strerror(status));
					}
					/*
					 * kill any remaining local processes
					 */
					for (plp = plist; plp; plp = plp->pl_next) {
						if (kill(plp->pl_pid, SIGINT) == -1) {
							switch (errno) {
								case ESRCH:
									if (Verbose) {
										printf("kill(%d, SIGINT): %s\n",
											plp->pl_pid, strerror(errno));
									}
									break;
								default:
									fprintf(stderr, "%s: kill(%d, SIGINT): %s",
										Progname, (int)plp->pl_pid,
										strerror(errno));
							}
						} else if (Verbose) {
							printf("Process %d signalled (SIGINT)\n",
								plp->pl_pid);
						}
					}
					/*
					 * collect process status and messges
					 */
					while (reap_processes(plist) == 0) ;
				}
			} else {
				fprintf(stderr, "%s: nprocessproc_runprocs_1: %s\n",
					Progname, strerror(status));
			}
		} else {
			fprintf(stderr, "%s: nprocessproc_runprocs_1 returned NULL\n",
				Progname);
			status = -1;
		}
		if ( !status ) {
			printf( "%s: passed\n", Progname );
		} else {
			printf( "%s: failed\n", Progname );
		}
	} else {
		status = -1;
	}
	return( status );
}
