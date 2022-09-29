/*
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Id: ttyname.c,v 2.6 1997/09/12 03:01:50 markgw Exp $"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <ftw.h>
#include "pmapi.h"
#include "impl.h"

/* devl granularity for structure allocation */
#define UDQ	50

static int	ndev;			/* number of devices */
static int	maxdev;			/* number of devl structures allocated */

#define TTYNAMELEN 16

static struct devl {				/* device list	 */
	char	dname[TTYNAMELEN];	/* device name	 */
	dev_t	dev;			/* device number */
} *devl;

static char	*magic = "<<pcp ttymap>>\n";

static char *excl[] = {			/* exclude these ones */
	"/dev/annex",
	"/dev/audio",
	"/dev/cent",
	"/dev/conslog",
	"/dev/console",
	"/dev/dev",
	"/dev/dials",
	"/dev/dmrb",
	"/dev/dn_",
	"/dev/ec",
	"/dev/fd",
	"/dev/flash",
	"/dev/fsctl",
	"/dev/gfx",
	"/dev/gpib",
	"/dev/graphics",
	"/dev/gse",
	"/dev/gtr",
	"/dev/hdsp",
	"/dev/hl",
	"/dev/icmp",
	"/dev/if_ppp",
	"/dev/if_sl",
	"/dev/imidi",
	"/dev/imon",
	"/dev/input",
	"/dev/keybd",
	"/dev/klog",
	"/dev/kmem",
	"/dev/lnktrace",
	"/dev/log",
	"/dev/mem",
	"/dev/midi",
	"/dev/mmem",
	"/dev/mouse",
	"/dev/null",
	"/dev/opengl",
	"/dev/par",
	"/dev/plp",
	"/dev/postwait",
	"/dev/prf",
	"/dev/ptc",
	"/dev/ptmx",
	"/dev/pts",		/* equivalent to dev/tty... */
	"/dev/qcntl",
	"/dev/rawip",
	"/dev/rroot",
	"/dev/rswap",
	"/dev/rusr",
	"/dev/rvh",
	"/dev/sad/",
	"/dev/scsi",
	"/dev/shmiq",
	"/dev/si",
	"/dev/syscon",
	"/dev/systty",
	"/dev/tablet",
	"/dev/tcp",
	"/dev/tek",
	"/dev/ticlts",
	"/dev/ticots",
	"/dev/ticotsord",
	"/dev/tport",
	"/dev/udp",
	"/dev/usema",
	"/dev/vers",
	"/dev/vid",
	"/dev/video",
	"/dev/vme",
	"/dev/vp0",
	"/dev/xpi",
	"/dev/zero",
        "/dev/dsk",
        "/dev/grin",
        "/dev/gro",
        "/dev/mt",
        "/dev/rdsk",
        "/dev/rmt",
        0
};

static int
notinteresting(const char *s)
{
    register char **pref;

    for (pref = &excl[0]; *pref; pref++) {
	if (strncmp(s, *pref, strlen(*pref)) == 0)
	    return(1);
    }
    return(0);
}

static int
findtty(const char *objptr, const struct stat *statp, int numb)
{
    register int	i;
    int			leng, start;

    if (numb == FTW_F && (statp->st_mode & S_IFMT) == S_IFCHR) {
	/* Cut search path */
	if (notinteresting(objptr))
	    return 0;

	/* Get more ... */
	while (ndev >= maxdev) {
	    maxdev += UDQ;
	    devl = (struct devl *)realloc(devl, sizeof(struct devl ) * maxdev);
	    if (devl == NULL) {
		__pmNoMem("procpmda.findtty:", sizeof(struct devl )*maxdev, PM_RECOV_ERR);
		maxdev = ndev;
		return 0;
	    }
	}

	leng = (int)strlen(objptr);
	/* Strip off /dev/ */
	if (leng < TTYNAMELEN + 4)
	    (void)strcpy(devl[ndev].dname, &objptr[5]);
	else {
	    start = leng - TTYNAMELEN - 1;
	    for (i = start; i < leng && (objptr[i] != '/'); i++)
		;
	    if (i == leng )
		(void)strncpy(devl[ndev].dname, &objptr[start], TTYNAMELEN);
	    else
		(void)strncpy(devl[ndev].dname, &objptr[i+1], TTYNAMELEN);
	}
	devl[ndev].dev = statp->st_rdev;
	ndev++;
    }
    return 0;
}

static int
compar(const void *a, const void *b)
{
    struct devl	*pa;
    struct devl	*pb;

    pa = (struct devl *)a;
    pb = (struct devl *)b;

    return (int)((long)pa->dev - (long)pb->dev);
}

static int
readtty(void)
{
    FILE	*f;
    char	lbuf[40];
    int		x;
    char	*p;
    char	*q;

    if ((f = fopen("/tmp/pcp.ttymap", "r")) == NULL)
	return 0;

    ndev = -1;
    while (fgets(lbuf, sizeof(lbuf), f) != NULL) {
	if (ndev == -1) {
	    if (strcmp(lbuf, magic) != 0)
		return 0;
	}
	else {
	    if ((sscanf(lbuf, "%x", &x)) != 1)
		break;
	    p = lbuf;
	    while (isxdigit(*p))
		p++;
	    p++;
	    q = p;
	    while (*q && *q != '\n')
		q++;
	    *q = '\0';
	    /* Get more ... */
	    while (ndev >= maxdev) {
		maxdev += UDQ;
		devl = (struct devl *)realloc(devl, sizeof(struct devl ) * maxdev);
		if (devl == NULL) {
		    __pmNoMem("procpmda.readtty:", sizeof(struct devl )*maxdev, PM_RECOV_ERR);
		    ndev--;
		    break;
		}
	    }
	    devl[ndev].dev = x;
	    (void)strcpy(devl[ndev].dname, p);
	}
	ndev++;
    }
    (void)fclose(f);

    if (ndev)
	qsort(devl, ndev, sizeof(struct devl), compar);

    return 1;
}

static void
writetty(void)
{
    FILE	*f;
    int		i;

    if (ndev)
	qsort(devl, ndev, sizeof(struct devl), compar);

    (void)umask(022);
    (void)unlink("/tmp/pcp.ttymap");
    if ((f = fopen("/tmp/pcp.ttymap", "w")) == NULL)
	return;

    (void)fprintf(f, "%s", magic);
    for (i = 0; i < ndev; i++) {
	(void)fprintf(f, "%08x %s\n", devl[i].dev, devl[i].dname);
    }
    (void)fclose(f);
}

char *get_ttyname_info(dev_t dev)
{
    static int	first = 1;
    int		i;

    if (first) {
	if (readtty() == 0) {
	    (void)ftw("/dev", findtty, 17);
	    writetty();
	}
	first = 0;
    }

    for (i = 0; i < ndev; i++) {
	if (dev == devl[i].dev)
	    return devl[i].dname;
	else if (dev < devl[i].dev)
	    break;
    }

    return "?";
}
