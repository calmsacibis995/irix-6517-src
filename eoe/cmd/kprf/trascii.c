#ident  "$Revision: 1.1 $"

/*
 * Name:
 *	trascii
 *
 * Usage:
 *	tracex ordfile eventfile
 *
 * Description:
 *	'ordfile' is a binary funcname-to-ordinal config file
 * 		created by "nm -B /unix | ord > ordfile"
 *	'eventfile' is created by tracex
 *
 * 	trascii converts the eventfile from internal data
 *	format to ascii format printing to stdout
 *
 * Example:
 *	tracex ordfile /tmp/out.cpu0 > /tmp/cpu0.ascii
 */

#include	<sys/types.h>
#include	<sys/stat.h>
#include	<sys/time.h>
#include	<sys/utsname.h>
#include	<sys/mman.h>
#include	<sys/syssgi.h>
#include	<limits.h>
#include	<fcntl.h>
#include	<stdio.h>
#include	<unistd.h>
#include	<nlist.h>

/*
 * Conversion between machine-specific realtime clock ticks
 * and microseconds is done at the time trace records
 * are read from the kernel.
 */
int	ticksperus;
int	tickwrap;

#define	USPERSEC	1000000
#define	BITSPERINT	32

#define	HIHALF		0xffff0000
#define	LOHALF		0x0000ffff

#define	MAXNAMETAB	10000

struct	nameent {
	char	name[28];
	int	ord;
};

int	numnames = 0;
struct	nameent	nametab[MAXNAMETAB];

struct	tracerec {
	uint_t	ord;
	uint_t	time;
};

#define	MAXNCPU		28

int	ncpu;

#define	TRRECSZ		(sizeof (struct tracerec))
#define	TRRECTABSZ	(64 * 1024)
#define	TRENABLETABSZ	(1024)

#define	TRMAXINDEX	(TRRECTABSZ/sizeof(struct tracerec))
#define	TRINDEXMASK	(TRMAXINDEX-1)

struct	tracerec	trrectab[TRMAXINDEX];
uint_t	trindextab[MAXNCPU];
uint_t	trenabletab[TRENABLETABSZ/sizeof(int)];

int	kmemfd;

FILE	*outfptab[MAXNCPU];

extern	char	*findname();

main(ac, av)
int	ac;
char	*av[];
{
	FILE	*fp;
	int	i;
	struct	tracerec	t;
	int	lasttime;
	uint_t	timeadj;

	if (ac != 3)
		usage(av[0]);
		
	loadnametab(av[1]);

	machdep();

	if ((fp = fopen(av[2], "r")) == NULL)
		syserr(av[2]);

	lasttime = 0;
	timeadj = 0;

	while (fread(&t, sizeof (struct tracerec), 1, fp) > 0) {

		t.time /= ticksperus;

		if (lasttime == 0)
			lasttime = t.time;

		if (t.time < (lasttime - 1000))
			timeadj += tickwrap;

		lasttime = t.time;
		t.time += timeadj;

		printf("%f\t%s\n",
			(float) t.time/USPERSEC,
			findname(t.ord));
	}
}

machdep()
{
	unsigned int cycleaddr, cycleval;

	if ((cycleaddr = syssgi(SGI_QUERY_CYCLECNTR, &cycleval)) < 0)
		syserr("syssgi SGI_QUERY_CYCLECNTR failed");

	ticksperus = (1000 * 1000) / cycleval;
	tickwrap = 0xffffffff / ticksperus;
}

syserr(s)
char	*s;
{
	perror(s);
	exit(1);
}

loadnametab(file)
char	*file;
{
	int	fd;
	int	n;

	if ((fd = open(file, 0)) < 0)
		syserr(file);
	if ((n = read(fd, nametab,
		MAXNAMETAB * sizeof (struct nameent))) < 0)
		syserr(file);
	close(fd);

	/*
	 * ordinals start at "1" and must be sorted
	 */
	if (nametab[0].ord != 1) {
		fprintf(stderr, "invalid ordfile: %s\n", file);
		exit(1);
	}

	numnames = n / sizeof (struct nameent);
}

/*
 * Binary sort exact match.
 */
findord(name)
char	*name;
{
	int	i, low, high;

	low = 0;
	high = numnames;
	i = (low + high)/2;

	while (1) {
		if (strcmp(name, nametab[i].name) == 0)
			return (nametab[i].ord);

		if ((high - low) <= 1)
			break;

		if (strcmp(name, nametab[i].name) < 0)
			high = i;
		else
			low = i;
		i = (low + high) / 2;
	}

	return (0);	/* unrecognized name */
}

/*
 * Binary sort exact match.
 */
char*
findname(ord)
int	ord;
{
	int	i, low, high;
	static	char	string[80];
	int	saveord;

	saveord = ord;
	ord &= 0x7fffffff;	/* ignore hi bit */

	low = 0;
	high = numnames;
	i = (low + high)/2;

	while (1) {
		if (nametab[i].ord == ord) {
			strcpy(string, nametab[i].name);
			if (saveord & 0x80000000)
				strcat(string, "_exit");
			else
				strcat(string, "_enter");
			return (string);
		}

		if ((high - low) <= 1)
			break;

		if (ord < nametab[i].ord)
			high = i;
		else
			low = i;
		i = (low + high) / 2;
	}

	sprintf(string, "ord%d", ord);
	return (string);
}

err(fmt, a1, a2, a3, a4)
char	*fmt;
char	*a1, *a2, *a3, *a4;
{
	fprintf(stderr, fmt, a1, a2, a3, a4);
	fprintf(stderr, "\n");
	exit(1);
}

usage(s)
char	*s;
{
	fprintf(stderr, "Usage:  %s ordfile eventfile\n", s);
	exit(1);
}
