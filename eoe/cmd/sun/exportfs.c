#ifndef lint
static char sccsid[] = 	"@(#)exportfs.c	1.2 88/06/15 4.0NFSSRC Copyr 1988 Sun Micro";
#endif

/*
 * Export directories (or files) to NFS clients Copyright (c) 1987 Sun
 * Microsystems, Inc.
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <exportent.h>
#include <netdb.h>
#include <strings.h>
#include <rpc/rpc.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/fs/export.h>
#include <sys/capability.h>

/*
 * options to command
 */
#define AFLAG 0x01
#define UFLAG 0x02
#define VFLAG 0x04
#define IFLAG 0x08

#define BUFINCRSZ 128
#define MAXNAMELEN 256
#define NETGROUPINCR 32

#define streq(a,b) (strcmp(a, b) == 0)

char *getline(FILE *);
char *accessjoin(char *, int ngrps, char **);
char *strtoken(char **, char *);
char *check_malloc(unsigned int);
char *check_realloc(char *, unsigned int);
void out_of_memory(void);
void cannot_open(int, char *);
void printexports(void);
void printexport(struct exportent *, int);
void fillex(struct export *, struct options *);
void export_full(char *, int);
int map_user(char *, int *);

char anon_opt[] = ANON_OPT;
char ro_opt[] = RO_OPT;
char rw_opt[] = RW_OPT;
char root_opt[] = ROOT_OPT;
char access_opt[] = ACCESS_OPT;
char nohide_opt[] = NOHIDE_OPT;
char wsync_opt[] = WSYNC_OPT;
char b32clnt_opt[] = B32CLNT_OPT;
char noxattr_opt[] = NOXATTR_OPT;

char EXPORTFILE[] = "/etc/exports";
static const cap_value_t cap_mount_read[] = {CAP_MOUNT_MGT, CAP_MAC_READ};

struct options {
	int anon;
	int flags;
	int auth;
	char **hostlist;
	int hostlistsize;
	int nhosts;
	char **accesslist;
	int accesslistsize;
	int naccess;
	char **writelist;
	int writelistsize;
	int nwrites;
};


main(argc, argv)
	int argc;
	char *argv[];

{
	int i;
	char *options = NULL;
	int Argc;
	char **Argv;
	int flags;
	int retstatus = 0;

	if (!parseargs(argc, argv, &flags, &options, &Argc, &Argv)) {
		(void) fprintf(stderr,
			"usage: %s [-aiuv] [-o options] [dir] ...\n", argv[0]);
		exit(1);
	}
	if ((Argc || (flags & AFLAG)) && geteuid() != 0) {
		fprintf(stderr, "must be superuser to %sexport\n",
			(flags & UFLAG) ? "un" : "");
		exit(1);
	}
	if (Argc == 0) {
		if (flags & AFLAG) {
			if (flags & UFLAG) {
				retstatus = (unexportall(flags & VFLAG) < 0);
			} else {
				retstatus = (exportall(flags & VFLAG, NULL) 
					     < 0);
			}
		} else {
			printexports();
		}
	} else if (flags & UFLAG) {
		if (options) {
			(void) fprintf(stderr,
				  "exportfs: options ignored for unexport\n");
		}
		for (i = 0; i < Argc; i++) {
			retstatus |= (unexport(Argv[i], flags & VFLAG) < 0);
		}
	} else {
		for (i = 0; i < Argc; i++) {
			if (flags & IFLAG) {
				retstatus |= (export(Argv[i], options, 
						    flags & VFLAG) < 0);
				
			} else {
				retstatus |= (exportall(flags & VFLAG, Argv[i])
					     < 0);
			}
		}
	}

	exit(retstatus);
	/* NOTREACHED */
}

/*
 * Print all exported pathnames
 */
