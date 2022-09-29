#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/syssgi.h>
#include <sys/procfs.h>
#include <signal.h>
#include <errno.h>
#include <netinet/in.h>
#include <syslog.h>
#include <alloca.h>
#include <ns_api.h>
#include <ns_daemon.h>

fd_set nsd_readset, nsd_exceptset, nsd_sendset;
long nsd_timeout = 60000;
int nsd_maxfd = 0;
int nsd_silent = TRUE;
int nsd_level = NSD_LOG_RESOURCE;
size_t __pagesize = 0;
static int nsd_mount = TRUE;
extern struct sockaddr_in _nfs_sin;

static char *nsd_program;
static int dump=0, reconfig=0;

extern int _ltoa(long, char *);

#ifdef sgi
static char *__nsd_version = "1.0 - SGI Irix";
#else
unsupported architecture
#endif

#ifndef NOUMOUNT
/*
** This routine is called on exit to unmount the .local namespace.
*/
static void
nsd_unmount(void)
{
	nsd_logprintf(NSD_LOG_OPER, "Exiting\n");
	if (execl("/sbin/umount", "umount", "/ns", (char *)0) < 0) {
		nsd_logprintf(NSD_LOG_CRIT,
		    "failed exec of umount for .local namespace: %s\n",
		    strerror(errno));
	}
}
#endif

static void
nsd_unlink_pidfile(void)
{
	char pidfile[MAXPATHLEN];

	nsd_cat(pidfile, MAXPATHLEN, 2, NS_HOME_DIR, "nsd.pid");
	unlink(pidfile);
}

static void
walk_tree(void)
{
	dump = 1;
}

static void
incr_debug(void)
{
	if (++nsd_level > NSD_LOG_MAX) {
		nsd_level = 0;
	}
	nsd_logprintf(NSD_LOG_OPER, "Log level set to %d\n",nsd_level);
}

static void
read_config(void)
{
	reconfig = 1;
}

