#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <paths.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <limits.h>
#include <fcntl.h>
#include <libelf.h>
#include <sys/types.h>
#include <sys/procfs.h>
#include <sys/stat.h>

#define DEVDIR		"/dev"
#define DEVZERO		"/dev/zero"
#define RLD		"/lib/rld"
#define DEFLIBPATH	"/lib:/usr/lib" \
			":/lib32:/usr/lib32" \
			":/lib64:/usr/lib64" \
			":/usr/lib/dmedia/compression" \
			":/usr/lib/dmedia/movie" \
			":/usr/lib/dmedia/video"

#define RLD_TEXT	0x0fb60000
#define RLD_BSS_LO	0x0fbc0000
#define RLD_BSS_HI	0x0fc40000

char   *progname;
int     longlist = 0;
int     period = 0;
int     hexsizes = 0;
int     pagesize;

#define btop(b)		((b) / pagesize)
#define btok(b)		((b) / 1024)
#define ptok(p)		((p) * pagesize / 1024)

struct displayops {
	void    (*do_pheader) (int);
	void    (*do_pmap) (prpsinfo_t *, prmap_sgi_t *, int);
};
struct displayops *curdisplayops;

void
usage(void)
{
	fprintf(stderr, "Usage: %s [-adklx][-c cmdname][-p pid][-P period]\n",
	    progname);
	fprintf(stderr, "\t-a\t\tdisplay all fields\n");
	fprintf(stderr, "\t-c cmdname\tonly for commands \"cmdname\"\n");
	fprintf(stderr, "\t-d\t\tdisplay numbers in decimal pages (default)\n");
	fprintf(stderr, "\t-k\t\tdisplay numbers in kbytes\n");
	fprintf(stderr, "\t-l\t\tlong listing\n");
	fprintf(stderr, "\t-p pid\t\tdisplay for pid \"pid\"\n");
	fprintf(stderr, "\t-P period\tredisplay every \"period\" seconds\n");
	fprintf(stderr, "\t-x\t\tdisplay numbers is hex pages\n");
	exit(1);
}


char   *
fmtdx(char *fmt)
{
	char   *cp, dx;

	dx = hexsizes ? 'x' : 'd';

	for (cp = fmt; *cp; cp++) {
		if (*cp == '?') {
			*cp = dx;
		}
	}
	return fmt;
}


char   *
wnormalize(int wsize)
{
	static char buf[16];
	double  normsize = (double)wsize / MA_WSIZE_FRAC;

	if (hexsizes) {
		sprintf(buf, "%6x", (int)normsize);
	} else {
		sprintf(buf, "%6.1lf", normsize);
	}

	return buf;
}


char   *
spacepad(int val, int len)
{
	static char buf[16];

	sprintf(buf, fmtdx("%?"), val);

	len -= strlen(buf);
	if (len < 0) {
		len = 0;
	}
	buf[len] = '\0';
	while (--len >= 0) {
		buf[len] = ' ';
	}
	return buf;
}


char   *
flagstostr(ulong_t flags, int all)
{
	static char buf[16];

	buf[0] = flags & MA_READ ? 'r' : '-';
	buf[1] = flags & MA_WRITE ? 'w' : '-';
	buf[2] = flags & MA_EXEC ? 'x' : '-';

	if (all) {
		buf[3] = flags & MA_SHARED ? 's' : ' ';
		buf[4] = flags & MA_BREAK ? 'B' : ' ';
		buf[5] = flags & MA_STACK ? 'S' : ' ';
		buf[6] = flags & MA_PHYS ? 'p' : ' ';
		buf[7] = flags & MA_PRIMARY ? 'P' : ' ';
		buf[8] = flags & MA_SREGION ? 'R' : ' ';
		buf[9] = flags & MA_COW ? 'C' : ' ';
		buf[10] = flags & MA_NOTCACHED ? 'N' : ' ';
		buf[11] = flags & MA_SHMEM ? 'M' : ' ';
	} else {
		buf[3] = '\0';
	}

	return buf;
}


int
isstack(prmap_sgi_t * m)
{
	return (int)(m->pr_mflags & MA_STACK);
}

int
isbreak(prmap_sgi_t * m)
{
	return (int)(m->pr_mflags & MA_BREAK);
}

int
isshmem(prmap_sgi_t * m)
{
	return (int)(m->pr_mflags & MA_SHMEM);
}

