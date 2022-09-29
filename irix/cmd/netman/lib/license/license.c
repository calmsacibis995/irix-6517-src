/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	license.c
 *
 *	$Revision: 1.5 $
 *
 *      Programs linked with this module must also be linked
 *      with libnetls.a and libnck.a.
 *
 *      Compiling this file requires the "-dollar" flag and the flag
 *      -I/usr/include/idl/c to the C compiler.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#include <sys/types.h>
#include <netinet/in.h>

#undef LICENSE
#if defined(LICENSE) && !defined(sun)

#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <nbase.h>
#include <uuid.h>
#include <libnetls.h>
#include "heap.h"
#include "exception.h"
#include "license.h"

/*
 * Error messages from license routines
 */
static const char badInitMessage[] =
	"Error initializing the network license system";
static const char noLicenseMessage[] =
	"Could not find a valid NetVisualyzer license";
static const char licenseWarningMessage[] =
	"The license for NetVisualyzer expires";
static const char licenseLimitMessage[] =
	"NetVisualyzer license limit reached";
static const char logOpenMessage[] =
	"Could not open license log file";
static const char logChmodMessage[] =
	"Could not change permissions of license log file";
static const char logLockMessage[] =
	"Could not lock license log file";
static const char logMagicMessage[] =
	"License log file has bad magic number";
static const char logReadMessage[] =
	"Could not read license log file";
static const char logWriteMessage[] =
	"Could not write license log file";
static const char logTruncMessage[] =
	"Could not truncate license log file";
static const char logUnlockMessage[] =
	"Could not unlock license log file";
static const char logRemoveMessage[] =
	"Could not find entry in license log file";

/*
 * Constants for NetLS
 */
static nls_vnd_id_t vendorID = "546fb4684914.02.c0.1a.3d.52.00.00.00";
static const long vendorKey = 27406;
static const long productID = 6;
static char version[] = "2.0";
static const long reqLicense = 1;
static const nls_time_t checkPeriod = 30;

/*
 * NetLS transaction ID
 */
static nls_lic_trans_id_t transID;

/*
 * Annotations used to modify license
 */
static const char *annotStrings[] = { "datastations=", 0 };
#define annotDataStation 0

/*
 * A license log file is used to keep track of license use by
 * multiple processes.
 */
static const char logFile[] = "/tmp/.nvlicense";
static const unsigned int logFileMagic = 0x6e657476;
static const unsigned int logFileBypass = 0x73676921;
struct ActiveList {
	struct in_addr addr;
	pid_t pid;
};

/*
 * maxDataStations is retrieved from the license annotation
 */
static unsigned int maxDataStations = 0;

/*
 * Issue a warning if the license will expire within warnOfExpiration days
 */
static const unsigned int warnOfExpiration = 30;

/*
 * Flag for unlicensed tools
 */
static unsigned int unlicensed = 1;


/*
 * Release the license back to NetLS
 */
static void
releaseLicense(void)
{
    long status;
    netls_release_license(transID, &status);
}

/*
 * Get a license from NetLS
 */
unsigned int
getLicense(char *prog, char **msg)
{
    long status;
    nls_job_id_t jobID;
    nls_lic_annot_t annot;
    nls_time_t expDate;
    long type;
    char *s;
    unsigned int i;
    unsigned int remaining;

    /* This is a licensed tool */
    unlicensed = 0;

    /* Initialize NetLS */
    netls_create_job_id(&jobID);
    netls_init(vendorID, vendorKey, &jobID, &status);
    if (status != status_$ok) {
	*msg = badInitMessage;
	return 0;
    }

    /* Get a license, for now this should return a nodelocked license */
    type = NETLS_ANY;
    if (netls_request_license(&jobID, &type, productID, version,
			      strlen(version), reqLicense, checkPeriod,
			      &transID, &expDate, annot, &status) == 0) {
	/* Check for bypass */
	unsigned int magic;
	int fd = open(logFile, O_RDWR | O_SYNC);
	if (fd >= 0) {
	    if (read(fd, &magic, sizeof magic) == sizeof magic
		&& magic == logFileBypass
	    	&& read(fd, &magic, sizeof magic) == sizeof magic
		&& magic > time(0)
		&& magic - time(0) < 24 * 60 * 60) {
		unlicensed = 1;
		*msg = 0;
		return 1;
	    }
	    close(fd);
	}
	*msg = noLicenseMessage;
	return 0;
    }

    /* Releae the license upon exit */
    atexit(releaseLicense);

    /* Parse the annotation string */
    for (s = strtok(annot, " \t\n"); s != 0; s = strtok(0, " \t\n")) {
	for (i = 0; annotStrings[i] != 0; i++) {
	    unsigned int len = strlen(annotStrings[i]);
	    if (strncasecmp(s, annotStrings[i], len) == 0) {
		switch (i) {
		    case annotDataStation:
			/* Set maximum number of DataStations */
			maxDataStations = atoi(s + len);
			break;
		}
	    }
	}
    }

    /* Warn if expiration date is approaching */
    remaining = (expDate - time(0)) / (60 * 60 * 24);
    if (remaining > warnOfExpiration)
	*msg = 0;
    else {
	*msg = tnew(char, strlen(licenseWarningMessage) + 32);
	i = sprintf(*msg, licenseWarningMessage, remaining);

	switch (remaining) {
	    case 0:
		sprintf(*msg, "%s today", licenseWarningMessage);
		break;
	    case 1:
		sprintf(*msg, "%s tomorrow", licenseWarningMessage);
		break;
	    default:
		sprintf(*msg, "%s in %d days", licenseWarningMessage,
						remaining);
		break;
	}
    }

    return 1;
}

