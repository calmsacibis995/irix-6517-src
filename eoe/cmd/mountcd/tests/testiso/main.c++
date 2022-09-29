/*
 *	main.c++
 *
 *	Description:
 *		Main program for the testiso program
 *
 *	History:
 *      rogerc      04/11/91    Created
 */

#pragma linkage C
#include <mntent.h>
#pragma linkage

#include <stdlib.h>
#include <stdio.h>
#include <stream.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>
#include <bstring.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "DirEntry.h"
#include "PathTable.h"
#include "util.h"

#define min(a,b) ((a) < (b) ? (a) : (b))
#define MNTOPT_NOTRANSLATE "notranslate"

#define ISO_CTIME 10
#define ISO_MTIME 27
#define HSFS_CTIME 10
#define HSFS_MTIME 27

char *progname;
int verbose = 0;
int level = 0;

void print_spaces()
{
    for (int i = 0; i < level; i++) {
	cerr << ' ' << ' ';
    }
}

char *
findMountPoint(char *devscsi, int &notranslate)
{
    FILE *fp;
    fp = setmntent(MOUNTED, "r");
    if (!fp)
	return NULL;

    notranslate = 0;
    struct mntent *mntp;
    while ((mntp = getmntent(fp)) != NULL)
	if (strcmp(mntp->mnt_fsname, devscsi) == 0)
	    break;

    char *mp = NULL;
    if (mntp) {
	mp = strdup(mntp->mnt_dir);
	if (hasmntopt(mntp, MNTOPT_NOTRANSLATE))
	    notranslate++;
    }

    endmntent(fp);
    return mp;
}

void
compare(char *path, struct stat *f, DirEntry *de)
{
    if (verbose) {
	print_spaces();
	cerr << "     File: " << path << '\n';
    }

    unsigned long len = de->length();
    if (f->st_size != len) {
	print_spaces();
	cerr << path << " has wrong size\n";
	return;
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
	perror(path);
	return;
    }

    unsigned long loc = de->location();
    char buf[CDROM_BLKSIZE];

    Block blk;

    int nread;
    int toread;
    int fu = de->fileUnitSize();
    int gap = de->interleaveGap();
    int interleave = 0;
    if (fu && gap)
	interleave = 1;
    loc += de->extAttribLen();
    while (len && (nread = read(fd, buf,
				toread = (int)min(blockSize, len)))) {
	blk.read(loc++);
	if (bcmp((void *)blk, buf, nread) != 0) {
	    print_spaces();
	    cerr << path << ": Compare error\n";
	    break;
	}
	len -= nread;
	/*
	 * Skip interleave gaps
	 */
	if (interleave && --fu == 0) {
	    loc += gap;
	    fu = de->fileUnitSize();
	}
    }

    if (len) {
	print_spaces();
	cerr << path << ": Compare error\n";
    }

    close (fd);
}

inline int
fourdigitstoint(char dd[])
{
	int year;
	year = (dd[3] - '0') + 10 * (dd[2] - '0')
		    + 100 * (dd[1] - '0') + 1000 * (dd[0] - '0');
	return (year);
}

inline int
twodigitstoint(char dd[])
{
    return (dd[1] - '0') + ((dd[0] - '0') * 10);
}