#if 0
int
isrld(prmap_sgi_t * m)
{
	static dev_t rlddev;
	static ino_t rldino;

	if (!rlddev) {
		struct stat sb;

		if (stat(RLD, &sb) >= 0) {
			rlddev = sb.st_dev;
			rldino = st.st_ino;
		}
	}
	return m->pr_ino == rldino && m->pr_dev == rlddev;
}

int
isrldtext(prmap_sgi_t * m)
{
	return m->p_vaddr == (char *)RLD_TEXT;
}

#endif

int
isrldbss(prmap_sgi_t * m)
{
	unsigned int vaddr = (unsigned int)m->pr_vaddr;

	return vaddr >= RLD_BSS_LO && vaddr < RLD_BSS_HI;
}

char   *
ino_name(char *dirpath, ino_t ino, int subdirs)
{
	DIR    *dirp;
	struct dirent *dp;
	char   *pathfound = NULL, *edirpath;
	struct stat sb;
	static char path[PATH_MAX];	/* NB: static and recursion */

	if ((dirp = opendir(dirpath)) == NULL) {
		return NULL;
	}

	if (dirpath != path) {
		strcpy(path, dirpath);
	}
	edirpath = &path[strlen(path)];
	*edirpath++ = '/';

	while (!pathfound && (dp = readdir(dirp)) != NULL) {
		if (strcmp(dp->d_name, ".") == 0 ||
		    strcmp(dp->d_name, "..") == 0) {
			continue;
		}

		strcpy(edirpath, dp->d_name);

		if (stat(path, &sb) < 0) {
			continue;
		}

		if (S_ISDIR(sb.st_mode) && subdirs) {
			pathfound = ino_name(path, ino, subdirs);
			continue;
		}
		if (sb.st_ino == ino) {
			pathfound = path;
		}
	}

	closedir(dirp);
	return pathfound;
}

char   *
devicename(prmap_sgi_t * m)
{
	static dev_t devdir_dev = 0;
	char   *dname = NULL;

	if (!devdir_dev) {
		struct stat sb;

		if (stat(DEVDIR, &sb) < 0) {
			perror("stat /dev");
		} else {
			devdir_dev = sb.st_dev;
		}
	}
	if (m->pr_dev == devdir_dev) {
		dname = ino_name(DEVDIR, m->pr_ino, 1);
	}

	return dname ? dname : "device";
}


char   *
shlibpath(prmap_sgi_t * m)
{
	static char *libdirs;
	char   *name, *libdir, *libend;

	if (!libdirs) {
		char   *d, *elibdirs;

		libdirs = getenv("PREG_LIBRARY_PATH");
		if (!libdirs) {
			libdirs = getenv("LD_LIBRARY_PATH");
		}
		if (!libdirs) {
			libdirs = DEFLIBPATH;
		}

		if (d = strstr(libdirs, "DEFAULT")) {
			int     newlen;

			/*
			 * Substitute for default path for "DEFAULT".
			 */
			newlen = strlen(libdirs) - strlen("DEFAULT") +
			    strlen(DEFLIBPATH) + 1;
			elibdirs = malloc(newlen);
			if (elibdirs) {
				*d = '\0';
				strcpy(elibdirs, libdirs);
				strcat(elibdirs, DEFLIBPATH);
				if (d[sizeof("DEFAULT") - 1]) {
					strcat(elibdirs,
					    &d[sizeof("DEFAULT") - 1]);
				}
			}
			libdirs = elibdirs;
		}
	}
	name = NULL;
	libdir = libdirs;
	while (!name && libdir) {
		struct stat sb;

		libend = strchr(libdir, ':');
		if (libend) {
			*libend = '\0';
		}

		if (stat(libdir, &sb) == 0 && sb.st_dev == m->pr_dev) {
			name = ino_name(libdir, m->pr_ino, 0);
		}

		if (libend) {
			*libend++ = ':';
		}
		libdir = libend;
	}

	return name;
}


char   *
flagstype(prmap_sgi_t * m)
{
	if (m->pr_mflags & MA_EXEC) {
		return "text";
	} else if (m->pr_mflags & MA_COW) {
		return "data";
	}
	return "";
}


char   *
basename(char *path)
{
	char   *slash;

	slash = strrchr(path, '/');

	return slash ? slash + 1 : path;
}


