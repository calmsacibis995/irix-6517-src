/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)inetd.c	5.14 (Berkeley) 1/23/89";
#endif /* not lint */

/*
 * Inetd - Internet super-server
 *
 * This program invokes all internet services as needed.
 * connection-oriented services are invoked each time a
 * connection is made, by creating a process.  This process
 * is passed the connection as file descriptor 0 and is
 * expected to do a getpeername to find out the source host
 * and port.
 *
 * Datagram oriented services are invoked when a datagram
 * arrives; a process is created and passed a pending message
 * on file descriptor 0.  Datagram servers may either connect
 * to their peer, freeing up the original socket for inetd
 * to receive further messages on, or ``take over the socket'',
 * processing all arriving datagrams and, eventually, timing
 * out.	 The first type of server is said to be ``multi-threaded'';
 * the second type of server ``single-threaded''.
 *
 * Inetd uses a configuration file which is read at startup
 * and, possibly, at some later time in response to a hangup signal.
 * The configuration file is ``free format'' with fields given in the
 * order shown below.  Continuation lines for an entry must begin with
 * a space or tab.  All fields must be present in each entry.
 *
 *	service name			must be in /etc/services or must
 *					name a tcpmux or rpc service
 *	socket type			stream/dgram/raw/rdm/seqpacket
 *	protocol			must be in /etc/protocols unless
 *					rpc service
 *	wait/nowait			single-threaded/multi-threaded
 *	user				user to run daemon as
 *	server program			full path name
 *	server program arguments	maximum of MAXARGS (10+1)
 *
 * TCP services without official port numbers are handled with the
 * RFC1078-based tcpmux internal service. Tcpmux listens on port 1 for
 * requests. When a connection is made from a foreign host, the service
 * requested is passed to tcpmux, which looks it up in the servtab list
 * and returns the proper entry for the service. Tcpmux returns a
 * negative reply if the service doesn't exist, otherwise the invoked
 * server is expected to return the positive reply if the service type in
 * inetd.conf file has the prefix "tcpmux/". If the service type has the
 * prefix "tcpmux/+", tcpmux will return the positive reply for the
 * process; this is for compatibility with older server code, and also
 * allows you to invoke programs that use stdin/stdout without putting any
 * special server code in them. Services that use tcpmux are "nowait"
 * because they do not have a well-known port and hence cannot listen
 * for new requests.
 *
 * Services based on RPC also constitute a special case.  These services
 * need not be listed in /etc/services, because their port numbers are
 * dynamically bound by the portmapper.  The portmapper protocol specifies
 * a service with the triple (program number, version min, version max).
 * The format for RPC service entries begins as follows:
 *
 *	service-name/version-set	service-name must be in /etc/rpc
 *					version-set has the form 1-3,7,10
 *	socket type			dgram or stream
 *	rpc/protocol			protocol is udp or tcp
 *	etc.				as above
 *
 * Comment lines are indicated by a `#' in column 1.
 *
 * Inetd normally logs an error to the syslog if a server program
 * is missing.  Placing a `?' at the beginning of the server program
 * path (for example, ?/usr/etc/podd) will suppress the error message.
 *
 * The following options are supported when inetd runs on a kernel that
 * supports IP Security Options, and silently ignored otherwise:
 *
 * The string "/lc" (label-cognizant) after "wait" or "nowait" causes
 * inetd to invoke the server at the default process label and to pass a
 * wildcard socket to the server. Otherwise, the server is invoked with a
 * socket and process label corresponding with that of the client.
 *
 * The string "/rcv" after the user name causes inetd to invoke the server
 * with the uid of the client.  This is useful for servers that
 * communicate with Xsgi.
 */

#define _BSD_SIGNALS

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <rpc/rpc.h>		/* includes <netinet/in.h> */
#include <rpc/pmap_prot.h>
#include <arpa/inet.h>
#include <rpc/pmap_clnt.h>

#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <netdb.h>
#include <syslog.h>
#include <pwd.h>
#include <grp.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <net/soioctl.h>
#include <sys/mac_label.h>
#include <clearance.h>
#include <sys/types.h>
#include <sys/mac.h>
#include <sys/capability.h>
#include <t6net.h>
#include <cap_net.h>

#define	TOOMANY		2000		/* don't start more than TOOMANY */
#define	LISTEN_QLEN	255		/* Maximum length of list queue */
#define	CNT_INTVL	60		/* servers in CNT_INTVL sec. */
#define	RETRYTIME	(60*10)		/* retry after bind or server fail */

#define	SIGBLOCK	(sigmask(SIGCHLD)|sigmask(SIGHUP)|sigmask(SIGALRM)| \
			 sigmask(SIGPIPE))

#define TCPMUX_TIMEOUT	120

static int	default_qlen = LISTEN_QLEN;
static int	debug = 0;
static int	nsock, maxsock;
static fd_set	allsock;
static int	options;
static int	timingout;
static int	toomany = TOOMANY;

struct rpcvers {
	u_long		rv_low;		/* low version number */
	u_long		rv_high;	/* same as low or high if range */
	struct rpcvers	*rv_more;	/* more numbers or ranges */
};

static struct servtab {
	char	*se_service;		/* name of service */
	int	se_socktype;		/* type of socket to use */
	char	*se_proto;		/* protocol used */
	short	se_wait;		/* single threaded server */
	short	se_checked;		/* looked at during merge */
	char	*se_user;		/* user name to run as */
	struct	biltin *se_bi;		/* if built-in, description */
	char	*se_server;		/* server program */
	pid_t	se_pid;			/* daemon process id */
#define MAXARGV 11
	char	*se_argv[MAXARGV+1];	/* program arguments */
	int	se_fd;			/* open descriptor */
	int	se_type;		/* discriminant for se_un */
	union {
		struct	sockaddr_in ctrladdr;
		struct {
			u_long prog;	/* RPC program number */
			u_short prot;	/* IP protocol number */
			u_short port;	/* port in net byte order */
			struct rpcvers vers;
		} rpcinfo;
	} se_un;
	int	se_count;		/* number started since se_time */
	struct	timeval se_time;	/* start of se_count */
	struct	servtab *se_aliases;	/* head of alias list */
	struct	servtab *se_nextalias;	/* alias forward linkage */
	struct	servtab *se_next;
	int	se_flags;

	/* extended service attributes */
	cap_t	se_cap;
} *servtab;

#define se_ctrladdr	se_un.ctrladdr
#define se_rpcinfo	se_un.rpcinfo
#define	se_prog		se_rpcinfo.prog
#define	se_prot		se_rpcinfo.prot
#define	se_port		se_rpcinfo.port
#define	se_vers		se_rpcinfo.vers

#define NORM_TYPE	0
#define RPC_TYPE	1
#define MUX_TYPE	2
#define MUXPLUS_TYPE	3
#define ISRPC(sep)	((sep)->se_type == RPC_TYPE)
#define ISMUX(sep)	(((sep)->se_type == MUX_TYPE) || \
			 ((sep)->se_type == MUXPLUS_TYPE))
#define ISMUXPLUS(sep)	((sep)->se_type == MUXPLUS_TYPE)

/* se_flags: */
#define	SE_MISSING_OK	0x00000001	/* Don't log error if missing */
#define IS_MISSING_OK(sep) ((sep)->se_flags & SE_MISSING_OK)
#define	SE_LBLCOG	0x00000002	/* Leave socket label wildcard */
#define IS_LBLCOG(sep) ((sep)->se_flags & SE_LBLCOG)
#define	SE_RCVUID	0x00000008	/* Use received (client) uid */
#define IS_RCVUID(sep) ((sep)->se_flags & SE_RCVUID)

#define XE_NONE	0x000
#define XE_CAP	0x001

/* feature types */
static struct {
	char *name;
	int   type;
} xe_names[] = {
	{"cap", XE_CAP},
	{(char *) NULL, XE_NONE},
};

typedef struct xe_feature {
	int   xe_type;
	char *xe_value;
} xe_feature;

struct xe_spec {
	char *xe_service;		/* service name */
	xe_feature *xe_fset[MAXARGV+1];	/* feature type/value pairs */
};

struct conf_file {
	FILE	*fptr;
	char	*name;
	int	line_no;
} conf, xconf;

struct cl_opt {
	int	specified;
	char	*val;
};

#define SETCLOPT(x,v) { (x).specified = 1; (x).val =(v); }
#define INITCLOPT(x) { memset(&(x), 0, sizeof(struct cl_opt)); }
#define CLOPT(x) (x).specified
#define CLOPT_ARG(x) (x).val

static void print_service(char *, const struct servtab *);
static void setup(struct servtab *);
static void rpcsetup(struct servtab *);
static void dupaliasfd(struct servtab *, int);
static int  eqrpcvers(const struct rpcvers *, const struct rpcvers *);
static void close_sep(struct servtab *);
static void pmap_reset_aliases(struct servtab *, int);
static int  getrpcsock(struct servtab *);
static int  getrpcvers(char *, struct rpcvers *);
static void freerpcvers(struct servtab *);
static void setsrvcap(const struct servtab *);

/* parse CONFIG */
static void config(int);
static int setconfig(void);
static void endconfig(void);
static void freeconfig(struct servtab *);
static struct servtab *getconfigent(void);

/* parse XCONFIG */
static void xconfig(struct servtab *);

/* signal handlers */
static void reapchild(int);
static void sigpipe(int);
static void sigterm(int);
static void sig_ign(int);
static void retry(int);