void
printexports(void)
{
	struct exportent *xent;
	FILE *f;
	int maxlen;

	f = fopen(TABFILE, "r");
	if (f == NULL) {
		(void) printf("nothing exported\n");
		return;
	}
	maxlen = 0;
	while (xent = getexportent(f)) {
		if (strlen(xent->xent_dirname) > maxlen) {
			maxlen = strlen(xent->xent_dirname);
		}
	}
	rewind(f);
	while (xent = getexportent(f)) {
		printexport(xent, maxlen + 1);
	}
	if (maxlen == 0) {
		(void) printf("nothing exported\n");
	}
	endexportent(f);
}

/*
 * Print just one exported pathname
 */
void
printexport(xent, col)
	struct exportent *xent;
	int col;
{
	int i;

	(void) printf("%s", xent->xent_dirname);

	if (xent->xent_options) {
		for (i = strlen(xent->xent_dirname); i < col; i++) {
			(void) putchar(' ');
		}
		(void) printf("-%s", xent->xent_options);
	}
	(void) printf("\n");
}

/*
 * Unexport everything in the export tab file
 */
unexportall(verbose)
	int verbose;
{
	struct exportent *xent;
	FILE *f;
	cap_t ocap;
	register int retcode = 0;

	f = setexportent();
	if (f == NULL) {
		cannot_open(0, TABFILE);
		return -1;
	}
	while (xent = getexportent(f)) {
		(void) remexportent(f, xent->xent_dirname);
		ocap = cap_acquire (2, cap_mount_read);
		if (exportfs(xent->xent_dirname, (struct export *) NULL) < 0) {
			(void) fprintf(stderr, "exportfs: %s: %s\n",
				xent->xent_dirname, strerror(errno));
			retcode = -1;
		} else {
			if (verbose) {
				(void) printf("unexported %s\n", xent->xent_dirname);
			}
		}
		cap_surrender(ocap);
	}
	endexportent(f);
	return retcode;
}

/*
 * Unexport just one directory
 */
unexport(dirname, verbose)
	char *dirname;
	int verbose;
{
	FILE *f;
	cap_t ocap;

	f = setexportent();
	if (f == NULL) {
		cannot_open(0, TABFILE);
		return -1;
	}
	(void) remexportent(f, dirname);
	ocap = cap_acquire (2, cap_mount_read);
	if (exportfs(dirname, (struct export *) NULL) < 0) {
		cap_surrender(ocap);
		(void) fprintf(stderr, "exportfs: %s: %s\n",
				dirname, strerror(errno));
		endexportent(f);
		return -1;
	}
	cap_surrender(ocap);
	endexportent(f);
	if (verbose) {
		(void) printf("unexported %s\n", dirname);
	}
	return 0;
}

/*
 * Export everything in /etc/exports
 */
exportall(verbose, which)
	int verbose;
	char *which;
{
	char *name;
	FILE *f;
	char *lp;
	char *dirname;
	char *options;
	char **grps;
	int grpsize;
	int ngrps;
	register int i;
	int exported, failed;

	f = fopen(EXPORTFILE, "r");
	if (f == NULL) {
		cannot_open(errno, EXPORTFILE);
		return -1;
	}
	grps = NULL;
	grpsize = 0;
	exported = failed = 0;
	while (
		(which == NULL || !exported)
		&& ( (lp = getline(f)) != NULL )
	) {
		dirname = NULL;
		options = NULL;
		ngrps = 0;
		while ((name = strtoken(&lp, " \t")) != NULL) {
			if (dirname == NULL) {
				dirname = name;
			} else if (options == NULL && name[0] == '-') {
				if ((options = strdup(name + 1)) == NULL)
					out_of_memory();
			} else {
				grpsize = addtogrouplist(&grps, grpsize,
							 ngrps, name);
				ngrps++;
			}
		}
		if (ngrps > 0) {
			options = accessjoin(options, ngrps, grps);
			for (i = 0; i < ngrps; i++)
				free(grps[i]);
		}
		if (dirname != NULL && (which == NULL || 
					strcmp(dirname, which) == 0)) {
			if (export(dirname, options, verbose) < 0)
				failed = 1;
			else 
				exported++;
		}
		if (options != NULL) {
			free(options);
		}
	}
	if (failed) {
		return -1;
	}
	if (which != NULL && !exported) {
		fprintf(stderr, "%s not found in %s\n", which, EXPORTFILE);
		return -1;
	}
	return 0;
}

