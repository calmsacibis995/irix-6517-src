#ident "$Revision: 1.10 $"

/*
 * print efs stuff in a parsable and compact format:
 *
 *	x <x-num> cgno blkno t <more-stuff>
 *
 *	i		inode info, x-num is inumber
 *	e		extent info, x-num is extent number
 *	x		indir extent info
 *	f		free "extent"
 */
#include "libefs.h"

char *fmtmode(ushort mode);
char *fmttime(time_t *time);

/*
 * Print inode and extent info.
 */
void
efs_prino(FILE *fp, struct efs_mount *mp, efs_ino_t inum,
		struct efs_dinode *dip, extent *ext, char *name, int nl)
{
        extent *ex;
        int dx;
	efs_daddr_t lastbn = 0;
	
       	fprintf(fp, "i%7d %4d %7d %c ",
               	inum,
               	EFS_ITOCG(mp->m_fs, inum),
               	EFS_ITOBB(mp->m_fs, inum),
               	filetype(dip->di_mode));
	if (name)
		if (nl == 0)
			fprintf(fp, "[%s] ", name);
		else
			fprintf(fp, "[%.*s] ", nl, name);
	fprintf(fp, "%d\n", dip->di_nlink);

        if (dip->di_numextents == 0)
                return;

	if (ext == NULL)
		return;

	if (dip->di_numextents > EFS_DIRECTEXTENTS) {
		int niext = dip->di_u.di_extents[0].ex_offset;
		ex = &dip->di_u.di_extents[0];
		for (dx=0; dx < niext; dx++, ex++)
			fprintf(fp, "x%7d %4d %7d %4d %7d\n",
				dx,
				EFS_BBTOCG(mp->m_fs, ex->ex_bn),
				ex->ex_bn,
				ex->ex_length,
				ex->ex_offset);
	}
	for (dx=0, ex=ext; dx < dip->di_numextents; dx++, ex++) {
		int cg = EFS_BBTOCG(mp->m_fs, ex->ex_bn);
		fprintf(fp, "e%7d %4d %7d %4d %7d ",
			dx,
			cg,
			ex->ex_bn,
			ex->ex_length,
			ex->ex_offset);
		if (ex->ex_bn == EFS_CGIMIN(mp->m_fs,cg))
			fputc('[', fp);
		else if (ex->ex_bn + ex->ex_length == EFS_CGIMIN(mp->m_fs,cg+1))
			fputc(']',fp);
		else if (ex->ex_bn == lastbn)
			fputc('*',fp);
		fputc('\n', fp);
		lastbn = ex->ex_bn + ex->ex_length;
	}
}

void
efs_prdinode(FILE *fp, struct efs_dinode *di)
{
	int ne;
	extent *ex;

	fprintf(fp,"%s nlink=%d uid=%d gid=%d",
		fmtmode(di->di_mode), di->di_nlink, di->di_uid, di->di_gid);
	fprintf(fp,"\t\t%04x %04x %04x %04x\n",
		di->di_mode, di->di_nlink, di->di_uid, di->di_gid);
	
	fprintf(fp,"size=%d\t\t\t\t%08x\n", di->di_size, di->di_size);
	fprintf(fp,"atime %s\t\t%08x\n",fmttime((time_t *)&di->di_atime),
		di->di_atime);
	fprintf(fp,"mtime %s\t\t%08x\n",fmttime((time_t *)&di->di_mtime),
		di->di_mtime);
	fprintf(fp,"ctime %s\t\t%08x\n",fmttime((time_t *)&di->di_ctime),
		di->di_ctime);

	fprintf(fp,"gen=%d nex=%d vers=%d",
		di->di_gen, di->di_numextents, di->di_version);
	fprintf(fp,"\t\t%08x %04x %04x\n",
		di->di_gen, di->di_numextents, di->di_version);

	ex = di->di_u.di_extents;
	for (ne = 0; ne < EFS_DIRECTEXTENTS; ne++, ex++)
		if (ex->ex_magic != 0 || ex->ex_bn != 0 ||
		    ex->ex_length != 0 || ex->ex_offset != 0)
			fprintf(fp,"%2d: magic=0x%x bn=%d len=%d off=%d\n",
				ne, ex->ex_magic, ex->ex_bn,
				ex->ex_length, ex->ex_offset);
}