static struct servtab *tcpmux(int);
static struct servtab *enter(struct servtab *);

/* builtin service functions */
static void echo_stream(int, struct servtab *);
static void echo_dg(int, struct servtab *);
static void discard_stream(int, struct servtab *);
static void discard_dg(int, struct servtab *);
static void machtime_stream(int, struct servtab *);
static void machtime_dg(int, struct servtab *);
static void daytime_stream(int, struct servtab *);
static void daytime_dg(int, struct servtab *);
static void chargen_stream(int, struct servtab *);
static void chargen_dg(int, struct servtab *);

/* internal utility functions */
static int check_loop(struct sockaddr_in *);
static int police_fork_rate(struct servtab *);
static void _w_syslog(int, char *, ...);
static void init_cfile(struct conf_file *, char *);

typedef void (*bi_fn_t)(int, struct servtab *);

static struct biltin {
	char	*bi_service;		/* internally provided service name */
	int	bi_socktype;		/* type of socket supported */
	short	bi_fork;		/* 1 if should fork before call */
	short	bi_wait;		/* 1 if should wait for child */
	bi_fn_t	bi_fn;			/* function which performs it */
} biltins[] = {
	/* Echo received data */
	"echo",		SOCK_STREAM,	1, 0,	echo_stream,
	"echo",		SOCK_DGRAM,	0, 0,	echo_dg,

	/* Internet /dev/null */
	"discard",	SOCK_STREAM,	1, 0,	discard_stream,
	"discard",	SOCK_DGRAM,	0, 0,	discard_dg,

	/* Return 32 bit time since 1900 */
	"time",		SOCK_STREAM,	0, 0,	machtime_stream,
	"time",		SOCK_DGRAM,	0, 0,	machtime_dg,

	/* Return human-readable time */
	"daytime",	SOCK_STREAM,	0, 0,	daytime_stream,
	"daytime",	SOCK_DGRAM,	0, 0,	daytime_dg,

	/* Familiar character generator */
	"chargen",	SOCK_STREAM,	1, 0,	chargen_stream,
	"chargen",	SOCK_DGRAM,	0, 0,	chargen_dg,

	"tcpmux",	SOCK_STREAM,	1, 0,	(bi_fn_t) tcpmux,

	0
};

static int havemac = 0;		/* kernel supports Mandatory Access Control */
static int havecipso = 0;	/* kernel supports IP secopts & socket ACL */
static int havecap = 0;		/* kernel supports Capabilities */

static int dryrun = 0;		/* are we in dry run mode */
static int dr_errors = 0;	/* number of error msgs counted in dry-run */
static char *progname = 0;	/* name of this program */

static void check_clearance(const struct clearance *, const struct servtab *);

static char *CONFIG = "/etc/inetd.conf";
static char *XCONFIG = "/etc/inetd.conf.sec";

extern char **environ;
static char timez[256] = { "TZ=" };
static char *env[] = {
	timez, NULL,
};

#define tmalloc(type)	((type *) emalloc(sizeof(type)))

static void *
emalloc(size_t size)
{
	void *new;

	new = malloc(size);
	if (new == 0) {
		_w_syslog(LOG_ERR, "Out of memory.");
		exit(1);
	}
	return new;
}

static char *
mystrdup(const char *cp)
{
	if (cp == NULL)
		cp = "";
	return strcpy((char *) emalloc(strlen(cp) + 1), cp);
}

/*
 * Check that we aren't looping.  If we are looping, logs this fact,
 * terminates the service and schedules a retry alarm.
 */
int police_fork_rate(register struct servtab *sep)
{
	if (sep->se_count++ == 0)
		(void) gettimeofday(&sep->se_time, NULL);
	else if (sep->se_count >= toomany) {
		struct timeval now;

		(void) gettimeofday(&now, NULL);
		if (now.tv_sec - sep->se_time.tv_sec >
		    CNT_INTVL) {
			sep->se_time = now;
			sep->se_count = 1;
		} else {
			_w_syslog(LOG_CRIT,
			"%s/%s server failing (looping), service terminated. "
			"Use -R to increase maximum rate if this is undesired",
				sep->se_service, sep->se_proto);
			close_sep(sep);
			sigsetmask(0L);
			if (!timingout) {
				timingout = 1;
				alarm(RETRYTIME);
			}
			return -1;
		}
	}
	return 0;
}

/*
 * _w_syslog is a wrapper for the syslog function.  In normal operation
 * it makes a syslog entry and in dry-run mode it reports an error to stderr.
 */
void _w_syslog(int pri, char *str, ...)
{
	va_list ap;

	va_start(ap, str);
	if(dryrun) {
		fprintf(stderr,"%s: ", progname);
		vfprintf(stderr, str, ap);
		fputc('\n', stderr);
		dr_errors++;
	} else {
		vsyslog(pri, str, ap);
	}
	va_end(ap);
}

static void init_cfile(struct conf_file *cf, char *name)
{
	memset(cf, 0, sizeof(struct conf_file));
	cf->name = name;
}