/*
** This routine sets up the incomming command socket and the dispatch
** callback.  It also daemonizes the program and sets up the logging
** if needed.
*/
static void
nsd_initialize(void)
{
	int pid=0, fd, n, pfd[2];
	char port[16], pidfile[MAXPATHLEN], buf[16], *cp;
	char procfile[MAXPATHLEN];
	prpsinfo_t psi;
	struct sigaction sa;

	/* This is used enough that we want to cache the results. */
	__pagesize = getpagesize();

	/* 
	** Check to see if another nsd is running
	*/
	nsd_cat(pidfile, MAXPATHLEN, 2, NS_HOME_DIR, "nsd.pid");
	fd=open(pidfile,O_RDONLY,0);
	if (fd >= 0) {
		if (read(fd, buf, sizeof(buf)) < 0) {
			close(fd);
			nsd_logprintf(NSD_LOG_RESOURCE,
			    "Failed to read pid from %s: %s\n", pidfile,
			    strerror(errno));
			exit(errno);
		}
		close(fd);
		pid = strtol(buf, &cp, 10);
		if (cp != buf && pid != getpid()) {
			sprintf(procfile,"/proc/%09ld",pid);
			if ((fd = open(procfile, O_RDONLY)) != -1) {
				if (ioctl (fd, PIOCPSINFO, &psi) != -1) {
					if (! strcmp(nsd_program,
					    psi.pr_fname)) {
						nsd_logprintf(NSD_LOG_CRIT,
			    "%s pid %d is already running .. Aborting\n",
						    nsd_program,pid);
						exit(1); 
					}
				}
				close (fd);
			}
		}
	}
			


	if (nsd_silent) {
		if (pipe(pfd) < 0) {
			fprintf(stderr, "failed to open pipe\n");
			exit(EXIT_FAILURE);
		}

		switch(fork()) {
		    case -1:
			fprintf(stderr, "failed to daemonize\n");
			exit(EXIT_FAILURE);
		    case 0:
			break;
		    default:
			/*
			** wait for pipe to close, which indicates completion
			** of mount.
			*/
			close(pfd[1]);
			read(pfd[0], buf, sizeof(buf));
			exit(EXIT_SUCCESS);
		}

		/*
		** Detach.
		*/
		if (_daemonize(0, pfd[1], -1, -1) < 0) {
			fprintf(stderr, "failed to daemonize\n");
			exit(EXIT_FAILURE);
		}

		/*
		** put our core dumps somewhere better than /
		*/
		chdir(NS_HOME_DIR);

		/*
		** Setup syslog style logging.  Note that file descriptor 4
		** should be the next descriptor since we just daemonized and
		** pfd[1] would be higher.  We set the non-blocking flag for
		** syslog because it is important that nsd never block.
		*/
		openlog(nsd_program, LOG_PID|LOG_CONS|LOG_NOWAIT|LOG_NDELAY,
		    LOG_DAEMON);
		fcntl(4, F_SETFL, FNONBLK);
	}
	nsd_logprintf(NSD_LOG_OPER, "Starting version: %s %s %s\n",
	    __nsd_version, __DATE__, __TIME__);

	if (nsd_nfs_init() != NSD_OK) {
		nsd_logprintf(NSD_LOG_CRIT,
		    "Failed to initialize /ns server, exiting\n");
		exit(EXIT_FAILURE);
	}

	/*
	** We unmount the .local namespace on exit.  We have to catch the
	** signals that would normally result in an exit as these bypass the
	** usual exit handlers.  Note that killall defaults to SIGKILL so
	** will not be caught here.
	*/
	memset(&sa, 0, sizeof(sa));
	sa.sa_flags = SA_RESTART;

	/* Exit on TERM or INT */
        sa.sa_handler = exit;
        sigaction(SIGINT, &sa, NULL);
        sigaction(SIGTERM, &sa, NULL);

	/* Reread config on HUP */
        sa.sa_handler = read_config;
        sigaction(SIGHUP, &sa, NULL);

	/* Dump on USR1 */
        sa.sa_handler = walk_tree;
        sigaction(SIGUSR1, &sa, NULL);
        
	/* Change the debug level on USR2 */
        sa.sa_handler = incr_debug;
        sigaction(SIGUSR2, &sa, NULL);

	/* Ignore PIPE and CHLD */
	sa.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sa, NULL);
	sa.sa_flags |=  SA_NOCLDWAIT;
	sigaction(SIGCHLD, &sa, NULL);	
        
#ifndef NOUMOUNT
	if (nsd_mount && (atexit(nsd_unmount) != 0)) {
		nsd_logprintf(NSD_LOG_CRIT,
		    "failed to register exit handler\n");
	}
#endif

	/*
	** register our pid in the pid file, add an atexit handler to 
	** clean it up.
	*/
	fd=open(pidfile,O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (fd < 0) {
		nsd_logprintf(NSD_LOG_CRIT, "Failed to write pid file %s: %s\n", 
			      pidfile,strerror(errno));
		exit(errno);
	}
	n = _ltoa(getpid(), pidfile);
	pidfile[n++] = '\n';
	write(fd, pidfile, n);
	close(fd);
	atexit(nsd_unlink_pidfile);

	/*
	** Initialize callout lists.
	*/
	if (nsd_mount_init() != NSD_OK) {
		nsd_logprintf(NSD_LOG_CRIT,
		    "Failed to initialize /ns mountpoint");
		exit(EXIT_FAILURE);
	}

	/*
	** Attempt to mount this filesystem on the local host.
	*/
	if (nsd_mount == TRUE) {
		pid = fork();
		switch (pid) {
		    case -1:
			nsd_logprintf(NSD_LOG_CRIT,
			    "failed fork to mount .local namespace\n");
			break;
		    case 0:
			/* child */
			n = _ltoa(_nfs_sin.sin_port, port);
			port[n] = 0;

			nsd_logprintf(NSD_LOG_MIN,
			    "execl nsmount  -x -p %s\n",port);

			/* note that the file ID for .local is initially 1 */
			execl("/sbin/nsmount", "nsmount", "-x", "-p", port,
			    (char *)0);
			nsd_logprintf(NSD_LOG_CRIT, "failed to exec nsmount\n");
			exit(EXIT_FAILURE);
		    default:
			/* register a callback in case we core dump */
			syssgi(SGI_UNFS);
			break;
		}
	}

	if (nsd_silent) {
		/* pipe was just for passing into mount */
		close(pfd[1]);
	}
}

