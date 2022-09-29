#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <libgen.h>
#include "fsdump.h"
#include "fenv.h"


void rcsfenvtreewalk (ino64_t dirino) {
	inod_t *p, *q;
	dent_t *ep;
	int i;
	char *c1, *endofname, *fname;
	size_t namelen;
	int isRCSdir;
	char *revp;
	char *lockp;
	char *datep;
	char *authp;

	if (!havethetime())
		return;
	p = PINO(&mh,dirino);
	if (p->i_mode == 0) {
		warn ("rcsfenvtreewalk bogus inode", Pathname);
		return;
	}
	if (p->i_xnlink == 0 && dirino != rootino)
		warn ("rcsfenvtreewalk zero nlink", Pathname);

	namelen = strlen (Pathname);
	c1 = Pathname + namelen;
	if (*(c1-1) =='/') --c1;
	endofname = c1;

	isRCSdir = ( strcmp ("RCS", basename(Pathname)) == 0 );

	for (i=0; i < p->i_xndent; i++) {

		ep = PENT (mh, p, i);

		fname = PNAM (mh, ep);
		if (fname[0] == '.'
		    && (fname[1] == '\0'
			|| fname[1] == '.'
			    && fname[2] == '\0'))
				continue;

		q = PINO(&mh,ep->e_ino);

		c1 = endofname;
		*c1++ = '/';
		if (namelen + 1 + strlen(fname) + 1 >= PATH_MAX)
			error ("fsdump: pathname too long");
		(void) strcpy (c1, fname);
#if 0
		/* Cleanse bonnie of rcsfenv's claiming that "strict"
		 * has the file locked.  It was the result of a bug
		 * that caused lock parsing to mess up on RCS 5.6 files.
		 */
		if ((revp = getfenv (q, FV_RCSLOCK)) != NULL && strcmp (revp, "strict") == 0)
			unsetfenv (q, FV_RCSREV);
#endif

		if (isRCSdir && S_ISREG(q->i_mode) && gmatch (fname, "*,v")
		    && ((revp = getfenv(q,FV_RCSREV))==NULL || strlen(revp)==0)
		    && havethetime()
		) {
			readrcs (Pathname, &revp, &lockp, &datep, &authp);
			putfenv (q, FV_RCSREV, revp);
			putfenv (q, FV_RCSLOCK, lockp);
			putfenv (q, FV_RCSDATE, datep);
			putfenv (q, FV_RCSAUTH, authp);
			free ((void *)revp);
			free ((void *)lockp);
			free ((void *)datep);
			free ((void *)authp);
		}

		if ( ! (S_ISDIR(q->i_mode) || S_ISLNK(q->i_mode)) )
			continue;

		if (S_ISLNK(q->i_mode))
			continue;
		if (ismounted (Pathname))
			continue;
		rcsfenvtreewalk (ep->e_ino);
	}
	*endofname = 0;
}