int
addtogrouplist(grplistp, grplistsize, index, group)
	char ***grplistp;
	int grplistsize;
	int index;
	char *group;
{
	if (index == grplistsize) {
		grplistsize += NETGROUPINCR;
		if (*grplistp == NULL)
			*grplistp =
				(char **) check_malloc(((unsigned) grplistsize * sizeof(char *)));
		else
			*grplistp = (char **) check_realloc((char *) *grplistp,
				   (unsigned) (grplistsize * sizeof(char *)));
	}
	(*grplistp)[index] = group;
	return grplistsize;
}

/*
 * Export just one directory
 */
export(dirname, options, verbose)
	char *dirname;
	char *options;
	int verbose;
{
	struct export ex;
	struct options opt;
	FILE *f;
	int redo = 0;
	cap_t ocap;

	if (!interpret(dirname, &opt, options)) {
		return -1;
	}
	fillex(&ex, &opt);
	f = setexportent();
	if (f == NULL) {
		cannot_open(0, TABFILE);
		return -1;
	}
	if (insane(f, dirname)) {
		endexportent(f);
		return -1;
	}
	if (xtab_test(f, dirname)) {
		(void) remexportent(f, dirname);
		redo = 1;
	}
	ocap = cap_acquire (2, cap_mount_read);
	if (exportfs(dirname, &ex) < 0) {
		cap_surrender(ocap);
		(void) fprintf(stderr, "exportfs: %s: %s\n",
				dirname, strerror(errno));
		endexportent(f);
		return -1;
	}
	cap_surrender(ocap);
	addexportent(f, dirname, options);
	endexportent(f);
	if (verbose) {
		if (redo) {
			(void) printf("re-exported %s\n", dirname);
		} else {
			(void) printf("exported %s\n", dirname);
		}
	}
	return 0;
}


/*
 * Interpret a line of options
 */
interpret(dirname, opt, line)
	char *dirname;
	struct options *opt;
	char *line;
{
	char *name;

	/*
	 * Initialize and set up defaults
	 */
	opt->anon = -2;
	opt->flags = 0;
	opt->auth = AUTH_UNIX;
	opt->hostlist = NULL;
	opt->hostlistsize = 0;
	opt->nhosts = 0;
	opt->accesslist = NULL;
	opt->accesslistsize = 0;
	opt->naccess = 0;
	opt->writelist = NULL;
	opt->writelistsize = 0;
	opt->nwrites = 0;
	if (line == NULL) {
		return 1;
	}
	for (;;) {
		name = strtoken(&line, ",");
		if (name == NULL) {
			return 1;
		}
		if (!dooption(dirname, name, opt)) {
			return 0;
		}
	}
}

/*
 * Fill in the root addresses
 */
static void
filladdrs(struct exaddrlist *addrs, char **names, int nnames, char *errstr, int max)
{
	struct hostent *h;
	struct sockaddr *sa;
	int i, n;
	char *machine, *user, *domain;

	addrs->naddrs = 0;
	sa = &addrs->addrvec[0];
	for (i = 0; i < nnames; i++) {
		/* check for netgroup */
		n = innetgr(names[i], (char *) 0, (char *) 0, (char *) 0);
		if (n) {
			setnetgrent(names[i]);
			while (getnetgrent(&machine, &user, &domain)) {
			   if (machine) {
				if ((h = gethostbyname(machine)) == NULL)
					fprintf(stderr,"exportfs: unknown host %s in netgroup %s\n",
						machine, names[i]);
				else {
					if (addrs->naddrs >= max) {
						export_full(errstr, max);
						endnetgrent();
						return;
					}
					if (fillone(h, sa) == 0) {
						addrs->naddrs++;
						sa++;
					}
				}
			   } else {
				sethostent(0);
				while (h = gethostent()) {
					if (addrs->naddrs >= max) {
						export_full(errstr, max);
						endnetgrent();
						return;
					}
					if (fillone(h, sa) == 0) {
						addrs->naddrs++;
						sa++;
					}
				}
			   }
			}	/* while */
			endnetgrent();
			continue;
		}
		h = gethostbyname(names[i]);
		if (h == NULL) {
			fprintf(stderr, "exportfs: bad host '%s' in %s: %s\n",
				names[i], errstr, hstrerror(h_errno));
			continue;
		}
		if (addrs->naddrs >= max) {
			export_full(errstr, max);
			return;
		}
		if (fillone(h, sa) == 0 ) {
			addrs->naddrs++;
			sa++;
		}
	}

}

