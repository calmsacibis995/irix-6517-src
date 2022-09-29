#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/syssgi.h>

#define USESYSSGI 0
#define USEDATAPATTERN 0
#define OFFSET 0
#define DELAY 0   /* delays before seek to allow more diskrands */

#define BUFSIZE (512 * 1024)
#if USEDATAPATTERN
unsigned int *buf;
unsigned int *buf2;
#else
char *buf;
char *buf2;
#endif

#define SIZE_PREV_BUF 50
int iterations = 0;
int64_t prevblock[SIZE_PREV_BUF];
int prevblockcount[SIZE_PREV_BUF];
int rotor = 0;


void
generate_pattern(char *string, int64_t block, int blockcount)
{
	register int i, j;
#if USEDATAPATTERN
	register unsigned *p;
	p = buf;
#endif

	for (i = 0; i < blockcount; i++)
	{
#if USEDATAPATTERN
		for (j = 0; j < 32; j++)
		{
			*p = 0xaaaaaaaa; p++;
			*p = 0x55555555; p++;
			*p =  (((block + i) << 8) + j); p++;
			*p = ~(((block + i) << 8) + j); p++;
		}
#else
		j = sprintf(&buf[512 * i + 0], "%s block %lld cacheline 0\n",
			string, block + i);
		memset(&buf[512 * i + j + 1], 0xAA, 127 - j);
		j = sprintf(&buf[512 * i + 128], "%s block %lld cacheline 1\n",
			string, block + i);
		memset(&buf[512 * i + j + 129], 0x55, 127 - j);
		j = sprintf(&buf[512 * i + 256], "%s block %lld cacheline 2\n",
			string, block + i);
		memset(&buf[512 * i + j + 257], block + i, 127 - j);
		j = sprintf(&buf[512 * i + 384], "%s block %lld cacheline 3\n",
			string, block + i);
		memset(&buf[512 * i + j + 385], -(block + i), 127 - j);
#endif
	}
}


void
read_data(int fd, int64_t block, int blockcount)
{
#if USESYSSGI
	if (syssgi(SGI_READB, fd, buf2+OFFSET, block, blockcount) != blockcount)
	{
		fprintf(stderr, "Unable to read block %lld\n", block);
		fflush(stderr);
		exit(1);
	}
#else
	register int count;
	register int wanted;

	if (lseek64(fd, block * 512, SEEK_SET) != block * 512)
	{
		fprintf(stderr, "Unable to seek to block %lld\n", block);
		fflush(stderr);
		exit(1);
	}
	wanted = blockcount * 512;
	if ((count = read(fd, buf2+OFFSET, wanted)) != wanted)
	{
		fprintf(stderr, "Error reading block %lld -- wanted %d bytes,"
				" got %d bytes: ", block, wanted, count);
		perror("");
		fflush(stderr);
		exit(1);
	}
#endif
}


void
droutput(char *data)
{
	register int j;

	j = 0;
	while (j < 128 && isprint(data[j]))
	{
		fprintf(stderr, "%c", data[j]);
		j++;
	}
	if (j > 0)
		fprintf(stderr, "\n");
	if (j % 16)
		fprintf(stderr, " %3d: ", j);
	while (j < 128)
	{
		if (j % 16 == 0)
			fprintf(stderr, " %3d: ", j);
		fprintf(stderr, "<%d>", data[j]);
		if (j % 16 == 15)
			fprintf(stderr, "\n");
		j++;
	}
}


int
check_data(int64_t block, int blockcount)
{
	register int i = 0;
	int error = 0;

#if USEDATAPATTERN
	if (memcmp(&buf[0], &buf2[OFFSET], blockcount * 512))
	{
		error = 1;
		for (i = 0; i < blockcount * 128; i++)
		{
		    if (buf[i] != buf2[OFFSET+i])
		    {
			fprintf(stderr,
			  "  sector %lld, offset %d, got 0x%x, expected 0x%x\n",
			  block + i / 128, 4 * (i % 128), buf2[OFFSET+i], buf[i]);
			fflush(stderr);
		    }
		}
	}
#else
	for (i = 0; i < blockcount * 512; i += 128)
	{
		if (memcmp(&buf[i], &buf2[OFFSET+i], 128))
		{
			error = 1;

			fprintf(stderr, "Expected:\n");
			droutput(buf + i);

			fprintf(stderr, "Got:\n");
			droutput(buf2 + OFFSET + i);

			fprintf(stderr, "\n");
			fflush(stderr);
		}
	}
#endif
	if (error)
	{
		fprintf(stderr, "Read miscompare(s) start block %lld blockcount %d"
			" iteration %d\n", block, blockcount, iterations);
		fflush(stderr);
		return 1;
	}
	return 0;
}