int
main(int argc, char *argv[])
{
	register struct servtab *sep, *next;
	register int tmp;
	struct sigvec sv;
	int opt, in_child;
	pid_t pid;
	char buf[50];
	char *tp;
	mac_t original_label;
	struct cl_opt opt_l, opt_R, opt_ERR;

	/*
	 * First record the options; how we handle the initialisation
	 * depends if we are running in dry-run mode or not.
	 */
	progname = *argv;
	opterr = 0;
	INITCLOPT(opt_l);
	INITCLOPT(opt_R);
	INITCLOPT(opt_ERR);
	while ((opt = getopt(argc, argv, "dfl:sR:X:")) != EOF)
		switch (opt) {
		case 'd':
			debug = 1;
#if 0
			options |= SO_DEBUG;
#endif
			break;
		case 'f':
			/*
			 * This option is no longer supported (it never
			 * worked in 6.5 anyway).
			 */
			_w_syslog(LOG_WARNING,
			  "usage: -f option no longer supported; ignoring.");
			break;
		case 'l':
			SETCLOPT(opt_l, optarg);
			break;
		case 's':
			dryrun = 1;
			break;
		case 'R':	/* invocation rate */
			SETCLOPT(opt_R, optarg);
			break;
		case 'X':	/* eXtended configuration */
			XCONFIG = optarg;
			break;
		default:
			SETCLOPT(opt_ERR, 0);
			break;
		}
	if(!dryrun) {
		openlog("inetd", LOG_PID | LOG_NOWAIT, LOG_DAEMON);
		if (getuid()) {
			fprintf(stderr, 
				"inetd: must be started by super-user\n");
			_w_syslog(LOG_ERR,
				  "usage: inetd must be started by super-user.");
			exit(1);
		}

		havemac = (sysconf(_SC_MAC) > 0);
		havecipso = (sysconf(_SC_IP_SECOPTS) > 0);
		havecap = (sysconf(_SC_CAP) > 0);

		if (havemac) {
			original_label = mac_get_proc ();
			if (original_label == NULL) {
				_w_syslog(LOG_ERR, 
					  "%s: mac_get_proc failed: %m");
				_exit(1);
			}
		}
	}
	/*
	 * Now process the options.  Do these after having read all
	 * the options so we know where to send any errors.
	 */
	if(CLOPT(opt_ERR)) {
		_w_syslog(LOG_ERR, 
			  "usage: inetd [-ds] [-l qlen] [-R rate] "
			  "[-X xconf-file] [conf-file]");
		exit(1);
	}
	if(CLOPT(opt_l)) {
		tmp = atoi(CLOPT_ARG(opt_l));
		if (tmp < 1) {
			_w_syslog(LOG_ERR,
				"-l %d: bad value for listen queue length",
				tmp);
		} else {
			default_qlen = tmp;
		}
	}
	if(CLOPT(opt_R)) {
		tmp = atoi(CLOPT_ARG(opt_R));
		if (tmp < 1)
			_w_syslog(LOG_ERR,
			 "-R %d: bad value for service invocation rate",
				tmp);
		else
			toomany = tmp;
	}
	argc -= optind;
	argv += optind;

	if (argc > 0)
		CONFIG = argv[0];

	init_cfile(&conf, CONFIG);
	init_cfile(&xconf, XCONFIG);

	/*
	 * In dry-run mode, just read and check the config file, then exit
	 */
	if(dryrun) {
		fprintf(stderr,"Checking \"%s\"\n", conf.name);
		config(0);
		fprintf(stderr,"%d error%s found\n", dr_errors, 
			dr_errors != 1 ? "s" : "");
		exit(dr_errors > 0 ? 1 : 0);
	}

	/* disable debugging if invoked from startup script */
	if (debug && !isatty(0))
		debug = 0;
	if (debug == 0) {
		if (_daemonize(0, -1, -1, -1) < 0)
			exit(1);
		/* _daemonize does a closelog() */
		openlog("inetd", LOG_PID | LOG_NOWAIT, LOG_DAEMON);
	}

	if ((tp = getenv("TZ")) != NULL)
		strncat(timez,tp,sizeof(timez)-3);
	else
		env[0] = NULL;
	environ = env;
	memset((void *) &sv, '\0', sizeof (sv));
	sv.sv_mask = SIGBLOCK;
	sv.sv_handler = retry;
	sigvec(SIGALRM, &sv, (struct sigvec *)0);
	config(0);
	sv.sv_handler = config;
	sigvec(SIGHUP, &sv, (struct sigvec *)0);
	sv.sv_handler = reapchild;
	sigvec(SIGCHLD, &sv, (struct sigvec *)0);
	sv.sv_handler = sigpipe;
	sigvec(SIGPIPE, &sv, (struct sigvec *)0);
	sv.sv_handler = sigterm;
	sigvec(SIGTERM, &sv, (struct sigvec *)0);
	/*
	 * Ignore any other signals that may kill the process
	 */
	sv.sv_handler = sig_ign;
	sigvec(SIGINT, &sv, (struct sigvec *)0);
	sigvec(SIGUSR1, &sv, (struct sigvec *)0);
	sigvec(SIGUSR2, &sv, (struct sigvec *)0);
	sigvec(SIGPOLL, &sv, (struct sigvec *)0);
	sigvec(SIGIO, &sv, (struct sigvec *)0);
	sigvec(SIGVTALRM, &sv, (struct sigvec *)0);
	sigvec(SIGPROF, &sv, (struct sigvec *)0);
	sigvec(SIGRTMIN, &sv, (struct sigvec *)0);
	sigvec(SIGRTMAX, &sv, (struct sigvec *)0);

	for (;;) {
	    int n, ctrl;
	    fd_set readable;

	    if (nsock == 0) {
		(void) sigblock(SIGBLOCK);
		while (nsock == 0)
		    sigpause(0L);
		(void) sigsetmask(0L);
	    }
	    readable = allsock;
	    if ((n = select(maxsock + 1, &readable, (fd_set *)0,
		(fd_set *)0, (struct timeval *)0)) <= 0) {
		    if (n < 0 && errno != EINTR){
			_w_syslog(LOG_WARNING, "select: %m\n");
			sginap(HZ);
		    }
		    else {
			/* Came here due to a signal while waiting on select
			 * Most often, it's due to SIGCHLD - Child process
			 * started by inetd created another one to do the
			 * work and dies. Allows inetd to start next instance
			 * of a "wait" type server. If inetd sleeps for
			 * a second in each such case, we end up allowing
			 * these servers to start at max rate of 1/sec.
			 * So sginap(1) should improve this rate.. But
			 * what about child death due to some server being
			 * removed ?? -> Let '-R' handle it.
			 */
			sginap(1);
		    }
		    continue;
	    }
	    for (sep = servtab; n && sep; sep = next) {
		struct servtab *alias;
		mac_t socket_label = NULL;
		uid_t socket_uid = -2;		/* make default uid invalid */
		cap_t ocap;
		int s;

		next = sep->se_next;
		if (sep->se_fd < 0 || !FD_ISSET(sep->se_fd, &readable))
			continue;
		n--;
		if (debug)
			fprintf(stderr, "someone wants %s on fd %d\n",
				sep->se_service, sep->se_fd);
		/*
		 * If we have a request for tcpmux, then fork now rather
		 * than later and set the in_child flag to rember that
		 * we are in the child process.
		 */
		in_child = 0;
		if (sep->se_socktype == SOCK_STREAM) {
			if (!sep->se_wait) {
				ctrl = accept(sep->se_fd,
					      (struct sockaddr *) NULL,
					      (int *) NULL);
				if (debug)
					fprintf(stderr, "accept, ctrl %d\n",
						ctrl);
				if (ctrl == -1) {
					if (errno != EINTR)
						_w_syslog(LOG_WARNING,
						       "accept: %m");
					continue;
				}

				/*
				 * Call tcpmux to find the real service to
				 * exec.
				 */
				if (sep->se_bi &&
				    sep->se_bi->bi_fn == (bi_fn_t) tcpmux) {
					/*
					 * Fork now rather than later to
					 * avoid having the inetd parent
					 * process block here.
					 */
					(void) sigblock(SIGBLOCK);
					if(police_fork_rate(sep) < 0) {
						(void)close(ctrl);
						continue;
					}
					if(pid = fork()) {
						if(pid > 0)
							close(ctrl);
						goto tcpmux_fork;
					}
					/* in child */
					in_child = 1;
					sep = tcpmux(ctrl);
					if (sep == NULL) {
						close(ctrl);
						exit(1);
					}
				}
			} else {
				ctrl = sep->se_fd;
			}
		} else {		/* SOCK_DGRAM */
			ctrl = sep->se_fd;
		}
		if (!IS_LBLCOG(sep)) {
			s = tsix_get_uid (ctrl, &socket_uid);
			if (s != -1)
				s = tsix_get_mac (ctrl, &socket_label);
			if (s == -1) {
				_w_syslog(LOG_ERR,
				       "%s/%s: cipso setup failed: %m",
				       sep->se_service, sep->se_proto);
				(void)close(sep->se_fd);
				if (ctrl != sep->se_fd)
					(void)close(ctrl);
				FD_CLR(sep->se_fd, &allsock);
				nsock--;
				sep->se_fd = -1;
				continue;
			}
		}
		/*
		 * Unless we already forked, fork now if necessary
		 */
		if(!in_child) {
			(void) sigblock(SIGBLOCK);
			pid = 0;
			if(sep->se_bi == 0 || sep->se_bi->bi_fork) {
				if(police_fork_rate(sep) < 0) {
					(void)close(ctrl);
					mac_free(socket_label);
					continue;
				}
				if(!(pid = fork()))
					in_child = 1;
			}
		}
    tcpmux_fork:
		if (pid < 0) {
			if (!sep->se_wait && sep->se_socktype == SOCK_STREAM)
				close(ctrl);
			sigsetmask(0L);
			sleep(1);
			mac_free(socket_label);
			continue;
		}
		if (pid && sep->se_wait) {
			alias = sep->se_aliases;
			if (alias == NULL) {
				sep->se_pid=pid;
				FD_CLR(sep->se_fd, &allsock);
				nsock--;
			} else {
				do {
					alias->se_pid = pid;
					FD_CLR(alias->se_fd, &allsock);
					nsock--;
					alias = alias->se_nextalias;
				} while (alias != NULL);
			}
		}
		sigsetmask(0L);
		if (pid == 0) {
			const cap_value_t cmrs = CAP_MAC_RELABEL_SUBJ;

			endpwent();
			if (debug && in_child)
				if (setsid() < 0)
					_w_syslog(LOG_ERR, "setsid failed: %m");

			/*
			 * MAC: set the label unless this server
			 * is label-cognizant (e.g. mountd).
			 */
			if (!IS_LBLCOG(sep) && havecipso && havemac) {
				ocap = cap_acquire (1, &cmrs);
				s = mac_set_proc (socket_label);
				cap_surrender (ocap);
				if (s == -1) {
					_w_syslog(LOG_ERR,
					       "%s/%s: mac_set_proc: %m",
					       sep->se_service, sep->se_proto);
					if (in_child) /* child */
						exit(1);
					if (ctrl != sep->se_fd)
						close(ctrl);
					mac_free(socket_label);
					continue;
				}
				s = tsix_set_solabel (ctrl, socket_label);
				if (s == -1) {
					_w_syslog(LOG_ERR,
					       "%s/%s: tsix_set_solabel: %m",
					       sep->se_service, sep->se_proto);
					if (in_child) /* child */
						exit(1);
					if (ctrl != sep->se_fd)
						close(ctrl);
					mac_free(socket_label);
					continue;
				}
				s = tsix_off(ctrl);
				if (s == -1) {
					_w_syslog(LOG_ERR,
					       "%s/%s: tsix_off: %m",
					       sep->se_service, sep->se_proto);
					if (in_child) /* child */
						exit(1);
					if (ctrl != sep->se_fd)
						close(ctrl);
					mac_free(socket_label);
					continue;
				}
			}

			if (sep->se_bi) {
				(*sep->se_bi->bi_fn)(ctrl, sep);
				if (!IS_LBLCOG(sep) && havecipso && havemac) {
					ocap = cap_acquire (1, &cmrs);
					s = mac_set_proc (original_label);
					cap_surrender (ocap);
					if (s == -1) {
						_w_syslog(LOG_ERR, "%s/%s: mac_set_proc: %m", sep->se_service, sep->se_proto);
						mac_free(original_label);
						exit(1);
					}
				}
			} else {
				int lastfd;
				struct clearance *clp;
				struct passwd *pwd;
				char *usename, audit_rationale[256];
				cap_value_t cap_audit_write = CAP_AUDIT_WRITE;

				if (debug) {
					fprintf(stderr, "%d execl %s\n",
						(int) getpid(),
						sep->se_server);
				}
				alias = sep->se_aliases;
				if (alias == NULL) {
					dup2(ctrl, 0);
					dup2(0, 1);
					dup2(0, 2);
					lastfd = 2;
				} else {
					for (lastfd = 0; ; lastfd++) {
						dupaliasfd(alias, lastfd);
						alias = alias->se_nextalias;
						if (alias == NULL)
							break;
					}
				}

				/*
				 * This server will be run with
				 * client's uid. (Used to restrict
				 * dgld's X server access.)
				 */
				if (IS_RCVUID(sep))
					pwd = getpwuid(socket_uid);
				else
					pwd = getpwnam(sep->se_user);
				if (pwd == NULL) {
					_w_syslog(LOG_ERR,
					       "%s/%s: %s: No such user",
					       sep->se_service, sep->se_proto,
					       sep->se_user);
					if (sep->se_socktype != SOCK_STREAM) {
						recv(0, buf, sizeof(buf), 0);
					}
					_exit(1);
				}
				usename = pwd->pw_name;

				/* 
				 * Check user clearance; audit the event.
				 */
				if (havemac) {
					clp = sgi_getclearancebyname (usename);
					if (clp == NULL) {
						_w_syslog(LOG_ERR,
			    "%s/%s: sgi_getclearancebyname: %s: No such user",
						       sep->se_service,
						       sep->se_proto, usename);
						if (sep->se_socktype != SOCK_STREAM)
							recv(0, buf,
							     sizeof (buf), 0);
						_exit(1);
					}
					check_clearance(clp, sep);
				}

				/*
				 * Audit
				 */
				sprintf(audit_rationale,
					"Remote %s service granted.", 
					sep->se_service);
				ocap = cap_acquire(1, &cap_audit_write);
				(void) ia_audit("inetd", usename, 1,
						audit_rationale);
				cap_surrender(ocap);

				if (pwd->pw_uid) {
					cap_value_t cap_setuid = CAP_SETUID;
					cap_value_t cap_setgid = CAP_SETGID;

					ocap = cap_acquire (1, &cap_setgid);
					if (setgid(pwd->pw_gid) == -1) {
						_w_syslog(LOG_ERR,
						 "%s: can't set gid %d: %m", 
						       sep->se_service, pwd->pw_gid);
						cap_surrender (ocap);
						_exit(1);
					}
					(void) initgroups(usename,
							  pwd->pw_gid);
					cap_surrender (ocap);
					ocap = cap_acquire (1, &cap_setuid);
					if (setuid(pwd->pw_uid) == -1) {
						_w_syslog(LOG_ERR,
					       "%s: can't set uid %d: %m", 
						       sep->se_service, 
						       pwd->pw_uid);
						cap_surrender (ocap);
						_exit(1);
					}
					cap_surrender (ocap);
				}

				/*
				 * Close all of the sockets opened by NIS to
				 * do the getpwnam()
				 */
				for (tmp = getdtablehi(); --tmp > lastfd; )
					(void)close(tmp);
				setsrvcap(sep);
				execv(sep->se_server, sep->se_argv);
				if (sep->se_socktype != SOCK_STREAM)
					recv(0, buf, sizeof (buf), 0);
				_w_syslog(LOG_ERR,
				       "cannot execute %s: %m",
				       sep->se_server);
				_exit(1);
			}
		}
		if (ctrl != sep->se_fd)	{
			close(ctrl);
		}
		if (pid > 0 && !IS_LBLCOG(sep) && havecipso && havemac &&
		    (sep->se_wait || sep->se_socktype == SOCK_DGRAM)) {
			close(sep->se_fd);
			FD_CLR(sep->se_fd, &allsock);
			nsock--;
			sep->se_fd = -1;
			if (ISRPC(sep)) 
				rpcsetup(sep);
			else
				setup(sep);
		}
		mac_free(socket_label);
	    }
	}
}

