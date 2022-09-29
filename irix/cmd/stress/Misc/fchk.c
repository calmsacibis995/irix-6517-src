/*
 * fchk [num_passes]
 *
 * Randomly open, write patterns to, close, open, read pattern, close
 * a bunch of randomly sized files, checking that the read pattern
 * is what was written last.
 *
 * If a numeric argument "num_passes" is provided,
 * then this program exits after that many passes.
 *
 * Paul Jackson
 * Silicon Graphics
 * 13 Sept 89
 */

/*
 * Demonstrably correct Prime Number Generator, using
 * a prime modulus multiplicative linear congruential generator.
 * See Communciations of the ACM, October 1988, Volume 31, Number 10,
 * page 1195 for the algorithm (* Interger Version 2 *), and the
 * method for demonstrating correct implementation.
 * This integer implementation assumes that MAXINT is >= 2**31 - 1.
 *
 * Paul Jackson
 * Convergent Technologies
 * 25 Oct 88
 */

int seed = 1;

int
ran(void)
{
	int m = 2147483647;	/* 2**31 - 1 */
	int a = 16807;	/* Alternatives: 48271 or 69621 */
	int q = 127773;	/* Alternatives: 44488 or 30845 */
	int r = 2836;	/* Alternatives:  3399 or 23902 */

	int lo, hi, test;

	hi = seed/q;
	lo = seed%q;
	test = a*lo - r*hi;
	if (test > 0)
		seed = test;
	else
		seed = test + m;
	return seed;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <getopt.h>
#include <malloc.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>

#define V_NFLS	300	/* Number of files to manipulate */
#define V_PLEN	8	/* length of stdout progress count */
#define V_FLSZ	23000	/* max length of any one file */
#define V_NMSZ	4	/* length ea. filename: >= strlen V_NFLS in decimal */
#define V_WRT	3	/* once in V_WRT times - write, other times check */
#define V_MODE	0644	/* file create mode in octal */

int pass = 0;
int maxpass = 0;
ssize_t maxlength = V_FLSZ;
int maxfiles = V_NFLS;
size_t namesize = V_NMSZ;
int nxtflnum = 0;
int verbose = 0;
char *td;		/* temp dir */
char *buf;
char *wd;

typedef enum { false, true } boolean;
enum { init, rewrite, check } act;

struct data {
	boolean valid_name;
	char *name;
	boolean valid_size;
	size_t size;
	int key;
} *data;

void cleanup(void);
void panic(char *msg);
void chk_old_pattern(struct data *dp);
void gen_new_pattern(struct data *dp);
boolean chkbuf(int key, size_t sz);
void filbuf(int key, size_t sz);
int select_rewrite(void);
char *nmalloc(int num);

int
main(int argc, char **argv)
{
	struct data *dp;
	int d;
	int c;


	while((c = getopt(argc, argv, "vl:n:f:")) != EOF)
		switch (c) {
		case 'v':
			verbose++;
			break;
		case 'l':
			maxlength = atoi(optarg);
			break;
		case 'n':
			maxpass = atoi(optarg);
			break;
		case 'f':
			maxfiles = atoi(optarg);
			namesize = strlen(optarg) + 1;
			break;
		default:
			fprintf(stderr, "Usage: fchk [-v][-l maxlen][-n maxpass][-f[maxfiles]\n");
			exit(-1);
		}

	wd = getcwd(NULL, PATH_MAX);
	buf = malloc(maxlength);
	data = malloc(maxfiles * sizeof(*data));

	for (d=0; d<maxfiles; d++) {
		data[d].valid_name = false;
		data[d].valid_size = false;
	}

	/* create temp directory for all the files */
	td = tempnam(NULL, "fchk");
	if (mkdir(td, 0777) != 0) {
		perror(td);
		panic("mkdir");
	}
	if (chdir(td) != 0) {
		perror(td);
		panic("chdir");
	}

	for(;;){
		if (maxpass != 0 && pass == maxpass) {
			puts("");
			cleanup();
			exit (0);
		}
		if (verbose)
			(void) printf ("\r%*d ", V_PLEN, pass);
		pass++;
		d = ran() % maxfiles;
		dp = &data[d];

		if (dp->valid_name == false) {
			dp->name = nmalloc (nxtflnum++);
			dp->valid_name = true;
		}

		if (dp->valid_size == false)
			act = init;
		else if (select_rewrite())
			act = rewrite;
		else
			act = check;

		if (verbose)
			if (act == init)
				(void) printf ("**");
			else
				(void) printf ("  ");

		switch (act) {
		case init:	/* this file needs init from scratch */
		case rewrite:	/* rewrite fresh data into this file */
			gen_new_pattern(dp);
			break;
		case check:	/* read and check for valid file contents */
			chk_old_pattern(dp);
			break;
		default:
			panic ("switch");
		}
		(void) fflush (stdout);
	}
	/* NOTREACHED */
}

char *
nmalloc(int num)
{
	char *s;

	if ((s = malloc (namesize + 2)) == 0)
		panic("no mem");
	(void) sprintf (s, "%0*d", namesize, num);
	return (s);
}

int
select_rewrite(void)
{
	return ((ran() % V_WRT) == 0);
}

void
filbuf(int key, size_t sz)
{
	int *p = (int *)buf;

	while (p < (int *)(((long)(buf+sz)) & ~(sizeof(*p)-1L)))
		*p++ = key++;
}

boolean
chkbuf(int key, size_t sz)
{
	int *p = (int *)buf;

	while (p < (int *)(((long)(buf+sz)) & ~(sizeof(*p)-1L)))
		if (*p++ != key++)
			return false;
	return true;
}

void
gen_new_pattern(struct data *dp)
{
	int fd;
	ssize_t i;

	if (verbose)
		(void) printf (" W");

	dp->size = ran() % maxlength;
	dp->valid_size = true;
	dp->key = ran();
	filbuf (dp->key, dp->size);

	if ((fd = open (dp->name, O_RDWR|O_CREAT|O_TRUNC, V_MODE)) < 0) {
		perror (dp->name);
		panic ("open for write");
	}

	if ((i = write (fd, buf, dp->size)) != dp->size) {
		if (i < 0)
			perror (dp->name);
		else
			fprintf(stderr, "fchk:%s:short write %d expected %d\n",
				dp->name, i, dp->size);
		panic ("write");
	}

	if (verbose)
		(void) printf ("   ");
	(void) close (fd);
}

void
chk_old_pattern(struct data *dp)
{
	int fd;
	ssize_t i;

	if (verbose)
		(void) printf (" R");

	if ((fd = open (dp->name, O_RDONLY)) < 0) {
		perror (dp->name);
		panic("open for read");
	}

	if ((i = read(fd, buf, maxlength)) != dp->size) {
		if (i < 0)
			perror (dp->name);
		else
			fprintf(stderr, "fchk:%s:short read %d expected %d\n",
				dp->name, i, dp->size);
		panic ("read");
	}

	if (chkbuf (dp->key, dp->size) != true) {
		(void) puts ("");
		(void) fflush (stdout);
		(void) fprintf (stderr,
			"fchk:Chk failed: name %s, sz %d, key %d\n",
			dp->name, dp->size, dp->key
		);
		/* leave files around */
		abort();
	}

	if (verbose)
		(void) printf (" OK");
	(void) close (fd);
}

void
panic(char *msg)
{
	(void) fprintf (stderr, "fchk:Panic: %s\n", msg);
	cleanup();
	abort();
}

void
cleanup(void)
{
	char cmd[1024];

	chdir(wd);
	strcpy(cmd, "rm -f ");
	strcat(cmd, td);
	strcat(cmd, "/*");
	printf("fchk doing %s\n", cmd);
	system(cmd);
	rmdir(td);
}