/*
** This routine just parses the arguments and uses these to set global
** variables.
*/
static void
nsd_parse_args(int argc, char **argv)
{
	int opt;
	extern char *optarg;
	char hostname[MAXNAMELEN];

	/*
	** Setup some global attributes.
	*/
	if (gethostname(hostname, MAXNAMELEN) < 0) {
		strcpy(hostname, "localhost");
	}
	nsd_attr_store((nsd_attr_t **)0, "hostname", hostname);

	/* timeout is the default cache file timeout */
	nsd_attr_store((nsd_attr_t **)0, "timeout", "300");

	/* SGI_MAC_FILE is a security thing for trusted Irix */
	if (sysconf(_SC_MAC) > 0) {
		nsd_attr_store((nsd_attr_t **)0, "SGI_MAC_FILE", "dblow");
	}

	/*
	** Loop through argument list.
	*/
	while ((opt = getopt(argc, argv, "a:l:nt:v")) != EOF) {
		switch (opt) {
		    case 'a':
			nsd_attr_parse((nsd_attr_t **)0, optarg, 0, (char *)0);
			break;
		    case 'l':
			nsd_level = atoi(optarg);
			break;
		    case 'n':
			nsd_mount = FALSE;
			break;
		    case 't':
			nsd_timeout = 1000 * atoi(optarg);
			break;
		    case 'v':
			nsd_silent = FALSE;
			break;
		    default:
			fprintf(stderr,
			    "usage: %s [-vn][-t timeout][-a key=value]\n",
			    nsd_program);
			exit(EXIT_FAILURE);
		}
	}
}

/*
** This routine is called from within the central select loop in
** main when a timeout has occurred.  In here we pop the timeout
** of the top of the list, and call the callback associated with
** it.  Then we spin down the callout list until we have an answer.
*/
static int
nsd_timeout_loop(void)
{
	nsd_times_t *tc;
	nsd_file_t *ca, *rq = 0;
	int status;

	/*
	** The select timed out, so one of the callouts must
	** have put something on the timeout queue.  We call
	** the timeout handler to deal with it.
	*/
	tc = nsd_timeout_next();
	if (tc && tc->t_proc) {
		status = (*tc->t_proc)(&rq, tc);
		free(tc);
		if (! rq) {
			return status;
		}
		ca = nsd_file_getcallout(rq);
		if (ca && (status != NSD_CONTINUE) &&
		    (ca->f_control[rq->f_status] & NSD_RETURN)) {
			/*
			** nsswitch.conf specifically told us to
			** return here.
			*/
			if (rq->f_send) {
				return (*rq->f_send)(rq);
			} else {
				nsd_file_clear(&rq);
			}
			return NSD_OK;
		}

		while (status == NSD_NEXT) {
			/*
			** This means that the method was called successfully,
			** but we did not get a complete bit of data.  We pop
			** the next method off the stack and execute that one.
			** If we had a tryagain result last time we use the
			** same callout again.
			*/
			if ((rq->f_status == NS_TRYAGAIN) &&
			    (rq->f_tries < 4)) {
				rq->f_tries++;
			} else {
				rq->f_tries = 0;
			}
			ca = nsd_file_nextcallout(rq);
			if (! ca) {
				/*
				** Tried every method and did not find
				** an answer.
				*/
				if (rq->f_send) {
					return (*rq->f_send)(rq);
				} else {
					nsd_file_clear(&rq);
				}
				return NSD_OK;
			}

			if (ca->f_lib && ca->f_lib->l_funcs[rq->f_index]) {
				nsd_attr_continue(&rq->f_attrs, ca->f_attrs);
				status = (*ca->f_lib->l_funcs[rq->f_index])(rq);
			} else {
				status = NSD_NEXT;
			}

			if ((status != NSD_CONTINUE) &&
			    ca->f_control[rq->f_status] & NSD_RETURN) {
				/*
				** nsswitch.conf specifically told us to
				** return here.
				*/
				if (rq->f_send) {
					return (*rq->f_send)(rq);
				} else {
					nsd_file_clear(&rq);
				}
				return NSD_OK;
			}
		}
		if (status == NSD_ERROR) {
			/*
			** Catastrophic failure.
			*/
			if (rq->f_send) {
				return (*rq->f_send)(rq);
			} else {
				nsd_file_clear(&rq);
			}
		}
		if (status == NSD_OK) {
			/*
			** found it.  The result gets placed in the
			** request structure.  Now we stuff it into the
			** cache if we were not specifically asked not
			** to, then we return the result, and free up
			** all the memory we have allocated.
			*/
			if (rq->f_send) {
				return (*rq->f_send)(rq);
			} else {
				nsd_file_clear(&rq);
			}
		}
		/*
		** Return type of NSD_CONTINUE falls through.  We assume that
		** the method setup a callback on some file descriptor to
		** finish the job.
		*/
	}

	return NSD_OK;
}

