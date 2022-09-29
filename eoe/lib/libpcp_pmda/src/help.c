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

#ident "$Id: help.c,v 1.4 1998/12/14 23:55:59 kenmcd Exp $"

/*
 * Get help text from files built using newhelp
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>
#include "pmapi.h"
#include "impl.h"
#include "pmda.h"

extern int	errno;

typedef struct {
    pmID	pmid;
    __uint32_t	off_oneline;
    __uint32_t	off_text;
} help_idx_t;

typedef struct {
    int		dir_fd;
    int		pag_fd;
    int		numidx;
    help_idx_t	*index;
    char	*text;
    int		textlen;
} help_t;

static help_t	*tab = NULL;
static int	numhelp = 0;

/*
 * open the help text files and return a handle on success
 */
int
pmdaOpenHelp(char *fname)
{
    char	pathname[MAXPATHLEN];
    int		sts;
    help_idx_t	hdr;
    help_t	*hp;
    struct stat	sbuf;

    for (sts = 0; sts < numhelp; sts++) {
	if (tab[sts].dir_fd == -1)
	    break;
    }
    if (sts == numhelp) {
	sts = numhelp++;
	tab = (help_t *)realloc(tab, numhelp * sizeof(tab[0]));
	if (tab == NULL) {
	    __pmNoMem("pmdaOpenHelp", numhelp * sizeof(tab[0]), PM_RECOV_ERR);
	    numhelp = 0;
	    return -errno;
	}
    }
    hp = &tab[sts];

    hp->dir_fd = hp->pag_fd = -1;
    hp->numidx = hp->textlen = 0;
    hp->index = NULL;
    hp->text = NULL;

    sprintf(pathname, "%s.dir", fname);
    hp->dir_fd = open(pathname, O_RDONLY);
    if (hp->dir_fd < 0) {
	sts = -errno;
	goto failed;
    }

    if (read(hp->dir_fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
	sts = -EINVAL;
	goto failed;
    }

    if (hdr.pmid != 0x50635068 ||
	(hdr.off_oneline & 0xffff0000) != 0x31320000) {
	sts = -EINVAL;
	goto failed;
    }

    hp->numidx = hdr.off_text;

    hp->index = (help_idx_t *)mmap(NULL, (hp->numidx + 1) * sizeof(help_idx_t),
				    PROT_READ, MAP_PRIVATE, hp->dir_fd, 0);
    if (hp->index == NULL) {
	sts = -errno;
	goto failed;
    }

    sprintf(pathname, "%s.pag", fname);
    hp->pag_fd = open(pathname, O_RDONLY);
    if (hp->pag_fd < 0) {
	sts = -errno;
	goto failed;
    }

    if (fstat(hp->pag_fd, &sbuf) < 0) {
	sts = -errno;
	goto failed;
    }

    hp->textlen = (int)sbuf.st_size;
    hp->text = (char *)mmap(NULL, hp->textlen, PROT_READ, MAP_PRIVATE, hp->pag_fd, 0);
    if (hp->text == NULL) {
	sts = -errno;
	goto failed;
    }

    return numhelp - 1;

failed:
    pmdaCloseHelp(numhelp-1);
    return sts;
}

/*
 * retrieve pmID help text, ...
 *
 */
char *
pmdaGetHelp(int handle, pmID pmid, int type)
{
    int		i;
    help_t	*hp;

    if (handle < 0 || handle >= numhelp)
	return NULL;
    hp = &tab[handle];

    /* search forwards -- could use binary chop */
    for (i = 1; i <= hp->numidx; i++) {
	if (hp->index[i].pmid == pmid) {
	    if (type & PM_TEXT_ONELINE)
		return &hp->text[hp->index[i].off_oneline];
	    else
		return &hp->text[hp->index[i].off_text];
	}
    }

    return NULL;
}

/*
 * retrieve pmInDom help text, ...
 *
 */
char *
pmdaGetInDomHelp(int handle, pmInDom indom, int type)
{
    int			i;
    help_t		*hp;
    pmID		pmid;
    __pmID_int		*pip = (__pmID_int *)&pmid;

    if (handle < 0 || handle >= numhelp)
	return NULL;
    hp = &tab[handle];

    *pip = *((__pmID_int *)&indom);
    /*
     * set a bit here to disambiguate pmInDom from pmID
     * -- this "hack" is shared between here and newhelp/newhelp.c
     */
    pip->pad = 1;

    /* search backwards ... pmInDom entries are at the end */
    for (i = hp->numidx; i >= 1; i--) {
	if (hp->index[i].pmid == pmid) {
	    if (type & PM_TEXT_ONELINE)
		return &hp->text[hp->index[i].off_oneline];
	    else
		return &hp->text[hp->index[i].off_text];
	}
    }

    return NULL;
}

/*
 * this is only here for chkhelp(1) ... export the control data strcuture
 */
void *
__pmdaHelpTab(void)
{
    return (void *)tab;
}

void
pmdaCloseHelp(int handle)
{
    help_t		*hp;

    if (handle < 0 || handle >= numhelp)
	return;
    hp = &tab[handle];

    if (hp->dir_fd != -1)
	close(hp->dir_fd);
    if (hp->pag_fd != -1)
	close(hp->pag_fd);
    if (hp->index != NULL)
	munmap(hp->index, (hp->numidx + 1) * sizeof(help_idx_t));
    if (hp->text != NULL)
	munmap(hp->text, hp->textlen);

    hp->dir_fd = hp->pag_fd = -1;
    hp->numidx = hp->textlen = 0;
    hp->index = NULL;
    hp->text = NULL;

    return;
}
