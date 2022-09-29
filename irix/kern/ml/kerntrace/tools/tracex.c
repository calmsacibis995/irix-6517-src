/*
 * Name:
 *	tracex
 *
 * Usage:
 *	tracex ordfile funcfile outfile kernel cmd [ args ]
 *
 * Description:
 *	'ordfile' is a binary funcname-to-ordinal config file
 * 		created by "nm -B /unix | ord > ordfile"
 *	'funcfile' is an ascii list of kernel function names,
 *		one per line
 *	'outfile' is the output file basename
 *	'kernel' is the name of the booted kernel
 *	'cmd' is the command to exec
 *	'args' are the args for that command
 *
 * Example:
 *	tracex ordfile funcfile /tmp/out kernel sleep 3000
 */

#include	<sys/types.h>
#include	<sys/mman.h>
#include	<sys/stat.h>
#include	<sys/sysmp.h>
#include	<sys/time.h>
#include	<sys/prctl.h>
#include	<sys/schedctl.h>
#include	<sys/utsname.h>
#include	<limits.h>
#include	<fcntl.h>
#include	<stdio.h>
#include	<unistd.h>
#include	<nlist.h>
#include	<signal.h>
#include	"trace.h"

#define	UNIXFILE	"/unix"
#define	KMEMFILE	"/dev/kmem"

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

int	numnames = 0;
struct	nameent	nametab[MAXNAMETAB];