static void
dupaliasfd(struct servtab *alias, int tofd)
{
	struct servtab *sep;

	if (alias->se_fd == tofd)
		return;
	for (sep = alias->se_aliases; sep != NULL; sep = sep->se_nextalias)
		if (sep != alias && sep->se_fd == tofd) {
			sep->se_fd = dup(tofd);
			if (sep->se_fd < 0)
				_w_syslog(LOG_ERR, "%s: %m", sep->se_server);
		}
	dup2(alias->se_fd, tofd);
	alias->se_fd = tofd;
}

/* ARGSUSED */
static void
reapchild(int sig)
{
	int status;
	pid_t pid;
	register struct servtab *sep;

	for (;;) {
		pid = waitpid((pid_t) -1, &status, WNOHANG);
		if (pid <= 0)
			break;
		if (debug)
			fprintf(stderr, "%d reaped, status %#x\n", 
				(int) pid, (int) WEXITSTATUS(status));
		for (sep = servtab; sep; sep = sep->se_next)
			if (sep->se_pid == pid) {
				if (WIFEXITED(status) && WEXITSTATUS(status))
					_w_syslog(LOG_WARNING,
					       "%s: exit status 0x%x",
					       sep->se_server,
					       (int) WEXITSTATUS(status));
				else if (WIFSIGNALED(status))
					_w_syslog(LOG_WARNING,
					       "%s: exit signal 0x%x",
					       sep->se_server,
					       (int) WTERMSIG(status));
				if (debug)
					fprintf(stderr, "restored %s, fd %d\n",
					    sep->se_service, sep->se_fd);
				if (sep->se_fd >= 0) {
					FD_SET(sep->se_fd, &allsock);
					nsock++;
				}
				sep->se_pid = 1;
			}
	}
}

/*
 * TCP Port-scanning inetd casued it to die mysteriously (see bug 627767)
 * This was due to error messages being written to phantom connections
 * causing the server to receive a SIGPIPE.  Here we capture the SIGPIPE
 * to avoid this problem. (sm@engr)
 */
void sigpipe(int sig)
{
        syslog(LOG_ERR,"received SIGPIPE signal (possible port-scan attack)\n");
}

void sigterm(int sig)
{
	syslog(LOG_DAEMON, "inetd received SIGTERM; terminating.");
	exit(0);
}

/*
 * Dummy signal handler; does nothing.
 */
void sig_ign(int sig)
{
}

/*
 * Remove a servtab entry from its lists of aliases, if it's on any.
 * We call this before calling freeconfig, so that we don't end up
 * having pointers to free memory on alias lists.
 */
static void
removealias(struct servtab *alias)
{
	struct servtab *sp, **app;

	for (sp = servtab; sp; sp = sp->se_next) {
		app = &sp->se_aliases;

		while (*app) {
			if (*app == alias) {
				*app = (*app)->se_nextalias;
			}
			if (*app) {
				app = &(*app)->se_nextalias;
			}
		}
	}
}


static void
config(int sig)
{
	register struct servtab *sep, *sep_found, *cp, **sepp;
	long omask;
	struct servtab *alias;
	struct servent *sp;
	int svccnt = 0;
	int res;
#ifdef sgi
	char cbuffer[1024];
	extern int _pw_stayopen, _getpwent_no_shadow;
#endif

	if (sig == SIGHUP) {
		syslog(LOG_INFO, "received SIGHUP: reconfiguring");
	}
	if (!setconfig()) {
		_w_syslog(LOG_ERR, "%s: %m", CONFIG);
		return;
	}
#ifdef sgi
	setbuffer(conf.fptr, cbuffer, sizeof cbuffer);
	_pw_stayopen = _getpwent_no_shadow = 1;
#endif
	for (sep = servtab; sep; sep = sep->se_next)
		sep->se_checked = 0;
	while (cp = getconfigent()) {
		/* skip non-existent servers */
		if (cp->se_bi == 0 && access(cp->se_server, EX_OK) < 0) {
			if (errno != ENOENT || !IS_MISSING_OK(cp))
				_w_syslog(LOG_ERR, "%s: %m", cp->se_server);
			continue;
		}
		if  ( (getpwnam(cp->se_user) == NULL) && !IS_RCVUID(cp) ) {
			_w_syslog(LOG_ERR,
				"%s/%s: No such user '%s', service ignored",
				cp->se_service, cp->se_proto, cp->se_user);
			continue;
		}
		alias = 0;
		for (sep = servtab; sep; sep = sep->se_next) {
			if (strcmp(sep->se_server, cp->se_server) != 0)
				continue;
			if (strcmp(sep->se_service, cp->se_service) == 0
			    && strcmp(sep->se_proto, cp->se_proto) == 0)
				break;
			if (sep->se_wait &&
			    strcmp(sep->se_user, cp->se_user) == 0)
				alias = sep;
		}
		sep_found = sep;
		if (sep != 0) {
			int i;

			omask = sigblock(SIGBLOCK);

			/*
			 * If the RPC service info has changed, close
			 * the existing socket.  Replace the version
			 * set with cp's set, which we hide from 
			 * freeconfig.
			 */
			if (ISRPC(sep) && sep->se_fd >= 0 &&
			    (sep->se_prog != cp->se_prog ||
			     !eqrpcvers(&sep->se_vers, &cp->se_vers))) {
				close_sep(sep);
				freerpcvers(sep);
				sep->se_rpcinfo = cp->se_rpcinfo;
				cp->se_vers.rv_more = NULL;
			}

			/*
			 * sep->se_wait may be holding the process id
			 * of a running daemon we are awaiting.  If so,
			 * don't lose it unless we are instructed now
			 * not to wait.
			 */
			if (cp->se_bi == 0) {
				sep->se_wait = cp->se_wait;
			}

#define SWAP(a, b) { char *c = a; a = b; b = c; }
			if (cp->se_user)
				SWAP(sep->se_user, cp->se_user);
			if (cp->se_server)
				SWAP(sep->se_server, cp->se_server);
			for (i = 0; i < MAXARGV; i++)
				SWAP(sep->se_argv[i], cp->se_argv[i]);
			sigsetmask(omask);
			/*
			 * We don't have to call removealias here,
			 * because there's no way that cp can be on an
			 * alias list.  We just modified sep, which
			 * will stay on any list it's already on.
			 */
			freeconfig(cp);
			if (debug)
				print_service("REDO", sep);
		} else {
			sep = enter(cp);
			if (alias) {
			    /*
			     * Sort sep into the alias list in canonical
			     * service and protocol order (reuse cp).
			     */
			    if (alias->se_aliases == NULL)
				    alias->se_aliases = alias;
			    for (sepp = &alias->se_aliases;
				 (cp = *sepp) != NULL &&
				 ((res = strcmp(cp->se_service,
					sep->se_service)) <= 0 ||
				 (res == 0 && strcmp(cp->se_proto,
					sep->se_proto) < 0));
				 sepp = &cp->se_nextalias)
				    continue;
			    sep->se_aliases = alias->se_aliases;
			    sep->se_nextalias = cp;
			    *sepp = sep;
			}
			if (debug)
				print_service("ADD ", sep);
		}
		sep->se_checked = 1;
		if (ISMUX(sep)) {
			sep->se_fd = -1;
			continue;
		}
		if (ISRPC(sep)) {
			/*
			 * Don't remap RPC services if they haven't changed.
			 */
			if (sep->se_fd < 0 && !dryrun)
				rpcsetup(sep);
			continue;
		}
		
		sp = getservbyname(sep->se_service, sep->se_proto);
		if (sp == 0) {
			_w_syslog(LOG_ERR, "%s/%s: unknown service",
			    sep->se_service, sep->se_proto);
			sep->se_checked = 0;
			continue;
		}
		if(!dryrun) {
			if(!sep_found) {
				/*
				 * Didn't find the service we wanted among 
				 * the list of existing services.  This could 
				 * be because the server field has changed, so 
				 * check for anything that is listening on 
				 * the port we are going to listen on and 
				 * close it (reuse cp for the search).
				 */
				for(sepp = &servtab; cp = *sepp; 
				    sepp = &cp->se_next) {
					if(!cp->se_checked && cp->se_fd >= 0 &&
				    	   cp->se_ctrladdr.sin_port == 
					   sp->s_port) {
						/*
						 * Close conflicting service
						 */
						*sepp = cp->se_next;
						close_sep(cp);
						if (debug)
						    print_service("FREE", cp);
						removealias(cp);
						freeconfig(cp);
						free((char *)cp);
						break;
					}
				}
			}
			if (sp->s_port != sep->se_ctrladdr.sin_port) {
				sep->se_ctrladdr.sin_port = sp->s_port;
				if (sep->se_fd >= 0)
					close_sep(sep);
			}
			if (sep->se_fd < 0)
				setup(sep);
		}
	}
	endconfig();
#ifdef sgi
	_pw_stayopen = 0;
	endpwent();
#endif
	/*
	 * Purge anything not looked at above.
	 */
	omask = sigblock(SIGBLOCK);
	sepp = &servtab;
	while (sep = *sepp) {
		if (sep->se_checked) {
			sepp = &sep->se_next;
			svccnt++;
			continue;
		}
		*sepp = sep->se_next;
		if (sep->se_fd >= 0) {
			close_sep(sep);
		}
		if (debug)
			print_service("FREE", sep);
		removealias(sep);
		freeconfig(sep);
		free((char *)sep);
	}
	(void) sigsetmask(omask);
	/*
	 * Add 2 fd's for accept and getpwnam calls.
	 */
	if (svccnt+2 >= getdtablesize())
		_w_syslog(LOG_ERR,
			  "too many services: open-file limit reached.");
	if(!dryrun)
		xconfig(servtab);
}