void
idregion(prpsinfo_t * pi, prmap_sgi_t * m, int longlist,
    char **rname, char **rtype, char *buf)
{
	char   *shlpath;

	if (isstack(m)) {
		*rname = pi->pr_fname;
		*rtype = "stack";
	} else if (isshmem(m)) {
		*rname = pi->pr_fname;
		*rtype = "shmem";
	} else if (isbreak(m)) {
		*rname = pi->pr_fname;
		*rtype = "break";
	} else if ((m->pr_mflags & MA_MAPZERO) && isrldbss(m)) {
		*rname = "rld";
		*rtype = "heap";
	} else if (m->pr_mflags & MA_PHYS) {
		*rname = devicename(m);
		*rtype = "device";
	} else if (m->pr_mflags & MA_PRIMARY) {
		*rname = pi->pr_fname;
		*rtype = flagstype(m);
	} else if ((shlpath = shlibpath(m)) != NULL) {
		*rname = longlist ? shlpath : basename(shlpath);
		*rtype = flagstype(m);
	} else if (m->pr_ino) {
		sprintf(buf, "mapfile #%llu", m->pr_ino);
		*rname = buf;
		*rtype = "file";
	} else if (m->pr_mflags & MA_MAPZERO) {
		*rname = pi->pr_fname;
		*rtype = "zero";
	} else {
		*rname = "unknown";
		*rtype = "unknown";
	}
}


void
pall_pheader(int longlist)
{
	printf("    VPAGE  SIZE     OFF  VALID PRIVAT  WSIZE  REF/MOD  FLAGS");
	if (longlist) {
		printf("             DEV INO");
	}
	printf("\n");
}

/*ARGSUSED */
void
pall_pmap(prpsinfo_t * pi, prmap_sgi_t * m, int longlist)
{
	printf(fmtdx("    %5x %5? %7? %6? %6? %.6s %4?/%?%s %s"),
	    btop((int)m->pr_vaddr), btop(m->pr_size), m->pr_off,
	    m->pr_vsize, m->pr_psize, wnormalize(m->pr_wsize),
	    m->pr_rsize, m->pr_msize, spacepad(m->pr_msize, 4),
	    flagstostr(m->pr_mflags, 1));
	if (longlist) {
		printf(fmtdx(" %8? %?"), m->pr_dev, m->pr_ino);
	}
	printf("\n");
}

/*ARGSUSED */
void
psizes_pheader(int longlist)
{
	printf("    VPAGE  SIZE USE VALID  WSIZE  REF/MOD PERM REGION\n");
}

void
psizes_pmap(prpsinfo_t * pi, prmap_sgi_t * m, int longlist)
{
	char   *rname, *rtype;
	char    buf[32];

	idregion(pi, m, longlist, &rname, &rtype, buf);
	printf(fmtdx("    %5x %5? %2d %6? %.6s %4?/%?%s %s %s %s\n"),
	    btop((int)m->pr_vaddr), btop(m->pr_size),
	    m->pr_mflags >> MA_REFCNT_SHIFT,
	    m->pr_vsize, wnormalize(m->pr_wsize),
	    m->pr_rsize, m->pr_msize, spacepad(m->pr_msize, 4),
	    flagstostr(m->pr_mflags, 0), rname, rtype);
}

void
pregions(prpsinfo_t * pi, prmap_sgi_t * maps)
{
	prmap_sgi_t *m;
	double  normwsize = 0.0;

	for (m = maps; m->pr_size; m++) {
		normwsize += (double)m->pr_wsize / MA_WSIZE_FRAC;
	}

	printf("%-8s %5d %5d %d:%d:%d\n",
	    pi->pr_fname, pi->pr_pid, pi->pr_ppid,
	    pi->pr_size, pi->pr_rssize, (int)normwsize);

	curdisplayops->do_pheader(longlist);

	for (m = maps; m->pr_size; m++) {
		curdisplayops->do_pmap(pi, m, longlist);
	}
}

struct displayops displayops[] =
{
#define DISPLAYALL	0
    {pall_pheader, pall_pmap},
#define DISPLAYSIZES	1
    {psizes_pheader, psizes_pmap},
    {0, 0}
#define DISPLAYDEFAULT	DISPLAYSIZES
};

