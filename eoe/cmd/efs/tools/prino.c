/*
 * Dump an inode out.  This is not meant as a complete diagnostic tool, just
 * something handy to look at inodes.
 *
 * Written by: Kipp Hickman
 *
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/efs/tools/RCS/prino.c,v $
 * $Revision: 1.4 $
 * $Date: 1997/06/03 20:58:11 $
 */

/*	unlike dino, this program wants a filename, so it only works
	on mounted filesystems, which is what I was ususally using
	dino for anyway.  This saves doing an ls -i on the file to
	get its inum, and remembering which drive it is on.

	By default, this one checks for contiguous blocks, not
	'aligned' blocks, which are pretty much meaningless on
	SCSI drives.
*/

#include "efs.h"
#include <sys/stat.h>
#include <sys/dir.h>
#include <strings.h>

char *progname;
int fs_fd;
struct efs *fs;
union {
	struct	efs fs;
	char	data[BBTOB(BTOBB(sizeof(struct efs)))];
} sblock;

int aligned_extents = 0, contig_extents = 0;
int docontig = 1;

main(argc, argv)
int argc;
char *argv[];
{
	efs_ino_t inum;
	register struct efs_dinode *di;
	register extent *ex, *exb = NULL;
	register int i;
	efs_daddr_t nextbn = 0;
	int contigext = -1, ext_len;
	struct stat sbuf;
	char *dname;

	progname = argv[0];
	if(argc == 3 && *argv[1] == '-' && argv[1][1] == 'a') {
		docontig = 0;
		argc--, argv++;
	}

	if (argc != 2) {
		fprintf(stderr, "usage: %s [-a] filename\n", progname);
		exit(-1);
	}

	if(stat(argv[1], &sbuf) == 0)
		inum = sbuf.st_ino;
	else {
		fprintf(stderr, "%s can't stat ", progname);
		perror(argv[1]);
		exit(1);
	}

	opendisk(sbuf.st_dev);

	if ((inum < EFS_ROOTINO) ||
	    (inum > fs->fs_ipcg * fs->fs_ncg)) {
		fprintf(stderr, "%s: inode number %d is not valid\n",
		    progname, inum);
		exit(-1);
	}

	di = efs_iget(inum);
	printf("inode %d at disk block %d offset %d\n",
	    inum, EFS_ITOBB(fs, inum),
	    EFS_ITOO(fs, inum) * sizeof(struct efs_dinode));
	printf("mode=%07o nlink=%d uid=%d gid=%d size=%d\n",
	    di->di_mode, di->di_nlink, di->di_uid,
	    di->di_gid, di->di_size);
	printf("atime=%s", ctime(&di->di_atime));
	printf("mtime=%s", ctime(&di->di_mtime));
	printf("ctime=%s", ctime(&di->di_ctime));
	printf("gen=%d numextents=%d vers=%d dev=%x\n",
	    di->di_gen, di->di_numextents, di->di_version,
	    di->di_u.di_dev);
	if (di->di_numextents > EFS_DIRECTEXTENTS)
		dumpindir(di);
	else if (di->di_numextents) {
		printf("Extent Magic #   Disk Block   Length   Offset    Cg\n");
		ex = &di->di_u.di_extents[0];
		for (i = 0; i < di->di_numextents; i++, ex++) {
			if(ex->ex_bn == nextbn) {
				contig_extents++;
				nextbn += ex->ex_length;
				if(docontig) {
					if(!exb) {
						exb = ex - 1;
						contigext = i-1;
						ext_len = exb->ex_length;
					}
					ext_len += ex->ex_length;
					continue;
				}
			}
			nextbn = ex->ex_bn + ex->ex_length;
			if(i == 0)
				continue;	/* catch next time or at end of loop */
			if(exb) {
				prextent(contigext, exb, ext_len);
				ext_len = 0;
				exb = NULL;
			}
			else /* print the previous one, since this one might be the
				start of a contiguous chunk */
				prextent(i-1, ex-1, 0);
		}
		/* print the last line, which may be a contigous block, or a
			single extent */
		if(exb)
			prextent(contigext, exb, ext_len);
		else /* print the previous one, since this one might be the
			start of a contiguous chunk */
			prextent(i-1, ex-1, 0);
	}
	if(docontig)
		printf("Total of %d contigous extents out of %d\n",
		    contig_extents, di->di_numextents);
	else
		printf("Total of %5.2f aligned extents (%d out of %d), %d contigous extents\n",
		    (float) aligned_extents / (float) di->di_numextents,
		    aligned_extents, di->di_numextents, contig_extents);
}

/*
 * Print out indirect extent information
 */