static int
eqrpcvers(const struct rpcvers *rv1,
	  const struct rpcvers *rv2)
{
	if (rv1->rv_low == rv2->rv_low && rv1->rv_high == rv2->rv_high) {
		if (rv1->rv_more && rv2->rv_more)
			return eqrpcvers(rv1->rv_more, rv2->rv_more);
		if (rv1->rv_more || rv2->rv_more)
			return 0;
		return 1;
	}
	return 0;
}

/* ARGSUSED */
static void
retry(int sig)
{
	register struct servtab *sep;

	timingout = 0;
	for (sep = servtab; sep; sep = sep->se_next) {
		if (sep->se_fd < 0) {
			if (ISRPC(sep))
				rpcsetup(sep);
			else
				setup(sep);
		}
	}
}

int
turnon( int fd, int opt)
{
	int on = 1;

	return (setsockopt(fd, SOL_SOCKET, opt, (char *) &on, sizeof on));
}

static void
setup(sep)
	register struct servtab *sep;
{
	if (sep->se_socktype == SOCK_STREAM || sep->se_socktype == SOCK_DGRAM)
		sep->se_fd = socket(AF_INET, sep->se_socktype, 0);
	else
		sep->se_fd = cap_socket(AF_INET, sep->se_socktype, 0);
	if (sep->se_fd < 0) {
		_w_syslog(LOG_ERR, "%s/%s: socket: %m",
		    sep->se_service, sep->se_proto);
		return;
	}

	if (tsix_on(sep->se_fd) == -1) {
		_w_syslog(LOG_ERR, "%s/%s: tsix_on: %m",
		       sep->se_service, sep->se_proto);
		return;
	}

	if (strcmp(sep->se_proto, "tcp") == 0 && debug &&
	    turnon(sep->se_fd, SO_DEBUG) == -1)
		_w_syslog(LOG_ERR, "setsockopt (SO_DEBUG): %m");
	if (turnon(sep->se_fd, SO_REUSEADDR) == -1)
		_w_syslog(LOG_ERR, "setsockopt (SO_REUSEADDR): %m");

	sep->se_ctrladdr.sin_family = AF_INET;
	if (cap_bind(sep->se_fd, (struct sockaddr *) &sep->se_ctrladdr,
		     sizeof (sep->se_ctrladdr)) < 0) {
		_w_syslog(LOG_ERR, "%s/%s: bind: %m",
		       sep->se_service, sep->se_proto);
		(void) close(sep->se_fd);
		sep->se_fd = -1;
		if (!timingout) {
			timingout = 1;
			alarm(RETRYTIME);
		}
		return;
	}
	if (sep->se_socktype == SOCK_STREAM)
		listen(sep->se_fd, default_qlen);
	FD_SET(sep->se_fd, &allsock);
	nsock++;
	if (sep->se_fd > maxsock)
		maxsock = sep->se_fd;
	if (debug) {
		fprintf(stderr, "registered %s on %d\n",
			sep->se_server, sep->se_fd);
	}
}

/*
 * Finish with a service and its socket.
 */
static void
close_sep(struct servtab *sep)
{
	if (sep->se_fd >= 0) {
		nsock--;
		FD_CLR(sep->se_fd, &allsock);
		(void) close(sep->se_fd);
		sep->se_fd = -1;
	}
	/*
	 * Don't keep the pid of this running daemon: when reapchild()
	 * reaps this pid, it would erroneously increment nsock.
	 */
	if (sep->se_pid > 1){
	  sep->se_pid=1;
	}
	sep->se_count = 0;
	if (ISRPC(sep)) {
		struct rpcvers *rvp;
		u_long vers;

		rvp = &sep->se_vers;
		do {
			for (vers = rvp->rv_low; vers <= rvp->rv_high; vers++)
				pmap_unset(sep->se_prog, vers);
			rvp = rvp->rv_more;
		} while (rvp);
		if (sep->se_aliases != NULL)
			pmap_reset_aliases(sep, 1);
	}
}

/*
 * Thanks to Sun's unorthogonal pmap_unset, which unsets all portmappings
 * for prog&vers, we must restore other protocols' mappings when unmapping
 * rpcsep's port.
 */
static void
pmap_reset_aliases(rpcsep, skipit)
	struct servtab *rpcsep;
	int skipit;
{
	struct servtab *sep;
	struct rpcvers *rvp;
	u_long vers;

	for (sep = rpcsep->se_aliases; sep; sep = sep->se_nextalias) {
		/* Restore just the other services' mappings, not rcpsep's */
		if ((skipit && sep == rpcsep) || (sep->se_checked == 0))
			continue;

		rvp = &sep->se_vers;
		do {
			for (vers = rvp->rv_low; vers <= rvp->rv_high; vers++) {
				pmap_set(sep->se_prog, vers, sep->se_prot,
					 sep->se_port);
			}
			rvp = rvp->rv_more;
		} while (rvp);
	}
}

static void
rpcsetup(sep)
	register struct servtab *sep;
{
	register int rpcsock;

	if (sep->se_fd >= 0)
		close_sep(sep);
	rpcsock = getrpcsock(sep);
	if (rpcsock < 0) {
		_w_syslog(LOG_ERR, "rpc/%s socket creation problem: %m",
		    sep->se_proto);
		sep->se_checked = 0;
		return;
	}
	sep->se_fd = rpcsock;
	FD_SET(rpcsock, &allsock);
	nsock++;
	if (rpcsock > maxsock)
		maxsock = rpcsock;
	if (debug) {
		fprintf(stderr, "registered %s on %d\n",
			sep->se_server, rpcsock);
	}
}

