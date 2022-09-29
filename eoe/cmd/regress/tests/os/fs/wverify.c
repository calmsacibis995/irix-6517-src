

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>



void
rewrite_verify(
	char	*work_dir,
	int	nfiles,
	int	*file_size,
	int	iteration,
	int	write_iterations,
	off64_t	woffset);

void
write_file(
	char	*file,
	int	size,
	int	byte_val,
	off64_t	woffset);

void
read_verify(
	char	*file,
	int	size,
	int	byte_val,
	off64_t	woffset);

void
dump_diff(
	char	*file,
	char	*expect,
	char	*got,
	int	offset,
	int	count);

char *
itoa(
	int	i);


int
main(
	int	argc,
	char	*argv[])
{
	char	work_dir[120];
	char	*wdir;
	int	iterations;
	int	write_iterations;
	int	nfiles;
	off64_t	woffset;
	int	fd;
	char	*buf;
	int	i;
	int	count;
	int	option;
	int	res;
	int	*file_size;
	char	str[80];
	char	file[80];
	char	*tmpdir;

	wdir = "wverifydir";
	iterations = 10;
	write_iterations = 1;
	nfiles = 100;
	woffset = (off64_t)0;
	tmpdir = getenv("TMPDIR");
	if (tmpdir == NULL) {
		tmpdir = "/var/tmp";
	}
	while ((option = getopt(argc, argv, "d:i:n:o:w:")) != EOF) {
		switch (option) {
		case 'd':
			wdir = optarg;
			break;
		case 'i':
			iterations = atoi(optarg);
			break;
		case 'n':
			nfiles = atoi(optarg);
			if (nfiles > 255) {
				printf("num files must be less than 256\n");
				exit(1);
			}
			break;
		case 'o':
			woffset = strtoll(optarg, NULL, 16);
			break;
		case 'w':
			write_iterations = atoi(optarg);
			break;
		default:
			printf("usage: wverify -d <work dir> -i iterations -n <num files> -o <hex offset for file writes> -w <write iterations per iteration>\n");
			exit(1);
		}
	}

	sprintf(work_dir, "%s/%s", tmpdir, wdir);

	file_size = calloc(nfiles, sizeof(int));

	if ((res = mkdir(work_dir, 0777)) < 0) {
		if (errno != EEXIST) {
			printf("ERROR:  Making dir %s failed.  Error is %d", work_dir, errno);
			exit(1);
		}
	}

	for (i = 0; i < nfiles; i++) {
		sprintf(file, "%s/%s", work_dir, itoa(i));
		fd = creat(file, 0666);
		if (fd < 0) {
			printf("ERROR: Making file %s failed.  Error is %d", file, errno);
			exit(1);
		}
		close(fd);
		file_size[i] = 0;
	}

	srandom(getpid());

	for (i = 1; i <= iterations; i++) {
		printf("INFO: Starting iteration %d\n", i);
		rewrite_verify(work_dir, nfiles, file_size,
			       i, write_iterations, woffset);
	}

	printf("INFO: wverify test completed successfully\n");
	exit(0);
}


void
rewrite_verify(
	char	*work_dir,
	int	nfiles,
	int	*file_size,
	int	iteration,
	int	write_iterations,
	off64_t	woffset)
{
	int	i;
	int	w;
	int	fd;
	int	index;
	int	size;
	char	file[80];

	/*
	 * Re-write half the files write_iterations times.
	 */
	for (w = 0; w < write_iterations; w++) {
		for (i = 0; i < (nfiles / 2); i++) {
			index = random() % nfiles;
			size = random() % 100000;
			while (size == 0) {
				size = random() % 100000000;
			}

			sprintf(file, "%s/%s", work_dir, itoa(index));
			write_file(file, size, index, woffset);

			file_size[index] = size;
		}
	}

	/*
	 * Read and verify the contents of all the files.
	 */
	for (i = 0; i < nfiles; i++) {
		sprintf(file, "%s/%s", work_dir, itoa(i));
		read_verify(file, file_size[i], i, woffset);
	}
}