dumpindir(di)
register struct efs_dinode *di;
{
	register extent *ex, *exb = NULL;
	register int i, j, k;
	char buf[BBSIZE];
	int left;
	char header = 0;
	long bn;
	efs_daddr_t nextbn = 0;
	int contigext = -1, ext_len, base_ext;

	left = di->di_numextents;
	for (i = 0; i < di->di_u.di_extents[0].ex_offset; i++) {
		printf("Indirect extent #%d: bn=%d len=%d\n",
		    i, di->di_u.di_extents[i].ex_bn,
		    di->di_u.di_extents[i].ex_length);
	}
	for (i = 0; i < di->di_u.di_extents[0].ex_offset && left; i++) {
		bn = di->di_u.di_extents[i].ex_bn;
		for (j = 0; j < di->di_u.di_extents[i].ex_length && left; j++, bn++) {
			lseek(fs_fd, BBTOB(bn), 0);
			if (read(fs_fd, buf, BBSIZE) != BBSIZE)
				error();
			ex = (extent *) buf;
			exb = NULL;
			nextbn = 0;
			base_ext = (j * BBSIZE / sizeof(extent));
			for (k = 0; k < BBSIZE / sizeof(extent) && left--; k++, ex++) {
				if (!header) {
					printf("Extent Magic #   Disk Block   Length   Offset    Cg\n");
					header = 1;
				}
				if(ex->ex_bn == nextbn) {
					contig_extents++;
					nextbn += ex->ex_length;
					if(docontig) {
						if(!exb) {
							exb = ex - 1;
							contigext = base_ext + k - 1;
							ext_len = exb->ex_length;
						}
						ext_len += ex->ex_length;
						continue;
					}
				}
				nextbn = ex->ex_bn + ex->ex_length;
				if(ex == (extent *)buf)
					continue;	/* catch next time or at end of loop */
				if(exb) {
					prextent(contigext, exb, ext_len);
					ext_len = 0;
					exb = NULL;
				}
				else	/* print the previous one, since this one might be the
					start of a contiguous chunk */
					prextent(base_ext + k-1, ex-1, 0);
			}
			if(exb) {	/* don't do across indirect extents, for now */
				prextent(contigext, exb, ext_len);
				ext_len = 0;
				exb = NULL;
			}
			else { /* print the previous one, since this one might be the
						start of a contiguous chunk */
				prextent(base_ext + k-1, ex-1, 0);
			}
		}
	}
}

error()
{
	int old_errno;

	old_errno = errno;
	fprintf(stderr, "%s: i/o error\n", progname);
	errno = old_errno;
	perror(progname);
	exit(-1);
}


prextent(extno, ex, contiglen)
register struct extent *ex;
int extno, contiglen;
{
	printf("%6d  %6d %12d   %6d   %6d  %4d\n",
	    extno, ex->ex_magic, ex->ex_bn,
	    contiglen,
	    ex->ex_offset,
	    EFS_BBTOCG(fs, ex->ex_bn));
}

char *
finddev(char *dname, dev_t dnum, unsigned type)
{
	DIR *dirp;
	struct direct *dp;
	struct stat sb;
	char path[MAXPATHLEN], *devnam;

	dirp = opendir((const char *)dname);
	if (dirp == NULL)
		return NULL;
	strcpy(path, dname);
	strcat(path, "/");
	devnam = path + strlen(path);

	while((dp = readdir(dirp)) != NULL) {
		strcpy(devnam, dp->d_name);
		if(stat(path, &sb) == 0 && sb.st_rdev == dnum &&
		    (sb.st_mode & S_IFMT) ==  type) {
			devnam = strdup(path);
			if(!devnam)
				fprintf(stderr, "couldn't allocate space for pathname %s\n", path);
			closedir(dirp);
			return devnam;
		}
	}
	closedir(dirp);
	return NULL;
}

/*	find the disk drive partition and open it.  Assumes the device
	will be found in /dev/dsk, since we are dealing with mounted
	filesystems
*/
opendisk(devnum)
{
	char *dname = finddev("/dev/dsk", devnum, S_IFBLK);

	if(dname == NULL) {
		fprintf(stderr, "%s: can't find device for filesystem\n", progname);
		exit(-1);
	}
	if ((fs_fd = open(dname, O_RDONLY)) < 0) {
		fprintf(stderr, "%s: can't open %s\n", progname, dname);
		exit(-1);
	}
	fs = &sblock.fs;
	if (efs_mount()) {
		fprintf(stderr, "%s: %s is not an extent filesystem\n",
		    progname, dname);
		exit(-1);
	}
}