int
pregpid(pid_t pid, char *cmdname)
{
	int     pfd, nmaps;
	prpsinfo_t psinfo;
	prmap_sgi_arg_t parg;
	static size_t maptabsz = 0;
	static prmap_sgi_t *maptab = NULL;
	static char pname[PATH_MAX];

	sprintf(pname, "%s%c%05d", _PATH_PROCFS, '/', pid);

	if ((pfd = open(pname, O_RDONLY)) < 0) {
		return -1;
	}

	/* Read process info. */
	if (ioctl(pfd, PIOCPSINFO, (char *)&psinfo) < 0) {
		fprintf(stderr, "%s: PIOCPSINFO %d: %s\n",
		    progname, pid, strerror(errno));
		(void)close(pfd);
		return -1;
	}
	if (cmdname && strncmp(psinfo.pr_fname, cmdname, strlen(cmdname))) {
		(void)close(pfd);
		return -1;
	}
	/* Get current number of mappings. */
	if (ioctl(pfd, PIOCNMAP, (char *)&nmaps) < 0) {
		fprintf(stderr, "%s: PIOCNMAP %d: %s\n",
		    progname, pid, strerror(errno));
		(void)close(pfd);
		return -1;
	}
	/* kthreads have 0 nmaps.  Skip 'em. */
	if (nmaps == 0) {
		(void)close(pfd);
		return -1;
	}
	/*
	 * Be sure we have enough space for mappings.
	 * "+ 2" slop for changes while we're running.
	 */
	if (maptabsz < (nmaps + 2) * sizeof(prmap_sgi_t)) {
		size_t  newsz = (nmaps + 2) * sizeof(prmap_sgi_t);

		maptab = (prmap_sgi_t *) realloc((char *)maptab, newsz);
		if (maptab == NULL) {
			fprintf(stderr, "%s: no memory for %d (%d)\n",
			    progname, pid, nmaps);
			maptab = NULL;	/* start over next time around */
			(void)close(pfd);
			return -1;
		}
		maptabsz = newsz;
	}
	parg.pr_vaddr = (char *)maptab;
	parg.pr_size = maptabsz;
	if (ioctl(pfd, PIOCMAP_SGI, (char *)&parg) < 0) {
		fprintf(stderr, "%s: PIOCCMAP_SGI %d: %s\n",
		    progname, pid, strerror(errno));
		(void)close(pfd);
		return -1;
	}
	pregions(&psinfo, maptab);

	(void)close(pfd);

	return 0;
}

void
main(int argc, char *argv[])
{
	int     opt;
	pid_t   pid = 0;
	char   *cmdname = NULL;
	DIR    *pidp;
	struct dirent *pient;
	time_t  starttm, nowtm;
	extern char *optarg;
	extern int optind;

	progname = basename(argv[0]);
	curdisplayops = &displayops[DISPLAYDEFAULT];
	pagesize = getpagesize();

	while ((opt = getopt(argc, argv, "ac:dlp:P:x")) != EOF) {
		switch (opt) {
		    case 'a':
			    curdisplayops = &displayops[DISPLAYALL];
			    break;
		    case 'c':
			    cmdname = optarg;
			    break;
		    case 'd':
			    hexsizes = 0;
			    break;
		    case 'l':
			    longlist = 1;
			    break;
		    case 'p':
			    pid = atoi(optarg);
			    break;
		    case 'P':
			    period = atoi(optarg);
			    break;
		    case 'x':
			    hexsizes = 1;
			    break;
		    case '?':
		    default:
			    usage();
			    /* NOTREACHED */
			    break;
		}
	}

	if (optind < argc) {
		if (isdigit(*argv[optind])) {
			pid = atoi(argv[optind]);
		} else {
			cmdname = argv[optind];
		}
	}
	time(&starttm);

      again:
	if (period) {
		time(&nowtm);
		printf("Time: %d\n", nowtm - starttm);
	}
	if (pid) {
		pregpid(pid, cmdname);
	} else {

		if ((pidp = opendir(_PATH_PROCFS)) == NULL) {
			fprintf(stderr, "%s: opendir %s: %s\n",
			    progname, _PATH_PROCFSPI, strerror(errno));
			exit(1);
		}
		while (pient = readdir(pidp)) {
			if (pient->d_name[0] == '.') {	/* skip "." and ".." */
				continue;
			}

			pregpid(atoi(pient->d_name), cmdname);
		}
		(void)closedir(pidp);
	}

	if (period) {
		fflush(stdout);
		sleep(period);
		goto again;
	}
	exit(0);
}
