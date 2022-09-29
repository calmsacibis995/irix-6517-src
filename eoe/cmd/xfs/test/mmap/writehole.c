
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>


/*
 * Prototypes.
 */
int
create_file(char	*file);

void
write_file(int		fd,
	   int		block_size,
	   int		size);

caddr_t
map_file(int		fd,
	 int		size);

void
map_read1(caddr_t	addr,
	  int		block_size,
	  int		size);

void
map_write(caddr_t	addr,
	  int		block_size,
	  int		size);

void
map_read2(caddr_t	addr,
	  int		block_size,
	  int		size);

void
read_file2(int		fd,
	   int		block_size,
	   int		size);

void
unmap_file(caddr_t	addr,
	   int		size);

void
usage(void)
{
	fprintf(stderr, "usage: mapwrite -f filename [-s filesize] [-b blocksize]\n");
	exit(EINVAL);
}

int
main(int	argc,
     char	*argv[])
{
	int		c;
	off_t		size;
	char		*file;
	int		fd;
	int		block_size;
	caddr_t		addr;

	size = 4096 * 4096;
	file = NULL;
	block_size = 512;

	while ((c = getopt(argc, argv, "b:f:s:")) != EOF) {
		switch (c) {
		case 'b':
			block_size = atoi(optarg);
			break;
		case 'f':
			file = optarg;
			break;
		case 's':
			size = atoi(optarg);
			break;
		case '?':
			usage();
		}
	}
	if (((argc - optind) != 0) || (file == NULL)) {
		usage();
	}

	fd = create_file(file);
	write_file(fd, block_size, size);
	fsync(fd);
	addr = map_file(fd, size);
	map_read1(addr, block_size, size);
	map_write(addr, block_size, size);
	map_read2(addr, block_size, size);
	msync(addr, size, MS_SYNC | MS_INVALIDATE);
	read_file2(fd, block_size, size);
	unmap_file(addr, size);
	close(fd);
	exit(0);
}


int
create_file(char *file)
{
	int	fd;

	fd = open(file, O_TRUNC | O_CREAT | O_RDWR, 0777);
	if (fd < 0) {
		perror("open failed");
		exit(1);
	}

	return fd;
}

void
write_file(int	fd,
	   int	block_size,
	   int	size)
{
	char	*buf;
	int	*int_buf;
	int	i;
	int	offset;
	int	count;

	printf("write_file\n");
	if (ftruncate(fd, size) < 0) {
		perror("write_file: truncate failed");
		exit(1);
	}
	offset = lseek(fd, 0, SEEK_SET);
	if (offset < 0) {
		perror("write_file: seek failed");
		exit(1);
	}

	buf = (char *)malloc(block_size);
	int_buf = (int *)buf;
	while (offset < size) {
		if ((offset + block_size) > size) {
			block_size = size - offset;
		}
		for (i = 0; i < (block_size / 4); i++) {
			int_buf[i] = offset + i;
		}
		if (lseek(fd, offset, SEEK_SET) < 0) {
			perror("write_file: seek failed");
			exit(1);
		}
		count = write(fd, buf, block_size);
		if (count < block_size) {
			perror("write failed");
			exit(1);
		}
		offset += 2 * block_size;
		printf(".");
		fflush(stdout);
	}
	free(buf);
	printf("\n");
}


caddr_t
map_file(int	fd,
	 int	size)
{
	caddr_t		addr;

	addr = (caddr_t)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED,
			     fd, 0);

	if (addr == (caddr_t)-1) {
		perror("mmap failed");
		exit(1);
	}

	return addr;
}

void
map_read1(caddr_t	addr,
	  int		block_size,
	  int		size)
{
	int	offset;
	int	*intp;
	int	i;

	printf("map_read1\n");
	offset = 0;
	intp = (int *)addr;
	while (offset < size) {
		if ((offset + block_size) > size) {
			block_size = size - offset;
		}
		for (i = 0; i < (block_size / 4); i++) {
			if (*intp != (offset + i)) {
				printf("map_read1: offset %d expected %x got %x\n", (offset + (i * 4)), (offset + i), *intp);
			}
			intp++;
		}
		offset += block_size;
		if (offset >= size) {
			break;
		}
		for (i = 0; i < (block_size / 4); i++) {
			if (*intp != 0) {
				printf("map_read1: offset %d expected 0 got %x\n", (offset + (i * 4)), *intp);
			}
			intp++;
		}
		offset += block_size;
		printf(".");
		fflush(stdout);
	}
	printf("\n");
}

void
map_write(caddr_t	addr,
	  int		block_size,
	  int		size)
{
	int	offset;
	int	*intp;
	int	i;

	printf("map_write\n");
	offset = 0;
	intp = (int *)addr;
	while (offset < size) {
		if ((offset + block_size) > size) {
			block_size = size - offset;
		}
		for (i = 0; i < (block_size / 4); i++) {
			*intp = -(offset + i);
			intp++;
		}
		offset += block_size;
		printf(".");
		fflush(stdout);
	}
	printf("\n");
}

void
map_read2(caddr_t	addr,
	  int		block_size,
	  int		size)
{
	int	offset;
	int	*intp;
	int	i;

	printf("map_read2\n");
	offset = 0;
	intp = (int *)addr;
	while (offset < size) {
		if ((offset + block_size) > size) {
			block_size = size - offset;
		}
		for (i = 0; i < (block_size / 4); i++) {
			if (*intp != -(offset + i)) {
				printf("map_read2: offset %d expected %x got %x\n", (offset + (i * 4)), (offset + i), *intp);
			}
			intp++;
		}
		offset += block_size;
		printf(".");
		fflush(stdout);
	}
	printf("\n");
}


void
read_file2(int	fd,
	   int	block_size,
	   int	size)
{
	int	offset;
	char	*buf;
	int	*intp;
	int	i;
	int	count;

	printf("read_file2\n");
	offset = lseek(fd, 0, SEEK_SET);
	if (offset < 0) {
		perror("read_file2: lseek failed");
		exit(1);
	}

	buf = (char *)malloc(block_size);
	while (offset < size) {
		if ((offset + block_size) > size) {
			block_size = size - offset;
		}
		count = read(fd, buf, block_size);
		if (count < block_size) {
			perror("read_file2: read failed");
			exit(1);
		}
		intp = (int *)buf;
		for (i = 0; i < (block_size / 4); i++) {
			if (*intp != -(offset + i)) {
				printf("read_file2: offset %d expected %x got %x\n", (offset + (i * 4)), -(offset + i), *intp);
			}
			intp++;
		}
		offset += block_size;
		printf(".");
		fflush(stdout);
	}
	free(buf);
	printf("\n");
}


void
unmap_file(caddr_t	addr,
	   int		size)
{
	munmap(addr, size);
}
