#if 0
static char sccsid[] = 	"@(#)rpcinfo.c	1.2 90/07/23 4.1NFSSRC Copyr 1990 Sun Micro";
#endif

/*
 * Copyright (C) 1986, Sun Microsystems, Inc.
 */

/*
 * rpcinfo: ping a particular rpc program
 *     or dump the portmapper
 */

#include <rpc/rpc.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <rpc/pmap_prot.h>
#include <rpc/pmap_clnt.h>
#include <signal.h>
#include <ctype.h>
#include <stdlib.h>
#include <syslog.h>

#define MAXHOSTLEN 256

#define	MIN_VERS	((u_long) 0)
#define	MAX_VERS	((u_long) 4294967295L)


static void	udpping(/*u_short portflag, int argc, char **argv*/);
static void	tcpping(/*u_short portflag, int argc, char **argv*/);
static int	pstatus(/*CLIENT *client, u_long prognum, u_long vers*/);
static void	pmapdump(/*int argc, char **argv*/);
static bool_t	reply_proc(/*void *res, struct sockaddr_in *who*/);
static void	manycast(/*int argc, char **argv, int*/);
static void	deletereg(/* int argc, char **argv */) ;
static void	usage(/*void*/);
static u_long	getprognum(/*char *arg*/);
static u_long	getvers(/*char *arg*/);
static void	get_inet_address(/*struct sockaddr_in *addr, char *host*/);

/*
 * Functions to be performed.
 */
#define	NONE		0	/* no function */
#define	PMAPDUMP	1	/* dump portmapper registrations */
#define	TCPPING		2	/* ping TCP service */
#define	UDPPING		3	/* ping UDP service */
#define	BRDCST		4	/* ping broadcast UDP service */
#define DELETES		5	/* delete registration for the service */
#define	MULTICST	6	/* ping multicast UDP service */

int callit = 0;

int
main(argc, argv)
	int argc;
	char **argv;
{
	register int c;
	extern char *optarg;
	extern int optind;
	int errflg;
	int function;
	char *strptr;
	u_short portnum;

	function = NONE;
	portnum = 0;
	errflg = 0;
	while ((c = getopt(argc, argv, "Cptubmdn:")) != EOF) {
		switch (c) {

		case 'p':
			if (function != NONE)
				errflg = 1;
			else
				function = PMAPDUMP;
			break;

		case 't':
			if (function != NONE)
				errflg = 1;
			else
				function = TCPPING;
			break;

		case 'u':
			if (function != NONE)
				errflg = 1;
			else
				function = UDPPING;
			break;

		case 'C':
			callit = 1;
			break;

		case 'b':
		case 'm':
			if (function != NONE)
				errflg = 1;
			else
				function = (c == 'b') ? BRDCST : MULTICST;
			break;

		case 'n':
			portnum = (u_short) strtoul(optarg, &strptr, 10);
			if (strptr == optarg || *strptr != '\0') {
				fprintf(stderr, "rpcinfo: %s is illegal port number\n",
				    optarg);
				exit(1);
			}
			break;

		case 'd':			
			if (function != NONE)
				errflg = 1;
			else
				function = DELETES;
			break;
			
		case '?':
			errflg = 1;
		}
	}

	if (callit && (function != UDPPING)) {
		usage();
		exit(1);
	}

	if (errflg || function == NONE) {
		usage();
		return (1);
	}

	/*
	// Start up the logging functions for sysmon
	*/
	openlog("rpcinfo", LOG_PID, LOG_USER);

	switch (function) {

	case PMAPDUMP:
		if (portnum != 0) {
			usage();
			return (1);
		}
		pmapdump(argc - optind, argv + optind);
		break;

	case UDPPING:
		udpping(portnum, argc - optind, argv + optind);
		break;

	case TCPPING:
		tcpping(portnum, argc - optind, argv + optind);
		break;

	case BRDCST:
	case MULTICST:
		if (portnum != 0) {
			usage();
			return (1);
		}
		manycast(argc - optind, argv + optind, function == MULTICST);
		break;

	case DELETES:
		deletereg(argc - optind, argv + optind);
		break;
	}

	return (0);
}
		
