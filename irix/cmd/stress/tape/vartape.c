/*	This program will read and write tapes with variable block sizes to be
	sure they work.  If no block size is specified, it will try a
	256K read on first call to see what the actual size is.  This is
	larger than any of the drives with variable mode support at this
	time (Exabyte 240K, Kennedy 64K).

	Further work:
	  Should also check to be sure buffer contents agree with what was
		written when using -c.

	  Could include using different bit patterns in the
		buffer, but that really tests the media more than the interface.

	  Might want to allow starting and ending other than at BOT

	Olson, 1/89
*/
#include <stdio.h>
#include <sys/types.h>
#include <sys/mtio.h>
#include <errno.h>
#include <string.h>


char *calloc(int, int);
int read(int, char *, int);
int write(int, char *, int);
char *prog;

#define NPASSES 512

int *counts;
int npasses = NPASSES;
int verbose;
int exactcmp;
int Random;
int oddonly;
int align = 1;	/* align size to multiple of this; must be power of 2 */

main(cnt, args)
char **args;
{
	int c;
	char *device = "/dev/tape";
	int mode = -1;	/* require one of rwc */
	int fd, initblksz;
	int compare = 0;
	struct mtblkinfo info;
	struct mtop		mtop;
	extern int optind, opterr;
	extern char *optarg;
	int maxiosz = 0;

	opterr = 0;	/* do our own errors */
	prog = *args;
	while((c=getopt(cnt, args, "d:crwvp:m:Rxa:o")) != -1) {
		switch(c) {
		case 'a':
			align = strtoul(optarg, NULL, 0);
			break;
		case 'x':
			exactcmp = 1;	/* try to read with same size we wrote on cmp*/
			break;
		case 'R':
			Random = 1;	/* random data instead of zeros */
			break;
		case 'r':
			mode = 0;
			break;
		case 'w':
			mode = 1;
			break;
		case 'c':
			compare = 1;
			mode = 1;
			break;
		case 'd':
			device = optarg;
			break;
		case 'o':
			oddonly = 1;
			break;
		case 'p':
			npasses = strtoul(optarg, NULL, 0);
			if(npasses == 0) {
				fprintf(stderr, "# of passes must be > 0\n");
				exit(1);
			}
			break;
		case 'm':
			maxiosz = strtoul(optarg, NULL, 0);
			if(maxiosz <= 0) {
				fprintf(stderr, "max i/o size must be > 0\n");
				exit(1);
			}
			break;
		case 'v':
			verbose++;
			break;
		default:
		case '?':
			usage();
			/*NOTREACHED*/
		}
	}
	if(mode == -1)
		usage();	/* require one of rwc */
	if(exactcmp && !compare) {
		fprintf(stderr, ", can't do -x without -c, -x ignored\n");
		exactcmp = 0;
	}
	if(device == NULL || optind != cnt)
		usage();
	if(compare && mode == 0)
		usage();

	/* open ro, wo, or rw as appropriate */
	if((fd=open(device, mode+compare)) == -1) {
		fprintf(stderr, "Can't open ");
		perror(device);
		exit(2);
	}

	if(ioctl(fd, MTIOCGETBLKINFO, &info) == -1) {
		perror("can't get block info");
		exit(3);
	}
	if(info.minblksz == info.maxblksz) {
		fprintf(stderr, "%s doesn't support variable size blocks\n",
			device);
		exit(4);
	}
	if(maxiosz && maxiosz < info.maxblksz) {
		if(verbose)
			printf("device supports %#x max blksize, but -m limits to %#x\n",
				info.maxblksz, maxiosz);
		info.maxblksz = maxiosz;
	}
	mtop.mt_op = MTREW;
	if(ioctl(fd, MTIOCTOP, &mtop) == -1) {
		perror("failed to rewind tape at start");
		exit(6);
	}

	setlinebuf(stdout);
	if(mode == 0 && (initblksz = readit(fd, maxiosz)) == -1)
		exit(3);

	if(compare || mode) {
		counts = (int *)calloc(npasses, sizeof(int));
		if(counts == NULL) {
			perror("couldn't allocate buffer for counts");
			exit(5);
		}
	}

	if(mode == 1)	/* if writing, do different patterns on each run */
		srandom(time(NULL));
	if(verbose)
		printf("%d %s passes start%s\n", npasses, mode==1? "write" : "read", Random&&mode?" w/random data":"");
	c = doit(fd, mode, info.minblksz, info.maxblksz);
	if(c != npasses)
		getreason(fd);
	if(mode) {
		/* write FM and rewind; otherwise we need to close and re-open;
		 * Don't worry about it maybe being a 9 track and wanting 2 FM's
		 * for this kind of test. */
		mtop.mt_op = MTWEOF;
		mtop.mt_count = 1;
		if(ioctl(fd, MTIOCTOP, &mtop) == -1) {
			perror("failed to write FM at end of pass");
			exit(6);
		}
		mtop.mt_op = MTREW;
		if(ioctl(fd, MTIOCTOP, &mtop) == -1) {
			perror("failed to rewind tape at end of pass");
			exit(6);
		}
	}
	if(compare && c > 1) {
		if(c < npasses) {
			npasses = c;
			printf("only doing %d compare passes due to write error\n", c);
		}
		if(--verbose < 0)	/* don't report read counts that match
			when comparing, unless very verbose */
			verbose = 0;
		mode = 0;
		initblksz = exactcmp ? counts[0] : maxiosz;
		if(verbose)
			printf("%d read passes start%s\n", npasses, Random&&mode?" w/random data":"");
		if((initblksz = readit(fd, initblksz)) == -1)
			exit(3);
		if(initblksz != counts[0])
			fprintf(stderr, "\nOn read 0, got %d, but should have gotten %d\n",
				initblksz, counts[0]);
		doit(fd, mode, info.minblksz, info.maxblksz);
		if(c != npasses)
			getreason(fd);
		mtop.mt_op = MTREW;
		if(ioctl(fd, MTIOCTOP, &mtop) == -1) {
			perror("failed to rewind tape at end");
			exit(6);
		}
		if(close(fd) == -1)
			perror("error on close after compare");
	}
	exit(0);
}

