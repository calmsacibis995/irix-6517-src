
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1988, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.3 $"

/* Subroutines dealing with /etc/lvtab for the Logical Volume utilities. */

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/dvh.h>
#include <sys/lv.h>
#include "lvutils.h"

extern int errno;
extern char *malloc();
extern struct volmember memhead;
extern struct logvol    *loghead;
extern struct logvol    *logtail;
extern int readonly;
extern char *progname;

void endlvent();
struct lvtabent *getlvent();
struct logvol *logvalloc();
struct volmember *volmemalloc();

/* importlvtab(). This scans the whole lvtab and imports
 * every entry, creating a logvol structure for each one. The logvol structs
 * are strung in a list off loghead (this is done by the allocator logvalloc).
 * Each logvol struct is provided with a pointer to the relevent lvtab entry.
 * Various simple checks are done to reject bogus lvtab entries.
 */

void
importlvtab(fname)
char *fname;
{
FILE *lvtabp;
register struct lvtabent *lvent;
register struct logvol *logv;
register int volstate;
register int i, devlen;
int	minor;
int 	error = 0;
int 	entries = 0;

	if ((lvtabp = fopen(fname, "r")) == NULL)
	{
		fprintf(stderr,"%s: cannot open lvtab file %s\n",progname, fname);
		exit(1);
	}

	while ((lvent = getlvent(lvtabp)) != NULL)
	{
		error = 0;
		minor = 0;
		if (!lvent->devname)
		{
			fprintf(stderr,"%s warning: lvtab entry with no device name: ignored\n", progname);
			continue;
		}
		if (strncmp(lvent->devname, "lv", 2))
		{
			fprintf(stderr,"%s warning: lvtab entry with illegal device name %s: ignored\n", progname, lvent->devname);
			continue;
		}
		devlen = strlen(lvent->devname);
		for (i = 2; i < devlen; i++)
		{
			if (!isdigit(*(lvent->devname + i)))
			{
				error = 1;
			fprintf(stderr,"%s warning: lvtab entry with illegal device name %s: ignored\n", progname, lvent->devname);
				break;
			}
			minor = minor * 10;
			minor += *(lvent->devname + i) - '0'; /* ugh! */
			if (minor > 255)
			{
				error = 1;
			fprintf(stderr,"%s warning: lvtab entry with illegal device name %s: ignored\n", progname, lvent->devname);
				break;
			}
		}
		if (error)
			continue;

		if (lvent->ndevs == 0)
		{
			fprintf(stderr,"%s warning: illegal entry %s in logical volume table: ignored\n", progname, lvent->devname);
			continue;
		}

		if (lvent->ndevs > MAXLVDEVS)
		{
			fprintf(stderr,"%s warning: illegal number of pathnames %d in lvtab entry %s: ignored\n", progname, lvent->ndevs, lvent->devname);
			continue;
		}

		/* if striping is specified, check that number
	 	 * of devices is a multiple of the striping.
	 	 */
	
		if (lvent->stripe > 1)
		{
			if (lvent->ndevs % lvent->stripe)
			{
				fprintf(stderr,"%s warning: number of pathnames in lvtab entry %s\n", progname, lvent->devname);
				fprintf(stderr,"is not multiple of striping: entry ignored\n");
				continue;
			}
			if (lvent->gran > MAXLVGRAN )
			{
				fprintf(stderr,"%s warning: illegal striping step in lvtab entry %s: ignored\n", progname, lvent->devname);
				continue;
			}
		}
		if (dupvoldevname(lvent->devname))
		{
			fprintf(stderr,"%s warning: duplicate lvtab entry %s: ignored\n", progname, lvent->devname);
			continue;
		}
		if (pathcheck(lvent))
			continue;
		if (duppath_in_lvent(lvent))
			continue;

		if (lvent->mindex == -1)
		{
			fprintf(stderr,"%s warning: illegal number of mirrors in lvtab entry %s: ignored\n", progname, lvent->devname);
			continue;
		}

		logv = logvalloc(lvent->ndevs);
		logv->tabent = lvent;
		logv->name = lvent->volname;
		logv->ndevs = lvent->ndevs;
		logv->minor = minor;
		logv->status = L_GOODTABENT;
		entries++;
	}
	fclose(lvtabp);
	if (!entries)
	{
		fprintf(stderr,"%s: lvtab file %s contains no valid entries\n",progname, fname);
		exit(-1);
	}
}

/* dupvoldevname(): checks thru vols already imported from the lvtab to
 * detect non-unique volume devnames. 
 */

dupvoldevname(p)
register char *p;
{
register struct logvol *logv;

	logv = loghead;
	while (logv)
	{
		if (streq(logv->tabent->devname, p))
			return (-1);
		logv = logv->next;
	}
	return (0);
}

/* pathcheck(): goes thru the pathnames in an lvtab entry checking that they
 * have the expected /dev/dsk/xyz#d#s# format.
 * This is of course wholly dependant on the SGI disk device naming
 * conventions! (checkformat is in pathnames.c).
 */

pathcheck(lvent)
register struct lvtabent *lvent;
{
char *path;
register int i, ndevs;

	ndevs = lvent->ndevs;
	for (i = 0; i < ndevs; i++)
	{
		path = lvent->pathnames[i];
		if (checkformat(path))
		{
			fprintf(stderr,"%s warning: bad pathname %s in lvtab entry %s: entry ignored\n", progname, path, lvent->devname);
			return (-1);
		}
	}
	return (0);
}