static int
nsd_callback_loop(int count, fd_set *readset, fd_set *exceptset,
    fd_set *sendset)
{
	nsd_file_t *ca, *rq;
	int i, status;
	nsd_callback_proc *bp;
	char *buf=0;

	/*
	** We have work to do.  We just call the callbacks
	** associated with the set file descriptors.
	*/
	for (i = 0; (i < nsd_maxfd) && (count > 0); i++) {
		if (FD_ISSET(i, readset) || FD_ISSET(i, exceptset) ||
		    FD_ISSET(i, sendset)) {
			bp = nsd_callback_get(i);
			if (!bp) {
				/*
				** We have no callback on this file
				** descriptor so we just read off the
				** waiting data and continue.
				*/
				if (! buf) {
					buf = alloca(1024);
				}
				read(i, buf, sizeof(buf));
				continue;
			}
			rq = (nsd_file_t *)0;
			count--;
			nsd_logprintf(NSD_LOG_LOW,
			    "incoming packet on %d\n", i);
			status = (*bp)(&rq, i);
			if (! rq) {
				continue;
			}
			ca = nsd_file_getcallout(rq);
			if ((status != NSD_CONTINUE) &&
			    ca && (ca->f_control[rq->f_status] & NSD_RETURN)) {
				/*
				** nsswitch.conf specifically told us
				** to return here.
				*/
				nsd_logprintf(NSD_LOG_LOW, "\tforced return\n");
				if (rq->f_send) {
					return (*rq->f_send)(rq);
				} else {
					nsd_file_clear(&rq);
				}
				status = NSD_CONTINUE;
				break;
			}

			while (status == NSD_NEXT) {
				/*
				** This means that the method was called
				** successfully, but nothing was found that
				** matched the key.  We pop the next method
				** off the stack and execute that one.
				*/
				if ((rq->f_status == NS_TRYAGAIN) &&
				    (rq->f_tries < 4)) {
					rq->f_tries++;
				} else {
					rq->f_tries = 0;
				}
				ca = nsd_file_nextcallout(rq);;
				if (! ca) {
					/*
					** Tried every method and did not
					** find an answer.
					*/
					if (rq->f_send) {
						return (*rq->f_send)(rq);
					} else {
						nsd_file_clear(&rq);
					}
					break;
				}

				if (ca->f_lib &&
				    ca->f_lib->l_funcs[rq->f_index]) {
					nsd_attr_continue(&rq->f_attrs,
					    ca->f_attrs);
					status =
					 (*ca->f_lib->l_funcs[rq->f_index])(rq);
				} else {
					status = NSD_NEXT;
				}

				if ((status != NSD_CONTINUE) &&
				    ca->f_control[rq->f_status] & NSD_RETURN) {
					/*
					** nsswitch.conf specifically told us
					** to return here.
					*/
					nsd_logprintf(NSD_LOG_LOW,
					    "\tforced return\n");
					if (rq->f_send) {
						return (*rq->f_send)(rq);
					} else {
						nsd_file_clear(&rq);
					}
					status = NSD_CONTINUE;
					break;
				}
			}
			if (status == NSD_ERROR) {
				/*
				** Catastrophic failure.
				*/
				if (rq->f_send) {
					return (*rq->f_send)(rq);
				} else {
					nsd_file_clear(&rq);
				}
			}
			if (status == NSD_OK) {
				/*
				** found it.  The result gets placed in the
				** request structure.  Now we stuff it into the
				** cache if we were not specifically asked not
				** to, then we return the result, and free up
				** all the memory we have allocated.
				*/
				if (rq->f_send) {
					return (*rq->f_send)(rq);
				} else {
					nsd_file_clear(&rq);
				}
			}
			/*
			** Return type of NSD_CONTINUE falls
			** through.  We assume that the
			** method setup a callback on some
			** file descriptor to finish the
			** job.
			*/
		}
	}

	return NSD_OK;
}

