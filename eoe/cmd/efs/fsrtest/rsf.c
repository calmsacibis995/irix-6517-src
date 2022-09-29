
/*
 * rsf elen file
 *
 * "disorganize" 'file' into extents of 'elen' blocks
 */
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <values.h>             /* MAXINT */
#include <sys/param.h>          /* MIN() */
#include <sys/immu.h>
#include <sys/stat.h>
#include <stdlib.h>		/* getenv() */
#include <malloc.h>
#include <assert.h>

#include "libefs.h"

#include <sys/fsctl.h>

#include "fsr.h"

char    *progname;

int vflag;
int dflag;

off_t rsf(struct efs_mount *mp, struct fscarg *fap, off_t startoff, int elen);

main(int argc, char **argv)
{
	char *devname, *filename;
	int elen, offset, nblocks;
	struct stat statbuf;
	struct fscarg fa;
	struct efs_mount *mp;
	int c;
	extern int optind;

	progname = argv[0];
	while ((c = getopt(argc, argv, "dv")) != -1 )
		switch (c) {
		case 'v':
			vflag = 1;
			break;
		case 'd':
			dflag = 1;
			break;
		default:
usage:
			fprintf(stderr,"usage: %s [-vd] elen file\n", progname);
			exit(1);
		}
	if (optind < argc - 2)
		goto usage;

	elen = atoi(argv[optind++]);
	if (elen > EFS_MAXEXTENTLEN) {
		fprintf(stderr, "%s: %d > %d (EFS_MAXEXTENTLEN)\n",
			progname, elen, EFS_MAXEXTENTLEN);
		exit(1);
	}
	filename = argv[optind];

	if (dflag) {
		printf("stat(%s)...\n", filename);
		mallopt(M_DEBUG, 1);
	}
	if (stat(filename, &statbuf) == -1) {
		perror("stat");
		exit(1);
	}
	efs_init(printf);

	devname = devnm(statbuf.st_dev);
	fa.fa_ino = statbuf.st_ino;
	nblocks = BTOBB(statbuf.st_size);

        mp = efs_mount(devname, O_RDWR);

        if (fsc_init(printf, dflag, vflag))
		exit(1);

	fa.fa_dev = mp->m_dev;
	if (fsc_ilock(&fa) == -1) {
		printf("%s: ILOCK failed e=%d\n", progname, errno);
		exit(0);
	}

	for (offset=0; offset < nblocks;) {
		iprintf(1,0,"%s: offset=%d (nblocks=%d)\n",
			progname, offset, nblocks);
		if ((offset = rsf(mp, &fa, offset, elen)) == -1) {
			iprintf(0,0,"%s: rsf(offset=%d) failed e=%d\n",
				progname, offset, errno);
			exit(1);
		}
		if (fsc_iunlock(&fa) == -1) {
			iprintf(0,0, "%s: IUNLOCK e=%d\n", progname, errno);
			exit(1);
		}
		sleep(2);
		if (fsc_ilock(&fa) == -1) {
			iprintf(0,0, "%s: ILOCK e=%d\n", progname, errno);
			exit(1);
		}
	}
	exit(0);
}



off_t
rsf(struct efs_mount *mp, struct fscarg *fap, off_t startoff, int elen)
{
	off_t offset;
	efs_daddr_t newbn;
	int fileblocks, nfree, wantfree, vecne, v0;
	extent *ex, *vecex;
	/*
	 * foreach (new) extent
	 *	look for elen+1 free space
	 *	movex current offset to freebn+1
	 */
	getex(mp, fap);
	if (vflag) {
		printf("rsf() startoff=%d elen=%d\n", startoff, elen);
		fprex(stdout, fap->fa_ne, fap->fa_ex, fap->fa_nie, fap->fa_ix);
	}

	fileblocks = getnblocks(fap);
	offset = startoff;
	while (offset < fileblocks) {
		wantfree = MIN((mp->m_fs->fs_cgfsize - mp->m_fs->fs_cgisize)/8,
				(fileblocks - offset) / elen * (elen+1));
		if ((newbn = freefs(mp, EFS_BBTOCG(mp->m_fs, fap->fa_ex->ex_bn),
				wantfree, &nfree)) == -1)
			return -1;
		nfree = MIN((mp->m_fs->fs_cgfsize - mp->m_fs->fs_cgisize)/8,
				nfree);
		if (nfree < elen+1) {
			fprintf(stderr, "%s: no free %d\n", progname, elen+1);
			exit(1);
		}
		vecne = MIN(nfree / (elen + 1), (fileblocks - offset)/elen);
		if ((vecex = (extent*)malloc(vecne * sizeof(extent))) == NULL){
			fprintf(stderr, "%s: malloc(vecne=%d) failed\n",
				progname, vecne);
			return -1;
		}
		for (ex = vecex, v0 = vecne; v0--; ex++) {
			ex->ex_bn = newbn;
			ex->ex_length = elen;
			newbn += elen + 1;
		}
		iprintf(2,0,"%s:rsf() vecne=%d offset=%d\n", progname,
			vecne, offset);
		if (vecmovex(mp, fap, vecne, vecex, offset, vecne * elen) ==-1){
			free(vecex);
			return -1;
		}
		free(vecex);
		offset += vecne * elen;
	}
	return fileblocks;
}