#if (_MIPS_SZPTR == 64) || (_MIPS_SZLONG == 64) || (_MIPS_SZINT == 64)
struct nlist64 nl[] = {
#else
struct nlist nl[] = {
#endif
	{ "_trrectab" },
	{ "_trenabletab" },
	{ "_trindextab" },
	{ "_trreg" },
	{ "_trmagic" },
	""
};

off_t	_trrectab_off, _trenabletab_off, _trindextab_off;
off_t	_trreg_off, _trmagic_off;

extern	void	sigcld();

struct	tracerec {
	uint_t	ord;
	uint_t	time;
};

#define	MAXNCPU		128

int	ncpu;

#define	TRRECSZ		(sizeof (struct tracerec))
#define	TRRECTABSZ	(64 * 1024)
#define	TRENABLETABSZ	(2048)

#define	TRMAXINDEX	(TRRECTABSZ/sizeof(struct tracerec))
#define	TRINDEXMASK	(TRMAXINDEX-1)

struct	tracerec	trrectab[TRMAXINDEX];
uint_t	trindextab[MAXNCPU];
char	trenabletab[TRENABLETABSZ];
uint_t	trmagic;

int	kmemfd;

char	*ordfile;
char	*funcfile;
char	*outfile;
char	*kernfile;

FILE	*outfptab[MAXNCPU];

int	lastindextab[MAXNCPU];

extern int	optind;

extern	char	*findname();

void closeoutfiles();
void disableall();
void enable( char *);
void enableall();
void loadnametab( char *);
void openoutfiles();
void prtrec( FILE *, struct tracerec *);
void rmem( int, off_t, void *, int);
void scheddeadline();
void sigcld();
void syserr( char *);
void trout();
void usage( char *);
void wmem( int, off_t, void *, int);


main(ac, av)
int	ac;
char	*av[];
{
	int	fd;
	off_t	off;
	int	rc;
	int	i;
	int	start;
	int	n;
	int	pid;
	int	new_trreg;

	if (ac < 6)
		usage(av[0]);
		
	ordfile = av[1];
	funcfile = av[2];
	outfile = av[3];
	kernfile = av[4];
	optind = 5;
	new_trreg = 0;
	if (ac > 6) {
		if (!strcmp( av[5], "prfcnt0")) {
	  		printf("logging PRFCNT0\n");
			new_trreg = 1;
			optind = 6;
		} else if (!strcmp( av[5], "prfcnt1")) {
		  	printf("logging PRFCNT1\n");
			new_trreg = 2;
			optind = 6;
		}
	}

	if ((ncpu = sysmp(MP_NPROCS)) < 0)
		syserr("sysmp(MP_NPROCS)");

#if (_MIPS_SZPTR == 64) || (_MIPS_SZLONG == 64) || (_MIPS_SZINT == 64)
	if ((rc = nlist64(kernfile, nl)) < 0) {
#else
	if ((rc = nlist(kernfile, nl)) < 0) {
#endif
		fprintf(stderr, "nlist returned %d\n", rc);
		exit(1);
	}

	if ((nl[0].n_value == 0) || (nl[1].n_value == 0) ||
	    (nl[2].n_value == 0) || (nl[3].n_value == 0) ||
	    (nl[4].n_value == 0)) {
		fprintf(stderr, "missing symbols - not a trace kernel\n");
		exit(1);
	}

	_trrectab_off = nl[0].n_value;
	_trenabletab_off = nl[1].n_value;
	_trindextab_off = nl[2].n_value;
	_trreg_off = nl[3].n_value;
	_trmagic_off = nl[4].n_value;

	loadnametab(ordfile);

	kmemfd = open(KMEMFILE, O_RDWR);
	if (kmemfd < 0)
		syserr(KMEMFILE);

	/* check kernel magic number */

	rmem(kmemfd, _trmagic_off, &trmagic, sizeof (uint_t));
	if (trmagic != 0xdead0110) {
		fprintf(stderr, "trace magic number valid (0x%x:0x%x)\n",
			_trmagic_off, trmagic);
		exit(1);
	}

	/* magic number OK, proceed */

	openoutfiles();

	/*
	 * Reset trace buffer index values to zero.
	 */
	bzero((caddr_t) trindextab, (ncpu * sizeof (int)));
	wmem(kmemfd, _trindextab_off, trindextab, (ncpu * sizeof (int)));

	/* set _trreg value to perform clock/prfcnt0/prfcnt1 traceing */

	wmem(kmemfd, _trreg_off, &new_trreg, sizeof (int));

	sigset(SIGINT, sigcld);
	sigset(SIGCLD, sigcld);

	pid = fork();
	if (pid < 0)
		syserr("fork");

	if (pid == 0) {	/* child */

		enable(funcfile);

		close(kmemfd);

		if (execvp(av[optind], &av[optind])  < 0)
			syserr(av[optind]);
	}

	/* parent */

	scheddeadline();
	trout();

	while (sginap(1) == 0)
		trout();

	disableall();
	closeoutfiles();
}

void
sigcld()
{
	disableall();

	trout();

	closeoutfiles();

	exit(0);
}

void
openoutfiles()
{
	char	buf[BUFSIZ];
	int	i;

	for (i = 0; i < ncpu; i++) {
		sprintf(buf, "%s.cpu%d", outfile, i);
		if ((outfptab[i] = fopen(buf, "w")) == NULL)
			syserr(buf);
	}
}

void
closeoutfiles()
{
	int	i;

	for (i = 0; i < ncpu; i++)
		fclose(outfptab[i]);
}

/*
 * Write whatever trace records have accumulated
 * for all cpus to the output files.
 */
void
trout()
{
	int	i;
	int	j;
	int	index;
	int	lastindex;
	struct	tracerec	*tr;
	off_t	off;
	int	size;

	sighold(SIGCLD);

	rmem(kmemfd, _trindextab_off, trindextab, (ncpu * sizeof (uint_t)));

	tr = trrectab;

	for (i = 0; i < ncpu; i++) {

		index = trindextab[i];
		lastindex = lastindextab[i];

		if (index == lastindex)
			continue;


		/*
		 * read() in new trace records
		 */
		off = _trrectab_off + (i * TRRECTABSZ);
		if (index > lastindex) {
			off += lastindex * TRRECSZ;
			size = (index - lastindex) * TRRECSZ;
			rmem(kmemfd, off, &tr[lastindex], size);
		} else {
			size = index * TRRECSZ;
			rmem(kmemfd, off, trrectab, size);
			off += lastindex * TRRECSZ;
			size = (TRMAXINDEX - lastindex) * TRRECSZ;
			rmem(kmemfd, off, &trrectab[lastindex], size);
		}

		for (j = lastindex; j != index; j = (++j & TRINDEXMASK))
			prtrec(outfptab[i], &tr[j]);

		lastindextab[i] = index;
	}

	sigrelse(SIGCLD);
}

void
prtrec(fp, p)
struct	tracerec	*p;
FILE	*fp;
{
	if (fwrite(p, sizeof (struct tracerec), 1, fp) != 1)
		syserr("prtrec:  fwrite");
}

void
scheddeadline()
{
#ifdef OBSOLETE
	struct	sched_deadline	dl;

	dl.dl_period.tv_sec = 0;
	dl.dl_period.tv_nsec = 10 * 1000 * 1000;	/* 10 ms */
	dl.dl_alloc.tv_sec = 0;
	dl.dl_alloc.tv_nsec = 1000 * 1000;		/*  1 ms */

	if (schedctl(DEADLINE, 0, &dl) < 0)
		syserr("sched_ctl");
#endif /* OBSOLETE */
	printf("scheddeadline: deadline scheduling no longer supported!\n");
}

#ifdef	notdef
maptabs()
{
	void	*pa;
	int	pagesize;
	int	pagemask;
	off_t	addr;
	int	size;
	off_t	off;

	pagesize = getpagesize();
	pagemask = ~(pagesize - 1);

	/*
	 * mmap() each of the three kernel tables
	 *
	 * - 5.2 io/mem.c mmmap() supports only restricted
	 *	addresses for /dev/mmem only, not /dev/kmem
	 *	(fixed in 5.3?)
	 *
	 * - mmap() requires 'off' to be page aligned, so
r	 *	we have to adjust it for each case below.
	 */


	addr = _trrectab_off & pagemask;
	off = _trrectab_off - addr;
	size = (ncpu * TRRECTABSZ) + off;
	if ((pa = mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED,
		kmemfd, addr)) == (void*) -1)
		syserr("mmap");
	trrectab = (struct tracerec*) ((off_t)pa + off);

	addr = _trenabletab_off & pagemask;
	off = _trenabletab_off - addr;
	size = TRENABLETABSZ + off;
	if ((pa = mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED,
		kmemfd, addr)) == (void*) -1)
		syserr("mmap");
	trenabletab = (uint_t*) ((off_t)pa + off);

