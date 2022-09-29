#ident "$Revision: 1.4 $"

/*
 * more handy EFS init and helper routines
 */
#include "libefs.h"

static char *devnmr(char *devdir, dev_t dev);

char *
devnm(dev_t dev)
{
	char *cp;

	cp = devnmr("/dev/rdsk/", dev);
	if (cp != NULL)
		return cp;

	cp = devnmr("/dev/", dev);
	if (cp != NULL)
		return cp;
	
	cp = devnmr("/dev/rdsk/xlv/", dev);
	if (cp != NULL)
		return cp;

	efs_errpr("could not find device file for dev number 0x%x\n", dev);

	return NULL;
}

/* 'r' stands for raw (chr) */
static char *
devnmr(char *devdir, dev_t dev)
{
	char *cp;
	static char devname[MAXPATHLEN+1];
	DIR *dirp;
	struct dirent *dp;
	struct stat statbuf;

	strcpy(devname, devdir);
	cp = devname + strlen(devdir);
	dirp = opendir(devname);
	while ((dp = readdir(dirp)) != NULL) {
		strcpy(cp, dp->d_name);
		if (stat(devname, &statbuf) < 0)
			continue;
		if (S_ISCHR(statbuf.st_mode) && statbuf.st_rdev == dev) {
			closedir(dirp);
			return devname;
		}
	}
	closedir(dirp);
	return NULL;
}

char
filetype(ushort mode)
{
        switch ( mode&S_IFMT ) {
        case S_IFDIR:   return 'd';
        case S_IFCHR:   return 'c';
        case S_IFBLK:   return 'b';
        case S_IFREG:   return 'r';
        case S_IFIFO:   return 'p';
        case S_IFLNK:   return 'l';
        case S_IFSOCK:  return 's';
        default:        return '?';
        }
}
