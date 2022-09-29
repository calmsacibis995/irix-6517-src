
/*
 * indir file len [len1 len2 ...]
 *
 * Make the 0th indirect extent 'len' blocks, 1st 'len1', 2nd 'len2'...
 */
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#include "libefs.h"

#include <sys/fsctl.h>

#include "fsr.h"

char    *progname;

main(int argc, char **argv)
{
        struct stat sb;
	struct fscarg fa;
	struct efs_mount *mp;
	char *devname, *filename;
	char **lenv;
	int lenc, oldnie;
	extent *ix, newix[EFS_DIRECTEXTENTS], *oldix;
	int indirbb, newindirbb = 0;
	int len, freelen;
	efs_daddr_t newbn;

        progname = argv[0];
        if (argc < 3) {
                fprintf(stderr,"usage: %s file len [len1 ...]\n", progname);
                exit(1);
        }
        filename = argv[1];
	lenv = argv + 2;
	lenc = argc - 2;

	if (stat(filename, &sb) == -1) {
		perror("stat");
		exit(1);
	}

	efs_init(printf);

	devname = devnm(sb.st_dev);
        mp = efs_mount(devname, O_RDWR);

        if (fsc_init(printf, 1, 1))
		exit(1);

	fa.fa_dev = sb.st_dev;
	fa.fa_ino = sb.st_ino;

	if (fsc_ilock(&fa) == EBUSY) {
		printf("%s: ino=%d in use\n", progname, fa.fa_ino);
		exit(0);
	}
	getex(mp, &fa);

	/*
	 * foreach new indirect extent (each 'len' on the command line)
	 *	find 'len' free blocks
	 *	add this to the list of indirect extents
	 */
	oldix = fa.fa_ix;
	oldnie = fa.fa_nie;

	indirbb = BTOBB(fa.fa_ne * sizeof(extent));
	newindirbb = 0;

	if (lenc > EFS_DIRECTEXTENTS) {
		fprintf(stderr,"%s: lenc=%d > EFS_DIRECTEXTENTS\n",
			progname, lenc);
		exit(1);
	}
	fa.fa_nie = lenc;
	fa.fa_ix = newix;
	for (ix = newix; lenc--; ix++) {
		len = atoi(*lenv++);
		newbn = freefs(mp, 0, len, &freelen);
		if (newbn == -1) {
			fprintf(stderr,"%s: freefs(len=%d) failed\n",
				progname, len);
			exit(1);
		}
		if (freelen >= len) {
			if (fsc_balloc(fa.fa_dev, newbn, len) == -1) {
				fprintf(stderr,"%s:BALLOC(%d+%d) e=%d\n",
					progname, newbn, len, errno);
				exit(1);
			}
			ix->ex_bn = newbn;
			ix->ex_length = len;
			newindirbb += len;
		} else {
			fprintf(stderr,"%s: wanted=%d got=%d\n",
				progname, len, freelen);
			exit(1);
		}
	}

	/*
	 * proper number of indirect extents for the number of direct extents?
	 * (should probably check this before BALLOC'ing the world!...)
	 */
	if (newindirbb != indirbb) {
		fprintf(stderr,"%s: newindirbb=%d != indirbb=%d\n",
			progname, newindirbb, indirbb);
		exit(1);
	}

	/*
	 * ICOMMIT
	 */
	if (fsc_icommit(&fa) == -1) {
		fprintf(stderr,"%s: ICOMMIT failed\n", progname);
		exit(1);
	}
	/*
	 * free up old indir extents
	 */
	for (ix = oldix; oldnie--; ix++)
		if (fsc_bfree(fa.fa_dev, ix->ex_bn, ix->ex_length) == -1) {
			fprintf(stderr,"%s: BFREE(%d+%d) e=%d\n",
				progname, ix->ex_bn, ix->ex_length, errno);
			exit(1);
		}

	exit(0);
}
