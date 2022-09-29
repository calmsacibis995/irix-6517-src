/*
 * corrupt path ...
 *
 * Randomly corrupt file headers of cached files in a specified cache.
 *
 * path is the path name for the cache to be corrupted.  These path names
 * are of the form cachedir/cacheid.  Multiple paths may be specified.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <ftw.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cachefs/cachefs_fs.h>

double Prob = (double)0.5;
int Verbose = 0;

int
corrupt(const char *path, const struct stat *sbp, int filetype,
	struct FTW *ftwp)
{
	int error;
	int newoffset;
	int fd;
	int bytes;
	off_t offset;
	char map[FILEHEADER_SIZE];
	char c;

	bzero(&map, FILEHEADER_SIZE);
	/*
	 * only operate on regular files
	 */
	if ((filetype == FTW_F) && (drand48() < Prob)) {
		fd = open(path, O_WRONLY);
		if (fd == -1) {
			perror(path);
			return(-1);
		}
		if (Verbose) {
			printf("corrupting %s\n", path);
		}
		/*
		 * pick a number of bytes between 0 and FILEHEADER_SIZE
		 * this will be the number of bytes to corrupt
		 */
		for (bytes = (int)(drand48() * (double)FILEHEADER_SIZE) + 1; bytes;
			bytes--) {
				assert(bytes <= FILEHEADER_SIZE);
				assert(bytes > 0);
				/*
				 * corrupt a byte
				 * don't do one we've already done
				 */
				do {
					newoffset = 0;
					/*
					 * randomly pick an offset between 0 and FILEHEADER_SIZE
					 * if we have not already corrupted that byte, do so
					 * otherwise, select another offset
					 */
					offset = (off_t)(drand48() * (double)FILEHEADER_SIZE);
					assert(offset < (off_t)FILEHEADER_SIZE);
					assert(offset >= (off_t)0);
					if (map[(int)offset]) {
						newoffset = 1;
					} else {
						/*
						 * indicate that this byte has been corrupted and
						 * seek to the selected offset
						 */
						map[(int)offset] = 1;
						if (lseek(fd, offset, SEEK_SET) == -1) {
							error = errno;
							fprintf(stderr, "lseek %s: %s\n", path,
								strerror(error));
							close(fd);
							return(-1);
						}
						/*
						 * randomly select a byte value and write it out
						 */
						c = (char)(drand48() * (double)256);
						if (write(fd, &c, 1) == -1) {
							fprintf(stderr, "write %s: %s\n", path,
								strerror(error));
							close(fd);
							return(-1);
						}
					}
				} while (newoffset);
		}
		close(fd);
	}
	return(0);
}

void
usage(char *progname)
{
	fprintf(stderr, "usage: %s [-v] [-p prob] path...\n", progname);
}

int
main(int argc, char **argv)
{
	int opt;
	int status = 0;
	char *progname = *argv;

	srand48(time(NULL));
	while ((opt = getopt(argc, argv, "vp:")) != EOF) {
		switch (opt) {
			case 'v':
				Verbose++;
				break;
			case 'p':
				Prob = strtod(optarg, NULL);
				break;
			default:
				usage(progname);
				exit(1);
		}
	}
	for ( ; optind < argc; optind++) {
		if (nftw(argv[optind], corrupt, 5, FTW_MOUNT | FTW_PHYS) == -1) {
			fprintf(stderr, "%s: nftw error for %s: %s\n", progname,
				argv[optind], strerror(errno));
			status++;
		}
	}
	return(status);
}
