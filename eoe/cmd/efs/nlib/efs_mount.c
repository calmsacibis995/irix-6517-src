#ident "$Revision: 1.9 $"

/*
 * init libefs.a, mount EFS file systems
 */
#include "libefs.h"

int (*efs_errpr)(const char*, ...) = _efsprintf;

void
efs_init(int (*func)(const char *, ...))
{
	efs_errpr = func;
}

int
_efsprintf(const char *fmt, ...)
{
	va_list ap;
	int i;

	va_start(ap, fmt);
	i = vfprintf(stderr, fmt, ap);
	va_end(ap);
	return i;
}

struct efs_mount *
efs_mount(char *devname, int rw)
{
	struct efs_mount *mp;
	struct stat sb;

	if (devname == NULL)
		return 0;

 	if ((mp = (struct efs_mount*)calloc(1,sizeof(struct efs_mount)))==NULL){
		efs_errpr("malloc failed mounting %s\n", devname);
		return 0;
	}
	
	if ((mp->m_devname = strdup(devname)) == NULL ) {
		efs_errpr("malloc failed mounting %s\n", devname);
		return 0;
	}

	if ((mp->m_fs = (struct efs*)malloc(BBSIZE)) == NULL) {
		efs_errpr("malloc failed mounting %s\n", devname);
		return 0;
	}

	if ((mp->m_fd = open(devname, rw)) == -1) {
		efs_errpr("open(%s) failed: %s\n", devname, strerror(errno));
		return 0;
	}
	
	if (fstat(mp->m_fd, &sb) == -1) {
		efs_errpr("fstat(%s) failed: %s\n", devname, strerror(errno));
		return 0;
	}

	if (efs_readb(mp->m_fd, (char *)mp->m_fs, BTOBB(EFS_SUPERBOFF), 1)!=1) {
		efs_errpr("efs_readb(%s) of superblock failed: %s\n", devname,
			strerror(errno));
		return 0;
	}
	if (!IS_EFS_MAGIC(mp->m_fs->fs_magic)) {
		efs_errpr("bad superblock magic number in %s\n", devname);
		return 0;
	}
	EFS_SETUP_SUPERB(mp->m_fs);

	mp->m_dev   = sb.st_rdev;
	mp->m_ilist = NULL;

	mp->active = 0;
	strcpy(mp->tofile, "");

	return mp;
}

void
efs_umount(struct efs_mount *mp)
{
	close(mp->m_fd);
	free(mp->m_fs);
	free(mp->m_devname);
	if (mp->m_ilist != NULL) free(mp->m_ilist);
	if (mp->m_bitmap)
		free(mp->m_bitmap);
	free(mp);
}

long
efs_checksum(struct efs *fs)
{
        ushort *sp;
        long checksum;

        checksum = 0;
        sp = (ushort *)fs;
        while (sp < (ushort *)&fs->fs_checksum) {
                checksum ^= *sp++;
                checksum = (checksum << 1) | (checksum < 0);
        }
	return checksum;
}
