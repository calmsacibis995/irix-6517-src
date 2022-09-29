/*
 * Create files 00000 to V_NFLS-1, and then read them back in order, checking
 * that the contents are the same as written.  Each block (sector) is given
 * a unique identifier consisting of the complete filename, block number,
 * and pass number, where pass number is the number of times that V_NFLS
 * files have been written and checked.  This program is most useful in
 * debugging and detecting sector scrambling.
 *
 * It is possible to choose directio, select the directory to run it in and
 * limit the count of files and interations.
 *
 */

#include <sys/types.h>
#include <getopt.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <malloc.h>
#include <sys/param.h>

#define V_NFLS	600	/* Number of files to manipulate */
#define V_FLSZ	0x100000	/* max length of any one file */
#define V_NMSZ	5	/* length ea. filename: >= strlen V_NFLS in decimal */
#define V_MODE	0644	/* file create mode in octal */
#define V_INTERVAL 1	/* Number of passes between printing status to stderr */
#define V_DIO_MEM dioa.d_mem	/* Memory alignment requirement for DIO */
#define V_DIO_MINSZ dioa.d_miniosz /* Minimum request size fo DIO */

int pass;
char *localbuf;
char workingdir[MAXPATHLEN];
int directio = 0;
int nfiles = V_NFLS;	/* Number of files to manipulate */

typedef enum { false, true } boolean;
enum { init, rewrite, check } act;

struct data {
	char *name;
	unsigned size;
};

static struct data data[V_NFLS];
static struct dioattr dioa;

static int pagesize;
#undef _PAGESZ
#define _PAGESZ		pagesize

int
ran()
{
	register int size;
	if (directio)
		size = (random() % V_FLSZ) & ~(V_DIO_MINSZ - 1);
	else
		size = (random() % V_FLSZ) & ~(BBSIZE - 1);
	return size;
}


void
panic (msg)
 char *msg;
{
	printf("sectorcheck: file i/o error in %s - %s\n", workingdir, msg);
	fprintf(stderr, "sectorcheck: file i/o error in %s\n", workingdir);
	fflush(stderr);
	exit(1);
}


char *
nmalloc (num)
 int num;
{
	char *s;

	if ((s = malloc (V_NMSZ + 2)) == 0)
		panic("no mem");
	sprintf (s, "%0*d", V_NMSZ, num);
	return (s);
}


void
filbuf (name, sz)
 char *name;
 unsigned sz;
{
	int i = 0;

	while (i < sz)
	{
		sprintf((localbuf+i), "File name %s/%s, pass %d, block %d\n",
			workingdir, name, pass, i/BBSIZE);
		i += BBSIZE;
	}
}


boolean
chkbuf (name, sz)
 char *name;
 unsigned sz;
{
	int i = 0;
	boolean fchkerr = false;
	char buffer[MAXPATHLEN];

	while (i < sz)
	{
		sprintf(buffer, "File name %s/%s, pass %d, block %d\n",
			workingdir, name, pass, i/BBSIZE);
		if (strcmp(buffer, (localbuf+i)) != 0)
		{
			if (!fchkerr)
				printf("error in file %s/%s -- offset = %d\n",
                                       workingdir, name, i);
			printf("read:     %sexpected: %s\n",
			       (localbuf+i), buffer);
			fchkerr = true;
		}
		i += BBSIZE;
	}
	return fchkerr;
}


void
gen_new_pattern(dp)
 struct data *dp;
{
	int fd;
	int opts = O_RDWR|O_CREAT|O_TRUNC;

	dp->size = ran();
	filbuf(dp->name, dp->size);

	if (directio)
		opts |= O_DIRECT;
	if ((fd = open (dp->name, opts, V_MODE)) < 0) {
		perror (dp->name);
		panic ("unable to open for write");
	}

	if (write (fd, localbuf, dp->size) != dp->size) {
		perror (dp->name);
		panic ("unable to write");
	}

	close (fd);
}


void
chk_old_pattern(dp)
 struct data *dp;
{
	int fd;
	int opts = O_RDONLY;
	int count;

	if (directio)
		opts |= O_DIRECT;
	if ((fd = open (dp->name, opts)) < 0) {
		perror (dp->name);
		panic("unable to open for read");
	}

	memset(localbuf, 'u', dp->size);
	if ((count = read(fd, localbuf, V_FLSZ)) < 0) {
		perror (dp->name);
		panic ("unable to read");
	} else if (count != dp->size)
		fprintf(stderr,
			"short read: expected %d, got %d\n", dp->size, count);

	if (chkbuf(dp->name, dp->size)) {
		fprintf (stderr, "sectorcheck: %s/%s - compare error\n",
		         workingdir, dp->name);
		fflush(stderr);
		exit (2);
	}

	close(fd);
}


void
usage(void)
{
	fprintf(stderr, "sectorcheck [-D] [-d direcotory] [-p pass] [-c count]\n");
	fprintf(stderr, "\t-D : use direct i/o (not implemented)\n");
	fprintf(stderr, "\t-d directory : chdir to the directory\n");
	fprintf(stderr, "\t-p pass : run pass iterations\n");
	fprintf(stderr, "\t-c count : test for count files\n");
	exit(1);
}

void
main (argc, argv)
 int argc;
 char *argv[];
{
	struct data	*dp;
	int		 maxpass = 0x7FFFFFFF;
	int		 i, d;
	signed char	 c;
	char		*directory = NULL;
	int              alignsz;

	pagesize = getpagesize();
	srandom(time(0));
	while ((c = getopt(argc, argv, "Dd:p:c:")) != EOF) {
		switch (c) {
		case 'D':
			directio = 1;
			break;
		case 'p':
			maxpass = atoi(optarg);
			break;
		case 'd':
			directory = optarg;
			break;
		case 'c':
			nfiles = atoi(optarg);
			if (nfiles > V_NFLS || nfiles < 0)
				nfiles = V_NFLS;
			break;
		default:
			usage();
		}
	}
	if (optind != argc)
		usage();
		
	for (d = 0; d < nfiles; d++)
		data[d].name = nmalloc(d);

	if ((directory != NULL) && (chdir(directory) < 0)) {
		perror("chdir");
		exit(1);
	}

	if (getcwd(workingdir, sizeof(workingdir)-1) == NULL)
		panic("unable to get current directory");

	if (directio) {
	  static int fd=0;

	  if ((fd = open(workingdir, O_RDONLY|O_DIRECT)) == -1) {
	    perror("open");
	    exit(1);
	  }
	  if (fcntl(fd, F_DIOINFO, &dioa)) {
	    perror("fcntl");
	    exit(1);
	  }	
	  close(fd);
	}

	alignsz = NBPC;
	if (directio && V_DIO_MEM > alignsz)
	  alignsz = V_DIO_MEM;
	if ((localbuf = (char *)malloc(V_FLSZ + alignsz)) == NULL) {
		perror("Cannot malloc buffer space");
		exit(1);
	}
	localbuf = (char *)((long)(localbuf + alignsz - 1) & ~(alignsz-1));

	for (pass = 1; pass <= maxpass; pass++) {
		for (d = 0; d < nfiles; d++) {
			dp = &data[d];
			gen_new_pattern(dp);
		}
		for (d = 0; d < nfiles; d++) {
			dp = &data[d];
			chk_old_pattern(dp);
		}
		if (pass % V_INTERVAL == 0)
		{
			fprintf(stderr, "sectorcheck: %s pass %d complete\n",
			        workingdir, pass);
			fflush(stderr);
		}
	}
	fprintf(stderr, "sectorcheck: %s done\n", workingdir);
	fflush(stderr);
	exit(0);
}
