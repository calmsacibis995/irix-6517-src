

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>


int
main(
	int	argc,
	char	*argv[])
{
	int	blocksize;
	int	blockcount;
	int	fd;
	int	option;
	char	*buf;
	int	i;
	int	j;
	int	k;
	int	count;
	off64_t	*patbuf;
	int	curr_block;
	int	skip;
	off64_t off;
	char	*buf2;
	int	last_block;
	char	file[200];
	char	*tmpdir;
	
	if ((tmpdir = getenv("TMPDIR")) == NULL) {
		tmpdir = "/var/tmp/";
	}
	blocksize = 512;
	blockcount = 10240;
	strcpy(file, tmpdir);
	while ((option = getopt(argc, argv, "b:c:f:")) != EOF) {
		switch (option) {
		case 'b':
			blocksize = atoi(optarg);
			break;
		case 'c':
			blockcount = atoi(optarg);
			break;
		case 'f':
			strcpy(file, optarg);
			break;
		default:
			printf("usage: wfile -b blocksize -c blockcount -f file\n");
			exit(1);
		}
	}

	if (strcmp(file, tmpdir) == 0) {
		strcat(file, "/rewritefile");
	}

	printf("INFO: rewrite: bsize %d count %d file %s\n",
	       blocksize, blockcount, file);
	fd = open(file, O_RDWR | O_CREAT | O_TRUNC, 0666);

	if (fd < 0) {
		printf("ERROR: rewrite create failed.  Error is %d\n",
		       errno);
		exit(1);
	}

	buf = memalign(4096, blocksize);
	printf("INFO: Seed will be %d\n", getpid());
	srandom(getpid());
	curr_block = 0;
	for (i = 0; i < blockcount; i++) {
		/*
		 * Skip a random number of blocks.
		 */
		skip = random() % 4;
		off = lseek64(fd, (off64_t)(skip * blocksize), SEEK_CUR);
		if (off != ((curr_block * blocksize) + (skip * blocksize))) {
			printf("ERROR: lseek failed.  Error is %d\n", errno);
			exit(1);
		}
		curr_block += skip + 1;
		for (j = 0; j < blocksize; j += sizeof(off64_t)) {
			patbuf = (off64_t*)&buf[j];
			*patbuf = off;
		}
		count = write(fd, buf, blocksize);
		if (count < blocksize) {
			printf("ERROR: write failed.  Error is %d\n", errno);
			exit(1);
		}
	}


	/*
	 * Now write every block, but in larger chunks.
	 * Use 3 so that we're more likely to get odd alignments
	 * and overlap hole-not hole sections of the file.
	 */
	off = lseek64(fd, (off64_t)0, SEEK_SET);
	if (off != (off64_t)0) {
		printf("ERROR: lseek failed.  Error is %d\n", errno);
		exit(1);
	}
	buf2 = memalign(4096, 3 * blocksize);
	off = 0;
	for (i = 0; i < curr_block; i += 3) {
		patbuf = (off64_t*)&buf2[0];
		for (j = 0; j < 3; j++) {
			for (k = 0;
			     k <  blocksize;
			     k += sizeof(off64_t), patbuf++) {
				*patbuf = off;
			}
			off += blocksize;
		}
		count = write(fd, buf2, 3 * blocksize);
		if (count < 3 * blocksize) {
			printf("ERROR: write failed.  Error is %d\n", errno);
			exit(1);
		}
	}


	off = lseek64(fd, (off64_t)0, SEEK_SET);
	if (off != (off64_t)0) {
		printf("ERROR: lseek failed.  Error is %d\n", errno);
		exit(1);
	}

	last_block = curr_block - 1;
	curr_block = 0;
	for (i = 0; i <= last_block; i++) {
		count = read(fd, buf2, blocksize);
		if (count < blocksize) {
			printf("ERROR: read failed count %d block %d\n", count, i);
			exit(1);
		}
		for (j = 0; j < blocksize; j += sizeof(off64_t)) {
			patbuf = (off64_t*)&buf2[j];
			if (*patbuf != (off64_t)(i * blocksize)) {
				printf("ERROR: bogus data 0x%x at offset %d in block %d should be 0x%x\n",
				       *patbuf, j * 4, i, i * blocksize);
				exit(1);
			}
		}
	}

	return 0;
}