/* fill an export struct for the given host */
fillone(h, sa)
struct hostent *h;
struct sockaddr *sa;
{
	bzero((char *) sa, sizeof *sa);
	sa->sa_family = h->h_addrtype;
	switch (h->h_addrtype) {
		case AF_INET:
			bcopy(h->h_addr,
			(char *) &((struct sockaddr_in *)sa)->sin_addr,
				h->h_length);
			break;
		default:
			(void) fprintf(stderr,
			       "exportfs: %d: unknown address type for %s\n",
				       h->h_addrtype, h->h_name);
			return(-1);
	}
	return(0);
}

/*
 * Fill an export structure given the options selected
 */
void
fillex(ex, ops)
	struct export *ex;
	struct options *ops;
{
	ex->ex_flags = ops->flags;
	if (ex->ex_flags & EX_RDMOSTLY) {
		ex->ex_writeaddrs.addrvec = (struct sockaddr *)
			check_malloc(EXMAXADDRS * sizeof(struct sockaddr));
		filladdrs(&ex->ex_writeaddrs, ops->writelist, ops->nwrites,
			"rw option", EXMAXADDRS);
	}
	ex->ex_anon = ops->anon;
	ex->ex_auth = ops->auth;
	switch (ops->auth) {
	case AUTH_UNIX:
		ex->ex_unix.rootaddrs.naddrs = ops->nhosts;
		ex->ex_unix.rootaddrs.addrvec = (struct sockaddr *)
			check_malloc(EXMAXROOTADDRS * sizeof(struct sockaddr));
		filladdrs(&ex->ex_unix.rootaddrs, ops->hostlist, ops->nhosts,
			"root option", EXMAXROOTADDRS);

		ex->ex_accessaddrs.addrvec = (struct sockaddr *)
			check_malloc(EXMAXADDRS * sizeof(struct sockaddr));
		filladdrs(&ex->ex_accessaddrs, ops->accesslist, ops->naccess,
			"access option", EXMAXADDRS);
		break;
	}
}


dooption(dirname, opstr, ops)
	char *dirname;
	char *opstr;
	struct options *ops;
{
#	define pstreq(a, b, blen) \
		((strncmp(a, b, blen) == 0) && (a[blen] == '='))

	if (streq(opstr, ro_opt)) {
		ops->flags |= EX_RDONLY;
	} else if (pstreq(opstr, rw_opt, sizeof(rw_opt) - 1)) {
		ops->flags |= EX_RDMOSTLY;
		if (!getacclist(&opstr[sizeof(rw_opt)],
				&ops->nwrites, &ops->writelist,
				&ops->writelistsize)) {
			out_of_memory();
		}
	} else if (pstreq(opstr, anon_opt, sizeof(anon_opt) - 1)) {
		if (!map_user(&opstr[sizeof(anon_opt)], &ops->anon))
			return 0;
	} else if (streq(opstr, nohide_opt)) {
		ops->flags |= EX_NOHIDE;
	} else if (streq(opstr, wsync_opt)) {
		ops->flags |= EX_WSYNC;
	} else if (streq(opstr, rw_opt)) {	/* read-write export */
		/* nothing to do */
	} else if (pstreq(opstr, root_opt, sizeof(root_opt) - 1)) {
		if (!getacclist(&opstr[sizeof(root_opt)],
				&ops->nhosts, &ops->hostlist,
				&ops->hostlistsize)) {
			out_of_memory();
		}
	} else if (pstreq(opstr, access_opt, sizeof(access_opt) - 1)) {
		ops->flags |= EX_ACCESS;
		if (!getacclist(&opstr[sizeof(access_opt)],
		     &ops->naccess, &ops->accesslist, &ops->accesslistsize)) {
			out_of_memory();
		}
	} else if (streq(opstr, b32clnt_opt)) {
		ops->flags |= EX_B32CLNT;
	} else if (streq(opstr, noxattr_opt)) {
		ops->flags |= EX_NOXATTR;
	} else {
		(void) fprintf(stderr, "exportfs: unknown option: %s\n", opstr);
		return 0;
	}
	return 1;
}

