
/* unixperf - an x11perf-style Unix performance benchmark */

/* utils.c contains various utility routines used by unixperf. */

#include "unixperf.h"

#include <string.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <dirent.h>

void
MemError(char *file, unsigned int line)
{
   fprintf(stderr, "unixperf(%d): memory allocation failed in %s at line %d\n", getpid(), file, line);
   RemoveAnyTmpFiles();
   exit(1);
}

void
SysError(char *msg, char *file, unsigned int line)
{
   fprintf(stderr, "unixperf(%d): system call failed in %s at line %d\n", getpid(), file, line);
   perror(msg);
   RemoveAnyTmpFiles();
   exit(1);
}

void
StartStopwatch(void)
{
#ifdef SVR4
    gettimeofday(&StartTime);
#else
    struct timezone foo;
    gettimeofday(&StartTime, &foo);
#endif
}

double
StopStopwatch(void)
{
    struct timeval StopTime;
#ifdef SVR4
    gettimeofday(&StopTime);
#else
    struct timezone foo;
    gettimeofday(&StopTime, &foo);
#endif
    if (StopTime.tv_usec < StartTime.tv_usec) {
	StopTime.tv_usec += 1000000;
	StopTime.tv_sec -= 1;
    }
    return (double)(StopTime.tv_usec - StartTime.tv_usec) +
	(1000000.0 * (double)(StopTime.tv_sec - StartTime.tv_sec));
}

double
RoundTo3Digits(double d)
{
    /* It's kind of silly to print out things like ``193658.4/sec'' so just
       junk all but 3 most significant digits. */
    double exponent, sign;

    exponent = 1.0;
    /* the code below won't work if d should happen to be non-positive. */
    if (d < 0.0) {
	d = -d;
	sign = -1.0;
    } else
        sign = 1.0;
    if (d >= 1000.0) {
        do {
            exponent *= 10.0;
        } while (d/exponent >= 1000.0);
        d = (double)((int) (d/exponent + 0.5));
        d *= exponent;
    } else {
        if (d != 0.0) {
            while (d*exponent < 100.0) {
                exponent *= 10.0;
            }
        }
        d = (double)((int) (d*exponent + 0.5));
        d /= exponent;
    }
    return d * sign;
}

void
ReportTimes(double usecs, unsigned int n, char *str, Bool average)
{
   double msecsperobj, objspersec;
   char *fmt;

   if(usecs != 0.0) {
       msecsperobj = usecs / (1000.0 * (double)n);
       objspersec = (double) n * 1000000.0 / usecs;

       /* Round obj/sec to 3 significant digits.  Leave msec untouched, to
	  allow averaging results from several repetitions. */
       objspersec =  RoundTo3Digits(objspersec);
       if (average) {
	   if(objspersec < 10.0) {
	       fmt = "%7d trep @ %8.4f msec (%8.2f/sec): %s\n";
	   } else {
	       fmt = "%7d trep @ %8.4f msec (%8.1f/sec): %s\n";
	   }
	   printf(fmt, n, msecsperobj, objspersec, str);
       } else {
	   if(objspersec < 10.0) {
	       fmt = "%7d reps @ %8.4f msec (%8.2f/sec): %s\n";
	   } else {
	       fmt = "%7d reps @ %8.4f msec (%8.1f/sec): %s\n";
	   }
	   printf(fmt, n, msecsperobj, objspersec, str);
       }
   } else {
       printf("%6d %sreps @ 0.0 msec (unmeasurably fast): %s\n",
	 n, average ? "t" : "", str);
   }
}

void
RemoveAnyTmpFiles(void)
{
    static int      beenhere = 0;
    DIR            *dirp, *dir2p;
    struct dirent  *direntp, *dirent2p;
    struct stat     statinfo;
    char            filename[200];
    int             len;

    if (beenhere)
	return;
    beenhere = 1;
    rc = chdir(TmpDir);
    dirp = opendir(".");
    if (dirp == NULL)
	return;
    sprintf(filename, "unixperf-%d", getpid());
    len = strlen(filename);
    while ((direntp = readdir(dirp)) != NULL) {
	if (!strncmp(direntp->d_name, filename, len)) {
	    rc = stat(direntp->d_name, &statinfo);
	    if ((statinfo.st_mode & S_IFMT) == S_IFDIR) {
		rc = chdir(direntp->d_name);
		dir2p = opendir(".");
		if (dir2p != NULL) {
		    while ((dirent2p = readdir(dir2p)) != NULL) {
			unlink(dirent2p->d_name);
			rmdir(dirent2p->d_name);
		    }
		}
		rc = chdir(TmpDir);
		rc = rmdir(direntp->d_name);
	    } else {
		rc = unlink(direntp->d_name);
	    }
	}
    }
    closedir(dirp);
    rc = chdir("/");
}

void
MicroSecondPause(int usec)
{
  struct timeval timeout;

  timeout.tv_sec = usec / 1000000;
  timeout.tv_usec = (usec % 1000000);
  select(0, NULL, NULL, NULL, &timeout);
}
