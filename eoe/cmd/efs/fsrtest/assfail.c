
/*
 * This caused an interesting enough assert to fail to keep around.
 * 
 * This will fail if fsctl (incorrectly) sets ii_indirbytes to actual
 * bytes instead of blocks needed expressed in units of bytes.  The failure
 * occurs when efs recycles an inode fsctl just wrote.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <values.h>

#include "nefs.h"

#include <sys/fsctl.h>

#include "fsr.h"

extern int errno;
char	*progname;

pwrite(char *filename, int addbb);
preorg(char *filename);

main(int argc, char **argv)
{
	char *filename = argv[1];

	progname = argv[0];
	nefs_init(printf);

	preorg(filename);
	pwrite(filename, 1);
}

preorg(char *filename)
{
	struct fscarg fa;
	struct stat sb;
	efs_daddr_t newbn;
	int nfree;
	struct efs_dinode *di;
	struct efs_mount *mp;

	if (fsc_init(printf, 0, 0))
		exit(1);

	if (stat(filename, &sb) == -1) {
		perror("stat");
		exit(1);
	}
	fa.fa_dev = sb.st_dev;
	fa.fa_ino = sb.st_ino;

	mp = nefs_mount(devnm(sb.st_dev), O_RDWR);

        /* lock the inode */
        if (fsc_ilock(&fa) == -1) {
        	if ( errno == EBUSY ) {
                	printf("preorg ino=%d in use...\n", fa.fa_ino);
                	exit(1);
        	}
        	if ( errno == EINVAL ) {
                	printf("preorg ino=%d invalid...\n", fa.fa_ino);
                	exit(1);
        	}
	}
	printf("%s:preorg ILOCK(%s) completed\n", progname, filename);

        /* get file info straight out of the file system device */
        di = nefs_iget(mp, fa.fa_ino);
        if (!S_ISREG(di->di_mode) || di->di_numextents < 1) {
		printf("%s:preorg not regfile or no extents\n", progname);
		exit(1);
	}
	getex(mp, &fa);

	/* move the 1st block of the file elsewhere */
	newbn = freefs(mp, 0, 1, &nfree);
	if ( nfree < 1 ) {
		printf("%s:preorg no blocks free!\n", progname);
		exit(1);
	}
	movex(mp, &fa, 0, newbn, 1);

	if (fsc_iunlock(&fa))
		exit(1);
}

pwrite(char *filename, int addbb)
{
	int fd, t0, nblocks = addbb;
	struct stat statbuf;

	fd = open(filename, O_WRONLY);
	if (fd == -1) {
		perror("open(O_WRONLY)");
		exit(1);
	}
	printf("%s:pwrite open(%s) completed\n", progname, filename);

        if (fstat(fd, &statbuf) == -1) {
                perror("stat");
                exit(1);
        }
        nblocks = BTOBB(statbuf.st_size) + nblocks;
        for (t0 = BTOBB(statbuf.st_size); t0 < nblocks; t0++) {
                char buf[32];
                lseek(fd, BBTOB(t0), SEEK_SET);
                sprintf(buf, "%15d %15d\n", t0, statbuf.st_ino);
                if ( write(fd, buf, 32) != 32 ) {
                        if ( errno != ENOSPC )
                                perror("write");
                        exit(1);
                }
        }
	printf("%s:pwrite exiting\n", progname);
        exit(0);
}
