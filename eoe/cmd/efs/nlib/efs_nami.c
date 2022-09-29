#ident "$Revision: 1.4 $"

/*
 * map pathname to i-number
 */
#include "libefs.h"

static efs_ino_t efs_namei0(struct efs_mount *mp, efs_ino_t dinum,
	char *filename);
static efs_ino_t searchdir(struct efs_mount *, efs_ino_t, char *, int len);

efs_ino_t
efs_namei(struct efs_mount *mp, char *filename)
{

	if (*filename == '/') {
		while (*filename++ == '/')
			;
		if (*filename == '\0')
			return efs_namei0(mp, 2, ".");
		else
			return efs_namei0(mp, 2, filename);
	} else
		return efs_namei0(mp, 2, filename);
}


static efs_ino_t
efs_namei0(struct efs_mount *mp, efs_ino_t dinum, char *filename)
{
	char *cp = strchr(filename, '/');
	int len;
	efs_ino_t inum;

	if (cp == NULL)
		len = strlen(filename);
	else
		len = cp - filename;
	/* XXX len == 0 */

	inum = searchdir(mp, dinum, filename, len);
	if (inum == 0)
		return 0;

	if (cp == NULL)
		return inum;

	while (*cp == '/')
		cp++;
	return efs_namei0(mp, inum, cp);
}

static efs_ino_t
searchdir(struct efs_mount *mp, efs_ino_t dinum, char *filename, int len)
{
	EFS_DIR *dirp;
	struct efs_dent *dentp;
	int inum = 0;
	
	dirp = efs_opendiri(mp, dinum);
	while ((dentp = efs_readdir(dirp)) != NULL)
		if (dentp->d_namelen == len
			&& strncmp(dentp->d_name, filename, len) == 0) {
			inum = EFS_GET_INUM(dentp);
			break;
		}
	efs_closedir(dirp);
	return inum ? inum : 0;
}