static void
udpping(portnum, argc, argv)
	u_short portnum;
	int argc;
	char **argv;
{
	struct timeval to;
	struct sockaddr_in addr;
	enum clnt_stat rpc_stat;
	CLIENT *client;
	u_long prognum, vers, minvers, maxvers;
	int sock = RPC_ANYSOCK;
	struct rpc_err rpcerr;
	int failure;
    
	if (argc < 2 || argc > 3) {
		usage();
		exit(1);
	}
	prognum = getprognum(argv[1]);
	get_inet_address(&addr, argv[0]);
	/* Open the socket here so it will survive calls to clnt_destroy */
	sock = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0) {
		perror("rpcinfo: socket");
		exit(1);
	}
	failure = 0;
	if (argc == 2) {
		/*
		 * A call to version 0 should fail with a program/version
		 * mismatch, and give us the range of versions supported.
		 */
		addr.sin_port = htons(portnum);
		to.tv_sec = 5;
		to.tv_usec = 0;
		if ((client = clntudp_create(&addr, prognum, (u_long)0,
		    to, &sock)) == NULL) {
			clnt_pcreateerror("rpcinfo");
			printf("program %lu is not available\n",
			    prognum);
			exit(1);
		}
		to.tv_sec = 10;
		to.tv_usec = 0;
		rpc_stat = clnt_call(client, NULLPROC, xdr_void, (char *)NULL,
		    xdr_void, (char *)NULL, to);
		if (rpc_stat == RPC_PROGVERSMISMATCH) {
			clnt_geterr(client, &rpcerr);
			minvers = rpcerr.re_vers.low;
			maxvers = rpcerr.re_vers.high;
		} else if (rpc_stat == RPC_SUCCESS) {
			/*
			 * Oh dear, it DOES support version 0.
			 * Let's try version MAX_VERS.
			 */
			addr.sin_port = htons(portnum);
			to.tv_sec = 5;
			to.tv_usec = 0;
			if ((client = clntudp_create(&addr, prognum, MAX_VERS,
			    to, &sock)) == NULL) {
				clnt_pcreateerror("rpcinfo");
				printf("program %lu version %lu is not available\n",
				    prognum, MAX_VERS);
				exit(1);
			}
			to.tv_sec = 10;
			to.tv_usec = 0;
			rpc_stat = clnt_call(client, NULLPROC, xdr_void,
			    (char *)NULL, xdr_void, (char *)NULL, to);
			if (rpc_stat == RPC_PROGVERSMISMATCH) {
				clnt_geterr(client, &rpcerr);
				minvers = rpcerr.re_vers.low;
				maxvers = rpcerr.re_vers.high;
			} else if (rpc_stat == RPC_SUCCESS) {
				/*
				 * It also supports version MAX_VERS.
				 * Looks like we have a wise guy.
				 * OK, we give them information on all
				 * 4 billion versions they support...
				 */
				minvers = 0;
				maxvers = MAX_VERS;
			} else {
				(void) pstatus(client, prognum, MAX_VERS);
				exit(1);
			}
		} else {
			(void) pstatus(client, prognum, (u_long)0);
			exit(1);
		}
		clnt_destroy(client);
		for (vers = minvers; vers <= maxvers; vers++) {
			addr.sin_port = htons(portnum);
			to.tv_sec = 5;
			to.tv_usec = 0;
			if ((client = clntudp_create(&addr, prognum, vers,
			    to, &sock)) == NULL) {
				clnt_pcreateerror("rpcinfo");
				printf("program %lu version %lu is not available\n",
				    prognum, vers);
				exit(1);
			}
			to.tv_sec = 10;
			to.tv_usec = 0;
			rpc_stat = clnt_call(client, NULLPROC, xdr_void,
			    (char *)NULL, xdr_void, (char *)NULL, to);
			if (pstatus(client, prognum, vers) < 0)
				failure = 1;
			clnt_destroy(client);
		}
	}
	else {
		if (callit) {
			u_long port;
			vers = getvers(argv[2]);
			addr.sin_port = htons(portnum);
			to.tv_sec = 1;
			to.tv_usec = 0;
			rpc_stat = pmap_rmtcall(&addr, prognum, vers,
				NULLPROC, xdr_void, (char *)0, xdr_void,
				(char *)0, to, &port);
			if (rpc_stat != RPC_SUCCESS) {
				printf("program %lu version %lu is not available\n",
			    prognum, vers);
			    failure = 1;
			} else {
				printf("program %lu version %lu ready and waiting\n",
			    prognum, vers);
			}
		} else {
			vers = getvers(argv[2]);
			addr.sin_port = htons(portnum);
			to.tv_sec = 5;
			to.tv_usec = 0;
			if ((client = clntudp_create(&addr, prognum, vers,
			    to, &sock)) == NULL) {
				clnt_pcreateerror("rpcinfo");
				printf("program %lu version %lu is not available\n",
			    prognum, vers);
				exit(1);
			}
			to.tv_sec = 10;
			to.tv_usec = 0;
			rpc_stat = clnt_call(client, 0, xdr_void, (char *)NULL,
			    xdr_void, (char *)NULL, to);
			if (pstatus(client, prognum, vers) < 0)
				failure = 1;
		}
	}
	(void) close(sock); /* Close it up again */
	if (failure)
		exit(1);
}

