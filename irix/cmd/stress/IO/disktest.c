#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <malloc.h>

char *buffer = NULL;
char *fname;
int rawdisk = 0;

void fillbuf(unsigned n, int bsize);
void badcount(int n, int bsize);
void ucrap(void);

void
main(int argc, char **argv)
{
	unsigned tot_bytes;
	unsigned written_bytes;
	unsigned start_off = 0;
	int nbytes, i, c, fd;
	int trunc = 0;
	int loop;
	int noread = 0;
	int errors = 0;
	int bsize = (32 * 1024) - 4;
	int readonly = 0;
	int stoperr = 1;
	int delondone = 0;
	int quiet = 0;
	unsigned limit = 0x7FFFFFFF;
	int keeploop = 0;
	struct stat sb;
	int pass = 1;
	int iosize = 0;
	int randombsize = 0;

   	extern int	optind;
   	extern char	*optarg;

	while ((c = getopt(argc, argv, "cdlqRrkb:B:o:O:s:S:")) != EOF) {
		switch (c) {
		case 'c':
			rawdisk = 1;
			stoperr = 1;
			break;
		case 'd':
			delondone = 1;
			break;
		case 'l':
			keeploop = 1;
			break;
		case 'q':
			quiet = 1;
			break;
		case 'r':
			readonly = 1;
			break;
		case 'R':
			randombsize = 1;
			srandom(time(0));
			break;
		case 'k':
			stoperr = 0;
			break;
		case 'b':
			if ((bsize = strtol(optarg, (char **) 0, 0)) <= 0) 
				ucrap();
			break;
		case 'B':
			if ((bsize = 512 * strtol(optarg, (char **) 0, 0)) <= 0) 
				ucrap();
			break;
		case 'o':
			if ((start_off = strtol(optarg, (char **) 0, 0)) <= 0) 
				ucrap();
			break;
		case 'O':
			if ((start_off = 512 * strtol(optarg, (char **) 0, 0)) <= 0) 
				ucrap();
			break;
		case 's':
			if ((limit = strtol(optarg, (char **) 0, 0)) <= 0) 
				ucrap();
			break;
		case 'S':
			if ((limit = 512 * strtol(optarg, (char **) 0, 0)) <= 0) 
				ucrap();
			break;
		default:
			ucrap();
		}
	}

	if (rawdisk || readonly)
		delondone = 0;

	if (rawdisk) 
		bsize = (((bsize + 511) / 512) * 512);

	argc -= optind;
	if (argc <= 0) ucrap();
	if (bsize > limit)
		bsize = limit;
	if (bsize % 4)
		bsize += (4 - (bsize % 4));

	if (!readonly)
		start_off = 0;

	if ((buffer = malloc(bsize)) == NULL)
	{
		fprintf(stderr,"Can't get memory\n");
		exit(-1);
	}

	fname = argv[optind];

	if (readonly) 
		goto readin;

fullloop:

	/* open new file. See if it exists first, if not create it,
	 * unless we're specified to be working on a device. */

	if (stat(fname, &sb) == 0)
	{
		if ((fd = open( fname, O_RDWR)) < 0)
		{
			fprintf(stderr,"Can't open %s\n", fname );
			perror("");
			exit(1);
		}
	}
	else 
	{
		if (rawdisk)
		{
			fprintf(stderr,"%s does not exist\n", fname );
			exit(1);
		}
		if ((fd = open( fname, O_RDWR | O_EXCL | O_CREAT , 
			S_IWRITE | S_IREAD )) < 0)
		{
			fprintf(stderr,"Can't create %s\n", fname );
			perror("");
			exit(1);
		}
	}

	/* Initialize counters */
	tot_bytes = 0;
	nbytes    = 0;

	/* 
	 * write out buffersized chunks until 
	 * remaining space exhausted or limit exceeded
	 */
	loop = 0;
	while (tot_bytes < limit)
	{
		loop++;

		if (randombsize)
			iosize = randomsize(bsize);
		else
			iosize = bsize;
		if (iosize + tot_bytes > limit)
			iosize = limit - tot_bytes;

		fillbuf(tot_bytes, iosize);
		nbytes = write(fd, buffer, iosize);
		if (nbytes < 0)
		{
			if (rawdisk)
				break;
			fprintf(stderr,
				"Error writing %s, loop %d, written %d\n",
				fname, loop, tot_bytes);
			perror("");
			exit(1);
		}
		tot_bytes += nbytes;
		if (nbytes == iosize)
		{
			if (!quiet)
			{
				printf("\r %d KBytes written ",
				       tot_bytes / 1024 );
				fflush(stdout);
			}
		}
		else 
		{
			if (!quiet)
				printf("\nEnd of space: requested %d write %d\n",
				       bsize, nbytes);
			break;
		}
	}

	written_bytes = tot_bytes;
	limit = written_bytes;
	if (!quiet)
		printf("\n");
	close(fd);

readin:
	if ((fd = open( fname, O_RDONLY)) < 0)
	{
		fprintf(stderr," Error on open for read of %s\n", fname );
		perror("");
		exit(1);
	}
	tot_bytes = 0;
	nbytes = 0;

	if (start_off)
	{
		loop = start_off / bsize;
		tot_bytes = loop * bsize;
		if (lseek(fd, tot_bytes, 0) < 0)
		{
			fprintf(stderr," Error on seek of %s\n", fname );
			perror("");
			exit(1);
		}
	}
	else
		loop = 0;

	while (tot_bytes < limit && !trunc)
	{
		loop++;

		if (randombsize)
			iosize = randomsize(bsize);
		else
			iosize = bsize;
		if (iosize + tot_bytes > limit)
			iosize = limit - tot_bytes;

		nbytes = read(fd, buffer, iosize);
		if (nbytes < 0)
		{
			fprintf(stderr, "Error reading %s, loop %d, read %ud\n",
			        fname, loop, tot_bytes);
			perror("");
			exit(1);
		}
		if (nbytes < iosize)
			trunc = 1;
		if (cmpbuf(tot_bytes, nbytes))
		{
			errors++;
			badcount(tot_bytes, iosize);
			if (stoperr || trunc)
				exit(1);
		}
		tot_bytes += nbytes;
		if (!quiet)
		{
			printf("\r %ud KBytes compared ", tot_bytes / 1024 );
			fflush(stdout);
		}
	}
	close(fd);
	if (!readonly && (tot_bytes != written_bytes))
	{
		fprintf(stderr,"%s: Read truncation: written %ud read %ud\n", 
			fname, written_bytes, tot_bytes);
		if (stoperr)
			exit(1);
	}
	if (errors)
	{
		printf("\n%s: %d Compare errors\n", fname, errors);
		exit(1);
	}
	else 
	{
		printf("\n%s: ", fname);
		if (keeploop)
			printf("Pass %d ", pass++);
		printf(" OK\n");
		fflush(stdout);
	}
	if (delondone && !rawdisk)
		unlink(fname);
	if (keeploop)
	{
		trunc = 0;
		if (readonly)
			goto readin;
		else
			goto fullloop;
	}
	exit(0);
}

