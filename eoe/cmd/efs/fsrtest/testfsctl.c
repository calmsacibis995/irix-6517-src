
/*
 * testfsctl file-with-many-extents-which-may-be-butchered
 * 
 * generate fsctl/EFS corner cases
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <values.h>

#include "libefs.h"

#include <sys/fsctl.h>

#include "fsr.h"

char    	*progname;

verifyexverify(struct efs_mount *mp, struct fscarg *fap);
randombetween(long lo, long hi);

verifybadindirbn(struct fscarg *fap);
verifybadbn(struct efs *fs, struct fscarg *fap);
verifybogusoffset(struct fscarg *fap);
verifybogusne(struct fscarg *fap);
verifybogussize(struct fscarg *fap);
verifybadindir1(struct fscarg *fap);
randombetween(long lo, long hi);

char *devname, *filename;

#define	MAXOFFSET	0x0ffffff

main(int argc, char **argv)
{
	struct stat sb;
	struct fscarg fa;
	struct efs_mount *mp;

	progname = argv[0];
	if (argc != 2) {
		fprintf(stderr, "usage: %s file\n", progname);
		exit(1);
	}
	filename = argv[1];

	srandom(time(0));

	if (stat(filename, &sb) == -1) {
		perror("stat(dir)");
		exit(1);
	}
	efs_init(printf);

	devname = devnm(sb.st_dev);

	mp = efs_mount(devname, O_RDWR);

	if (fsc_init(printf, 0, 0)) /* don't want dbg checks */
		exit(1);

	/* 
	 * check out ICOMMIT failure modes:
	 * 	cook up some bad extents, bad indirect extents
	 */
	if (stat(filename, &sb) == -1) {
		perror("stat");
		exit(1);
	}
	fa.fa_dev = sb.st_dev;
	fa.fa_ino = sb.st_ino;
	if (fsc_ilock(&fa) == -1) {
		if (errno == EBUSY)
			printf("%s: ino=%d busy\n", progname, sb.st_ino);
		exit(1);
	}
	verifyexverify(mp, &fa);

	if (fsc_iunlock(&fa) == -1)
		perror("IUNLOCK");


	/*
	 * XXX check out ILOCK failure modes
	 * 	try simultaneous opens in an MP machine...
	 */
}

/*
 * tests each 'exverify()' code path -- pretty much independently
 */
verifyexverify(struct efs_mount *mp, struct fscarg *fap)
{
	/*
	 * direct extents
	 */

	verifybogusne(fap);	/* bogus number of extents */

	/*
	 * get some extents to butcher!
	 */
	getex(mp, fap);

	/*
	 * extents covering unallocated blocks
	 */


	/*
	 * bogus offset
	 * pick an ex_offset at random and set it to a random value
	 */
	verifybogusoffset(fap);

	/* 
	 * bogus #blocks for file size: remove a block
	 */
	verifybogussize(fap);
			

	/* 
	 * indirect extents
	 */
	verifybadindir1(fap);

	verifybadindirbn(fap);

	verifybadbn(mp->m_fs, fap);	/* bad data block numbers */
}

verifybadindirbn(struct fscarg *fap)
{
	extent *ex;
	int t0;
	/* pass in bogus bn+len's for indirect extents */

	/*
	 * pass in bogus #blocks of indirect extents for #direct extents
	 */
	for ( ex=fap->fa_ix, t0=fap->fa_nie; t0--; ex++ )
		if ( t0 = fsc_tstfree(fap->fa_dev, ex->ex_bn + ex->ex_length)){
			if ( t0 == -1 ) {
				iprintf(0,0,
				"%s:test9:fsc_tstfree(0x%x %d+%d) e=%d\n",
					progname, fap->fa_dev, ex->ex_bn,
					ex->ex_length, errno);
				exit(1);
			}
			ex->ex_length++;
			iprintf(1,0,
				"%s: test9: bad #indirbbs (+1)\n", progname);
			if ( fsc_icommit(fap) == 0 ) {
				iprintf(0,0,
					"%s:test9:bad #indirbbs\n",progname);
				exit(1);
			}
			ex->ex_length--;
			break;
		}

	/*
	 * pass in bogus #blocks of indirect extents for #direct extents
	 */
	for ( ex=fap->fa_ix, t0=fap->fa_nie; t0--; ex++ )
		if ( ex->ex_length > 1 ) {
			ex->ex_length--;
			iprintf(1,0,
				"%s: test10: bad #indirbbs (-1)\n", progname);
			if ( fsc_icommit(fap) == 0 ) {
				iprintf(0,0,
					"%s:test10:bad #indirbbs\n",progname);
				exit(1);
			}
			ex->ex_length++;
			break;
		}
			
}


