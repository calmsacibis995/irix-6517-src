/*****************************************************************************
 * This program tests raw device by:
 * 1) Divide the whole disk into randomly sized regions and then fork a child
 *    for each one.  The number of regions is between 5 and 10.
 * 2) Each child will start at the beginning of its region and write a random
 *    number of blocks sequentially until the whole region is filled.
 * 3) The child will seek back to the start of its region and read a random
 *    number of blocks.  The child will verify that the blocks contain the
 *    appropriate contents, and then do another read.
 * 4) After the contents of all the blocks have been verified, the child will
 *    exit.
 * 5) When all child processes have completed, the parent will log the time to
 *    standard output.
 * 
 *    Ling Lee 		8/1/91
 *****************************************************************************
*/
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/signal.h>
#include <sys/errno.h>
#include <sys/wait.h>

#define MIN_PROCS	5	/* minimum # conurrent processes */
#define MAX_PROCS	10	/* maximum # conurrent processes */
#define BLOCK_SIZE	512	/* basic block size */
#define MIN_BLOCKS	8	/* min read/write unit (4KB) */
#define MAX_BLOCKS	80	/* max read/write unit (32KB) */
#define BUF_SIZE 	(MAX_BLOCKS*BLOCK_SIZE)

struct testrec {
	int reg_base;	/* offset from the beginning of the partition */
	int reg_size;	/* size of the region in basic blocks */
	int pid;	/* pid of the child process checking this region */	
} region[MAX_PROCS];

int	num_regions;	/* number of regions partition divided into */
int	iterations;	/* number of times entire partition read/verified */
int	fullcompare;	/* if 1 then compare all words in read/write buffers */


void abnormal_exit(child_pid)
int child_pid;
{
	int i;

	for (i = 0; i < num_regions; i++) {
		if (region[i].pid == child_pid) {
			fprintf(stderr, "\tRegion %d (PID %d) exited abnormally: blocks %d to %d\n",
					i, child_pid, region[i].reg_base, 
					region[i].reg_size+region[i].reg_base-1);
			return;
		}
	}
	fprintf(stderr, "Unexpected exit - pid %d\n", child_pid);
	return;
}


waitforchildren()
{
	int status, child_pid;
	extern int errno;

	for(;;) {
		child_pid = wait(&status);
		if ((child_pid < 0) && (errno == ECHILD))
			break;
		else if (!WIFEXITED(status))
			abnormal_exit(child_pid);
#ifdef DEBUG
		else {
			int i;

			for (i = 0; i < num_regions; i++) {
				if (region[i].pid == child_pid) {
					printf("Rawtest(%d): exited OK\n", i);
					break;
				}
			}
			if (i == num_regions)
				fprintf(stderr, "Unexpected exit - pid %d\n",
						child_pid);
		}
#endif
	}
#ifdef DEBUG
	sleep(8);
#endif
	return 0;
}


/* get a random number of basic blocks to read/write */
get_num_blocks()
{
	return((rand() % (MAX_BLOCKS - MIN_BLOCKS + 1)) + MIN_BLOCKS);
}

/*
 * set the sizes for each of the regions
 */
int
set_regions(int part_size)
{
	int numregs, basicsize;
	int base, size, i;

	numregs = (rand() % (MAX_PROCS - MIN_PROCS + 1)) + MIN_PROCS;
	basicsize = part_size / numregs;
	for (base = 0, i = 0; base < part_size; base += size, i++) { 
		size = rand() % (basicsize / 4); /* 0% to 25%  of partition */
		size -= (basicsize / 8);	 /* range of -12% to +12% */
		size += basicsize;		 /* new region size */
		if ((base + size) > part_size)
			size = part_size - base;
		region[i].reg_base = base;
		region[i].reg_size = size;
		printf("Region %2d: from %8d, to %8d (%8d blocks)\n",
			       i, base, base+size-1, size);
	}
	return(i);
}


void
data_check(int id, int num_blocks, int start_block, char *buf)
{
	register unsigned *rp, testval;
	register int i, j;

	if (fullcompare) {
		rp = (unsigned *) buf;
		for (j = 0; j < num_blocks; j++) {
			testval = (id << 24) | (j + start_block);
			for (i = 0; i < BLOCK_SIZE/4; rp++, i++) {
				if (*rp != testval) {
					fprintf(stderr, "%d: bad compare at %x, expected: %x, got: %x\n",
							id, start_block + j, 
							testval, *rp);
				}
			}
		}
	} else {
		for (rp = (unsigned *) buf, j = 0;
				j < num_blocks;
				rp += (BLOCK_SIZE/4), j++)
		{
			testval = (id << 24) | (j + start_block);
			if (*rp != testval) {
				fprintf(stderr, "%d: bad compare at %x, expected: %x, got: %x\n",
						id, start_block + j, 
						testval, *rp);
			}
		}
	}
}


void
fillbuf(int id, int num_blocks, int start_block, char *buf)
{
	register unsigned *rp, setval;
	register int i, j;

	if (fullcompare) {
		rp = (unsigned *) buf;
		for (j = 0; j < num_blocks; j++) {
			setval = (id << 24) | (j + start_block);
			for (i = 0; i < BLOCK_SIZE/4; rp++, i++)
				*rp = setval;
		}
	} else {
		for (rp = (unsigned *) buf, j = 0;
				j < num_blocks;
				rp += (BLOCK_SIZE/4), j++)
		{
			*rp = (id << 24) | (j + start_block);
		}
	}
}