static void
tcpping(portnum, argc, argv)
	u_short portnum;
	int argc;
	char **argv;
{
	struct timeval to;
	struct sockaddr_in addr;
	enum clnt_stat rpc_stat;
	CLIENT *client;
	u_long prognum, vers, minvers, maxvers;
	int sock = RPC_ANYSOCK;
	struct rpc_err rpcerr;
	int failure;

	if (argc < 2 || argc > 3) {
		usage();
		exit(1);
	}
	prognum = getprognum(argv[1]);
	get_inet_address(&addr, argv[0]);
	failure = 0;
	if (argc == 2) {
		/*
		 * A call to version 0 should fail with a program/version
		 * mismatch, and give us the range of versions supported.
		 */
		addr.sin_port = htons(portnum);
		if ((client = clnttcp_create(&addr, prognum, MIN_VERS,
		    &sock, 0, 0)) == NULL) {
			clnt_pcreateerror("rpcinfo");
			printf("program %lu is not available\n",
			    prognum);
			exit(1);
		}
		to.tv_sec = 10;
		to.tv_usec = 0;
		rpc_stat = clnt_call(client, NULLPROC, xdr_void, (char *)NULL,
		    xdr_void, (char *)NULL, to);
		if (rpc_stat == RPC_PROGVERSMISMATCH) {
			clnt_geterr(client, &rpcerr);
			minvers = rpcerr.re_vers.low;
			maxvers = rpcerr.re_vers.high;
		} else if (rpc_stat == RPC_SUCCESS) {
			/*
			 * Oh dear, it DOES support version 0.
			 * Let's try version MAX_VERS.
			 */
			addr.sin_port = htons(portnum);
			if ((client = clnttcp_create(&addr, prognum, MAX_VERS,
			    &sock, 0, 0)) == NULL) {
				clnt_pcreateerror("rpcinfo");
				printf("program %lu version %lu is not available\n",
				    prognum, MAX_VERS);
				exit(1);
			}
			to.tv_sec = 10;
			to.tv_usec = 0;
			rpc_stat = clnt_call(client, NULLPROC, xdr_void,
			    (char *)NULL, xdr_void, (char *)NULL, to);
			if (rpc_stat == RPC_PROGVERSMISMATCH) {
				clnt_geterr(client, &rpcerr);
				minvers = rpcerr.re_vers.low;
				maxvers = rpcerr.re_vers.high;
			} else if (rpc_stat == RPC_SUCCESS) {
				/*
				 * It also supports version MAX_VERS.
				 * Looks like we have a wise guy.
				 * OK, we give them information on all
				 * 4 billion versions they support...
				 */
				minvers = 0;
				maxvers = MAX_VERS;
			} else {
				(void) pstatus(client, prognum, MAX_VERS);
				exit(1);
			}
		} else {
			(void) pstatus(client, prognum, MIN_VERS);
			exit(1);
		}
		clnt_destroy(client);
		(void) close(sock);
		sock = RPC_ANYSOCK; /* Re-initialize it for later */
		for (vers = minvers; vers <= maxvers; vers++) {
			addr.sin_port = htons(portnum);
			if ((client = clnttcp_create(&addr, prognum, vers,
			    &sock, 0, 0)) == NULL) {
				clnt_pcreateerror("rpcinfo");
				printf("program %lu version %lu is not available\n",
				    prognum, vers);
				exit(1);
			}
			to.tv_usec = 0;
			to.tv_sec = 10;
			rpc_stat = clnt_call(client, 0, xdr_void, (char *)NULL,
			    xdr_void, (char *)NULL, to);
			if (pstatus(client, prognum, vers) < 0)
				failure = 1;
			clnt_destroy(client);
			(void) close(sock);
			sock = RPC_ANYSOCK;
		}
	}
	else {
		vers = getvers(argv[2]);
		addr.sin_port = htons(portnum);
		if ((client = clnttcp_create(&addr, prognum, vers, &sock,
		    0, 0)) == NULL) {
			clnt_pcreateerror("rpcinfo");
			printf("program %lu version %lu is not available\n",
			    prognum, vers);
			exit(1);
		}
		to.tv_usec = 0;
		to.tv_sec = 10;
		rpc_stat = clnt_call(client, 0, xdr_void, (char *)NULL,
		    xdr_void, (char *)NULL, to);
		if (pstatus(client, prognum, vers) < 0)
			failure = 1;
	}
	if (failure)
		exit(1);
}

