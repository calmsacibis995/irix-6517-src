#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include "fsdump.h"
#include "fenv.h"


/*
 * We used to re-read every symlink during the xscandir routine,
 * but with file systems on bonnie having 100's of thousands of
 * symlinks, and (guessing) with each readlink of a symlink costing
 * a disk revolution, this was too expensive.  So now we rely on
 * dualscandir copying over the symlink values for unchanged inodes,
 * and scan here to read just the changed ones from disk again.
 */

void symlinkfenvtreewalk (ino64_t dirino) {
	inod_t *p, *q;
	dent_t *ep;
	int i;
	char *c1, *endofname, *fname;
	size_t namelen;
	char *symp;
	extern char filenamebuf[];

	p = PINO(&mh,dirino);
	if (p->i_mode == 0) {
		warn ("symlinkfenvtreewalk bogus inode", Pathname);
		return;
	}
	if (p->i_xnlink == 0 && dirino != rootino)
		warn ("symlinkfenvtreewalk zero nlink", Pathname);

	namelen = strlen (Pathname);
	c1 = Pathname + namelen;
	if (*(c1-1) =='/') --c1;
	endofname = c1;

	for (i=0; i < p->i_xndent; i++) {

		ep = PENT (mh, p, i);

		fname = PNAM (mh, ep);
		if (fname[0] == '.'
		    && (fname[1] == '\0'
			|| fname[1] == '.'
			    && fname[2] == '\0'))
				continue;

		q = PINO(&mh, ep->e_ino);

		c1 = endofname;
		*c1++ = '/';
		if (namelen + 1 + strlen(fname) + 1 >= PATH_MAX)
			error ("fsdump: pathname too long");
		(void) strcpy (c1, fname);

		if (S_ISLNK(q->i_mode)
		 && ((symp = getfenv(q, FV_SYMLINK))==NULL || strlen(symp)==0)) {
			int sz = readlink (Pathname, filenamebuf, PATH_MAX);
			if (sz > 0 && sz < PATH_MAX)
				filenamebuf[sz] = '\0';
			else
				filenamebuf[0] = '\0';
			putfenv (q, FV_SYMLINK, filenamebuf);
		}

		if ( ! S_ISDIR(q->i_mode) )
			continue;
		if (ismounted (Pathname))
			continue;
		symlinkfenvtreewalk (ep->e_ino);
	}
	*endofname = 0;
}
