
/* unixperf - an x11perf-style Unix performance benchmark */

/* test_fileio.c measures filesystem IO performance. */

#include "unixperf.h"
#include "test_fileio.h"
#include "rgbdata.h"

#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <ndbm.h>
#include <sys/stat.h>
#include <dirent.h>

static char tmpdir[1024];
static char filename[200];
static char digits[] = "0123456789";
static char longdir1[100];
static char longdir2[100];

Bool
InitSetupDir(Version version)
{
    sprintf(tmpdir, "%s/unixperf-%d", TmpDir, getpid());
    sprintf(filename, "%s/XX", tmpdir);
    ptr = strchr(filename, 'X');
    rc = mkdir(tmpdir, 0755);
    SYSERROR(rc, "mkdir");
    rc = chdir(tmpdir);
    SYSERROR(rc, "chdir");
    return TRUE;
}

Bool
InitSetupDirLong(Version version)
{
    sprintf(tmpdir, "%s/unixperf-%d", TmpDir, getpid());
    rc = mkdir(tmpdir, 0755);
    SYSERROR(rc, "mkdir");

    sprintf(longdir1, "%s/foo", tmpdir);
    rc = mkdir(longdir1, 0755);
    SYSERROR(rc, "mkdir");

    sprintf(longdir2, "%s/bar", longdir1);
    rc = mkdir(longdir2, 0755);
    SYSERROR(rc, "mkdir");

    rc = chdir(tmpdir);
    SYSERROR(rc, "chdir");
    return TRUE;
}

Bool
InitSetupFiles(Version version)
{
    int i, j;

    sprintf(tmpdir, "%s/unixperf-%d", TmpDir, getpid());
    sprintf(filename, "%s/XX", tmpdir);
    ptr = strchr(filename, 'X');
    rc = mkdir(tmpdir, 0755);
    SYSERROR(rc, "mkdir");
    rc = chdir(tmpdir);
    SYSERROR(rc, "chdir");
   for(i=0;i<10;i++) {
       for(j=0;j<10;j++) {
           ptr[0] = digits[i];
           ptr[1] = digits[j];
           rc = open(filename, O_RDWR|O_CREAT, 0644);
           SYSERROR(rc, "open");
           rc = close(rc);
           SYSERROR(rc, "close");
       }
   }
   return TRUE;
}

unsigned int
DoCreateAndUnlink(void)
{
   int i, j;

   for(i=0;i<10;i++) {
       for(j=0;j<10;j++) {
           ptr[0] = digits[i];
           ptr[1] = digits[j];
           rc = open(filename, O_RDWR|O_CREAT, 0644);
           debugSYSERROR(rc, "open");
           rc = close(rc);
           debugSYSERROR(rc, "close");
       }
   }
   for(i=0;i<10;i++) {
       for(j=0;j<10;j++) {
           ptr[0] = digits[i];
           ptr[1] = digits[j];
           rc = unlink(filename);
           debugSYSERROR(rc, "unlink");
       }
   }
   return 100;
}

unsigned int
DoMkdirAndRmdir(void)
{
    int i, j;

    for(i=0;i<10;i++) {
	for(j=0;j<10;j++) {
	    ptr[0] = digits[i];
	    ptr[1] = digits[j];
	    rc = mkdir(filename, 0755);
	    debugSYSERROR(rc, "mkdir");
        }
    }
    for(i=0;i<10;i++) {
	for(j=0;j<10;j++) {
	    ptr[0] = digits[i];
	    ptr[1] = digits[j];
	    rc = rmdir(filename);
	    debugSYSERROR(rc, "mkdir");
        }
    }
    return 100;
}

unsigned int
DoOpenAndClose(void)
{
   int i, j;

   for(i=0;i<10;i++) {
       for(j=0;j<10;j++) {
           ptr[0] = digits[i];
           ptr[1] = digits[j];
           rc = open(filename, O_RDWR, 0644);
           debugSYSERROR(rc, "open");
           rc = close(rc);
           debugSYSERROR(rc, "close");
       }
   }
   return 100;
}

unsigned int
DoChmod(void)
{
   int i, j;
   char name[] = "XX";

   for(i=0;i<10;i++) {
       for(j=0;j<10;j++) {
           name[0] = digits[i];
           name[1] = digits[j];
           chmod(name, 0444); /* disable user write permission */
       }
   }
   for(i=0;i<10;i++) {
       for(j=0;j<10;j++) {
           name[0] = digits[i];
           name[1] = digits[j];
           chmod(name, 0644); /* enable user write permission */
       }
   }
   return 200;
}

unsigned int
DoStatInDir(void)
{
   DIR *dirp;
   struct dirent *direntp;
   struct stat statinfo;
#ifdef DEBUG
   int count = 0;
#endif
   
   dirp = opendir(".");
   while ((direntp = readdir(dirp)) != NULL) {
       rc = stat(direntp->d_name, &statinfo);
       debugSYSERROR(rc, "stat");
#ifdef DEBUG
       count++;
#endif
   }
#ifdef DEBUG
   assert(count == 100);
#endif
   closedir(dirp);
   return 1;
}