/* duppath_in_lvent(): checks an lvtab entry for duplicated device pathnames.
 * For each pathname in the entry, we check against all pathnames of 
 * previously known volumes (locating these via the logvol chain rooted at 
 * loghead). Note that only "good" volumes are considered, we ignore any
 * already known to be bogus.
 * Next, for each pathname, we check against other pathnames in the current 
 * entry to guard against duplicates within an entry.
 */

duppath_in_lvent(lvent)
register struct lvtabent *lvent;
{
register struct logvol *logv;
register struct lvtabent *olvent;
register int i, j, ndevs;
char *path;

	ndevs = lvent->ndevs;
	for (i = 0; i < ndevs; i++)
	{
		path = lvent->pathnames[i];
		logv = loghead;
		while (logv)
		{
			if (logv->status & L_GOODTABENT)
			{
				olvent = logv->tabent;
				for (j = 0; j < olvent->ndevs; j++)
					if streq(path, olvent->pathnames[j]) 
						goto dupfound;
			}
		logv = logv->next;
		}
		for (j = i + 1; j < ndevs; j++)
		{
			if(streq(path, lvent->pathnames[j]))
				goto dupfound;
		}
	}
	return (0);

dupfound:
	fprintf(stderr,"%s warning: duplicate pathname %s in lvtab entry %s: entry ignored\n", progname, path, lvent->devname);
	return (-1);
}

/* devnametovol(): takes an lv devname (eg lv2) and looks for a corresponding
 * logvol struct (with associated lvtab entry).
 */

struct logvol *
devnametovol(p)
register char *p;
{
register struct logvol *logv;

	logv = loghead;
	while (logv)
	{
		if (logv->tabent && logv->tabent->devname)
		{
			if (streq(logv->tabent->devname, p))
				return (logv);
		}
	logv = logv->next;
	}
	return (NULL);
}

/* Volmemsetup(): instantiates the volmember structs for the given vol.
 * The struct volmembers are allocated & a pointer to them is placed
 * in the vol. The pathnames are copied from the lvtab entry into the
 * volmember structures (actually, just the pointers are copied).
 */

volmemsetup(vol)
register struct logvol *vol;
{
register struct logvol *tvol;
register struct volmember *vmem;
register struct lvtabent *lvent;
register int i;

	if (!(vol->status & L_GOODTABENT)) return (-1);
	lvent = vol->tabent;
	vmem = volmemalloc(lvent->ndevs, vol);
	for (i = 0; i < lvent->ndevs; i++, vmem++)
	{
		vol->vmem[i] = vmem;
		vmem->pathname = lvent->pathnames[i];
		vmem->status = 0;
	}
}

/* logvalloc(): allocates space for a logvol structure containing 'n'
 * member devices. Strings it on to the logvol chain rooted at loghead.
 */

struct logvol *
logvalloc(n)
int n;
{
register struct logvol *logv, *tlogv;
register int i;

	logv = (struct logvol *)malloc(sizeof (struct logvol) + 
		((n - 1) * sizeof (vmp)));
	if (!logv)
	{
		fprintf(stderr,"%s: Can't allocate memory for logical volume structures!\n", progname);
		exit(1);
	}
	bzero((caddr_t)logv, sizeof (*logv));
	for (i = 0; i < n; i++) logv->vmem[i] = NULL;
	if (!loghead) loghead = logv;
	else
	{
		tlogv = loghead;
		while(tlogv->next) tlogv = tlogv->next;
		tlogv->next = logv;
	}
	return (logv);
}

/* volmemalloc(): allocates n volmember structures (contiguously).
 * Strings them on the double linked list rooted at memhead.
 * Puts the pointer to vol in each one (may be null)!
 * Returns a pointer to the first element.
 */

vmp
volmemalloc(n, vol)
register unsigned n;
register struct logvol *vol;
{
register vmp vmem, tvmem;

	vmem = (struct volmember *)calloc(n, sizeof (struct volmember));
	if (!vmem)
	{
		fprintf(stderr,"%s: Can't allocate memory for logical volume structures!\n", progname);
		exit(1);
	}
	/* plug in vol & add them to the circular list. */

	tvmem = vmem;
	while(n--)
	{
		tvmem->lvol = vol;
		vmemlinkon(tvmem, &memhead);
		tvmem++;
	}

	return (vmem);
}

/* vmemlinkon(): links the given volmember struct on to the circular list
 * rooted at head.
 */

vmemlinkon(vmem, head)
register vmp vmem, head;
{
	if ((head->next == NULL) || (head->next == head))
	{
		head->next = vmem;
		head->prev = vmem;
		vmem->prev = head;
	}
	else
	{
		head->prev->next = vmem;
		vmem->prev = head->prev;
		head->prev = vmem;
	}
	vmem->next = head;
}

/* vmemlinkoff(): removes the given volmember struct from its circular list
 */

vmemlinkoff(vmem)
register vmp vmem;
{
	vmem->next->prev = vmem->prev;
	vmem->prev->next = vmem->next;
}


/*****************************************************************************/