/*
** This is the top of the nsd daemon.  This will check for sufficient
** permissions, process the arguments, setup the request sockets and
** default callbacks, then fall into an eternal select loop.
**
** When new requests come in they are dispatched to callouts.
**
** When information comes in on other sockets this gets forwarded to
** a handler callback which should be setup by the callouts.
**
** Methods can also setup timeouts by appending an element to the
** timeout queue.  When the select times out we call the timeout
** handler for the first thing in the queue.
*/
int
main(int argc, char **argv)
{
	int count;
	struct timeval *timeout = 0;
	fd_set readset, expset, sendset;
	FILE *fp;
	char *dumpfile = "/var/tmp/nsd.dump";

	if (nsd_program = strrchr(argv[0], '/')) {
		nsd_program++;
	} else {
		nsd_program = argv[0];
	}
	nsd_attr_store((nsd_attr_t **)0, "program", nsd_program);

	if (getuid() != 0) {
		fprintf(stderr, "%s: must be root to execute\n", nsd_program);
		exit(EXIT_FAILURE);
	}

	nsd_parse_args(argc, argv);

	FD_ZERO(&nsd_readset);

	/*
	** The initialize routine will setup all of the sockets for incoming
	** requests, parse nsswitch.conf files, etc.
	*/
	nsd_initialize();
	
	for (;;) {
		readset = nsd_readset;
		expset = nsd_exceptset;
		sendset = nsd_sendset;
#ifdef DEBUG
               if (timeout) {
                       nsd_logprintf(NSD_LOG_MAX,
			   "maxfd: %d, tv_sec: %d, tv_usec: %d\n",
                           nsd_maxfd, timeout->tv_sec, timeout->tv_usec);
               } else {
                       nsd_logprintf(NSD_LOG_MAX, "maxfd: %d, timeout: %x\n",
			   nsd_maxfd, timeout);
               }
#endif
		count = select(nsd_maxfd, &readset, &sendset, &expset,
		    timeout);
		switch (count) {
		    case -1:
			/*
			** We got an error from select.  If this was from
			** a signal we just drop back into the select.  If
			** not then we exit.
			*/
			if (errno != EINTR) {
				nsd_logprintf(NSD_LOG_RESOURCE,
				    "error in select: %s\n", strerror(errno));
				exit(EXIT_FAILURE);
			}
			break;

		    case 0:
			nsd_timeout_loop();
			break;

		    default:
			nsd_callback_loop(count, &readset, &expset, &sendset);
		}

		if (dump) {
			dump = 0;
			fp = fopen(dumpfile, "w");
			if (fp) {
				nsd_dump(fp);
			} else {
				nsd_logprintf(NSD_LOG_OPER,
				    "failed to open dumpfile\n");
			}
			fclose(fp);
		}
		if (reconfig) {
			reconfig = 0;
			nsd_nsw_init();
			if (nsd_mount_init() != NSD_OK) {
				nsd_logprintf(NSD_LOG_CRIT,
				    "reinit failed, exiting.\n");
				exit(EXIT_FAILURE);
			}
			nsd_logprintf(NSD_LOG_OPER,
			    "rereading configuration files\n");
			nsd_nsw_init();
		}

		/*
		** We set the timeout to the value of the first element in
		** the timeout list.
		*/
		timeout = nsd_timeout_set();
	}
}