static int
getrpcsock(struct servtab *sep)
{
	int s, len;
	struct sockaddr_in addr;
	struct rpcvers *rvp;
	int registrations = 0;
	int port;

	if (sep->se_socktype == SOCK_STREAM || sep->se_socktype == SOCK_DGRAM)
		s = socket(AF_INET, sep->se_socktype, 0);
	else
		s = cap_socket(AF_INET, sep->se_socktype, 0);
	if (s < 0) {
			_w_syslog(LOG_ERR, "socket create failed: type %d",
				sep->se_socktype);
		return -1;
	}

	if (tsix_on(s) == -1) {
		_w_syslog(LOG_ERR, "%s/%s: tsix_on: %m",
		       sep->se_service, sep->se_proto);
		return -1;
	}

	addr.sin_family = AF_INET;
	addr.sin_port = 0;
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(s, &addr, sizeof(addr)) < 0) {
		_w_syslog(LOG_ERR, "bind failed");
		(void)close(s);
		return -1;
	}

	len = sizeof(struct sockaddr_in);
	if (getsockname(s, &addr, &len) != 0) {
		_w_syslog(LOG_ERR, "getsockname failed");
		(void)close(s);
		return -1;
	}
	sep->se_prot = (sep->se_socktype == SOCK_DGRAM) ? IPPROTO_UDP
			: IPPROTO_TCP;
	sep->se_port = htons(addr.sin_port);
	rvp = &sep->se_vers;
	do {
		register u_long vers;

		for (vers = rvp->rv_low; vers <= rvp->rv_high; vers++) {
			if (debug) {
				fprintf(stderr, "%s: %d/%d/%d port = %d\n",
					sep->se_service,
					sep->se_prog, vers,
					sep->se_prot,
					sep->se_port);
			}
			if (!pmap_set(sep->se_prog, vers, sep->se_prot,
				      sep->se_port)) {
				if (pmap_unset(sep->se_prog, vers)) {
					if (pmap_set(sep->se_prog,
					    vers, sep->se_prot,
					    sep->se_port)) {
						registrations++;
					}
					if (sep->se_aliases) {
						pmap_reset_aliases(sep, 0);
					}
				}
			} else
				registrations++;
		}
		rvp = rvp->rv_more;
	} while (rvp);

	if (registrations == 0) {
		/*
		 * all attempts to register this service failed!
		 * forget this port number
		 */
		_w_syslog(LOG_ERR, "%s/%s: failed to register service",
			sep->se_service, sep->se_proto);

		sep->se_port = 0;
		close(s);
		return -1;
	}
skip_registration:
	if (sep->se_socktype == SOCK_STREAM)
		listen(s, default_qlen);
	return s;
}

static struct servtab *
enter(struct servtab *cp)
{
	register struct servtab *sep;
	long omask;

	sep = tmalloc(struct servtab);
	*sep = *cp;
	sep->se_fd = -1;
	omask = sigblock(SIGBLOCK);
	sep->se_next = servtab;
	servtab = sep;
	sigsetmask(omask);
	return (sep);
}

static struct servtab serv;
#define MAX_LINE_LEN	512
static char line[MAX_LINE_LEN];

static char *fskip(char **, struct conf_file *);
static char *p_skip(char **);
static char *skip(char **);
static char *nextline(struct conf_file *);

static int
setconfig(void)
{
	conf.line_no = 0;
	if (conf.fptr != NULL) {
		fseek(conf.fptr, 0L, L_SET);
		return (1);
	}
	conf.fptr = fopen(conf.name, "r");
	return (conf.fptr != NULL);
}

static void
endconfig(void)
{
	if (conf.fptr) {
		(void) fclose(conf.fptr);
		conf.fptr = NULL;
	}
}

static struct servtab *
getconfigent(void)
{
	register struct servtab *sep = &serv;
	char *cp, *arg, *usrp, *tmpstr, *usrnm;
	int argc;
	static char TCPMUX_TOKEN[] = "tcpmux/";

#define MUX_LEN		(sizeof(TCPMUX_TOKEN)-1)

more:
	while ((cp = nextline(&conf)) && 
		(*cp == '#' || *cp == '\0'))
		;
	if (cp == NULL)
		return ((struct servtab *)0);
	/*
	 * clear the static buffer, since some fields (se_ctrladdr,
	 * for example) don't get initialized here.
	 */
	memset((void *) sep, '\0', sizeof(*sep));
	if((arg = p_skip(&cp)) == NULL)
		goto more;
	if (cp == NULL) {
		/* got an empty line containing just blanks/tabs. */
		goto more;
	}

	if (strncmp(arg, TCPMUX_TOKEN, MUX_LEN) == 0) {
		char *c = arg + MUX_LEN;
		if (*c == '+') {
			sep->se_type = MUXPLUS_TYPE;
			c++;
		} else
			sep->se_type = MUX_TYPE;
		sep->se_service = mystrdup(c);
	} else {
		sep->se_service = mystrdup(arg);
		sep->se_type = NORM_TYPE;
	}

	if((arg = p_skip(&cp)) == NULL)
		goto more;
	if (strcmp(arg, "stream") == 0)
		sep->se_socktype = SOCK_STREAM;
	else if (strcmp(arg, "dgram") == 0)
		sep->se_socktype = SOCK_DGRAM;
	else if (strcmp(arg, "rdm") == 0)
		sep->se_socktype = SOCK_RDM;
	else if (strcmp(arg, "seqpacket") == 0)
		sep->se_socktype = SOCK_SEQPACKET;
	else if (strcmp(arg, "raw") == 0)
		sep->se_socktype = SOCK_RAW;
	else
		sep->se_socktype = -1;

	if((arg = p_skip(&cp)) == NULL)
		goto more;
	if (strncmp(arg, "rpc/", 4) != 0) {
		sep->se_proto = mystrdup(arg);
	} else {
		register struct rpcent *rp;

		sep->se_proto = mystrdup(arg + 4);
		sep->se_type = RPC_TYPE;
		arg = strchr(sep->se_service, '/');
		if (arg == NULL) {
			_w_syslog(LOG_ERR, "%s: missing rpc version for %s",
			       CONFIG, sep->se_service);
			goto more;
		}
		*arg++ = '\0';
		rp = getrpcbyname(sep->se_service);
		if (rp == NULL) {
			_w_syslog(LOG_ERR, "%s: unknown rpc service %s", CONFIG,
			       sep->se_service);
			goto more;
		}
		sep->se_prog = rp->r_number;
		if (!getrpcvers(arg, &sep->se_vers)) {
			_w_syslog(LOG_ERR, "%s: malformed rpc version in %s",
			       CONFIG, sep->se_service);
			goto more;
		}
	}

	if((arg = p_skip(&cp)) == NULL)
		goto more;
	if (strcmp(arg, "wait/lc") == 0 || strcmp(arg, "nowait/lc") == 0)
		sep->se_flags |= SE_LBLCOG;
	sep->se_wait = (strncmp(arg, "wait", 4) == 0);

	if (ISMUX(sep)) {
		/*
		 * Silently enforce "nowait" for TCPMUX services since
		 * they don't have an assigned port to listen on.
		 */
		sep->se_wait = 0;

		if (strcmp(sep->se_proto, "tcp")) {
			_w_syslog(LOG_ERR,
			       "%s: bad protocol for tcpmux service %s",
			       CONFIG, sep->se_service);
			goto more;
		}
		if (sep->se_socktype != SOCK_STREAM) {
			_w_syslog(LOG_ERR,
			       "%s: bad socket type for tcpmux service %s",
			       CONFIG, sep->se_service);
			goto more;
		}
	}

	if((usrnm = p_skip(&cp)) == NULL)
		goto more;
	sep->se_user = mystrdup(usrnm);
	if ((usrp = strstr(sep->se_user, "/rcv")) != 0) {
		*usrp = 0;
		if (havecipso)
			sep->se_flags |= SE_RCVUID;
	}
	if((tmpstr = p_skip(&cp)) == NULL)
		goto more;
	if (*tmpstr == '?') {
		sep->se_flags |= SE_MISSING_OK;
		sep->se_server = mystrdup(tmpstr+1);
	} else
		sep->se_server = mystrdup(tmpstr);
	if (strcmp(sep->se_server, "internal") == 0) {
		register struct biltin *bi;

		for (bi = biltins; bi->bi_service; bi++)
			if (bi->bi_socktype == sep->se_socktype &&
			    strcmp(bi->bi_service, sep->se_service) == 0)
				break;
		if (bi->bi_service == 0) {
			_w_syslog(LOG_ERR, "internal service %s unknown",
			       sep->se_service);
			goto more;
		}
		sep->se_bi = bi;
		sep->se_wait = bi->bi_wait;
	} else
		sep->se_bi = NULL;

	if (havecipso || havemac) {
		/* additional sanity checks */
		if (sep->se_socktype == SOCK_STREAM &&
		    strcmp(sep->se_proto, "tcp") ||
		    sep->se_socktype == SOCK_DGRAM &&
		    strcmp(sep->se_proto, "udp") ||
		    sep->se_type == RPC_TYPE && sep->se_bi) {
			_w_syslog(LOG_ERR,
			       "%s: bad configuration combination for service %s",
			       CONFIG, sep->se_service);
			goto more;
		}
	}

	argc = 0;
	for (arg = skip(&cp); cp; arg = skip(&cp))
		if (argc < MAXARGV)
			sep->se_argv[argc++] = mystrdup(arg);
	while (argc <= MAXARGV)
		sep->se_argv[argc++] = NULL;
	sep->se_aliases = sep->se_nextalias = NULL;

	/* extended service attributes */
	sep->se_cap = (havecap ? cap_init() : NULL);

	return (sep);
}

static int
setxconfig(void)
{
	if (xconf.fptr != NULL) {
		fseek(xconf.fptr, 0L, SEEK_SET);
		return (1);
	}
	xconf.fptr = fopen(xconf.name, "r");
	return (xconf.fptr != NULL);
}

