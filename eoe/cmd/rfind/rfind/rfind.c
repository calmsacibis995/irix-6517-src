#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <rpc/rpc.h>
#include <sys/time.h>
#include "argv.h"
#include "rfind.h"
#include "externs.h"

#include <sys/param.h>
char hostname[MAXHOSTNAMELEN], domainname[MAXHOSTNAMELEN];
int Argc, Ai, MaxArgc;
char **Argv;
#if LOGGING
#define LOGGING 0
static char *loginfo();
#endif

static void getdata (CLIENT *);
char *filesystem = NULL;
char *server = NULL;
char *mntpt = NULL;

main(int argc, char **argv){
	CLIENT *cl;
	int *result;
	int debug = 0;
	int i;

	if (argc > 1 && strcmp (argv[1], "-debug") == 0) {
		debug++;
		argc--;
		argv++;
	}

	if (argc < 3) {
		fprintf (stderr,
			"Usage: rfind [-debug] <file-system> <search-expression>\n");
		exit (1);
	}

	if ((filesystem = getaliases (argv[1])) == NULL) {
		fprintf (stderr, "rfind: file-system <%s> not known\n", argv[1]);
		showaliases();
		ypshowaliases();
		exit(1);
	}

	if (debug)
		printf("file-system: %s\n", filesystem);

	if (strchr (filesystem, ':') == NULL) {
		fprintf (stderr, "rfind: invalid <%s> ==> <%s> alias - no ':'\n",
					argv[1], filesystem);
		exit(1);
	}

	if ((server = strtok (filesystem, ":")) == NULL) {
		fprintf (stderr, "rfind: server name null: <%s>\n", filesystem);
		exit (1);
	}
	if ((mntpt = strtok (NULL, "")) == NULL) {
		fprintf (stderr, "rfind: mount-point null: <%s:>\n", filesystem);
		exit (1);
	}

	MaxArgc = argc + 4;
	if ((Argv = calloc (MaxArgc, sizeof(*Argv))) == NULL) {
		perror ("rfind: no memory");
		exit (1);
	}
	i = Ai = Argc = 0;
	Argv[Argc++] = argv[i++];		/* argv[0]: "rfind" */

	if (debug) {
		printf("search-expression parse tree:\n");
		Argv[Argc++] = "-DUMP";
	}
#if LOGGING
	Argv[Argc++] = loginfo();		/* "-LOGGING: <hostname>" */
#endif
	Argv[Argc++] = mntpt;			/* <mount-point> - from file-sys alias */
	i++;					/* skip user provided file-sys alias */
	while (i < argc)
		Argv[Argc++] = argv[i++];	/* <search-expression> */
	if (Argc > MaxArgc) {
		fprintf (stderr, "rfind: argument parsing botch\n");
		exit (1);
	}

	cl = clnt_create(server,RFINDPROG,RFINDVERS,"tcp");
	if (cl == NULL) {
		if (rpc_createerr.cf_stat == RPC_PROGNOTREGISTERED)
		    fprintf(stderr,"rfind: rfindd daemon not installed on server <%s>\n",server);
		else
		    clnt_pcreateerror(server);
		exit(1);
		/* NOTREACHED */
	}
	result = rfind(Argc, Argv, cl);
	if ( result == NULL || *result != 1) {
		clnt_perror(cl,server);
		exit(1);
		/* NOTREACHED */
	}
	getdata(cl);
	exit(0);
	/* NOTREACHED */
}

/* Default timeout can be changed using clnt_control() */
static struct timeval TIMEOUT = { 25, 0 };

int * rfind(u_int argc, char **argv, CLIENT *clnt) {
	static int res;
	arguments args;

	bzero((char *)&res, sizeof(res));
	args.argc = argc;
	args.argv = argv;
	if (clnt_call(clnt, RFIND, xdr_arguments, &args, xdr_int, &res, TIMEOUT) != RPC_SUCCESS)
		return (NULL);
	return (&res);
}


struct rfindhdr rfh;

extern enum clnt_stat clnttcp_getreply (
	register CLIENT *h,
        xdrproc_t xdr_results,
        caddr_t results_ptr,
        struct timeval timeout);

void
getdata(CLIENT *cl)
{
	enum clnt_stat clnt_stat;
	struct timeval tv;
	int rval;

	/* Do the appropriate getreply */
	tv.tv_sec = 600;
	tv.tv_usec = 0;
	clnt_control (cl, CLSET_TIMEOUT, &tv);
	rfh.bufp = calloc (1, RPCBUFSIZ);

	for (;;) {
		clnt_stat = (enum clnt_stat) clnttcp_getreply (
		    cl, xdr_rfind, (caddr_t) &rfh, tv);

		/* Process the reply */
		if (clnt_stat != RPC_SUCCESS)
			{clnt_perror(cl,server); exit(1);}

		switch (rfh.fd) {
		case 1:
			printf("%.*s", rfh.sz, rfh.bufp);
			break;
		case 2:
			fprintf(stderr,"%.*s", rfh.sz, rfh.bufp);
			break;
		case 3:
			sscanf (rfh.bufp, "exit status %d\n", &rval);
			exit (rval);
			/* NOTREACHED */
		default:
			fprintf(stderr, "rfind: bogus fd: %d\n", rfh.fd);
			exit (1);
			/* NOTREACHED */
		}
	}
	/* NOTREACHED */
}

#if LOGGING
char *loginfo() {
	char *buf;
	char *cp;
	ulong_t len;
	char *fmt = "%s%s.%s\n";
	char *logmsg = "-LOGGING: ";

	if (gethostname (hostname, sizeof(hostname))) {
		perror ("rfind: gethostname");
		exit (1);
	}
	if ((cp = strchr (hostname, '.')) != NULL)
		*cp = '\0';
	if (getdomainname (domainname, sizeof(domainname))) {
		perror ("rfind: getdomainname");
		exit (1);
	}
	len = 1+strlen(fmt)+strlen(logmsg)+strlen(hostname)+strlen(domainname);
	if ((buf = malloc (len)) == NULL) {
		perror ("rfind: no memory");
		exit (1);
	}
	sprintf (buf, fmt, logmsg, hostname, domainname);
	return buf;
}
#endif
