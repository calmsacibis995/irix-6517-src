#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h>

int aflag = 0;	/* Attribute fork. */
int lflag = 0;	/* list number of blocks with each extent */
int nflag = 0;	/* number of extents specified */
int bmv_iflags = 0;	/* Input flags for F_GETBMAPX */

int dofile(char *);
off64_t file_size(int fd, char * fname);

main(int argc, char **argv)
{
	char	*fname;
	int	i = 0;
	int	option;

	while ((option = getopt(argc, argv, "adln:p")) != EOF) {
		switch (option) {
		case 'a':
			bmv_iflags |= BMV_IF_ATTRFORK;
			aflag = 1;
			break;
		case 'l':
			lflag = 1;
			break;
		case 'n':
			nflag = atoi(optarg);
			break;
		case 'd':
		/* do not recall possibly offline DMAPI files */
			bmv_iflags |= BMV_IF_NO_DMAPI_READ;
			break;
		case 'p':
		/* report unwritten preallocated blocks */
			bmv_iflags |= BMV_IF_PREALLOC;
			break;
		default:
			fprintf(stderr, "Usage: xfs_bmap [-adlp] [-n nx] filename ...\n");
			exit(1);
		}
	}
	if (aflag) 
		bmv_iflags &=  ~(BMV_IF_PREALLOC|BMV_IF_NO_DMAPI_READ);
	while (optind < argc) {
		fname = argv[optind];
		i += dofile(fname);
		optind++;
	}
	return(i ? 1 : 0);
}

off64_t
file_size(int	fd, char *fname)
{
	struct	stat64	st;
	int		i;
	int		errno_save;

	errno_save = errno;	/* in case fstat64 fails */
	i = fstat64(fd, &st);
	if (i < 0) {
		fprintf(stderr,"fstat64 failed for %s", fname);
		perror("fstat64");
		errno = errno_save;
		return -1;
	}
	return st.st_size;
}
	

int
dofile(char *fname)
{
	int		fd;
	struct fsxattr	fsx;
	int		i;
	struct getbmapx	*map;
	char		mbuf[1024];
	int		map_size;
	int		loop = 0;

	fd = open(fname, O_RDONLY);
	if (fd < 0) {
		sprintf(mbuf, "open %s", fname);
		perror(mbuf);
		return 1;
	}
	map_size = nflag ? nflag+1 : 32;	/* initial guess - 256 for checkin KCM */
	map = malloc(map_size*sizeof(*map));
	if (map == NULL) {
		fprintf(stderr, "malloc of %d bytes failed.\n",
							map_size*sizeof(*map));
		close(fd);
		return 1;
	}
		

/*	Try the fcntl(FGETBMAPX) for the number of extents specified by
 *	nflag, or the initial guess number of extents (256).
 *
 *	If there are more extents than we guessed, use fcntl 
 *	(F_FSGETXATTR[A]) to get the extent count, realloc some more 
 *	space based on this count, and try again.
 *
 *	If the initial FGETBMAPX attempt returns EINVAL, this may mean
 *	that we tried the FGETBMAPX on a zero length file.  If we get
 *	EINVAL, check the length with fstat() and return "no extents"
 *	if the length == 0.
 *
 *	Why not do the fcntl(F_FSGETXATTR[A]) first?  Two reasons:
 *	(1)	The extent count may be wrong for a file with delayed
 *		allocation blocks.  The F_GETBMAPX forces the real
 *		allocation and fixes up the extent count.
 *	(2)	For F_GETBMAP[X] on a DMAPI file that has been moved 
 *		offline by a DMAPI application (e.g., DMF) the 
 *		F_FSGETXATTR only reflects the extents actually online.
 *		Doing F_GETBMAPX call first forces that data blocks online
 *		and then everything proceeds normally (see PV #545725).
 *		
 *		If you don't want this behavior on a DMAPI offline file,
 *		try the "-d" option which sets the BMV_IF_NO_DMAPI_READ
 *		iflag for F_GETBMAPX.
 */

	do {	/* loop a miximum of two times */

		bzero(map, sizeof(*map));	/* zero header */
		map->bmv_length = -1;
		map->bmv_count = map_size;
		map->bmv_iflags = bmv_iflags;
		i = fcntl(fd, F_GETBMAPX, map);
		if (i < 0) {
			if (errno == EINVAL && !aflag && file_size(fd, fname) == 0) {
				break;
			} else	{
				sprintf(mbuf, "fcntl(F_GETBMAPX (iflags 0x%x) %s",
							map->bmv_iflags, fname);
				perror(mbuf);
				close(fd);
				free(map);
				return 1;
			}
		}
		if (nflag)
			break;
		if (map->bmv_entries < map->bmv_count-1)
			break;
		/* Get number of extents from fcntl F_FSGETXATTR[A]
		 * syscall.
		 */
		i = fcntl(fd, aflag ? F_FSGETXATTRA : F_FSGETXATTR, &fsx);
		if (i < 0) {
			sprintf(mbuf, "fcntl(F_FSGETXATTR%s) %s",
				aflag ? "A" : "", fname);
			perror(mbuf);
			close(fd);
			free(map);
			return 1;
		}
		if (fsx.fsx_nextents >= map_size-1) {
			map_size = 2*(fsx.fsx_nextents+1);
			map = realloc(map, map_size*sizeof(*map));
			if (map == NULL) {
				fprintf(stderr,"cannot realloc %d bytes.\n",
						map_size*sizeof(*map));
				close(fd);
				return 1;
			}
		}
	} while (++loop < 2);
	if (!nflag) {
		if (map->bmv_entries <= 0) {
			printf("%s: no extents\n", fname);
			close(fd);
			free(map);
			return 0;
		}
	}
	close(fd);
	printf("%s:\n", fname);
	for (i = 0; i < map->bmv_entries; i++) {
		printf("\t%d: [%lld..%lld]: ", i, map[i + 1].bmv_offset,
			map[i + 1].bmv_offset + map[i + 1].bmv_length - 1LL);
		if (map[i + 1].bmv_block == -1)
			printf("hole");
		else
			printf("%lld..%lld", map[i + 1].bmv_block,
				map[i + 1].bmv_block +
					map[i + 1].bmv_length - 1LL);
		if (lflag)
			printf(" %lld blocks\n", map[i+1].bmv_length);
		else
			printf("\n");
	}
	free(map);
	return 0;
}