/*
 * Read the license log
 */
static unsigned int
getLicenseLog(int *filedes, struct ActiveList **activeList,
	      unsigned int *n, unsigned int *u)
{
    struct flock lock;
    struct ActiveList *al;
    int fd, len, size;
    unsigned int a[3], i;

    /* Open (or possibly create) the log file */
    for ( ; ; ) {
	fd = open(logFile, O_RDWR | O_SYNC);
	if (fd >= 0)
	    break;

	/* Open failed */
	if (errno != ENOENT) {
	    exc_raise(errno, logOpenMessage);
	    return 0;
	}

	/* File does not exist, try to create */
	fd = open(logFile, O_RDWR | O_SYNC | O_CREAT | O_EXCL, 0666);
	if (fd >= 0) {
	    if (fchmod(fd, 0666) != 0) {
		exc_raise(errno, logChmodMessage);
		return 0;
	    }
	    break;
	}

	/*
	 * Create failed.  If file now exists, someone just created it.
	 * Go back and try to open it again.
	 */
	if (errno != EEXIST) {
	    exc_raise(errno, logOpenMessage);
	    return 0;
	}
    }
    *filedes = fd;

    /* Lock the file, sleeping until accomplished */
    lock.l_type = F_WRLCK;
    lock.l_whence = 0;
    lock.l_start = 0;
    lock.l_len = 0;
    if (fcntl(fd, F_SETLKW, &lock) < 0) {
	int err = errno;
	close(fd);
	exc_raise(err, logLockMessage);
	return 0;
    }

    /* Read magic number, number of entries and unique entries */
    size = 3 * sizeof(unsigned int);
    if (read(fd, a, size) < size) {
	/* New file */
	*n = 0;
	*u = 0;
	*activeList = new(struct ActiveList);
	return 1;
    }

    /* Verify magic number */
    if (a[0] != logFileMagic) {
        /* Unlock the file */
        int err = errno;
        lock.l_type = F_UNLCK;
        lock.l_whence = 0;
        lock.l_start = 0;
        lock.l_len = 0;
        (void) fcntl(fd, F_SETLK, &lock);
        exc_raise(err, logMagicMessage);
        return 0;
    }

    *n = a[1];
    *u = a[2];

    /* Allocate structure leaving room to add on end */
    *activeList = al = vnew((int) *n + 1, struct ActiveList);

    /* Read into structure */
    size = *n * sizeof(struct ActiveList);
    len = read(fd, *activeList, size);
    if (len < 0) {
        /* Unlock the file */
        int err = errno;
        lock.l_type = F_UNLCK;
        lock.l_whence = 0;
        lock.l_start = 0;
        lock.l_len = 0;
        (void) fcntl(fd, F_SETLK, &lock);
        exc_raise(err, logReadMessage);
        return 0;
    }

    /* If the size isn't really *n, set *n appropriately */
    if (len != size)
	*n = len / sizeof(struct ActiveList);

    /* Verify entries in log */
    for (i = 0; i < *n; ) {
	if (kill(al[i].pid, 0) == 0 || errno == EPERM)
	    i++;
	else {
	    /* Entry is not valid, remove from list */
            unsigned int j, k;
            for (k = 1, j = i + 1; j < *n; j++) {
                if (al[j].addr.s_addr == al[i].addr.s_addr)
                    k = 0;
                al[j - 1] = al[j];
            }
            (*n)--;
            *u -= k;
	}
    }

    return 1;
}

