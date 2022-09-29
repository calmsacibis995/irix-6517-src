/*
 * find a front file given a file ID (FID data) for the back file
 */
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <ftw.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/fcntl.h>

#define FID_OFFSET	(off_t)152		/* byte offset for the file ID */

char *Progname;
fid_t Searchfid;
char *Hexdigits = "0123456789abcdef";

extern int errno;

int
visit(const char *name, const struct stat *sb, int ftype, struct FTW *ftwp)
{
	int status = 0;
	int fd;
	fid_t filefid;
	off_t offset;
	int bytes;

	switch (ftype) {
		case FTW_F:
			/*
			 * Open the indicated file
			 */
			fd = open(name, O_RDONLY);
			if (fd == -1) {
				status = errno;
				perror(name);
			} else {
				/*
				 * seek to the file ID offset
				 */
				offset = lseek(fd, FID_OFFSET, SEEK_SET);
				if (offset == -1) {
					status = errno;
					perror(name);
				} else if (offset != FID_OFFSET) {
					fprintf(stderr, "visit: bad seek\n");
					status = -2;
				} else {
					/*
					 * read the file ID
					 */
					bytes = read(fd, &filefid, sizeof(fid_t));
					if (bytes == -1) {
						status = errno;
						perror(name);
					} else if (bytes != sizeof(fid_t)) {
						fprintf(stderr, "visit: short read (%d of %d)\n",
							bytes, sizeof(fid_t));
						status = -2;
					} else {
						/*
						 * if the file ID matches the search ID, print the
						 * name
						 */
						if ((filefid.fid_len == Searchfid.fid_len) &&
							(bcmp(filefid.fid_data, Searchfid.fid_data,
							filefid.fid_len) == 0)) {
								printf("%s\n", name);
						}
					}
				}
				close(fd);
			}
			break;
		case FTW_D:			/* skip directories */
		case FTW_DP:
			break;
		case FTW_SLN:
			fprintf(stderr, "visit: dangling symlink %s\n", name);
			break;
		case FTW_DNR:
			fprintf(stderr, "visit: unreadable directory %s\n", name);
			break;
		case FTW_NS:
			fprintf(stderr, "visit: permission denied for %s\n", name);
			break;
		default:
			fprintf(stderr, "visit: bad ftype %d namd %s\n", ftype, name);
			status = -2;
	}
	return(status);
}

int
main(int argc, char **argv)
{
	int status;
	char *data;
	char *dir;
	int i;

	Progname = *argv++;
	argc--;
	if (argc < 2) {
		fprintf(stderr, "File ID data and search dir must be supplied\n");
		exit(-1);
	}
	data = *argv++;
	dir = *argv++;
	for (i = 0; (i < MAXFIDSZ) && *data; i++, data += 2) {
		Searchfid.fid_data[i] = (strchr(Hexdigits, *data) - Hexdigits) << 4 |
			((strchr(Hexdigits, *(data+1)) - Hexdigits) & 0xff);
	}
	if (*data) {
		fprintf(stderr, "FID data too long\n");
		exit(-1);
	}
	Searchfid.fid_len = i;
	if ((status = nftw(dir, visit, 5, FTW_PHYS | FTW_MOUNT)) != 0) {
		if (status == -1) {
			perror("nftw");
			exit(errno);
		} else {
			exit(status);
		}
	}
	exit(0);
}
