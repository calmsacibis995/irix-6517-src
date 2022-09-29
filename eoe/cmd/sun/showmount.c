/*
 * Copyright (c) 1987 Sun Microsystems, Inc.
 */

/*
 * showmount
 */
#include <stdio.h>
#include <rpc/rpc.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <exportent.h>
#include <sys/fs/nfs.h>
#include <rpcsvc/mount.h>

struct timeval TIMEOUT = { 25, 0 };

int sorthost();
int sortpath();

main(argc, argv)
	int argc;
	char **argv;
{
	
	int aflg = 0, dflg = 0, eflg = 0;
	int nentries;
	enum clnt_stat err;
	struct mountlist *ml = NULL;
	struct mountlist *mnt;
	struct mountlist **tb, **endtb, **table;
	struct hostent *hp;
	char *host = NULL, hostbuf[256];
	char *last;
	CLIENT *cl;

	int opt, xflg = 0;
	extern int optind;

	while ((opt = getopt(argc, argv, "adex")) != EOF) {
		switch (opt) {
		  case 'a':
			aflg++;
			break;
		  case 'd':
			dflg++;
			break;
		  case 'e':
			eflg++;
			break;
		  case 'x':
			xflg++;
			break;
		  default:
			usage();
		}
	}
	if (optind < argc - 1)
		usage();
	if (optind == argc - 1)
		host = argv[optind];
	if (host == NULL) {
		if (gethostname(hostbuf, sizeof(hostbuf)) < 0) {
			perror("showmount: gethostname");
			exit(1);
		}
		host = hostbuf;
	}

	/*
	 * First try tcp, then drop back to udp if
	 * tcp is unavailable (an old version of mountd perhaps)
	 * Using tcp is preferred because it can handle
	 * arbitrarily long export lists.
	 */
	cl = clnt_create(host, MOUNTPROG, MOUNTVERS, "tcp");
	if (cl == NULL) {
		cl = clnt_create(host, MOUNTPROG, MOUNTVERS, "udp");
		if (cl == NULL) {
			fprintf(stderr, "showmount: ");
			clnt_pcreateerror(host);
			exit(1);
		}
	}

	if (xflg) {
		showexports(cl, host);
		if (!aflg && !dflg)
			exit(0);
	} else
	if (eflg) {
		printex(cl, host);
		if (aflg + dflg == 0) {
			exit(0);
		}
	}

	if (err = clnt_call(cl, MOUNTPROC_DUMP,
			    xdr_void, 0, xdr_mountlist, &ml, TIMEOUT)) {
		fprintf(stderr, "showmount: %s\n", clnt_sperrno(err));
		exit(1);
	}
	nentries = 0;
	for (mnt = ml; mnt != NULL; mnt = mnt->ml_nxt)
	    nentries++;
	table = (struct mountlist **)malloc(nentries*sizeof(struct mountlist *));
	if (table == (struct mountlist **)0) {
	    perror("showmount: no memory for table\n");
	    exit(-1);
	}
	tb = table;
	for (; ml != NULL; ml = ml->ml_nxt)
		*tb++ = ml;
	endtb = tb;
	if (dflg)
	    qsort(table, endtb - table, sizeof(struct mountlist *), sortpath);
	else
	    qsort(table, endtb - table, sizeof(struct mountlist *), sorthost);
	if (aflg) {
		for (tb = table; tb < endtb; tb++)
			printf("%s:%s\n", (*tb)->ml_name, (*tb)->ml_path);
	}
	else if (dflg) {
		last = "";
		for (tb = table; tb < endtb; tb++) {
			if (strcmp(last, (*tb)->ml_path))
				printf("%s\n", (*tb)->ml_path);
			last = (*tb)->ml_path;
		}
	}
	else {
		last = "";
		for (tb = table; tb < endtb; tb++) {
			if (strcmp(last, (*tb)->ml_name))
				printf("%s\n", (*tb)->ml_name);
			last = (*tb)->ml_name;
		}
	}
}

sorthost(a, b)
	struct mountlist **a,**b;
{
	return strcmp((*a)->ml_name, (*b)->ml_name);
}

sortpath(a, b)
	struct mountlist **a,**b;
{
	return strcmp((*a)->ml_path, (*b)->ml_path);
}

usage()
{
	fprintf(stderr, "showmount [-adex] [host]\n");
	exit(1);
}

showexports(oldcl, host)
	CLIENT *oldcl;
	char *host;
{
	CLIENT *cl;
	enum clnt_stat stat;
	struct exportlist *el;
	struct exports *ex;

	cl = clnt_create(host, MOUNTPROG_SGI, MOUNTVERS_SGI_ORIG, "tcp");
	if (cl == NULL) {
		cl = clnt_create(host, MOUNTPROG_SGI, MOUNTVERS_SGI_ORIG, "udp");
		if (cl == NULL)
			goto oldway;
	}

	el = 0;
	stat = clnt_call(cl, MOUNTPROC_EXPORTLIST, xdr_void, 0,
			 xdr_exportlist, &el, TIMEOUT);
	if (stat == RPC_SUCCESS) {
		while (el) {
			printf("%-19s", el->el_entry.ee_dirname);
			if (el->el_entry.ee_options)
				printf(" -%s", el->el_entry.ee_options);
			putchar('\n');
			el = el->el_next;
		}
		return;
	}

oldway:
	ex = 0;
	stat = clnt_call(oldcl, MOUNTPROC_EXPORT, xdr_void, 0,
			 xdr_exports, &ex, TIMEOUT);
	if (stat != RPC_SUCCESS) {
		fprintf(stderr, "showmount: ");
		clnt_perrno(stat);
		putc('\n', stderr);
		exit(1);
	}

	while (ex) {
		struct groups *gr;

		printf("%-19s", ex->ex_name);
		for (gr = ex->ex_groups; gr; gr = gr->g_next)
			printf(" %s", gr->g_name);
		putchar('\n');
		ex = ex->ex_next;
	}
}

int
showgroups(name, gr, commaflag)
	char *name;
	struct groups *gr;
	int commaflag;
{
	if (gr == 0)
		return;
	printf("%s%s=", commaflag ? "," : "", name);
	for (;;) {
		printf("%s", gr->g_name);
		gr = gr->g_next;
		if (gr == 0)
			break;
		putchar(':');
	}
}

printex(cl, host)
	CLIENT *cl;
	char *host;
{
	struct exports *ex = NULL;
	struct exports *e;
	struct groups *gr;
	enum clnt_stat err;
	int max;

	if (err = clnt_call(cl, MOUNTPROC_EXPORT,
	    xdr_void, 0, xdr_exports, &ex, TIMEOUT)) {
		fprintf(stderr, "showmount: %s\n", clnt_sperrno(err));
		exit(1);
	}

	if (ex == NULL) {
		fprintf(stdout, "no exported file systems for %s\n", host);
	} else {
		fprintf(stdout, "export list for %s:\n", host);
	}
	max = 0;
	for (e = ex; e != NULL; e = e->ex_next) {
		if (strlen(e->ex_name) > max) {
			max = strlen(e->ex_name);
		}
	}
	while (ex) {
		fprintf(stdout, "%-*s ", max, ex->ex_name);
		gr = ex->ex_groups;
		if (gr == NULL) {
			fprintf(stdout, "(everyone)");
		}
		while (gr) {
			fprintf(stdout, "%s", gr->g_name);
			gr = gr->g_next;
			if (gr) {
				fprintf(stdout, ",");
			}
		}
		fprintf(stdout, "\n");
		ex = ex->ex_next;
	}
}