	addr = _trindextab_off & pagemask;
	off = _trindextab_off - addr;
	size = (ncpu * sizeof (int)) + off;
	if ((pa = mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED,
		kmemfd, addr)) == (void*) -1)
		syserr("mmap");
	trindextab = (uint_t*) ((off_t)pa + off);
}
#endif

void
syserr(s)
char	*s;
{
	perror(s);
	exit(1);
}

void
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

void
rmem(fd, offset, addr, len)
int	fd;
off_t	offset;
void	*addr;
int	len;
{
	lseek(fd, offset, 0L);
	if (read(fd, addr, len) < 0) {
		printf("rmem: read error fd %d offset 0x%x addr 0x%x len 0x%x\n",
		       fd, offset, addr, len);
		syserr("read");
	}
}

void
wmem(fd, offset, addr, len)
int	fd;
off_t	offset;
void	*addr;
int	len;
{
	lseek(fd, offset, 0L);
	if (write(fd, addr, len) < 0)
		syserr("write");
}

void
enable(file)
char	*file;
{
	char	line[BUFSIZ];
	FILE	*fp;
	int	ord;
	int	word;

	if ((fp = fopen(file, "r")) == NULL)
		syserr(file);

	bzero(trenabletab, TRENABLETABSZ);

	while (fgets(line, BUFSIZ, fp)) {
		if (line[0] == '#')	/* comment */
			continue;

		line[strlen(line)-1] = '\0';	/* strip newline */

		if (strcmp(line, "ALL") == 0) {
			enableall();
			fclose(fp);
			return;
		}

		ord = findord(line);

		if (ord == 0) {
/*
			fprintf(stderr,
			"unknown func (ignored): \"%s\"\n", line);
*/
			continue;
		}

		trenabletab[ord/8] |= (1 << (ord % 8));
	}

	fclose(fp);

	wmem(kmemfd, _trenabletab_off, trenabletab, TRENABLETABSZ);
}

void
disableall()
{
	bzero(trenabletab, TRENABLETABSZ);
	wmem(kmemfd, _trenabletab_off, trenabletab, TRENABLETABSZ);
}

void
enableall()
{
	memset(trenabletab, 0xff, TRENABLETABSZ);
	wmem(kmemfd, _trenabletab_off, trenabletab, TRENABLETABSZ);
}

void
err(fmt, a1, a2, a3, a4)
char	*fmt;
char	*a1, *a2, *a3, *a4;
{
	fprintf(stderr, fmt, a1, a2, a3, a4);
	fprintf(stderr, "\n");
	exit(1);
}

void
usage(s)
char	*s;
{
	fprintf(stderr, "Usage:  %s ordfile funcfile outfile kernel [prfcnt{0|1}] cmd [args]\n",
		s);
	exit(1);
}
