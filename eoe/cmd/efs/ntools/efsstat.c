/*
 * Walks efs name space of file system and prints info about disk locations
 * of directory inode, directory blocks, file inodes and file blocks
 *
 * efsstat [options] [-e efsdev] [dirname|dirinum]
 *
 *	-f	- print superblock info
 *	-b	- print bitmap
 *	-i	- print inode info	
 *	-x	- print extent info per inode (implies -i)
 *	-s	- gather and print stats
 *
 *	-d	- print inode/extent info for directories only
 *	-m	- print/summarize info for files with multiple extents only
 */
static char ident[] = "@(#) efsstat.c $Revision: 1.7 $";

#include <sys/types.h>
#include <sys/stat.h>	/* S_ISDIR() */
#include <sys/param.h>	/* MAXPATHLEN */
#include <fcntl.h>
#include <string.h>
#include <malloc.h>

#include "libefs.h"

void args(int argc, char **argv);
void ddent(EFS_MOUNT *mp, struct efs_dent *dentp, char *dirname, int flags);
void gatheristats(struct efs_dinode *dip);
void gatherestats(EFS_MOUNT *mp, efs_ino_t inum, extent *ext, int numextents);
void prsum();

char *progname;
char *dirname;
efs_ino_t dirinum;
char *special;

int fflag;	/* specify any of these and the rest get turned off */
int bflag;
int iflag;
int xflag;
int sflag;
int rflag;

int dflag;


main(int argc, char **argv)
{
	struct efs_mount *mp;

	args(argc, argv);
	if ((mp = efs_mount(special, O_RDONLY)) == 0)
		exit(1);

	if (fflag)
		efs_prsuper(stdout, mp);
	if (bflag) {
		if (efs_bget(mp))
			exit(1);
		efs_prbit(stdout, mp);
	}
	/*
	 * call 'ddent()' for each entry in the hierarchy starting
	 * at 'dirname'.
	 */
	if (dirinum == 0)
		dirinum = efs_namei(mp, dirname);
	if (iflag || sflag)
		efs_walk(mp, 
			dirinum,
			dirname,
			rflag ? DO_RECURSE : 0,
			ddent);

	if (sflag)
		prsum();
}

void
args(int argc, char **argv)
{
        int errflag = 0;
        char c;
        extern char *optarg;
        extern int optind;


	progname = argv[0];

        while ((c = getopt(argc, argv, "e:fbixsrd")) != (char)-1)
                switch (c) {
		case 'h': goto usage;
                case 'f': fflag = 1; break;
                case 'b': bflag = 1; break;
                case 'i': iflag = 1; break;
                case 'x': xflag = 1; break;
		case 's': sflag = 1; break;
                case 'r': rflag = 1; break;
		case 'd': dflag = 1; break;
		case 'e': special = optarg; break;
                case '?': errflag++;
                }

        if (errflag || !special) {
usage:
printf("This prints location info about inodes and extents\n");
printf("usage: %s [-fbixsr] [-d] -e efsdev [dirname]\n",
	progname);
printf(" -h		this\n");
printf(" -f		super block info 	(default)\n");
printf(" -b		bitmap info 		(default)\n");
printf(" -i		inode info 		(default)\n");
printf(" -x		extent info 		(default && implies -i)\n");
printf(" -s		summary statistics	(default)\n");
printf(" -r 		recurse			(default)\n");
printf(" -d		directories only\n");
printf(" -e efsdev	block device or file of efs\n");
printf("			or set EFSDEV=efsdev in environment\n");
/*printf(" name 		relative to root of file system\n"); */
printf("			default is \".\"\n");
printf("use \"%s -e efsdev \" for cgshow\n", progname);
exit(1);
        }
	if (fflag == 0 && bflag == 0 && iflag == 0 &&
	    xflag == 0 && sflag == 0 && rflag == 0)
		fflag = bflag = iflag = xflag = sflag = rflag = 1;
	
	if (xflag)
		iflag = 1;

	if (argc > optind) {
		if (isdigit(*argv[optind])) {
			dirinum = atoi(argv[optind]);
			dirname = "<>";
		} else {
        		dirname = argv[optind++];
			dirinum = 0;
		}
		fflag = 0;
		bflag = 0;
		rflag = 0;
	} else {
		dirname = ".";
		dirinum = 2;
	}
}