usage()
{
	fprintf(stderr, "Usage: %s [-d device] -r|w|c [-v] [-p #] [-m #] [-R] [-x] [-a #] [-o]\n", prog);
	fprintf(stderr, "   -r: readonly, w: writeonly, c: write, read, cmp sizes\n");
	fprintf(stderr, "   -p passes: # of i/o's, default %d, m: max i/o sz, default max drive can do\n", NPASSES);
	fprintf(stderr, "   -v: verbose (twice makes cmp verbose also), -R: random data\n");
	fprintf(stderr, "   -x: use expected cnt on cmp pass, otherwise max, -o: odd byte cnt only\n");
	fprintf(stderr, "   -a: align byte cnt on power of two; default 1, -d device: default /dev/tape\n");
	fprintf(stderr, "   Numeric values use strtoul, any base is OK\n");
	exit(1);
}


doit(fd, mode, minblksz, maxblksz)
register mode;
{
	register i;
	char *buffer;
	int (*func)();
	register int cnt, ret;
	int precision, passprec, nperline;

	buffer = calloc(maxblksz, 1);
	if(buffer == NULL) {
		perror("couldn't allocate buffer for i/o");
		return;
	}

	for(precision=1,i=maxblksz; i; i/= 10)
		precision++;
	for(passprec=1,i=npasses; i; i/= 10)
		passprec++;
	nperline = 80/(precision+passprec+3);

	if(mode == 1) {
		func = write;
		for(i=0; i<npasses; i++)
			counts[i] = randblk(minblksz, maxblksz);
		i = 0;
	}
	else {
		i = 1; /* if reading, first was already done */
		func = read;
	}

	for(; i<npasses; i++) {
		if(exactcmp)
			/* if mode is 0, but counts are set, we are doing compare; see
			 * if this works around a bug in ardat 2.26-2.57 firmware */
			cnt = (mode == 0) ? (counts[i]?counts[i]:maxblksz) : counts[i];
		else
			cnt = (mode == 0) ? maxblksz : counts[i];
		if(mode == 1 && Random) {
			int wcnt = cnt/sizeof(int);
			int *wbuf = (int *)buffer;
			while(wcnt-- > 0)
				*wbuf++ = random();
		}
		ret = (*func)(fd, buffer, cnt);
		if(ret == -1) {
			int saverr = errno;	/* be paranoid */
			fprintf(stderr, "%s %#x ", mode==1? "write" : "read", cnt);
			perror("failure");
			if(saverr == EINVAL)
				fprintf(stderr, "Probably wrong minor # for variable mode\n");
			break;
		}
		if(mode == 0) {
			if(counts && ret != counts[i]) {
				fprintf(stderr, "\nOn read %d, got %d, but should have gotten %d\n",
					i, ret, counts[i]);
				if(ret <= 0)
					break;
			}
			else if(verbose)
				printf("%*d:%*d,", passprec, i, precision, ret);
		}
		else if(ret != cnt) {
			fprintf(stderr, "\nOn write %d, tried to write %d, but only wrote %d\n",
				i, cnt, ret);
			break;
		}
		else if(verbose)
			printf("%*d:%*d,", passprec, i, precision, ret);
		if(verbose && (i%nperline) == (nperline-1))
			printf("\n");
	}
	if(verbose && (i%nperline))
		printf("\n");
	free(buffer);
	return i;
}


/*	read the first block on the tape to determine it's size, return the
	count in bytes; the argument is in bytes.
*/
readit(fd, bcnt)
{
	int cnt;
	char *buffer;

	buffer = calloc(bcnt, 1);
	if(buffer == NULL) {
		perror("couldn't allocate buffer for first read");
		return -1;
	}
	if(verbose>1) printf("try to read %d\n", bcnt);
	cnt = read(fd, buffer, bcnt);
	if(cnt == -1)
		perror("first read fails");
	free(buffer);
	if(verbose && cnt>=0)
		printf("%d: %d, ", 0, cnt);
	return cnt;
}

/*	return a 'random' # in the given range. simplistic on the min,
	because the min for these purposes is much less than the max
	(1 byte vs 64K or 240K currently
*/
randblk(minb, maxb)
register minb, maxb;
{
	register int ret = random();

	ret %= maxb;
	if(align)
		ret &= ~(align-1);

	if(ret < minb)
		return minb;
	if(oddonly && !(ret&1)) {
		ret++;
		if(ret > maxb)
			ret -= 2;
	}
	return ret;
}


getreason(fd)
{
	struct mtget mtget;
	unsigned long posn;

	if(ioctl(fd, MTIOCGET, &mtget) == -1) {
		perror("mtiocget fails");
		return;
	}
	printf("dsreg %x; ", mtget.mt_dsreg);
	if(mtget.mt_dsreg & MT_QIC24)
		printf("QIC24 tape ");
	if(mtget.mt_dsreg & MT_QIC120)
		printf("QIC120 tape ");
	posn = mtget.mt_dposn;
	if(posn & MT_EOT)
		printf("at EOT ");
	if(posn & MT_BOT)
		printf("at BOT ");
	if(posn & MT_EOD)
		printf("at EOD ");
	if(posn & MT_WPROT)
		printf("write protected ");
	if(posn & MT_FMK)
		printf("at FM ");
	if(!(posn & MT_POSMSK))
		printf("unknown posn. ");
	printf("\n");
}