void
export_full(msg, max)
char *msg;
int max;
{
	(void) fprintf(stderr,
		"exportfs: export address list for %s is full (max %d).\n",
		msg, max);
}


int
getstaticnamelist(list, nnames, namelist, max)
	char *list;
	int *nnames;
	char *namelist[];
	int max;

{
	char *name;

	for (;;) {
		name = strtoken(&list, ":");
		if (name == NULL) {
			return 1;
		}
		if (*nnames == max) {
			return 0;
		}
		namelist[*nnames] = name;
		(*nnames)++;
	}
}

int
getacclist(list, nnames, namelist, namelistsize)
	char *list;
	int *nnames;
	char ***namelist;
	int *namelistsize;
{
	char *name;

	for (;;) {
		name = strtoken(&list, ":");
		if (name == NULL) {
			return 1;
		}
		if (!innetgr(name, (char *) 0, (char *) 0, (char *) 0) &&
			(gethostbyname(name) == 0)) {
			fprintf(stderr,
		    "exportfs: unknown host or netgroup in access list: %s\n",
				name);
		} else {
			if ((*namelistsize = addtogrouplist(namelist, 
					*namelistsize, *nnames, name)) == 0)
				return 0;
			(*nnames)++;
		}
	}
}

char *
accessjoin(options, ngrps, grps)
	char *options;
	int ngrps;
	char **grps;
{
	register char *str;
	register int i;
	register unsigned int len;
	register char *p;

	if (options != NULL) {
		len = strlen(options);	/* <options> */
		if (len != 0)
			len++;	/* , */
	} else
		len = 0;
	len += (sizeof(access_opt) - 1) + 1;	/* <access_opt>= */
	for (i = 0; i < ngrps; i++) {
		len += strlen(grps[i]) + 1;	/* group: or group\0 */
	}
	str = check_malloc(len);
	if (options == NULL || *options == '\0') {
		(void) sprintf(str, "%s=%s", access_opt, grps[0]);
	} else {
		(void) sprintf(str, "%s,%s=%s", options, access_opt, grps[0]);
	}
	p = str;
	for (i = 1; i < ngrps; i++) {
		p += strlen(p);
		(void) sprintf(p, ":%s", grps[i]);
	}
	if (options != NULL)
		free(options);
	return str;
}

parseargs(argc, argv, flags, options, nargc, nargv)
	int argc;
	char *argv[];
	int *flags;
	char **options;
	int *nargc;
	char ***nargv;

{
	int i;
	int j;

	*flags = 0;
	for (i = 1; i < argc && argv[i][0] == '-'; i++) {
		if (argv[i][1] == 0) {
			return 0;
		}
		for (j = 1; argv[i][j] != 0; j++) {
			switch (argv[i][j]) {
			case 'a':
				*flags |= AFLAG;
				break;
			case 'u':
				*flags |= UFLAG;
				break;
			case 'v':
				*flags |= VFLAG;
				break;
			case 'i':
				*flags |= IFLAG;
				break;
			case 'o':
				if (j != 1 || argv[i][2] != 0) {
					return 0;
				}
				if (i + 1 >= argc) {
					return 0;
				}
				*options = argv[++i];
				goto breakout;
			default:
				return 0;
			}
		}
breakout:
		;
	}
	*nargc = argc - i;
	*nargv = argv + i;
	return 1;
}