void
check_time(char *dir, struct dirent *dp, struct stat *st, DirEntry *de)
{
    int xattrLen;
    time_t modifyTime;
    time_t createTime;
    if ((xattrLen = de->extAttribLen()) != 0) {
	Block xattr(de->location());
	if (de->getType() == ISO) {
	    char *ctimep = xattr + ISO_CTIME;
	    char *mtimep = xattr + ISO_MTIME;

	    struct tm tm;
	    tm.tm_sec = twodigitstoint(&ctimep[XTMSEC]);
	    tm.tm_min = twodigitstoint(&ctimep[XTMMIN]);
	    tm.tm_hour = twodigitstoint(&ctimep[XTMHOUR]);
	    tm.tm_mday = twodigitstoint(&ctimep[XTMDAY]);
	    tm.tm_mon = twodigitstoint(&ctimep[XTMMONTH]) - 1;
	    tm.tm_year = fourdigitstoint(&ctimep[XTMYEAR]) - 1900;
	    tm.tm_isdst = 0;

	    createTime = mktime(&tm);
	    createTime -= timezone; // mktime adds this in; we subtract it
	    createTime -= 15 * 60 * (signed char)ctimep[XTMOFF];

	    tm.tm_sec = twodigitstoint(&mtimep[XTMSEC]);
	    tm.tm_min = twodigitstoint(&mtimep[XTMMIN]);
	    tm.tm_hour = twodigitstoint(&mtimep[XTMHOUR]);
	    tm.tm_mday = twodigitstoint(&mtimep[XTMDAY]);
	    tm.tm_mon = twodigitstoint(&mtimep[XTMMONTH]) - 1;
	    tm.tm_year = fourdigitstoint(&mtimep[XTMYEAR]) - 1900;
	    tm.tm_isdst = 0;

	    modifyTime = mktime(&tm);
	    modifyTime -= timezone; // mktime adds this in; we subtract it
	    modifyTime -= 15 * 60 * (signed char)mtimep[XTMOFF];
	}
	else {
	    char *ctimep = xattr + HSFS_CTIME;
	    char *mtimep = xattr + HSFS_MTIME;

	    struct tm tm;
	    tm.tm_sec = twodigitstoint(&ctimep[XTMSEC]);
	    tm.tm_min = twodigitstoint(&ctimep[XTMMIN]);
	    tm.tm_hour = twodigitstoint(&ctimep[XTMHOUR]);
	    tm.tm_mday = twodigitstoint(&ctimep[XTMDAY]);
	    tm.tm_mon = twodigitstoint(&ctimep[XTMMONTH]) - 1;
	    tm.tm_year = fourdigitstoint(&ctimep[XTMYEAR]) - 1900;
	    tm.tm_isdst = 0;

	    createTime = mktime(&tm);
	    createTime -= timezone; // mktime adds this in; we subtract it

	    tm.tm_sec = twodigitstoint(&mtimep[XTMSEC]);
	    tm.tm_min = twodigitstoint(&mtimep[XTMMIN]);
	    tm.tm_hour = twodigitstoint(&mtimep[XTMHOUR]);
	    tm.tm_mday = twodigitstoint(&mtimep[XTMDAY]);
	    tm.tm_mon = twodigitstoint(&mtimep[XTMMONTH]) - 1;
	    tm.tm_year = fourdigitstoint(&mtimep[XTMYEAR]) - 1900;
	    tm.tm_isdst = 0;

	    modifyTime = mktime(&tm);
	    modifyTime -= timezone; // mktime adds this in; we subtract it
	}
    }
    else
	modifyTime = createTime = de->time();
    
    if (st->st_atime != modifyTime) {
	print_spaces();
	cerr << dir << "/" << dp->d_name << " has wrong access time\n";
	cerr << "\t" << ctime(&st->st_atime) <<
	    " should be " << ctime(&modifyTime) << "\n";
    }

    if (st->st_mtime != modifyTime) {
	print_spaces();
	cerr << dir << "/" << dp->d_name << " has wrong modification time\n";
	cerr << "\t" << ctime(&st->st_mtime) <<
	    " should be " << ctime(&modifyTime) << "\n";
    }

    if (st->st_ctime != createTime) {
	print_spaces();
	cerr << dir << "/" << dp->d_name << " has wrong change time\n";
	cerr << "\t" << ctime(&st->st_ctime) <<
	    " should be " << ctime(&createTime) << "\n";
    }
}

void
traverse(char *dir, int mntPntLen, PathTable &pt)
{
    /*
     * Don't do anything for "." and ".."
     */
    char *p = strrchr(dir, '/');
    if (p && (strcmp(p + 1, ".") == 0 || strcmp(p + 1, "..") == 0))
	return;

    if (verbose) {
	print_spaces();
	cerr << "Directory: " << dir << '\n';
    }
    
    DIR	*stream = opendir(dir);
    if (!stream) {
	print_spaces();
	cerr << "cannot open " << dir << "\n";
	return;
    }

    Directory *dirp;
    dirp = pt.Dir(dir + mntPntLen);
    if (dirp == NULL) {
	print_spaces();
	cerr << dir << " does not exist on disc\n";
	return;
    }

    struct dirent *dp;

    while ((dp = readdir(stream)) != NULL) {
	char filename[300];
	sprintf(filename, "%s/%s", dir, dp->d_name);

	struct stat f;
	if (stat(filename, &f) < 0) {
	    perror(filename);
	    continue;
	}

	DirEntry *de;
	de = dirp->DirEnt(dp->d_name);
	if (!de) {
	    print_spaces();
	    cerr << dir + mntPntLen << "/"
		<< dp->d_name << " does not exist on disc\n";
	    continue;
	}
	de->count()++;

	/*
	 * NFS doesn't use the "." and ".." entries in the pwd
	 * to report the time; it uses parent dir entries
	 */
	if (strcmp(dp->d_name, "..") != 0 &&
	    strcmp(dp->d_name, ".") != 0)
	    check_time(dir + mntPntLen, dp, &f, de);

	if (!(f.st_mode & S_IFDIR) ^ !(de->flags() & FFLAG_DIRECTORY)) {
	    print_spaces();
	    cerr << dp->d_name << " inconsistent flags\n";
	    continue;
	}

	if (f.st_mode & S_IFDIR) {
	    level++;
	    traverse(filename, mntPntLen, pt);
	    level--;
	} else {
	    compare(filename, &f, de);
	}
    }
    closedir(stream);
}

main(int argc, char *argv[])
{
    progname = argv[0];
    char *device;

    if (argc == 3 && strcmp(argv[1], "-v") == 0) {
	verbose++;
	device = argv[2];
    } else if (argc == 2) {
	device = argv[1];
    } else {
	cerr << "Usage: " << progname << " [ -v ] <devscsi>" << '\n';
	return (1);
    }

    int notranslate;
    char *mntpnt = findMountPoint(device, notranslate);
    if (mntpnt == NULL) {
	cerr << progname << ": " << device << " not mounted\n";
	return (1);
    }
    PathTable pt(device, notranslate);

    if (verbose) {
	pt.dump();
    }

    traverse(mntpnt, strlen(mntpnt), pt);
    return pt.check();
}
