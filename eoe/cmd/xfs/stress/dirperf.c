#ident "$Revision: 1.1 $"

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * Loop over directory sizes:
 *	make m directories
 *	touch n files in each directory
 *	stat the files round-robin
 *	readdir/unlink the files 
 * Change directory sizes by multiplication or addition.
 * Allow control of starting & stopping sizes, name length, target directory.
 * Print size and wallclock time (ms per file).
 * Output can be used to make graphs (gnuplot)
 */

static uint_t	addval;
static uint_t	dirchars;
static char	*dirname;
static uint_t	firstsize;
static uint_t	lastsize;
static uint_t	minchars;
static double	mulval;
static uint_t	nchars;
static uint_t	ndirs;
static uint_t	pfxchars;
static uint_t	stats;

static void	filename(int, int, char *);
static int	hexchars(uint_t);
static uint_t	nextsize(uint_t);
static double	now(void);
static void	usage(void);

/*
 * Maximum size allowed, this is pretty nuts.
 * The largest one we've ever built has been about 2 million.
 */
#define	MAX_DIR_SIZE	(16 * 1024 * 1024)
#define	DFL_FIRST_SIZE	1
#define	DFL_LAST_SIZE	(1024 * 1024)
#define	MAX_DIR_COUNT	1024

int
main(int argc, char **argv)
{
	int		c;
	uint_t		cursize;
	DIR		*dirp;
	int		i;
	int		j;
	char		name[MAXNAMELEN];
	struct stat	stb;
	double		stime;

	while ((c = getopt(argc, argv, "a:c:d:f:l:m:n:s:")) != -1) {
		switch (c) {
		case 'a':
			addval = (uint_t)atoi(optarg);
			break;
		case 'c':
			nchars = (uint_t)atoi(optarg);
			break;
		case 'd':
			dirname = optarg;
			break;
		case 'f':
			firstsize = (uint_t)atoi(optarg);
			break;
		case 'l':
			lastsize = (uint_t)atoi(optarg);
			break;
		case 'm':
			mulval = atof(optarg);
			break;
		case 'n':
			ndirs = (uint_t)atoi(optarg);
			break;
		case 's':
			stats = (uint_t)atoi(optarg);
			break;
		case '?':
		default:
			usage();
			exit(1);
		}
	}
	if (!addval && !mulval)
		mulval = 2.0;
	else if ((addval && mulval) || mulval < 0.0) {
		usage();
		exit(1);
	}
	if (stats == 0)
		stats = 1;
	if (!dirname)
		dirname = ".";
	else {
		if (mkdir(dirname, 0777) < 0 && errno != EEXIST) {
			perror(dirname);
			exit(1);
		}
		if (chdir(dirname) < 0) {
			perror(dirname);
			exit(1);
		}
	}
	if (firstsize == 0)
		firstsize = DFL_FIRST_SIZE;
	else if (firstsize > MAX_DIR_SIZE)
		firstsize = MAX_DIR_SIZE;
	if (lastsize == 0)
		lastsize = DFL_LAST_SIZE;
	else if (lastsize > MAX_DIR_SIZE)
		lastsize = MAX_DIR_SIZE;
	if (lastsize < firstsize)
		lastsize = firstsize;
	minchars = hexchars(lastsize - 1);
	if (nchars < minchars)
		nchars = minchars;
	else if (nchars >= MAXNAMELEN)
		nchars = MAXNAMELEN - 1;
	if (ndirs > MAX_DIR_COUNT)
		ndirs = MAX_DIR_COUNT;
	dirchars = hexchars(ndirs);
	pfxchars = nchars - minchars;
	if (pfxchars)
		memset(&name[dirchars + 1], 'a', pfxchars);
	for (j = 0; j < ndirs; j++) {
		filename(0, j, name);
		name[dirchars] = '\0';
		mkdir(name, 0777);
	}
	for (cursize = firstsize;
	     cursize <= lastsize;
	     cursize = nextsize(cursize)) {
		stime = now();
		for (i = 0; i < cursize; i++) {
			for (j = 0; j < ndirs; j++) {
				filename((i + j) % cursize, j, name);
				close(creat(name, 0666));
			}
		}
		for (i = 0; i < cursize * stats; i++) {
			for (j = 0; j < ndirs; j++) {
				filename((i + j) % cursize, j, name);
				stat(name, &stb);
			}
		}
		for (j = 0; j < ndirs; j++) {
			filename(0, j, name);
			name[dirchars] = '\0';
			dirp = opendir(name);
			while (readdir(dirp))
				continue;
			closedir(dirp);
		}
		for (i = 0; i < cursize; i++) {
			for (j = 0; j < ndirs; j++) {
				filename((i + j) % cursize, j, name);
				unlink(name);
			}
		}
		printf("%d %.3f\n", cursize,
			(now() - stime) * 1.0e3 / (cursize * ndirs));
	}
	for (j = 0; j < ndirs; j++) {
		filename(0, j, name);
		name[dirchars] = '\0';
		rmdir(name);
	}
	return 0;
}

static void
filename(int idx, int dir, char *name)
{
	static char	hexc[16] = "0123456789abcdef";
	int		i;

	for (i = dirchars - 1; i >= 0; i--)
		*name++ = hexc[(dir >> (4 * i)) & 0xf];
	*name++ = '/';
	name += pfxchars;		/* skip pfx a's */
	for (i = minchars - 1; i >= 0; i--)
		*name++ = hexc[(idx >> (4 * i)) & 0xf];
	*name = '\0';
}

static int
hexchars(uint_t maxval)
{
	if (maxval < 16)
		return 1;
	if (maxval < 16 * 16)
		return 2;
	if (maxval < 16 * 16 * 16)
		return 3;
	if (maxval < 16 * 16 * 16 * 16)
		return 4;
	if (maxval < 16 * 16 * 16 * 16 * 16)
		return 5;
	if (maxval < 16 * 16 * 16 * 16 * 16 * 16)
		return 6;
	if (maxval < 16 * 16 * 16 * 16 * 16 * 16 * 16)
		return 7;
	return 8;
}

static uint_t
nextsize(uint_t cursize)
{
	double	n;

	n = cursize;
	if (addval)
		n += addval;
	else
		n *= mulval;
	if (n > (double)lastsize + 0.5)
		return lastsize + 1;	/* i.e. out of bounds */
	else if ((uint_t)n == cursize)
		return cursize + 1;
	else
		return (uint_t)n;
}

static double
now(void)
{
	struct timeval	tv;

	gettimeofday(&tv, NULL);
	return (double)tv.tv_sec + 1.0e-6 * (double)tv.tv_usec;
}

static void
usage(void)
{
	fprintf(stderr,
		"usage: dirperf [-d dir] [-a addstep | -m mulstep] [-f first] "
		"[-l last] [-c nchars] [-n ndirs] [-s nstats]\n");
}