/*
 * Write the license log
 */
static unsigned int
setLicenseLog(int fd, struct ActiveList *activeList,
	      unsigned int n, unsigned int u)
{
    struct flock lock;
    int len, size;
    unsigned int a[3];

    /* Set up flock structure */
    lock.l_type = F_UNLCK;
    lock.l_whence = 0;
    lock.l_start = 0;
    lock.l_len = 0;

    /* Rewind and truncate file */
    if (lseek(fd, 0, SEEK_SET) != 0 || ftruncate(fd, 0) != 0) {
	int err = errno;
	(void) fcntl(fd, F_SETLK, &lock);
	exc_raise(errno, logTruncMessage);
	return 0;
    }

    /* Write magic number, number of entries and unique entries */
    a[0] = logFileMagic;
    a[1] = n;
    a[2] = u;
    size = 3 * sizeof(unsigned int);
    if (write(fd, a, size) < size) {
	int err = errno;
	(void) fcntl(fd, F_SETLK, &lock);
        exc_raise(err, logWriteMessage);
        close(fd);
        return 0;
    }

    /* Write structure */
    size = n * sizeof(struct ActiveList);
    len = write(fd, activeList, size);
    if (len < 0) {
	int err = errno;
	(void) fcntl(fd, F_SETLK, &lock);
        exc_raise(err, logWriteMessage);
        close(fd);
        return 0;
    }

    /* Free the list */
    delete(activeList);

    /* Unlock the file */
    if (fcntl(fd, F_SETLK, &lock) < 0) {
	exc_raise(errno, logUnlockMessage);
	close(fd);
	return 0;
    }

    /* Close the stream and the file */
    close(fd);
    return 1;
}

/*
 * Request to activate a DataStation
 */
unsigned int
activateLicense(struct in_addr addr)
{
    struct ActiveList *activeList;
    unsigned int n, u, i;
    int fd;

    /* Check for unlicensed tool */
    if (unlicensed)
	return 1;

    /* Get list from log file */
    if (getLicenseLog(&fd, &activeList, &n, &u) == 0)
	return 0;

    /* Check if already being used */
    for (i = 0; i < n; i++) {
	if (activeList[i].addr.s_addr == addr.s_addr)
	    break;
    }
    if (i == n) {
	/* Not being used, see if another is possible */
	if (u == maxDataStations) {
	    (void) setLicenseLog(fd, activeList, n, u);
	    exc_raise(0, licenseLimitMessage);
	    return 0;
	}
	u++;
    }

    /* Add in this entry */
    activeList[n].addr.s_addr = addr.s_addr;
    activeList[n].pid = getpid();
    n++;

    /* Sync list to file */
    if (setLicenseLog(fd, activeList, n, u) < 0)
	return 0;

    return 1;
}

/*
 * Request to deactivate a DataStation
 */
unsigned int
deactivateLicense(struct in_addr addr)
{
    struct ActiveList *activeList;
    unsigned int n, u, i;
    int fd;

    /* Check for unlicensed tool */
    if (unlicensed)
	return 1;

    /* Get list from log file */
    if (getLicenseLog(&fd, &activeList, &n, &u) == 0)
	return 0;

    /* Remove this entry */
    for (i = 0; i < n; i++) {
	if (activeList[i].addr.s_addr == addr.s_addr) {
	    unsigned int j, k;
	    for (k = 1, j = i + 1; j < n; j++) {
		if (activeList[j].addr.s_addr == addr.s_addr)
		    k = 0;
		activeList[j - 1] = activeList[j];
	    }
	    i = n--;	/* Assign to i to make sure i != n */
	    u -= k;
	    break;
	}
    }

    /* Make sure entry was found */
    if (i == n) {
	(void) setLicenseLog(fd, activeList, n, u);
	exc_raise(0, logRemoveMessage);
	return 0;
    }

    /* Sync list to file */
    if (setLicenseLog(fd, activeList, n, u) < 0)
	return 0;

    return 1;
}

#else /* !LICENSE || sun */

/*
 * Stubs for no licensing
 */
unsigned int
getLicense(char *name, char **msg)
{
    *msg = 0;
    return 1;
}

unsigned int
activateLicense(struct in_addr addr)
{
    return 1;
}

unsigned int
deactivateLicense(struct in_addr addr)
{
    return 1;
}

#endif /* defined(LICENSE) && !defined(sun) */