/*
 * This routine should take a pointer to an "rpc_err" structure, rather than
 * a pointer to a CLIENT structure, but "clnt_perror" takes a pointer to
 * a CLIENT structure rather than a pointer to an "rpc_err" structure.
 * As such, we have to keep the CLIENT structure around in order to print
 * a good error message.
 */
static int
pstatus(client, prognum, vers)
	register CLIENT *client;
	u_long prognum;
	u_long vers;
{
	struct rpc_err rpcerr;

	clnt_geterr(client, &rpcerr);
	if (rpcerr.re_status != RPC_SUCCESS) {
		clnt_perror(client, "rpcinfo");
		printf("program %lu version %lu is not available\n",
		    prognum, vers);
		return (-1);
	} else {
		printf("program %lu version %lu ready and waiting\n",
		    prognum, vers);
		return (0);
	}
}

static void
pmapdump(argc, argv)
	int argc;
	char **argv;
{
	struct sockaddr_in server_addr;
	register struct hostent *hp;
	struct pmaplist *head = NULL;
	int socket = RPC_ANYSOCK;
	struct timeval minutetimeout;
	register CLIENT *client;
	struct rpcent *rpc;
	
	if (argc > 1) {
		usage();
		exit(1);
	}
	if (argc == 1)
		get_inet_address(&server_addr, argv[0]);
	else {
		bzero((char *)&server_addr, sizeof server_addr);
		server_addr.sin_family = AF_INET;
		if ((hp = gethostbyname("localhost")) != NULL)
			bcopy(hp->h_addr, (caddr_t)&server_addr.sin_addr,
			    hp->h_length);
		else
			server_addr.sin_addr.s_addr = inet_addr("0.0.0.0");
	}
	minutetimeout.tv_sec = 60;
	minutetimeout.tv_usec = 0;
	server_addr.sin_port = htons(PMAPPORT);
	if ((client = clnttcp_create(&server_addr, PMAPPROG,
	    PMAPVERS, &socket, 50, 500)) == NULL) {

		syslog(LOG_ALERT, "can't contact portmapper [HELP=NO_PORTMAPPER]");
		closelog();

		clnt_pcreateerror("rpcinfo: can't contact portmapper");
		exit(1);
	}
	if (clnt_call(client, PMAPPROC_DUMP, xdr_void, NULL,
	    xdr_pmaplist, &head, minutetimeout) != RPC_SUCCESS) {

		syslog(LOG_ALERT, "can't contact portmapper [HELP=NO_PORTMAPPER]");
		closelog();

		fprintf(stderr, "rpcinfo: can't contact portmapper: ");
		clnt_perror(client, "rpcinfo");
		exit(1);
	}
	if (head == NULL) {
		printf("No remote programs registered.\n");
	} else {
		printf("   program vers proto   port\n");
		for (; head != NULL; head = head->pml_next) {
			printf("%10ld%5ld",
			    head->pml_map.pm_prog,
			    head->pml_map.pm_vers);
			if (head->pml_map.pm_prot == IPPROTO_UDP)
				printf("%6s",  "udp");
			else if (head->pml_map.pm_prot == IPPROTO_TCP)
				printf("%6s", "tcp");
			else
				printf("%6ld",  head->pml_map.pm_prot);
			printf("%7ld",  head->pml_map.pm_port);
			rpc = getrpcbynumber(head->pml_map.pm_prog);
			if (rpc)
				printf("  %s\n", rpc->r_name);
			else
				printf("\n");
		}
	}
}

/* 
 * reply_proc collects replies from the broadcast. 
 * to get a unique list of responses the output of rpcinfo should
 * be piped through sort(1) and then uniq(1).
 */

