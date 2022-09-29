#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/dirent.h>
#include <sys/fcntl.h>
#include <sys/stat.h>

char *
filetype(mode_t mode)
{
	static char *typestr;

	if (S_ISFIFO(mode)) {
		typestr = "FIFO";
	} else if (S_ISCHR(mode)) {
		typestr = "CHAR";
	} else if (S_ISDIR(mode)) {
		typestr = "DIR";
	} else if (S_ISBLK(mode)) {
		typestr = "BLOCK";
	} else if (S_ISREG(mode)) {
		typestr = "REG";
	} else if (S_ISLNK(mode)) {
		typestr = "SLNK";
	} else if (S_ISSOCK(mode)) {
		typestr = "SOCK";
	} else {
		typestr = "UNKNOWN";
	}
	return(typestr);
}

int
main(int argc, char **argv)
{
	char dirbuf[1024];
	dirent64_t *dep;
	char *progname = *argv++;
	int status = 0;
	int fd;
	int bytes;
	struct stat sb;
	char namebuf[1024];
	char *dirname;

	for (--argc; argc; --argc, ++argv) {
		dirname = *argv;
		if ((fd = open(dirname, O_RDONLY)) == -1) {
			perror(dirname);
			status = 1;
		} else {
			while ((bytes = getdents64(fd, (dirent64_t *)dirbuf, 1024)) > 0) {
				printf("getdents64 returned %d bytes\n", bytes);
				for (dep = (dirent64_t *)dirbuf; bytes; bytes -= dep->d_reclen,
					dep = (dirent64_t *)((long)dep + (long)dep->d_reclen)) {
						if (dep->d_reclen == 0) {
							fprintf( stderr, "bad dirent, zero reclen\n");
							status = 1;
							goto loopexit;
						} else if (strlen(dep->d_name) == 0) {
							printf("%d\t%lld\t%lld\t%d\tNULL\n", bytes,
								dep->d_ino, dep->d_off, (int)dep->d_reclen);
						} else {
							sprintf(namebuf, "%s/%s", dirname, dep->d_name);
							if (stat(namebuf, &sb) == -1) {
								fprintf(stderr, "%d\t%lld\t%lld\t%d\t%s: ",
									bytes, dep->d_ino, dep->d_off,
									(int)dep->d_reclen, dep->d_name);
								perror(dep->d_name);
								status = 1;
								goto loopexit;
							} else {
								printf("%d\t%lld\t%lld\t%d\t%s\t%s\t0x%x\n",
									bytes, dep->d_ino, dep->d_off,
									(int)dep->d_reclen, dep->d_name,
									filetype(sb.st_mode), sb.st_mode);
							}
						}
				}
			}
loopexit:
			if (bytes == -1) {
				perror("getdents64");
				status = 1;
			}
			close(fd);
		}
	}
	return(status);
}