char buffer[BUF_SIZE];

int
dorawtest(char *fname, int id, int base, int size)
{
	int numblocks, offset, fd;

#ifdef DEBUG
	printf("Rawtest(%d): base %d, size %d\n", id, base, size);
#endif
	srand((unsigned) getpid()); /* seed the randomizer in this child */

	/* open the raw device */
	if ((fd = open(fname, O_RDWR)) == -1) {
		fprintf(stderr, "Can't open: %s \n", fname);
		exit (1);
	}

	/*
	 * seek to base of region within device
	 */
	if (lseek(fd, (long)(base*BLOCK_SIZE), SEEK_SET) < 0) {
		fprintf(stderr, "%d: lseek(%d) failed\n",
				id, (base*BLOCK_SIZE));
		return (-1);
	}
#ifdef DEBUG2
	printf("process %d lseek to block %d\n", id, base);
#endif

	/*
	 * write data to fill region
	 */
	for (offset = 0; offset < size; offset += numblocks) {
		numblocks = get_num_blocks();
		if ((numblocks + offset) > size)
			numblocks = size - offset;
		fillbuf(id, numblocks, offset, buffer);
#ifdef DEBUG2
		printf("process %d writing blocks %ld to %ld\n",
				id, offset, offset+numblocks-1);
#endif
		if (write(fd, buffer, numblocks*BLOCK_SIZE) !=
			 (numblocks*BLOCK_SIZE)) {
			perror("write");
			exit(1);
		}
	}
		
	/*
	 * seek back to the base of the region
	 */
	if (lseek(fd, (long)(base*BLOCK_SIZE), SEEK_SET) < 0) {
		fprintf(stderr, "%d: lseek(%d) failed\n",
				id, (base*BLOCK_SIZE));
		return (-1);
	}
#ifdef DEBUG2
	printf("process %d lseek to block %d\n", id, base);
#endif

	/*
	 * read and verify data in region
	 */
	for (offset = 0; offset < size; offset += numblocks) {
		numblocks = get_num_blocks();
		if ((numblocks + offset) > size)
			numblocks = size - offset;
#ifdef DEBUG2
		printf("process %d reading blocks %ld to %ld\n",
				id, offset, offset+numblocks-1);
#endif
		if (read(fd, buffer, numblocks*BLOCK_SIZE) !=
			 (numblocks*BLOCK_SIZE)) {
			perror("read");
			exit(1);
		}
		data_check(id, numblocks, offset, buffer);
	}
	return 0;
}


void
main(argc, argv)
int argc;
char **argv;
{
	char	*fname, *tstamp;
	int	fsize; /* raw disk size */
	int	i, k, fd;
	time_t  now;
	struct stat sb;

	if (argc < 3) {
		fprintf(stderr, "Usage:\t%s <file_to_stat> <file_to_test> [[-]iterations]\n");
		fprintf(stderr, "\twhere iterations <  0 means compare 128 words/block\n");
		fprintf(stderr, "\t      iterations >= 0 means compare 1 word/block\n");
		exit (1);
	}

	if (argc < 4)
		iterations = 1;
	else
		iterations = atoi(argv[3]);
	fullcompare = 0;
	if (iterations < 0) {
		iterations = -iterations;	/* make it positive again */
		fullcompare = 1;
	}

#define STAT_WORKING
#ifdef STAT_WORKING
	if ((fd = open(argv[1], O_RDWR)) < 0) {
		fprintf(stderr, "Can't open %s\n", argv[1]);
		exit(1);
	}
	if (fstat(fd, &sb) < 0) {
		fprintf(stderr, "Can't fstat %s\n", argv[1]);
		exit(1);
	}
	fsize = sb.st_size; /* file size in # of blocks */
	if (fsize <= 0) {
		fprintf(stderr, "Bad device size (%ld) on device %s\n",
				fsize, argv[1]);
		exit(1);
	}
	close(fd);
#else /* STAT_WORKING */
	fsize = 100000; /* 2023350; file size in # of blocks */
#endif /* STAT_WORKING */

#ifdef DEBUG
	printf("fsize = %ld (pid = %d)\n", fsize, getpid());
#endif

	/* open the raw device */
	if ((fd = open(argv[2], O_RDWR)) == -1) {
		fprintf(stderr, "Can't open: %s ", argv[2]);
		exit (1);
	}
	close (fd);
	fname = argv[2];

	srand((unsigned) getpid()); /* seed the randomizer */

	for (k = 0; k < iterations; k++) {
		time(&now);
		tstamp = ctime(&now);
		tstamp[24] = 0;
		printf("\nMain Loop Iteration %d (started: %s)\n", k, tstamp);
		num_regions = set_regions(fsize);

		for (i = 0; i < num_regions; i++) {
			int pid;
		
			if ((pid = fork()) < 0) { 
				perror("fork()"); 
				sleep(10);
			} else if (pid == 0) {
				int ret;	/* child proc */
				ret = dorawtest(fname, i, region[i].reg_base,
						       region[i].reg_size);
				exit(ret);
			} 
			/* parent, successful fork */
			region[i].pid = pid;
		}

		waitforchildren();

		time(&now);
		tstamp = ctime(&now);
		tstamp[24] = 0;
		printf("Ended: %s\n", tstamp);
	}
}