static void
endxconfig(void)
{
	if (xconf.fptr) {
		(void) fclose(xconf.fptr);
		xconf.fptr = NULL;
	}
}

static int
getxent(struct xe_spec *xe)
{
	char *cp, *arg;
	int i = 0;

	xe->xe_service = NULL;
	xe->xe_fset[i] = NULL;

more:
	/* skip comments and empty lines */
	while ((cp = nextline(&xconf)) && (*cp == '#' || *cp == '\0'))
		;
	if (cp == NULL)
		return (0);

	arg = fskip(&cp, &xconf);
	if (cp == NULL)
		goto more;	/* empty line containing just blanks/tabs */

	/* copy service name */
	xe->xe_service = mystrdup(arg);
	if (debug)
		fprintf(stderr, "service name:  %s\n", xe->xe_service);

	/* copy each extended feature spec */
	while ((arg = fskip(&cp, &xconf)) != NULL && i < MAXARGV) {
		char *equ, *name, *value;
		int j;

		/* must have '=' in entry */
		if ((equ = strchr(arg, '=')) == NULL)
			continue;

		/* split into name/value pair */
		name = arg;
		*equ = '\0';
		value = equ + 1;

		/* find matching feature name */
		for (j = 0; xe_names[j].name != NULL; j++)
			if (strcmp(xe_names[j].name, name) == 0)
				break;

		/* if it matches, save it */
		if (xe_names[j].name != NULL) {
			xe->xe_fset[i] = tmalloc (struct xe_feature);
			xe->xe_fset[i]->xe_type = xe_names[j].type;
			xe->xe_fset[i]->xe_value = mystrdup(value);
			xe->xe_fset[++i] = NULL;
		}

		if (debug) {
			fprintf(stderr, "feature name:  %s\n", name);
			fprintf(stderr, "feature value: %s\n", value);
		}
	}

	return(1);
}

static void
xe_spec_free(struct xe_spec *xe)
{
	int i;

	free(xe->xe_service);
	for (i = 0; xe->xe_fset[i] != NULL; i++) {
		free(xe->xe_fset[i]->xe_value);
		free(xe->xe_fset[i]);
	}
}

static void
update_xent(struct servtab *sep, struct xe_spec *xe)
{
	int i;

	/* operate according to attribute type */
	for (i = 0; xe->xe_fset[i] != NULL; i++) {
		switch (xe->xe_fset[i]->xe_type) {
			case XE_CAP:
				if (!havecap)
					break;
				cap_free(sep->se_cap);
				sep->se_cap = cap_from_text(xe->xe_fset[i]->xe_value);
				if (sep->se_cap == NULL)
					_w_syslog(LOG_ERR, 
						"%s: invalid capability set", 
						xe->xe_fset[i]->xe_value);
				break;
		}
	}
}

static void
xconfig(struct servtab *tab)
{
	struct xe_spec xe;

	if (!setxconfig()) {
		if (debug)
			perror(XCONFIG);
		_w_syslog(LOG_ERR, "%s: %m", XCONFIG);
		return;
	}
	while (getxent(&xe)) {
		struct servtab *sep, *alias;

		/* find matching entry in service table */
		for (sep = tab; sep; sep = sep->se_next) {
			/* check primary service */
			if (strcmp(sep->se_service, xe.xe_service) == 0)
				update_xent(sep, &xe);

			/* check alias list */
			for (alias = sep->se_aliases; alias != NULL;
			     alias = alias->se_nextalias)
			{
				if (strcmp(alias->se_service,
					   xe.xe_service) == 0)
					update_xent(alias, &xe);
			}
		}
		xe_spec_free(&xe);
	}
	endxconfig();
}

static int
getrpcvers(char *buf,
	   struct rpcvers *rvp)
{
	char *cp;

	rvp->rv_low = strtoul(buf, &cp, 0);
	if (cp == buf)
		return 0;
	if (*cp == '-') {
		buf = cp + 1;
		rvp->rv_high = strtoul(buf, &cp, 0);
		if (cp == buf)
			return 0;
	} else
		rvp->rv_high = rvp->rv_low;
	if (*cp == ',') {
		rvp->rv_more = tmalloc(struct rpcvers);
		if (!getrpcvers(cp + 1, rvp->rv_more)) {
			free((char *) rvp->rv_more);
			return 0;
		}
		return 1;
	}
	rvp->rv_more = 0;
	return (*cp == '\0');
}

static void
freeconfig(cp)
	register struct servtab *cp;
{
	int i;

	if (cp->se_service && !ISRPC(cp))
		free(cp->se_service);
	if (cp->se_proto)
		free(cp->se_proto);
	if (cp->se_user)
		free(cp->se_user);
	if (cp->se_server)
		free(cp->se_server);
	if (cp->se_cap)
		cap_free(cp->se_cap);
	for (i = 0; i < MAXARGV; i++)
		if (cp->se_argv[i])
			free(cp->se_argv[i]);
	if (ISRPC(cp))
		freerpcvers(cp);
}

static void
freerpcvers(sep)
	struct servtab *sep;
{
	struct rpcvers *rvp, *more;

	for (rvp = sep->se_vers.rv_more; rvp; rvp = more) {
		more = rvp->rv_more;
		free((char *) rvp);
	}
}

/*
 * Safe skip - if skip returns null, log a syntax error in the
 * configuration file and exit.
 */
static char *
p_skip(char **cpp)
{
	char *cp = skip(cpp);

	if (cp == NULL) {
		_w_syslog(LOG_ERR, "%s: syntax error on line %d; ignoring line",
			CONFIG, conf.line_no);
	}
	return cp;
}

/*
 * fskip returns a pointer to the next symbol in the configuration
 * file *fp, or NULL if the end of the file has been reached.
 */
static char *
fskip(char **cpp, struct conf_file *fp)
{
	register char *cp = *cpp;
	char *start;

again:
	while (*cp == ' ' || *cp == '\t')
		cp++;
	if (*cp == '\0') {
		int c;

		c = getc(fp->fptr);
		(void) ungetc(c, fp->fptr);
		if (c == ' ' || c == '\t')
			if (cp = nextline(fp))
				goto again;
		*cpp = (char *)0;
		return ((char *)0);
	}
	start = cp;
	while (*cp && *cp != ' ' && *cp != '\t')
		cp++;
	if (*cp != '\0')
		*cp++ = '\0';
	*cpp = cp;
	return (start);
}

static char *
skip(char **cpp)
{
	return(fskip(cpp, &conf));
}

static char *
nextline(struct conf_file *flptr)
{
	char *cp;

	if (fgets(line, sizeof (line), flptr->fptr) == NULL)
		return ((char *)0);
	cp = strchr(line, '\n');
	if (cp)
		*cp = '\0';
	flptr->line_no++;
	return (line);
}

/*
 * Internet services provided internally by inetd:
 */
#define	BUFSIZE	(16 * 1024)

/* ARGSUSED */
static void
echo_stream(s, sep)		/* Echo service -- echo data back */
	int s;
	struct servtab *sep;
{
	char buffer[BUFSIZE];
	int i;

	while ((i = read(s, buffer, sizeof(buffer))) > 0 &&
	    write(s, buffer, i) > 0)
		;
	exit(0);
}

/* ARGSUSED */
static void
echo_dg(s, sep)			/* Echo service -- echo data back */
	int s;
	struct servtab *sep;
{
	char buffer[BUFSIZE];
	int i, size;
	struct sockaddr_in sin;

	size = sizeof(sin);
	if ((i = recvfrom(s, buffer, sizeof(buffer), 0, &sin, &size)) < 0)
		return;
	if (!check_loop(&sin))
		return;
	(void) sendto(s, buffer, i, 0, &sin, sizeof(sin));
}

/* ARGSUSED */
static void
discard_stream(s, sep)		/* Discard service -- ignore data */
	int s;
	struct servtab *sep;
{
	char buffer[BUFSIZE];
	errno = 0;

	while (1) {
		while (read(s, buffer, sizeof(buffer)) > 0)
			;
		if (errno != EINTR)
			break;
	}
	exit(0);
}

/* ARGSUSED */
static void
discard_dg(s, sep)		/* Discard service -- ignore data */
	int s;
	struct servtab *sep;
{
	char buffer[BUFSIZE];

	(void) read(s, buffer, sizeof(buffer));
}

#include <ctype.h>
#define LINESIZ 72
static char ring[128];
static char *endring;

static void
initring(void)
{
	register int i;

	endring = ring;

	for (i = 0; i <= 128; ++i)
		if (isprint(i))
			*endring++ = i;
}

/* ARGSUSED */
static void
chargen_stream(s, sep)		/* Character generator */
	int s;
	struct servtab *sep;
{
	register char *rs;
	int len;
	char text[LINESIZ+2];


	if (!endring) {
		initring();
		rs = ring;
	}

	text[LINESIZ] = '\r';
	text[LINESIZ + 1] = '\n';
	for (rs = ring;;) {
		if ((len = endring - rs) >= LINESIZ)
			memcpy(text, rs, LINESIZ);
		else {
			memcpy(text, rs, len);
			memcpy(text + len, ring, LINESIZ - len);
		}
		if (++rs == endring)
			rs = ring;
		if (write(s, text, sizeof(text)) != sizeof(text))
			break;
	}
	exit(0);
}