/*
 * given an efs_dent print out cg and bb info about the inode
 * and the (data) extents
 */
void
ddent(EFS_MOUNT *mp, struct efs_dent *dentp, char *dirname, int flags)
{
        register struct efs_dinode *dip;
	extent *ext;
	efs_ino_t inum = EFS_GET_INUM(dentp);

	if ((dip = efs_figet(mp, inum)) == 0)
		return;
	if (dflag &&
	   (!S_ISDIR(dip->di_mode) || !ISDOT(dentp->d_name, dentp->d_namelen)))
		return;

	if (!ISDOT(dentp->d_name, dentp->d_namelen))
		gatheristats(dip);

	if (xflag && dip->di_numextents > 0) {
		if ((ext = efs_getextents(mp, dip, inum)) == 0)
			return;
	} else
		ext = 0;

	if (iflag)
		if (ISDOT(dentp->d_name, dentp->d_namelen))
			efs_prino(stdout, mp, inum, dip, ext, dirname, 0);
		else
			efs_prino(stdout, mp, inum, dip, ext, dentp->d_name,
				dentp->d_namelen);

	/* check both to survice a totally broken inode */
	if (dip->di_numextents == 0 || ext == 0)
		return;

	if (sflag && !ISDOT(dentp->d_name, dentp->d_namelen))
		gatherestats(mp, inum, ext, dip->di_numextents); 

	if (ext)
		free(ext);
}


/*
 * some buckets
 */
int summult;    /* # files with multiple extents */
int sumsingle;  /* # files with < 2 extents */

int sumperfect; /* # files with optimal extent usage */
int sumfragf;   /* # files with sub-optimal extent usage */
int sumfullext; /* # full-size extents */
int sumothext;  /* # all other extents */
int sumfarino;  /* # files whose inode is in different cylinder group */
int summanycg;  /* # files with extents in multiple cylinder groups */

int sum1k;      /* # files <= 1k */
int sum4k;      /*         <= 4k */
int sum16k;     /*         <= 16k */
int sumbig;     /*         over 16k */
int suminder;   /* # files using indirect extents */

void
gatherestats(EFS_MOUNT *mp, efs_ino_t inum, extent *ext, int numextents)
{
	int cgprev, ncgs, nshort, nfull, cg1;
	int dx;
	extent *ex;
	struct efs *fs = mp->m_fs;

        cgprev = EFS_BBTOCG(fs, ext->ex_bn);
        if (abs(EFS_ITOCG(fs, inum) - cgprev))		 	sumfarino++;

	ncgs = nshort = nfull = 0;
	for (dx=0, ex=ext; dx < numextents; dx++, ex++) {
                /* count any hop to a diff cg for adjacent extents */
                cg1 = EFS_BBTOCG(fs, ex->ex_bn);
                if (cg1 != cgprev) {
                        cgprev = cg1;
                        ++ncgs;
                }
                if (ex->ex_length < EFS_MAXEXTENTLEN) nshort++;
                if (ex->ex_length == EFS_MAXEXTENTLEN) nfull++;
	}
        sumfullext += nfull;
        sumothext += nshort;

        if (ncgs > 0)						summanycg++;

        /* gather info on files with multiple extents */
        if (numextents > 1)
                if (nfull >= numextents - 1)			sumperfect++;
                else						sumfragf++;
}

void
gatheristats(struct efs_dinode *dip)
{
        if (dip->di_size <= 102 ) 				sum1k++;
        else if (dip->di_size <= 4*1024) 			sum4k++;
        else if (dip->di_size <= 16*1024) 			sum16k++;
        else							sumbig++;

        if (dip->di_numextents > EFS_DIRECTEXTENTS)		suminder++;
        if (dip->di_numextents < 2)				sumsingle++;
        else							summult++;
}

void
prsum()
{

	printf("multf <2exf =<1kf   =<4  =<16  >16k indrf ");
	printf("optif fragf fulle othre finof nncgf");
	putchar('\n');
	printf("%5d %5d %5d %5d %5d %5d %5d ",
		summult,
		sumsingle,
		sum1k,
		sum4k,
		sum16k,
		sumbig,
		suminder);
	printf("%5d %5d %5d %5d %5d %5d",
		sumperfect,
		sumfragf,
		sumfullext,
		sumothext,
		sumfarino,
		summanycg);
	putchar('\n');
}
