#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syssgi.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_itable.h>
#include <sys/param.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "types.h"
#include "jdm.h"

void
dotime(void *ti, char *s)
{
	char	*c;
	xfs_bstime_t *t;

	t = (xfs_bstime_t *)ti;

	c = ctime(&t->tv_sec);
	printf("\t%s %19.19s.%09d %s", s, c, t->tv_nsec, c + 20);
}

void
printbstat(xfs_bstat_t *sp)
{
	printf("ino %lld mode %#o nlink %d uid %d gid %d rdev %#x\n",
		sp->bs_ino, sp->bs_mode, sp->bs_nlink,
		sp->bs_uid, sp->bs_gid, sp->bs_rdev);
	printf("\tblksize %d size %lld blocks %lld xflags %#x extsize %d\n",
		sp->bs_blksize, sp->bs_size, sp->bs_blocks,
		sp->bs_xflags, sp->bs_extsize);
	dotime(&sp->bs_atime, "atime");
	dotime(&sp->bs_mtime, "mtime");
	dotime(&sp->bs_ctime, "ctime");
	printf( "\textents %d %d gen %d\n",
		sp->bs_extents, sp->bs_aextents, sp->bs_gen);
	printf( "\tDMI: event mask 0x%08x state 0x%04x\n",
		sp->bs_dmevmask, sp->bs_dmstate);
}

void
printstat(struct stat *sp)
{
	printf("ino %lld mode %#o nlink %d uid %d gid %d rdev %#x\n",
		sp->st_ino, sp->st_mode, sp->st_nlink,
		sp->st_uid, sp->st_gid, sp->st_rdev);
	printf("\tblksize %d size %lld blocks %lld\n",
		sp->st_blksize, sp->st_size, sp->st_blocks);
	dotime(&sp->st_atim, "atime");
	dotime(&sp->st_mtim, "mtime");
	dotime(&sp->st_ctim, "ctime");
}

main(int argc, char **argv)
{
	int		count;
	int		total = 0;
	int		fsfd;
	int		i;
	ino64_t		last = 0;
	char		*name;
	int		nent;
	int		quiet = 0;
	int		statit = 0;
	int		verbose = 0;
	xfs_bstat_t	*t;
	int		ret;
	jdm_fshandle_t	*fshandlep;
	int		fd;
	struct stat	sb;
	int nread;
	char *cc_readlinkbufp;
	int cc_readlinkbufsz;
	int 		c;

	while ((c = getopt(argc, argv, "cl:qv")) != -1) {
		switch (c) {
		case 'q':
			quiet = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'c':
			statit = 1;
			break;
		case 'l':
			last = atoi(optarg);
			break;
		case '?':
		printf("usage: xfs_bstat [-c] [-q] [-v] [ dir [ batch_size ]]\n");
		printf("   -c   Check the results against stat(3) output\n");
		printf("   -q   Quiet\n");
		printf("   -l _num_  Inode to start with\n");
		printf("   -v   Verbose output\n");
			exit(1);
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 1)
		name = ".";
	else
		name = *argv;

	fsfd = open(name, O_RDONLY);
	if (fsfd < 0) {
		perror(name);
		exit(1);
	}
	if (argc < 2)
		nent = 4096;
	else
		nent = atoi(*++argv);

	if (verbose)
		printf("Bulkstat test on %s, batch size=%d statcheck=%d\n", 
			name, nent, statit);

	if (statit) {
		fshandlep = jdm_getfshandle( name );
		if (! fshandlep) {
			printf("unable to construct sys handle for %s: %s\n",
			  name, strerror(errno));
			return -1;
		}
	}

	t = malloc(nent * sizeof(*t));
	if (verbose)
		printf("SGI_FS_BULKSTAT test: last=%lld nent=%d\n", last, nent);
	while ((ret = syssgi(SGI_FS_BULKSTAT, fsfd, &last, nent, t, &count)) == 0) {
		total += count;
		if (count == 0)
			exit(0);
		if (verbose)
		printf("SGI_FS_BULKSTAT test: last=%lld ret=%d count=%d total=%d\n", 
		        last, ret, count, total);
		if ( quiet && ! statit )
			continue;
		for (i = 0; i < count; i++) {
			if (! quiet) {
				printbstat(&t[i]);
			}
	
			if (statit) {
				switch(t[i].bs_mode & S_IFMT) {
				case S_IFLNK:
					cc_readlinkbufsz = MAXPATHLEN;
					cc_readlinkbufp = (char *)calloc( 
							      1, 
							      cc_readlinkbufsz);
					nread = jdm_readlink( 
						   fshandlep,
						   &t[i],
						   cc_readlinkbufp,
						   cc_readlinkbufsz);
					free(cc_readlinkbufp);
					if ( nread < 0 ) {
						printf(
					      "could not read symlink ino %llu\n",
						      t[i].bs_ino );
						printbstat(&t[i]);
					}
					break;

				case S_IFCHR:
				case S_IFBLK:
				case S_IFIFO:
				case S_IFNAM:
				case S_IFSOCK:
					break;

				case S_IFREG:
				case S_IFDIR:
					fd = jdm_open( fshandlep, &t[i], O_RDONLY);
					if (fd < 0) {
						printf(
					"unable to open handle ino %lld: %s\n",
						  t[i].bs_ino, strerror(errno));
						continue;
					}
					if (fstat(fd, &sb) < 0) {
						printf(
					"unable to stat ino %lld: %s\n",
						  t[i].bs_ino, strerror(errno));
					}
					close(fd);

					/*
					 * Don't compare blksize or blocks, 
					 * they are used differently by stat
					 * and bstat.
					 */
					if ( (t[i].bs_ino != sb.st_ino) ||
					     (t[i].bs_mode != sb.st_mode) ||
					     (t[i].bs_nlink != sb.st_nlink) ||
					     (t[i].bs_uid != sb.st_uid) ||
					     (t[i].bs_gid != sb.st_gid) ||
					     (t[i].bs_rdev != sb.st_rdev) ||
					     (t[i].bs_size != sb.st_size) ||
					     /* (t[i].bs_blksize != sb.st_blksize) || */
					     (t[i].bs_mtime.tv_sec != sb.st_mtim.tv_sec) ||
					     (t[i].bs_ctime.tv_sec != sb.st_ctim.tv_sec) ) {
						printf("\nstat/bstat missmatch\n");
						printbstat(&t[i]);
						printstat(&sb);
					}
				}
			}
		}
	}
	if (fsfd)
		close(fsfd);
	if (ret < 0 )
		perror("syssgi(SGI_FS_BULKSTAT)");
	if (verbose)
		printf("SGI_FS_BULKSTAT test: last=%lld nent=%d ret=%d count=%d\n", 
	               last, nent, ret, count);
	return 1;
}