/* ARGSUSED */
static void
chargen_dg(s, sep)		/* Character generator */
	int s;
	struct servtab *sep;
{
	struct sockaddr_in sin;
	static char *rs;
	int len, size;
	char text[LINESIZ+2];

	if (endring == 0) {
		initring();
		rs = ring;
	}

	size = sizeof(sin);
	if (recvfrom(s, text, sizeof(text), 0, &sin, &size) < 0)
		return;

	if (!check_loop(&sin))
		return;

	if ((len = endring - rs) >= LINESIZ)
		memcpy(text, rs, LINESIZ);
	else {
		memcpy(text, rs, len);
		memcpy(text + len, ring, LINESIZ - len);
	}
	if (++rs == endring)
		rs = ring;
	text[LINESIZ] = '\r';
	text[LINESIZ + 1] = '\n';
	(void) sendto(s, text, sizeof(text), 0, &sin, sizeof(sin));
}

/*
 * Return a machine readable date and time, in the form of the
 * number of seconds since midnight, Jan 1, 1900.  Since gettimeofday
 * returns the number of seconds since midnight, Jan 1, 1970,
 * we must add 2208988800 seconds to this figure to make up for
 * some seventy years Bell Labs was asleep.
 */

static long
machtime(void)
{
	struct timeval tv;

	if (gettimeofday(&tv, 0) < 0) {
		if (debug)
			fprintf(stderr, "Unable to get time of day\n");
		return (0L);
	}
	return (htonl((long)tv.tv_sec + 2208988800));
}

/* ARGSUSED */
static void
machtime_stream(s, sep)
	int s;
	struct servtab *sep;
{
	long result;

	result = machtime();
	(void) write(s, (char *) &result, sizeof(result));
}

/* ARGSUSED */
static void
machtime_dg(s, sep)
	int s;
	struct servtab *sep;
{
	long result;
	struct sockaddr_in sin;
	int size;

	size = sizeof(sin);
	if (recvfrom(s, &result, sizeof(result), 0, &sin, &size) < 0)
		return;
	if (!check_loop(&sin))
		return;
	result = machtime();
	(void) sendto(s, &result, sizeof(result), 0, &sin, sizeof(sin));
}

/* ARGSUSED */
static void
daytime_stream(s, sep)		/* Return human-readable time of day */
	int s;
	struct servtab *sep;
{
	char buffer[256];
	time_t clock;

	clock = time(0);

	(void) sprintf(buffer, "%.24s\r\n", ctime(&clock));
	(void) write(s, buffer, strlen(buffer));
}

/* ARGSUSED */
static void
daytime_dg(int s,
	   struct servtab *sep)
{
	char buffer[256];
	time_t clock;
	struct sockaddr_in sin;
	int size;

	clock = time(0);

	size = sizeof(sin);
	if (recvfrom(s, buffer, sizeof(buffer), 0, &sin, &size) < 0)
		return;
	if (!check_loop(&sin))
		return;
	(void) sprintf(buffer, "%.24s\r\n", ctime(&clock));
	(void) sendto(s, buffer, strlen(buffer), 0, &sin, sizeof(sin));
}

/*
 * print_service:
 *	Dump relevant information to stderr
 */
static void
print_service(action, sep)
	char *action;
	const struct servtab *sep;
{
	fprintf(stderr,
		"%s: %s proto=%s, wait=%d, user=%s builtin=%x server=%s",
		action, sep->se_service, sep->se_proto, sep->se_wait,
		sep->se_user, (int) sep->se_bi, sep->se_server);
	if (sep->se_cap) {
		char *s = cap_to_text(sep->se_cap, (size_t *) NULL);
		if (s != NULL) {
			fprintf(stderr, ", cap=%s", s);
			cap_free(s);
		}
	}
	fprintf(stderr, "\n");
}

/*
 *  Based on TCPMUX.C by Mark K. Lottor November 1988
 *  sri-nic::ps:<mkl>tcpmux.c
 */


static int				/* # of characters upto \r,\n or \0 */
getline(int fd,
	char *buf,
	int len)
{
	int count = 0, n, sr;
	struct timeval timo;
	struct timeval gl_stop;
	fd_set fdr;

	do {
		FD_ZERO(&fdr);
		FD_SET(fd, &fdr);
		(void) gettimeofday(&gl_stop, NULL);
		gl_stop.tv_sec += TCPMUX_TIMEOUT;
		do {
			(void) gettimeofday(&timo, NULL);
			if(timo.tv_sec >= gl_stop.tv_sec)
				break;
			timo.tv_sec = gl_stop.tv_sec - timo.tv_sec;
			timo.tv_usec = 0;
			sr = select(fd+1, &fdr, (fd_set *)0, (fd_set *)0, 
				    &timo);
		} while(sr < 0 && errno == EINTR);
		if(sr <= 0)
			/*
			 * If sr == 0, we timed out.
			 */
			return 0;
		n = read(fd, buf, len-count);
		if (n == 0)
			return count;
		if (n < 0) {
			if (debug)
				fprintf(stderr,
					"getline: read: %s", strerror(errno));
			return (-1);
		}
		while (--n >= 0) {
			if (*buf == '\r' || *buf == '\n' || *buf == '\0')
				return count;
			count++;
			buf++;
		}
	} while (count < len);
	return (count);
}

#define MAX_SERV_LEN	(MAX_LINE_LEN+2)	/* 2 bytes for \r\n */

#define strwrite(fd, buf)	(void) write(fd, buf, sizeof(buf)-1)

static struct servtab *
tcpmux(int s)
{
	register struct servtab *sep;
	char service[MAX_SERV_LEN+1];
	int len;

	/* Get requested service name */
	if ((len = getline(s, service, MAX_SERV_LEN)) < 0) {
	    strwrite(s, "-Error reading service name\r\n");
	    return(NULL);
	}
	if(len == 0) {
		/*
		 * Read from connection failed due to error or timeout
		 */
		return NULL;
	}
	service[len] = '\0';

	if (debug)
	    fprintf(stderr, "tcpmux: someone wants %s\n", service);

	/*
	 * Help is a required command, and lists available services,
	 * one per line.
	 */
	if (!strcasecmp(service,"help")) {
	    for (sep = servtab; sep; sep = sep->se_next) {
		if (!ISMUX(sep))
		    continue;
		(void) write(s, sep->se_service, strlen(sep->se_service));
		strwrite(s, "\r\n");
	    }
	    return(NULL);
	}

	/* Try matching a service in inetd.conf with the request */
	for (sep = servtab; sep; sep = sep->se_next) {
	    if (!ISMUX(sep))
		continue;
	    if (!strcasecmp(service,sep->se_service)) {
		if (ISMUXPLUS(sep)) {
		    strwrite(s, "+Go\r\n");
		}
		return(sep);
	    }
	}
	strwrite(s, "-Service not available\r\n");
	return(NULL);
}


/* Check that an internal datagram service is not being abused in
 * an echo-loop
 */
static int				/* 0=dump it */
check_loop(struct sockaddr_in *sinp)
{
	/*
	 * Reject some likely abused ports.
	 */
	switch (sinp->sin_port) {
	case 7:				/* echo */
	case 11:			/* systat */
	case 13:			/* daytime */
	case 17:			/* qotd */
	case 19:			/* chargen */
	case 37:			/* time */
	case 53:			/* DNS */
		return 0;
	}

	/* Drop an occasional packet in case the other port is
	 * an unfamiliar echo port.
	 */
	if ((random() & 0x1f) == 0)
		return 0;
	return 1;
}


/* Check user clearance for current process label.  */
static void
check_clearance(const struct clearance *clp, const struct servtab *sep)
{
	const cap_value_t cap_audit_write = CAP_AUDIT_WRITE;
	cap_t ocap;
	mac_t proc_label = mac_get_proc ();

	if (mac_clearedlbl (clp, proc_label) != MAC_CLEARED) {
		char *labelname;

		labelname = mac_to_text (proc_label, (size_t *) NULL);
		mac_free (proc_label);
		_w_syslog (LOG_ERR | LOG_AUTH,
			"%s not cleared for %s at label %.40s",
			clp->cl_name, sep->se_service,
			labelname ? labelname : "** INVALID LABEL **");
		mac_free (labelname);
		ocap = cap_acquire(1, &cap_audit_write);
		(void) ia_audit ("inetd", clp->cl_name, 0,
				 "Not cleared for label.");
		cap_surrender(ocap);
		_exit (1);
	}
	mac_free (proc_label);
	ocap = cap_acquire(1, &cap_audit_write);
 	(void) ia_audit ("inetd", clp->cl_name, 1, "Cleared for label.");
	cap_surrender(ocap);
}

/*
 * Execute a server with an appropriate capability set.
 *
 * If a server has no capability set defined, it is
 * executed with an empty capability set.
 *
 * If inetd cannot set its capability set, it does not
 * execute the server.
 */
static void
setsrvcap (const struct servtab *sep)
{
	cap_t ocap;
	const cap_value_t cap_setpcap = CAP_SETPCAP;
	char buf[50];

	if (!havecap)
		return;
	ocap = cap_acquire (1, &cap_setpcap);
	if (cap_set_proc (sep->se_cap) == -1) {
		_w_syslog(LOG_ERR,
		       "cannot set capability state for %s: %m",
		       sep->se_server);
		cap_free(ocap);
		if (sep->se_socktype != SOCK_STREAM)
			recv(0, buf, sizeof (buf), 0);
		_exit(1);
	}
	cap_free(ocap);
}