verifybadbn(struct efs *fs, struct fscarg *fap)
{
	extent ext;
	int numi0 = (fs->fs_ncg + 2) * 3;
	int cgno, i0;
	efs_daddr_t *badbn = (efs_daddr_t*)malloc(numi0 * sizeof(efs_daddr_t));

	iprintf(1,0,
	    "%s: test1: trying to ICOMMIT bad data block numbers,(ncg=%d)\n",
	    progname, fs->fs_ncg);
	badbn[0] = -1;
	badbn[1] = 2;
	badbn[2] = fs->fs_firstcg/2;
	i0 = 3;

	ext.ex_offset = 0;
	ext.ex_length = 1;
	ext.ex_magic = 0;

	/* first, middle, and last block of i-list in each cg */
	for ( cgno=0; cgno < fs->fs_ncg; cgno++ ) {
		badbn[i0++] = EFS_CGIMIN(fs, cgno);
		badbn[i0++] = (EFS_CGIMIN(fs, cgno) +
				EFS_CGIMIN(fs,cgno) + fs->fs_cgisize)/2;
		badbn[i0++] = EFS_CGIMIN(fs, cgno) + fs->fs_cgisize - 1;
	}

	/* beyond */
	badbn[i0++] = EFS_CGIMIN(fs, fs->fs_ncg);
	badbn[i0++] = EFS_CGIMIN(fs, fs->fs_bmblock);
	badbn[i0] = fs->fs_size - 1;

	fap->fa_ne = 1;
	fap->fa_nie = 0;
	fap->fa_ex = &ext;
	for ( i0 = 0; i0 < numi0; i0++ ) {
		ext.ex_bn = badbn[i0];
		iprintf(2,0, "%d ", ext.ex_bn);
		if ( fsc_icommit(fap) == 0 && errno != EINVAL ) {
			iprintf(0,0,
				"%s: test1: fsctl failed to detect bn=%d\n",
				progname, ext.ex_bn);
			exit(1);
		}
	}
	iprintf(2,0, "\n");
}

verifybogusoffset(struct fscarg *fap)
{
	int t0, t1;
	extent *x0;

	t0 = randombetween(1, fap->fa_ne-1);
	x0 = fap->fa_ex + t0;
	t1 = x0->ex_offset;
	x0->ex_offset = randombetween(0, MAXOFFSET);
	iprintf(1,0, "%s: test4: bad offset %d (should be %d)\n",
		progname, x0->ex_offset, t1);
	if ( fsc_icommit(fap) == 0 && errno != EINVAL ) {
		iprintf(0,0, "%s: test4: bad ex_offset %d (should be %d)\n",
			progname, x0->ex_offset, t1);
		exit(1);
	}
	x0->ex_offset = t1;
}

verifybogusne(struct fscarg *fap)
{
	int t0;
	
	/* 
	 * bogus number of extents
	 *
	 * numextents == 0
	 */
	iprintf(1,0,"%s: test2: ne=%d\n", progname, 0);
	t0 = fap->fa_ne;
	fap->fa_ne = 0;
	if ( fsc_icommit(fap) == 0 && errno != EINVAL ) {
		iprintf(0,0,"%s: fsctl failed to detect numextents=0\n",
			progname);
		exit(1);
	}
	fap->fa_ne = t0;

	/* 
	 * bogus number of extents
	 *
	 * numextents == EFS_MAXEXTENTS + 1
	 */
	t0 = fap->fa_ne;
	fap->fa_ne = EFS_MAXEXTENTS+1;
	iprintf(1,0,
		"%s: test3: ne=%d (EFS_MAXEXTENTS+1)\n", progname, fap->fa_ne);
	if ( fsc_icommit(fap) == 0 && errno != EINVAL ) {
		iprintf(0,0,
			"%s: test3: fsctl failed to detect numextents=%d\n",
			progname, fap->fa_ne);
		exit(1);
	}
	fap->fa_ne = t0;
}

verifybogussize(struct fscarg *fap)
{
	int t0;
	extent *ex;

	for ( ex=fap->fa_ex, t0=fap->fa_ne; t0--; ex++ )
		if ( t0 = fsc_tstfree(fap->fa_dev, ex->ex_bn + ex->ex_length)) {
			if ( t0 == -1 ) {
				iprintf(0,0,
				"%s:test5:fsc_tstfree(0x%x %d+%d) e=%d\n",
					progname, fap->fa_dev, ex->ex_bn,
					ex->ex_length, errno);
				exit(1);
			}
			ex->ex_length++;
			iprintf(1,0, "%s: test5: bad #blocks (+1)\n", progname);
			if ( fsc_icommit(fap) == 0 && errno != EINVAL ) {
				iprintf(0,0, "%s: bad #blocks\n", progname);
				exit(1);
			}
			ex->ex_length--;
			break;
		}

	/* 
	 * bogus #blocks for file size: remove a block
	 */
	for ( ex=fap->fa_ex, t0=fap->fa_ne; t0--; ex++ )
		if ( ex->ex_length > 1 ) {
			ex->ex_length--;
			iprintf(1,0, "%s: test6: bad #blocks (-1)\n", progname);
			if ( fsc_icommit(fap) == 0 && errno != EINVAL ) {
				iprintf(0,0,
					"%s:test6:bad #blocks\n", progname);
				exit(1);
			}
			ex->ex_length++;
			break;
		}
}

verifybadindir1(struct fscarg *fap)
{
	int t0;
	/* 
	 * pass in EFS_DIRECTEXTENTS++ with no indirect extents
	 */
	t0 = fap->fa_nie;
	fap->fa_nie = 0;
	iprintf(1,0, "%s: test7: ne=%d with no indirect extents\n",
		progname, fap->fa_ne);
	if ( fsc_icommit(fap) == 0 && errno != EINVAL ) {
		iprintf(0,0, "%s: test7: no indir extents\n", progname);
		exit(1);
	}
	fap->fa_nie = t0;

	/* 
	 * pass in EFS_DIRECTEXTENTS + 1 indirect extents
	 */
	iprintf(1,0, "%s: test8: ne=%d with indirect extents ex_offset=%d\n",
		progname, fap->fa_ne, EFS_DIRECTEXTENTS + 1);
	t0 = fap->fa_nie;
	fap->fa_nie = EFS_DIRECTEXTENTS+1;
	if ( fsc_icommit(fap) == 0 && errno != EINVAL ) {
		iprintf(0,0, "%s: test8: count EFS_DIR+1 indir extents\n",
			progname);
		exit(1);
	}
	fap->fa_nie = t0;
}

randombetween(long lo, long hi)
{
	return (float) (hi - lo) * random() / MAXINT + lo;
}