char *
fmttime(time_t *time)
{
	static char buf[25];

	ascftime(buf, " %a %h %d %H:%M:%S %Y", localtime(time));

	return buf;
}


char *
fmtmode(ushort mode)
{
	static char modestr[10];

#define	MODESTR "----------"
	strcpy(modestr, MODESTR);

	switch (mode & S_IFMT) {
	case S_IFDIR:	modestr[0] = 'd';	break;
	case S_IFCHR:	modestr[0] = 'c';	break;
	case S_IFBLK:	modestr[0] = 'b';	break;
	case S_IFREG:	modestr[0] = '-';	break;
	case S_IFIFO:	modestr[0] = 'f';	break;
	case S_IFLNK:	modestr[0] = 'l';	break;
	case S_IFSOCK:	modestr[0] = 's';	break;
	}

	if (mode & S_IRUSR)
		modestr[1] = 'r';
	if (mode & S_IWUSR)
		modestr[2] = 'w';
	if (mode & S_ISUID)
		modestr[3] = 's';
	else if (mode & S_IXUSR)
		modestr[3] = 'x';

	if (mode & S_IRGRP)
		modestr[4] = 'r';
	if (mode & S_IWGRP)
		modestr[5] = 'w';
	if (mode & S_ISGID)
		modestr[6] = 's';
	else if (mode & S_IXGRP)
		modestr[5] = 'x';

	if (mode & S_IROTH)
		modestr[7] = 'r';
	if (mode & S_IWOTH)
		modestr[8] = 'w';
	if (mode & S_ISVTX)
		modestr[9] = 't';
	else if (mode & S_IXOTH)
		modestr[9] = 'x';

	return modestr;
}


void
efs_prsuper(FILE *fp, struct efs_mount *mp)
{
	struct efs *fs = mp->m_fs;

        fprintf(fp, "size     = %d [blocks]\n", fs->fs_size);
        fprintf(fp, "firstcg  = %d [blocks]\n", fs->fs_firstcg);
        fprintf(fp, "cgfsize  = %d [blocks]\n", fs->fs_cgfsize);
        fprintf(fp, "cgisize  = %d [blocks]\n", fs->fs_cgisize);
        fprintf(fp, "ncg      = %d\n", fs->fs_ncg);
        fprintf(fp, "bmsize   = %d [bytes]\n", fs->fs_bmsize);
        fprintf(fp, "tfree    = %d [blocks]\n", fs->fs_tfree);
        fprintf(fp, "tinode   = %d [inodes]\n", fs->fs_tinode);
        fputc('\n', fp);
}


void
efs_prbit(FILE *fp, struct efs_mount *mp)
{
        int start, end, len;
        int save, cg, count = 0;
	struct efs *fs = mp->m_fs;

        for ( cg=0; cg < fs->fs_ncg; cg++ ) {
                start = fs->fs_firstcg + cg * fs->fs_cgfsize + fs->fs_cgisize;
                end = start + fs->fs_cgfsize - fs->fs_cgisize;
                while ( start < end ) {
                        save = start;
                        if ( btst(mp->m_bitmap, start ) ) {
                                len = 0;
                                while ( start < end && btst(mp->m_bitmap, start) ) {
                                        start++;
                                        len++;
                                }
                                fprintf(fp, "f%7d %4d %7d %4db\n",
                                        count++,
                                        EFS_BBTOCG(fs, save),
                                        save,
                                        len);
                        } else {
                                while ( start < end && !btst(mp->m_bitmap, start) )
                                        start++;
                        }
                }
        }
}