unsigned int
DoStat(void)
{
   DIR *dirp;
   struct stat statinfo;
   int i;
   
   for(i=500;i>0;i--) {
       rc = stat(".", &statinfo);
       debugSYSERROR(rc, "stat");
   }
   return 500;
}

unsigned int
DoStatLong(void)
{
   DIR *dirp;
   struct stat statinfo;
   int i;
   
   for(i=500;i>0;i--) {
       rc = stat(longdir2, &statinfo);
       debugSYSERROR(rc, "stat");
   }
   return 500;
}

void
CleanupSetupDir(void)
{
    rc = chdir("/");
    SYSERROR(rc, "chdir");
    rc = rmdir(tmpdir);
    SYSERROR(rc, "rmdir");
}

void
CleanupSetupDirLong(void)
{
    rc = chdir("/");
    SYSERROR(rc, "chdir");
    rc = rmdir(longdir2);
    SYSERROR(rc, "rmdir");
    rc = rmdir(longdir1);
    SYSERROR(rc, "rmdir");
    rc = rmdir(tmpdir);
    SYSERROR(rc, "rmdir");
}

void
CleanupSetupFiles(void)
{
   int i, j;

   for(i=0;i<10;i++) {
       for(j=0;j<10;j++) {
           ptr[0] = digits[i];
           ptr[1] = digits[j];
           rc = unlink(filename);
           SYSERROR(rc, "unlink");
       }
   }
    rc = chdir("/");
    SYSERROR(rc, "chdir");
    rc = rmdir(tmpdir);
    SYSERROR(rc, "rmdir");
}

Bool
InitWriteLog(Version version)
{
    sprintf(filename, "%s/unixperf-%d-log", TmpDir, getpid());
    fd = open(filename, O_WRONLY|O_APPEND|O_CREAT, 0644);
    SYSERROR(fd, "open");
    return TRUE;
}

unsigned int
DoWriteLog(void)
{
    static char msg[] = "This is the 80-column log file message to be written out as a performance test.\n";
    int i;

    for(i=50;i>0;i--) {
        rc = write(fd, msg, sizeof(msg));
        debugSYSERROR(rc, "write");
        rc = write(fd, msg, sizeof(msg));
        debugSYSERROR(rc, "write");
        rc = write(fd, msg, sizeof(msg));
        debugSYSERROR(rc, "write");
        rc = write(fd, msg, sizeof(msg));
        debugSYSERROR(rc, "write");
    }
    return 200;
}

void
CleanupWriteLogPass(void)
{
    rc = ftruncate(fd, 0);
    SYSERROR(rc, "ftruncate");
}

void
CleanupWriteLog(void)
{
    rc = close(fd);
    SYSERROR(rc, "close");
    rc = unlink(filename);
    SYSERROR(rc, "unlink");
}

DBM *dbm;
int numDbmEntries;

Bool
InitDbmLookup(Version version)
{
    char filename[100];
    datum key, content;
    int triple[3], i;

    sprintf(filename, "%s/unixperf-%d-dbm", TmpDir, getpid());
    dbm = dbm_open(filename, O_WRONLY|O_CREAT, 0644);
    if(dbm == NULL) DOSYSERROR("dbm_open");
    for(i=0; rgbData[i].color != NULL; i++) {
	key.dptr = rgbData[i].color;
	key.dsize = strlen(rgbData[i].color);
	triple[0] = rgbData[i].red;
	triple[1] = rgbData[i].green;
	triple[2] = rgbData[i].blue;
	content.dptr = (char*) triple;
	content.dsize = sizeof(triple);
	rc = dbm_store(dbm, key, content, DBM_REPLACE);
	if(rc < 0) DOSYSERROR("dbm_store");
    }
    numDbmEntries = i;
    dbm_close(dbm); /* close to eliminate chance of in-memory caching */
    dbm = dbm_open(filename, O_RDONLY, 0);
    if(dbm == NULL) DOSYSERROR("dbm_open");
    return TRUE;
}

unsigned int
DoDbmLookup(void)
{
    datum key, content;
    int i;

    for(i=0; i<numDbmEntries; i++) {
	key.dptr = rgbData[i].color;
	key.dsize = strlen(rgbData[i].color);
	content = dbm_fetch(dbm, key);
	if(content.dsize == 0) DOSYSERROR("dbm_fetch");
    }
    return numDbmEntries;
}

void
CleanupDbmLookup(void)
{
    char filename[100];

    dbm_close(dbm);
    sprintf(filename, "%s/unixperf-%d-dbm.dir", TmpDir, getpid());
    rc = unlink(filename);
    SYSERROR(rc, "unlink");
    sprintf(filename, "%s/unixperf-%d-dbm.pag", TmpDir, getpid());
    rc = unlink(filename);
    SYSERROR(rc, "unlink");
}