xtab_test(f, dirname)
	FILE *f;
	char *dirname;
{
	struct exportent *xent;

	rewind(f);
	while (xent = getexportent(f)) {
		if (streq(xent->xent_dirname, dirname)) {
			return 1;
		}
	}
	return 0;
}


direq(dir1, dir2)
	char *dir1;
	char *dir2;
{
	struct stat64 st1;
	struct stat64 st2;

	if (stat64(dir1, &st1) < 0) {
		return 0;
	}
	if (stat64(dir2, &st2) < 0) {
		return 0;
	}
	return (st1.st_ino == st2.st_ino && st1.st_dev == st2.st_dev);
}


insane(f, dir)
	FILE *f;
	char *dir;
{
	struct exportent *xent;

	rewind(f);
	while (xent = getexportent(f)) {
		if (direq(xent->xent_dirname, dir)) {
			continue;
		}
		if (issubdir(xent->xent_dirname, dir)) {
			(void) fprintf(stderr,
			"exportfs: %s: sub-directory (%s) already exported\n",
				       dir, xent->xent_dirname);
			return 1;
		}
		if (issubdir(dir, xent->xent_dirname)) {
			(void) fprintf(stderr,
				       "exportfs: %s: parent-directory (%s) already exported\n",
				       dir, xent->xent_dirname);
			return 1;
		}
	}
	return 0;
}

#define buf_append(c, bp, buf, bufsz)						\
	(									\
		( bp == (bufsz) ) ? (						\
			(buf) = check_realloc((buf), (bufsz)+BUFINCRSZ),	\
			(bufsz) += BUFINCRSZ,					\
			(buf)[bp++] = (c)					\
		) : (								\
			(buf)[bp++] = (c)					\
		)								\
	)

/*
 * getline() Read a line from a file, digesting backslash-newline.
 * Returns a pointer to the read string, or NULL on EOF.
 * Backslash-newline counts as whitespace except when:
 *   we are processing the options section
 *   and the preceeding char is ':', '=', or ','
 */