void
write_file(
	char	*file,
	int	size,
	int	byte_val,
	off64_t	woffset)
{
	int	fd;
	int	offset;
	int	count;
	int	iosize;
	off64_t	seekval;
	char	str[100];
	char	buf[512];

	fd = open(file, O_CREAT | O_TRUNC | O_WRONLY, 0666);
	if (fd < 0) {
		printf("ERROR: Create %s failed in write_file.  Error is %d", file, errno);
		exit(1);
	}

	/*
	 * Pick a number between 256 and 512 for the size of or I/Os.
	 */
	iosize = (random() % 256) + 256;

	memset(buf, byte_val, iosize);

	seekval = lseek64(fd, woffset, SEEK_SET);
	if (seekval < 0) {
		printf("ERROR: Seek to 0x%llx on file %s failed. Error is %d",
			offset, file, errno);
		exit(1);
	}

	offset = 0;
	while (offset < size) {
		if ((offset + iosize) > size) {
			iosize = size - offset;
		}
		count = write(fd, buf, iosize);
		if (count < 0) {
			printf("ERROR: Write to %s failed in write_file. Error is %d",
				file, errno);
			exit(1);
		}
		offset += count;
	}

	close(fd);
}

void
read_verify(
	char	*file,
	int	size,
	int	byte_val,
	off64_t	woffset)
{
	int	fd;
	int	offset;
	int	diff;
	int	count;
	int	iosize;
	off64_t	seekval;
	char	str[100];
	char	buf[512];
	char	buf2[512];

	fd = open(file, O_RDONLY);
	if (fd < 0) {
		printf("ERROR: Open %s failed in read_verify.  Error is %d",
		       file, errno);
		exit(1);
	}

	if (size == 0) {
		close(fd);
		return;
	}

	/*
	 * Pick a number between 256 and 512 for the size of or I/Os.
	 */
	iosize = (random() % 256) + 256;

	memset(buf, byte_val, 512);

	seekval = lseek64(fd, woffset, SEEK_SET);
	if (seekval < 0) {
		printf("ERROR: Seek to 0x%llx on file %s failed. Error is %d",
			offset, file, errno);
		exit(1);
	}

	offset = 0;
	while (offset < size) {
		count = read(fd, buf2, iosize);
		if (count < 0) {
			printf("ERROR: Read from %s failed in read_verify. Error is %d",
				file, errno);
			exit(1);
		}

		if (count == 0) {
			printf("ERROR: Read from %s failed in read_verify: unexpected EOF", file);
			exit(1);
		}

		diff = bcmp(buf, buf2, count);
		if (diff != 0) {
			dump_diff(file, buf, buf2, offset, count);
			exit(1);
		}
		offset += count;
	}

	close(fd);
}


void
dump_diff(
	char	*file,
	char	*expect,
	char	*got,
	int	offset,
	int	count)
{
	int	i;
	int	j;

	printf("ERROR: File %s got unexpected data at offset %d count %d\n",
	       file, offset, count);
	printf("WARNING: Expected:\n");

	for (i = 0; i < count; i++) {
		printf("WARNING: ");
		for (j = 0; ((j < 20) && (i < count)); j++, i++) {
			printf("%x ", (int)expect[i]);
		}
		printf("\n");
	}

	printf("\nWARNING: Got:\n");

	for (i = 0; i < count; i++) {
		printf("WARNING: ");
		for (j = 0; ((j < 20) && (i < count)); j++, i++) {
			printf("%x ", (int)got[i]);
		}
		printf("\n");
	}
}




char *
itoa(
	int	i)
{
	char		*digit;
	int		idigit;
	static char	str[32];

	str[31] = 0;
	str[30] = '0';
	digit = &str[30];

	if (i == 0) {
		return digit;
	}
	while (i > 0) {
		idigit = i % 10;
		*digit = '0' + idigit;
		i = i / 10;
		digit--;
	}

	digit++;
	return digit;
}

