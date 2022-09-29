
/*
 * fop -o mk|app|trun|ver [-vdf] [-b nblocks] filename
 *	-f	fork a child to do the work
 *
 */
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/param.h>	/* BBTOB */

int vflag;
int dflag;
int fflag;
char *progname;

mkfile(char *filename, int nblocks);
verify(char *filename);
appfile(char *filename, int nblocks);

main(int argc, char **argv)
{
	int nblocks;
	char *filename;
	char *op;
	int c;
	extern int optind;
	extern char *optarg;
	
	progname = argv[0];
	while ((c = getopt(argc, argv, "o:b:dvf")) != -1)
		switch (c) {
		case 'v':	vflag = 1;			break;
		case 'd':	dflag = 1;			break;
		case 'f':	fflag = 1;			break;
		case 'o':	op = optarg;			break;
		case 'b':	nblocks = atoi(optarg);		break;
		default:
usage:
			fprintf(stderr,
			"usage: %s -o m|a|t|v [-dvf] [-b nblocks] file\n",
			progname);
			exit(1);
		}
	filename = argv[optind];

	if (getenv("DEBUG"))
		dflag = 1;
	if (getenv("VERBOSE"))
		vflag = 1;

	if (fflag && fork())
			exit(0);

	switch ( *op ) {
	case 'm': 	mkfile(filename, nblocks);		break;
	case 'a':	appfile(filename, nblocks);		break;
	case 't':	truncate(filename, BBTOB(nblocks));	break;
	case 'v':	verify(filename);			break;
	}
	
}

mkfile(char *filename, int nblocks)
{
	int fd;
	struct stat sb;
	int t0;

	fd = open(filename, O_RDWR|O_CREAT|O_TRUNC, 0644);
	if (fd == -1) {
		fprintf(stderr, "%s: open(%s): %s\n", progname, filename,
			strerror(errno));
		exit(1);
	}
	if (fstat(fd, &sb) == -1) {
		fprintf(stderr, "%s: fstat(%s): %s\n", progname, filename,
			strerror(errno));
		exit(1);
	}

	for (t0 = 0; t0 < nblocks; t0++) {
		char buf[64];

		lseek(fd, BBTOB(t0), SEEK_SET);
		sprintf(buf, "%15d %15d mkfile\n", t0, sb.st_ino);
		if (write(fd, buf, strlen(buf)) != strlen(buf)) {
			if (errno != ENOSPC)
				fprintf(stderr, "%s: write(%s): %s\n",
					progname, filename, strerror(errno));
			exit(1);
		}
	}
	fsync(fd);
	close(fd);
	return 0;
}

verify(char *filename)
{
	int fd;
	struct stat sb;
	char buf[32];
	int offset, offval, ino;

	fd = open(filename, O_RDONLY);
	if (fd == -1) {
		perror("open");
		exit(1);
	}

	if (fstat(fd, &sb) == -1) {
		perror("fstat");
		exit(1);
	}

	for (offset=0; offset < sb.st_size; offset += BBSIZE) {
		lseek(fd, offset, SEEK_SET);
		if (read(fd, buf, 32) != 32) {
			perror("read");
			exit(1);
		}
		offval = atoi(buf);
		ino = atoi(buf+15);
		if (offval != offset/BBSIZE || ino != sb.st_ino) {
			fprintf(stderr,
			"verify: %s offset=%d (should=%d) ino=%d (should=%d)\n",
				filename,
				offval, offset/BBSIZE, ino, sb.st_ino);
			exit(2);
		}
	}
	return 0;
}


appfile(char *filename, int nblocks)
{
	int fd;
	struct stat statbuf;
	int t0;
	
	fd = open(filename, O_WRONLY, 0644);
	if (fd == -1) {
		/*perror("open");*/
		exit(1);
	}
	if (fstat(fd, &statbuf) == -1) {
		perror("stat");
		exit(1);
	}
	nblocks = BTOBB(statbuf.st_size) + nblocks;

	for (t0 = BTOBB(statbuf.st_size); t0 < nblocks; t0++) {
		char buf[64];

		lseek(fd, BBTOB(t0), SEEK_SET);
		sprintf(buf, "%15d %15d appfile\n", t0, statbuf.st_ino);
		if (write(fd, buf, strlen(buf)) != strlen(buf)) {
			if (errno != ENOSPC)
				perror("write");
			exit(1);
		}
	}
	return 0;
}