char *
getline(f)
	FILE *f;
{
	static char *buf = NULL;
	static size_t bufsz = 0;
	size_t bp = 0;
	int cc;
	enum getline_states {
		init_state,	/* start here, eat blank lines, comments, etc */
		bsi_state,	/* backslash newline that counts as init */
		comment_state,	/* comment #..*$ */
		dir_state,	/* name of exported directory */
		bs1_state,	/* backslash newline that counts as ws1 */
		ws1_state,	/* whitespace between dir and options */
		opt_state,	/* options section */
		bs2_state,	/* backslash newline conditionally eaten */
		ws2_state,	/* whitespace after options section */
		grp_state,	/* hosts and netgroups */
		bs3_state,	/* backslash newline that counts as ws2 */
		done_state	/* we're done */
	} state, return_state;

	/* initialize buffer if needed */
	if ( buf == NULL ) {
		buf = check_malloc(BUFINCRSZ);
		bufsz = BUFINCRSZ;
	}

	state = init_state;
	return_state = done_state;

	do {
		cc = getc(f);
		if ( cc == EOF ) {
			/*
			 * if there is any data in the buffer, return it
			 */
			if ( bp > 0 ) {
				state = done_state;
			} else {
				return NULL;
			}
		}

		/* another simplification */
		if ( cc == '\t' ) {
			cc = ' ';
		}

		switch ( state ) {
		case init_state:
			switch ( cc ) {
			/*
			 * We haven't gotten any data yet.  Chew up blank
			 * lines, continued lines, and comments until we
			 * hit something of interest.
			 */
			case ' ':
				break;
			case '\\':
				state = bsi_state;
				break;
			case '#':
				return_state = init_state;
				state = comment_state;
				break;
			case '\n':
				break;
			default:
				buf_append(cc, bp, buf, bufsz);
				state = dir_state;
				break;
			}
			break;
		case bsi_state:
			/*
			 * We've hit a backslash.  If it turns out to be a
			 * backslash-newline, continue on processing in
			 * the init state.  Otherwise, we have the first
			 * char of our exported directory.
			 *
			 * Can the first char of the directory ever be
			 * anything but '/'?
			 *
			 * Right now, the directory name does not have to
			 * start in column 0.
			 *
			 * Allow backslash to quote '\\' and '#'.
			 */
			switch ( cc ) {
			case ' ':
				state = init_state;
				break;
			case '\n':
				state = init_state;
				break;
			default:
				buf_append(cc, bp, buf, bufsz);
				state = dir_state;
				break;
			}
			break;
		case comment_state:
			/*
			 * We've hit a comment.  After consuming it we
			 * either will be done or we will continue init
			 * processing depending on the value of 
			 * return_state.
			 */
			switch ( cc ) {
			case '\n':
				state = return_state;
				break;
			default:
				break;
			}
			break;
		case dir_state:
			/*
			 * We are now processing the exported directory
			 * portion of the line.
			 * Line continuation terminates this section.
			 * We are guaranteed to have at least one char
			 * of the directory name already.
			 *
			 * Question, shouldn't directory names ALWAYS
			 * begin with a '/'?  Can I enforce this?
			 */
			switch ( cc ) {
			case ' ':
				buf_append(cc, bp, buf, bufsz);
				state = ws1_state;
				break;
			case '\\':
				state = bs1_state;
				break;
			case '#':
				return_state = done_state;
				state = comment_state;
				break;
			case '\n':
				state = done_state;
				break;
			default:
				buf_append(cc, bp, buf, bufsz);
				break;
			}
			break;
		case bs1_state:
			/*
			 * We ran into a backslash while processing the
			 * exported directory name.  If it is a line
			 * continuation, move on to the options
			 * processing.  If it is just escaping a char, go
			 * back to dir_state.
			 */
			switch ( cc ) {
			case ' ':
				buf_append(' ', bp, buf, bufsz);
				state = ws1_state;
				break;
			case '\n':
				buf_append(' ', bp, buf, bufsz);
				state = ws1_state;
				break;
			default:
				buf_append(cc, bp, buf, bufsz);
				state = dir_state;
				break;
			}
			break;
		case ws1_state:
			/*
			 * White space between the directory and the
			 * options or host/grouplist.
			 * We are guaranteed that there is already a
			 * space in the buffer after the directory.
			 */
			switch ( cc ) {
			case ' ':
				break;
			case '\\':
				state = bs2_state;
				break;
			case '#':
				return_state = done_state;
				state = comment_state;
				break;
			case '\n':
				state = done_state;
				break;
			case '-':
				buf_append(cc, bp, buf, bufsz);
				state = opt_state;
				break;
			default:
				buf_append(cc, bp, buf, bufsz);
				state = grp_state;
				break;
			}
			break;
		case opt_state:
			/*
			 * We're now processing the options part of the
			 * line.
			 */
			switch ( cc ) {
			case ' ':
				buf_append(cc, bp, buf, bufsz);
				state = ws2_state;
				break;
			case '\\':
				state = bs2_state;
				break;
			case '#':
				return_state = done_state;
				state = comment_state;
				break;
			case '\n':
				state = done_state;
				break;
			default:
				buf_append(cc, bp, buf, bufsz);
				break;
			}
			break;
		case bs2_state:
			/*
			 * We ran into a backslash while processing the
			 * options.  This is the only place where a
			 * line continuation cannot always be counted as
			 * a whitespace since it mucks up option
			 * processing.
			 *
			 * There is a slight bug here as
			 *    "/dir \foo..."
			 * will process foo as an option and use the wrong
			 * line continuation rules.
			 */
			switch ( cc ) {
			case ' ':
				buf_append(' ', bp, buf, bufsz);
				state = ws2_state;
				break;
			case '\n':
				switch ( buf[bp-1] ) {
				case '-':
				case '=':
				case ':':
				case ',':
					state = opt_state;
					break;
				case ' ':
					state = ws2_state;
					break;
				default:
					buf_append(' ', bp, buf, bufsz);
					state = ws2_state;
					break;
				}
				break;
			default:
				buf_append(cc, bp, buf, bufsz);
				state = opt_state;
				break;
			}
			break;
		case ws2_state:
			/*
			 * This is the whitespace before a hostname or
			 * netgroup.  There is already a space in the
			 * buffer.
			 */
			switch ( cc ) {
			case ' ':
				break;
			case '\\':
				state = bs3_state;
				break;
			case '#':
				return_state = done_state;
				state = comment_state;
				break;
			case '\n':
				state = done_state;
				break;
			default:
				buf_append(cc, bp, buf, bufsz);
				state = grp_state;
				break;
			}
			break;
		case grp_state:
			switch ( cc ) {
			case ' ':
				buf_append(cc, bp, buf, bufsz);
				state = ws2_state;
				break;
			case '\\':
				state = bs3_state;
				break;
			case '#':
				return_state = done_state;
				state = comment_state;
				break;
			case '\n':
				state = done_state;
				break;
			default:
				buf_append(cc, bp, buf, bufsz);
				break;
			}
			break;
		case bs3_state:
			/*
			 * We ran into a backslash while processing the
			 * hosts/grouplists.
			 */
			switch ( cc ) {
			case ' ':
				buf_append(' ', bp, buf, bufsz);
				state = ws2_state;
				break;
			case '\n':
				buf_append(' ', bp, buf, bufsz);
				state = ws2_state;
				break;
			default:
				buf_append(cc, bp, buf, bufsz);
				state = grp_state;
				break;
			}
			break;
		case done_state:
			/* do nothing */
			break;
		default:
			/* should never be here - punt */
			state = done_state;
			break;
		}
	} while ( state != done_state );

	buf_append('\0', bp, buf, bufsz);
	return buf;

} /* getline() */