/*ARGSUSED*/
static bool_t
reply_proc(res, who)
	void *res;		/* Nothing comes back */
	struct sockaddr_in *who; /* Who sent us the reply */
{
	register struct hostent *hp;

	hp = gethostbyaddr((char *) &who->sin_addr, sizeof who->sin_addr,
	    AF_INET);
	printf("%s %s\n", inet_ntoa(who->sin_addr),
	    (hp == NULL) ? "(unknown)" : hp->h_name);
	return(FALSE);
}

static void
manycast(argc, argv, mcast)
	int argc;
	char **argv;
	int mcast;
{
	enum clnt_stat rpc_stat;
	u_long prognum, vers;

	if (argc != 2) {
		usage();
		exit(1);
	}
	prognum = getprognum(argv[0]);
	vers = getvers(argv[1]);
	if (mcast) {
	    rpc_stat = clnt_multicast(prognum, vers, NULLPROC, xdr_void,
		(char *)NULL, xdr_void, (char *)NULL, reply_proc);
	} else {
	    rpc_stat = clnt_broadcast(prognum, vers, NULLPROC, xdr_void,
		(char *)NULL, xdr_void, (char *)NULL, reply_proc);
	}
	if ((rpc_stat != RPC_SUCCESS) && (rpc_stat != RPC_TIMEDOUT)) {
		fprintf(stderr, "rpcinfo: %scast failed: %s\n",
		    mcast ? "multi" : "broad", clnt_sperrno(rpc_stat));
		exit(1);
	}
	exit(0);
}

static void
deletereg(argc, argv)
	int argc;
	char **argv;
{	u_long prog_num, version_num ;

	if (argc != 2) {
		usage() ;
		exit(1) ;
	}
	if (getuid()) { /* This command allowed only to root */
		fprintf(stderr, "Sorry. You are not root\n") ;
		exit(1) ;
	}
	prog_num = getprognum(argv[0]);
	version_num = getvers(argv[1]);
	if ((pmap_unset(prog_num, version_num)) == 0) {
		fprintf(stderr, "rpcinfo: Could not delete registration for prog %s version %s\n",
			argv[0], argv[1]) ;
		exit(1) ;
	}
}

static void
usage()
{
	fprintf(stderr, "Usage: rpcinfo [-C] [ -n portnum ] -u host prognum [ versnum ]\n");
	fprintf(stderr, "       rpcinfo [ -n portnum ] -t host prognum [ versnum ]\n");
	fprintf(stderr, "       rpcinfo -p [ host ]\n");
	fprintf(stderr, "       rpcinfo -b prognum versnum\n");
	fprintf(stderr, "       rpcinfo -m prognum versnum\n");
	fprintf(stderr, "       rpcinfo -d prognum versnum\n") ;
}

static u_long
getprognum(arg)
	char *arg;
{
	char *strptr;
	register struct rpcent *rpc;
	register u_long prognum;

	if (isalpha(*arg)) {
		rpc = getrpcbyname(arg);
		if (rpc == NULL) {
			fprintf(stderr, "rpcinfo: %s is unknown service\n",
			    arg);
			exit(1);
		}
		prognum = rpc->r_number;
	} else {
		prognum = strtoul(arg, &strptr, 10);
		if (strptr == arg || *strptr != '\0') {
			fprintf(stderr, "rpcinfo: %s is illegal program number\n",
			    arg);
			exit(1);
		}
	}

	return (prognum);
}

static u_long
getvers(arg)
	char *arg;
{
	char *strptr;
	register u_long vers;

	vers = (int) strtoul(arg, &strptr, 10);
	if (strptr == arg || *strptr != '\0') {
		fprintf(stderr, "rpcinfo: %s is illegal version number\n",
		    arg);
		exit(1);
	}
	return (vers);
}

static void
get_inet_address(addr, host)
	struct sockaddr_in *addr;
	char *host;
{
	register struct hostent *hp;

	bzero((char *)addr, sizeof *addr);
	addr->sin_addr.s_addr = inet_addr(host);
	if (addr->sin_addr.s_addr == -1 || addr->sin_addr.s_addr == 0) {
		if ((hp = gethostbyname(host)) == NULL) {
			fprintf(stderr, "rpcinfo: %s is unknown host\n", host);
			exit(1);
		}
		bcopy(hp->h_addr, (char *)&addr->sin_addr, hp->h_length);
	}
	addr->sin_family = AF_INET;
}