void
fillbuf(unsigned n, int bsize)
{
	int i = bsize / (sizeof (unsigned));
	register unsigned *p = (unsigned *) &buffer[0];

	while (i > 0) 
	{
		*p++ = n;
		n += sizeof(unsigned);
		*p++ = n;
		n += sizeof(unsigned);
		*p++ = n;
		n += sizeof(unsigned);
		*p++ = n;
		n += sizeof(unsigned);
		i -= 4;
	}
}


int
cmpbuf(n, bsize)
register unsigned n;
int bsize;
{
	register unsigned *p = (unsigned *) &buffer[0];
	register int i = 0;
	register int omega = bsize / (sizeof (unsigned));

	while (i < omega)
	{
		if (*p != n)
			goto cmperr;
		n += sizeof(unsigned);
		p++;
		if (*p != n)
			goto cmperr;
		n += sizeof(unsigned);
		p++;
		if (*p != n)
			goto cmperr;
		n += sizeof(unsigned);
		p++;
		if (*p != n)
			goto cmperr;
		n += sizeof(unsigned);
		p++;
		i += 4;
	}
	return 0;

cmperr:
	fprintf(stderr, "\n%s: compare error at byte %ud 0x%x KB %ud 0x%x\n", 
		fname, n, n, n / 1000, n / 1000);
	fprintf(stderr, "Expected %ud 0x%x, saw %ud 0x%x\n", n, n, *p, *p);
	return 1;
}

randomsize(bsize)
int bsize;
{
	register int a;

	a = random() % bsize;
	if (rawdisk)
		a = (a + 511) & ~511;
	else
		a = (a + 15) & ~15;
	return a;
}

void
badcount(int n, int bsize)
{
	int i = bsize / (sizeof (unsigned));
	register unsigned *p = (unsigned *) &buffer[0];
	unsigned count = 0;

	while (i > 0) 
	{
		if (*p++ != n)
			count++;
		i--;
		n += 4;
	}
	fprintf(stderr, "Mismatches %d ", count);
	if (count == bsize / sizeof(unsigned))
		fprintf(stderr,"(complete buffer)");
	fprintf(stderr,"\n");
}

void
ucrap(void)
{
	fprintf(stderr,"usage: disktest [-dhqr] [-b buffersize] [-B buffersize_in_blocks]\n");
	fprintf(stderr,"\t[-s filesize] [-S filesize_in_blocks] filename\n");
	exit(1);
	/*NOTREACHED*/
}