void
write_data(int fd, int64_t block, int blockcount)
{
	int i;
#if USESYSSGI
	if ((i = syssgi(SGI_WRITEB, fd, buf, block, blockcount)) != blockcount)
	{
		fprintf(stderr, "write to block %lld returns %d\n", block, i);
		fflush(stderr);
		exit(1);
	}
#else
	if (lseek64(fd, block * 512, SEEK_SET) != block * 512)
	{
		fprintf(stderr, "Unable to seek to block %lld\n", block);
		fflush(stderr);
		exit(1);
	}
	if ((i = write(fd, buf, blockcount * 512)) != blockcount * 512)
	{
		fprintf(stderr, "write to block %lld returns %d\n", block, i);
		fflush(stderr);
		exit(1);
	}
#endif
}


void
usage(char *argv0)
{
	fprintf(stderr, "Usage: %s <partition name> <start block> <block count>\n",
		argv0);
	fflush(stderr);
	exit(1);
}


void
main(argc, argv)
int argc;
char **argv;
{
	register int j;
	int64_t minblock;
	int64_t blockextent;
	int64_t maxblock;
	int fd;
	int64_t block;
	int blockcount;
	unsigned sequential;
	int action;

	buf = memalign(getpagesize(), BUFSIZE+1024);
	buf2 = memalign(getpagesize(), BUFSIZE+1024);
	if (mpin(buf2, BUFSIZE))
	{
		fprintf(stderr, "Pin didn't work\n");
		fflush(stderr);
		exit(1);
	}

#define PHYSPAGES 0
#if PHYSPAGES
	printf("Physical pages are:\n");
	for (j = 0; j < BUFSIZE; j += getpagesize())
	{
		block = syssgi(1010, ((char *) buf2) + j, &sequential);
		if (block != 0)
		{
			printf("error in syssgi\n");
			exit(1);
		}
		printf("0x%x ", sequential);
	}
	printf("\n");
	fflush(stdout);
#endif

	if (argc < 3 || argc > 4)
		usage(argv[0]);

	if ((fd = open(argv[1], O_RDWR)) == -1)
	{
		fprintf(stderr,
			"%s: unable to open %s\n", argv[0], argv[1]);
		usage(argv[0]);
	}

	if (argc == 3)
	{
		minblock = 0;
		blockextent = (int64_t) atoll(argv[2]);
	}
	else
	{
		minblock = (int64_t) atoll(argv[2]);
		blockextent = (int64_t) atoll(argv[3]);
	}
	if (minblock < 0 || blockextent < 0)
		usage(argv[0]);
	maxblock = minblock + blockextent;

	memset(buf, 0, BUFSIZE);
	block = minblock;
	do
	{
		blockcount = BUFSIZE / 512;
		if (block + blockcount > maxblock)
			blockcount = maxblock - block;

		generate_pattern(argv[1], block, blockcount);
		write_data(fd, block, blockcount);
		block += blockcount;
	}
	while (block < maxblock);
	printf("%s initialized block %lld to %lld\n", argv[1], minblock, maxblock - 1);
	fflush(stdout);

	srandom(time(0) + getpid());
	action = 1;
	while (1)
	{
		sequential = random() & 1;
		if (sequential)
		{
			block += blockcount;
			if (block == maxblock)
				block = minblock;
		}
		else
			block = minblock + (random() % blockextent);
		blockcount = (random() % (BUFSIZE / 512)) + 1;
		if (block + blockcount > maxblock)
			block = maxblock - blockcount;

		if (!sequential)
		{
			action = random() & 3;
#if DELAY
			if (random() & 1)
				sginap(random() % 50);
#endif
		}
		if (action)
		{
			read_data(fd, block, blockcount);
			generate_pattern(argv[1], block, blockcount);
			if (check_data(block, blockcount))
				break;
			prevblock[rotor] = block;
		}
		else
		{
			generate_pattern(argv[1], block, blockcount);
			write_data(fd, block, blockcount);
			prevblock[rotor] = -block;
		}

		prevblockcount[rotor] = blockcount;
		rotor++;
		if (rotor == SIZE_PREV_BUF)
			rotor = 0;

		iterations++;
		if ((iterations & (1024 - 1)) == 0)
		{
			printf("-- %d iterations completed on %s --\n",
			       iterations, argv[1]);
			fflush(stdout);
		}
	}

	printf("Data was DMA'd into buffer at 0x%lx\n", buf2+OFFSET);
	printf("Previous block/blockcounts:\n");
	j = rotor;
	do {
		printf("    %s block %lld blockcount %d\n",
		       prevblock[j] < 0 ? " write to" : "read from",
		       prevblock[j] < 0 ? -prevblock[j] : prevblock[j],
		       prevblockcount[j]);
		if (++j == SIZE_PREV_BUF)
			j = 0;
	}
	while (j != rotor);
	close(fd);
	exit(1);
}