/*
 * Like strtok(), but no static data
 */
char *
strtoken(string, sepset)
	char **string;
	char *sepset;
{
	char *p;
	char *q;
	char *r;
	char *res;

	p = *string;
	if (p == 0) {
		return NULL;
	}
	q = p + strspn(p, sepset);
	if (*q == 0) {
		return NULL;
	}
	r = strpbrk(q, sepset);
	if (r == NULL) {
		*string = 0;
		if ((res = strdup(q)) == NULL)
			out_of_memory();
	} else {
		res = check_malloc((unsigned) (r - q + 1));
		(void) strncpy(res, q, r - q);
		res[r - q] = 0;
		*string = ++r;
	}
	return res;
}

char *
check_malloc(size)
	unsigned int size;
{
	register char *memoryp;

	if ((memoryp = malloc(size)) == NULL)
		out_of_memory();
	return memoryp;
}

char *
check_realloc(memoryp, size)
	char *memoryp;
	unsigned int size;
{

	if ((memoryp = realloc(memoryp, size)) == NULL)
		out_of_memory();
	return memoryp;
}

void
out_of_memory(void)
{
	(void) fprintf(stderr, "exportfs: Out of memory\n");
	exit(1);
}

void
cannot_open(error, filename)
	int error;
	char *filename;
{
	fprintf(stderr, "exportfs: cannot open %s", filename);
	if (error)
		fprintf(stderr, ": %s", strerror(error));
	fputc('\n', stderr);
}

#include <ctype.h>
#include <pwd.h>

int
map_user(name, uidp)
	char *name;
	int *uidp;
{
	struct passwd *pw;

	if (isdigit(*name) || (name[0] == '-' && isdigit(name[1]))) {
		*uidp = atoi(name);
		return 1;
	}
	pw = getpwnam(name);
	if (pw) {
		*uidp = pw->pw_uid;
		return 1;
	}
	fprintf(stderr, "exportfs: unknown user: %s\n", name);
	return 0;
}
