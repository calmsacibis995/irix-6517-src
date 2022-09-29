/*
 *  Direntry.c++
 *
 *  Description:
 *	Implementation of DirEntry class
 *
 *  History:
 *      rogerc      04/11/91    Created
 */

#include <bstring.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include <stream.h>
#include "DirEntry.h"
#include "util.h"

DirEntry::DirEntry(RAWDIRENT *raw, int notranslate)
{
    xattrLen = raw->de_xattr_len;
    len = CHARSTOLONG(raw->de_len);
    fu = raw->de_fu;
    gap = raw->de_gap;
    loc = CHARSTOLONG(raw->de_loc);
    int stlen;
    char *str;
    str = to_unixname(raw->de_name, raw->de_namelen, notranslate);
    stlen = strlen(str);
    fflags = raw->de_flags;
    filename = new char[stlen + 1];
    strncpy(filename, str, stlen);
    filename[stlen] = '\0';

    struct tm tm;
    tm.tm_sec = raw->de_date[TMSEC];
    tm.tm_min = raw->de_date[TMMIN];
    tm.tm_hour = raw->de_date[TMHOUR];
    tm.tm_mday = raw->de_date[TMDAY];
    tm.tm_mon = raw->de_date[TMMONTH] - 1;
    tm.tm_year = raw->de_date[TMYEAR];
#if 0
    tm.tm_isdst = _daylight;
#else
    tm.tm_isdst = 0;
#endif

    recTime = mktime(&tm);
    recTime -= timezone; // mktime adds this in; we subtract it
    recTime -= 15 * 60 * (signed char)raw->de_date[TMOFF];

    type = ISO;

    refCount = 0;
}

DirEntry::DirEntry(HSFS_RAWDIRENT *raw, int notranslate)
{
    xattrLen = raw->de_xattr_len;
    len = CHARSTOLONG(raw->de_len);
    fu = raw->de_fu;
    gap = raw->de_gap;
    loc = CHARSTOLONG(raw->de_loc);
    int stlen;
    char *str;
    str = to_unixname(raw->de_name, raw->de_namelen, notranslate);
    stlen = strlen(str);
    fflags = raw->de_flags;
    filename = new char[stlen + 1];
    strncpy(filename, str, stlen);
    filename[stlen] = '\0';

    struct tm tm;
    tm.tm_sec = raw->de_date[TMSEC];
    tm.tm_min = raw->de_date[TMMIN];
    tm.tm_hour = raw->de_date[TMHOUR];
    tm.tm_mday = raw->de_date[TMDAY];
    tm.tm_mon = raw->de_date[TMMONTH] - 1;
    tm.tm_year = raw->de_date[TMYEAR];
#if 0
    tm.tm_isdst = _daylight;
#else
    tm.tm_isdst = 0;
#endif

    recTime = mktime(&tm);
    time_t clock = recTime;
    struct tm *atime = localtime(&clock);
    recTime -= timezone; // mktime adds this in; we subtract it

    type = HSFS;
    refCount = 0;
}

DirEntry::~DirEntry()
{
    delete filename;
}

char *
DirEntry::name()
{
    return filename;
}

void
DirEntry::dump()
{
    printf("\tName: %s\tloc: %d\tlen: %d\tflags: %x\n",
	   filename, loc, len, fflags);
}

int
DirEntry::check()
{
    /*
     * Associated files should NOT have been available.
     */
    if (fflags & FFLAG_ASSOC) {
	if (refCount != 0)
	    cerr << progname << ": associated file " << filename
		<< " reference count of " << refCount << "\n ";
    }
    else if (refCount != 1) {
	cerr << progname << ": " << filename << " reference count of " <<
	    refCount << "\n";
	return 1;
    }
    return 0;
}
