#ident "$Revision: 1.6 $"

/*
 * EFS "ftw"
 */
#include "libefs.h"

void
efs_walk(EFS_MOUNT *mp, efs_ino_t inum, char *dirname, int flags,
	void (*dodent)(EFS_MOUNT *mp, struct efs_dent *, char *, int))
{
	EFS_DIR *dirp;
	struct efs_dent *dentp;
	char *cp, pname[MAXPATHLEN];
	struct efs_dinode *dip;

	if ((dirp = efs_opendiri(mp, inum)) == NULL) {
		fprintf(stderr, "efs_opendiri(inum=%d) failed\n", inum);
		return;
	}
		
	while ((dentp=efs_readdir(dirp)) != NULL) {
		if (dentp->d_namelen == 2 &&
				 strncmp(dentp->d_name, "..", 2) == 0)
			continue;
		(*dodent)(mp, dentp, dirname, flags);
	}

	if ((flags&DO_RECURSE) == 0) {
		efs_closedir(dirp);
		return;
	}

	strcpy(pname, dirname);
	strcat(pname, "/");
	cp = pname + strlen(pname);
	efs_rewinddir(dirp);
	while ((dentp=efs_readdir(dirp)) != NULL) {
		if (ISDOTORDOTDOT(dentp->d_name, dentp->d_namelen))
			continue;
		strncpy(cp, dentp->d_name, dentp->d_namelen);
		cp[dentp->d_namelen] = '\0';
		if ((dip = efs_figet(mp, EFS_GET_INUM(dentp))) == 0)
			return;
		if (S_ISDIR(dip->di_mode))
			efs_walk(mp, EFS_GET_INUM(dentp), pname, flags, dodent);
	}
	efs_closedir(dirp);
}

/*
 * given an efs_dent print out cg and bb info about the inode
 * and the (data) extents
 */
void
efs_ddent(EFS_MOUNT *mp, struct efs_dent *dentp, char *dirname, int flags)
{
        struct efs_dinode *dip;
	extent *ext;
	efs_ino_t inum = EFS_GET_INUM(dentp);

	dip = efs_figet(mp, inum);

	if (flags&DO_EXT && dip->di_numextents > 0) {
		if ((ext = efs_getextents(mp, dip, inum)) == 0)
			return;
	} else
		ext = 0;

	if (dentp->d_namelen == 1 && dentp->d_name[0] == '.')
		efs_prino(stdout, mp, inum, dip, ext, dirname, 0);
	else
		efs_prino(stdout, mp, inum, dip, ext, dentp->d_name,
				dentp->d_namelen);

	/* don't assume a sane inode */
	if (dip->di_numextents == 0 || ext == 0)
		return;

	if (ext)
		free(ext);
}

