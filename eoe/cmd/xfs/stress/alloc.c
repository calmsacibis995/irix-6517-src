#include <sys/param.h>
#include <sys/statvfs.h>
#include <sys/fs/xfs_itable.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define	FSBTOBB(f)	OFFTOBBT(FSBTOOFF(f))
#define	BBTOFSB(b)	OFFTOFSB(BBTOOFF(b))
#define	OFFTOFSB(o)	((o) / blocksize)
#define	FSBTOOFF(f)	((f) * blocksize)

int
main(int argc, char **argv)
{
	int		blocksize = 0;
	struct getbmap	bm[2];
	int		c;
	char		*dirname = NULL;
	int		done = 0;
	struct flock64	f;
	char		*filename = NULL;
	int		fd;
	off64_t		len;
	char		line[1024];
	off64_t		off;
	int		oflags;
	static char	*opnames[] =
		{ "freesp", "allocsp", "unresvsp", "resvsp" };
	int		opno;
	static int	optab[] =
		{ F_FREESP64, F_ALLOCSP64, F_UNRESVSP64, F_RESVSP64 };
	char		*p;
	int		rflag = 0;
	struct statvfs64	svfs;
	int		tflag = 0;
	int		unlinkit = 0;
	__int64_t	v;

	while ((c = getopt(argc, argv, "b:d:f:rt")) != -1) {
		switch (c) {
		case 'b':
			blocksize = atoi(optarg);
			break;
		case 'd':
			if (filename) {
				printf("can't specify both -d and -f\n");
				exit(1);
			}
			dirname = optarg;
			break;
		case 'f':
			if (dirname) {
				printf("can't specify both -d and -f\n");
				exit(1);
			}
			filename = optarg;
			break;
		case 'r':
			rflag = 1;
			break;
		case 't':
			tflag = 1;
			break;
		default:
			printf("unknown option\n");
			exit(1);
		}
	}
	if (!dirname && !filename)
		dirname = ".";
	if (!filename) {
		static char	tmpfile[] = "allocXXXXXX";

		mktemp(tmpfile);
		filename = malloc(strlen(tmpfile) + strlen(dirname) + 2);
		sprintf(filename, "%s/%s", dirname, tmpfile);
		unlinkit = 1;
	}
	oflags = O_RDWR | O_CREAT | (tflag ? O_TRUNC : 0);
	fd = open(filename, oflags, 0666);
	if (fd < 0) {
		perror(filename);
		exit(1);
	}
	if (unlinkit)
		unlink(filename);
	if (fstatvfs64(fd, &svfs) < 0) {
		perror(filename);
		exit(1);
	}
	if (!blocksize)
		blocksize = (int)svfs.f_bsize;
	printf("blocksize %d\n", blocksize);
	if (rflag) {
		struct fsxattr a;

		if (fcntl(fd, F_FSGETXATTR, &a) < 0) {
			perror("F_FSGETXATTR");
			exit(1);
		}
		a.fsx_xflags |= XFS_XFLAG_REALTIME;
		if (fcntl(fd, F_FSSETXATTR, &a) < 0) {
			perror("F_FSSETXATTR");
			exit(1);
		}
	}
	while (!done && gets(line)) {
		opno = 0;
		switch (line[0]) {
		case 'r':
			opno++;
		case 'u':
			opno++;
		case 'a':
			opno++;
		case 'f':
			v = strtoll(&line[2], &p, 0);
			if (*p == 'b') {
				off = FSBTOOFF(v);
				p++;
			} else
				off = v;
			f.l_whence = SEEK_SET;
			f.l_start = off;
			if (*p == '\0')
				v = 1;
			else
				v = strtoll(p, &p, 0);
			if (*p == 'b') {
				len = FSBTOOFF(v);
				p++;
			} else
				len = v;
			f.l_len = len;
			c = fcntl(fd, optab[opno], &f);
			if (c < 0) {
				perror(opnames[opno]);
				break;
			}
			bm[0].bmv_offset = OFFTOBBT(off);
			bm[0].bmv_length = OFFTOBB(len);
			bm[0].bmv_count = 2;
			for (;;) {
				if (fcntl(fd, F_GETBMAP, bm) < 0) {
					perror("getbmap");
					break;
				}
				if (bm[0].bmv_entries == 0)
					break;
				printf("[%lld,%lld]: ",
					BBTOFSB(bm[1].bmv_offset),
					BBTOFSB(bm[1].bmv_length));
				if (bm[1].bmv_block == -1)
					printf("hole");
				else
					printf("%lld..%lld",
						BBTOFSB(bm[1].bmv_block),
						BBTOFSB(bm[1].bmv_block +
							bm[1].bmv_length - 1));
				printf("\n");
			}
			break;
		case 'm':
			p = &line[1];
			v = strtoll(p, &p, 0);
			if (*p == 'b') {
				off = FSBTOBB(v);
				p++;
			} else
				off = v;
			bm[0].bmv_offset = off;
			if (*p == '\0')
				len = -1;
			else {
				v = strtoll(p, &p, 0);
				if (*p == 'b')
					len = FSBTOBB(v);
				else
					len = OFFTOBB(v);
			}
			bm[0].bmv_length = len;
			bm[0].bmv_count = 2;
			for (;;) {
				if (fcntl(fd, F_GETBMAP, bm) < 0) {
					perror("getbmap");
					break;
				}
				if (bm[0].bmv_entries == 0)
					break;
				printf("[%lld,%lld]: ",
					BBTOFSB(bm[1].bmv_offset),
					BBTOFSB(bm[1].bmv_length));
				if (bm[1].bmv_block == -1)
					printf("hole");
				else
					printf("%lld..%lld",
						BBTOFSB(bm[1].bmv_block),
						BBTOFSB(bm[1].bmv_block +
							bm[1].bmv_length - 1));
				printf("\n");
			}
			break;
		case 't':
			p = &line[1];
			v = strtoll(p, &p, 0);
			if (*p == 'b')
				off = FSBTOOFF(v);
			else
				off = v;
			if (ftruncate64(fd, off) < 0) {
				perror("ftruncate");
				break;
			}
			break;
		case 'q':
			done = 1;
			break;
		default:
			printf("unknown command '%s'\n", line);
			break;
		}
	}
	close(fd);
	exit(0);
	/* NOTREACHED */
}
