#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/param.h>
#include <sys/buf.h>
#include <sys/stat.h>
#include <cachefs/cachefs_fs.h>
#include "subr.h"

char *Progname;

extern int errno;

void
print_dirents(char *dirbuf, int dirbuflen)
{
	dirent64_t *dep = (dirent64_t *)dirbuf;

	for (dep = (dirent64_t *)dirbuf; dirbuflen; dirbuflen -= dep->d_reclen,
		dep = (dirent64_t *)((u_long)dep + (u_long)dep->d_reclen)) {
			if (dep->d_reclen > dirbuflen) {
				pr_err("truncated directory entry");
			}
			printf("%d\t%d\t%d\t%s\n", dep->d_ino, (int)dep->d_off,
				(int)dep->d_reclen, dep->d_name);
	}
}

int
main(int argc, char **argv)
{
	int dirbuflen;
	char *dirbuf;
	int fd;
	char *file;
	int status = 0;
	fileheader_t fh;
	int bytes;
	struct stat sb;

	Progname = *argv++;
	argc--;
	for (file = *argv; file && argc; argc--, file = *++argv) {
		fd = open(file, O_RDONLY);
		if (fd == -1) {
			pr_err("open error for %s: %s", file, strerror(errno));
			status = 1;
		} else if (fstat(fd, &sb) == -1) {
			pr_err("stat error for %s: %s", file, strerror(errno));
			status = 1;
		} else {
			bytes = read(fd, &fh, FILEHEADER_SIZE);
			switch (bytes) {
				case -1:
					pr_err("read error for %s: %s", file, strerror(errno));
					status = 1;
					break;
				case FILEHEADER_SIZE:
					dirbuflen = sb.st_size - FILEHEADER_SIZE;
					dirbuf = malloc(dirbuflen);
					if (dirbuf) {
						bytes = read(fd, dirbuf, dirbuflen);
						if (bytes == dirbuflen) {
							print_dirents(dirbuf, dirbuflen);
						} else if (bytes == -1) {
							pr_err("read error for %s: %s", file,
								strerror(errno));
							status = 1;
						} else {
							pr_err("Short read (%d of %d): %s", bytes,
								dirbuflen, file);
							status = 1;
						}
						free(dirbuf);
					} else {
						pr_err("Out of memory");
						status = 1;
					}
					break;
				case 0:
					pr_err("Empty file: %s", file);
					status = 1;
					break;
				default:
					pr_err("Short file header (%d of %d): %s", bytes,
						FILEHEADER_SIZE, file);
					status = 1;
			}
			close(fd);
		}
	}
	return(status);
}
