#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <limits.h>
#include "fsdump.h"
#include "fenv.h"
#include "externs.h"


/*
 * We want to quit qsum'ing when our quantum is up, but
 * there is not enough time in a typical quantum to qsum
 * all the files in a large file system.  So we use the
 * FV_QFLAG value of "0" (OFF) or "1" (ON) in a crude
 * sort of coloring algorithm.  The idea is that (likely
 * over several quantums) we walk the tree, only qsuming
 * files with this flag, say, in the ON state, turning the
 * flag OFF as we go.  Once we get all the flags OFF, we go
 * back to the top, qsuming all those files with the flag
 * OFF, and turning it back ON.
 * 
 * The private "qsumflag" is set to ON or OFF, depending
 * on which value we are looking for currently.  The
 * value of qsumflag is remembered from one quantum to
 * the next by stashing it in the fenv of the root inode, as the
 * value FV_QFLAG.  This overloading is safe because the root inode
 * is a directory, and we only compute qsums for regular
 * files.
 * 
 * The main fsdump routine that invokes us is responsible
 * for calling "initqsumflag" before calling
 * qsumfenvtreewalk, and for calling "dumpqsumflag" after
 * the qsumfenvtreewalk returns.  This gives us the hooks
 * needed to setup a private variable "qsumflag" to the
 * state that was stashed in the root inode, and if we managed
 * to finish walking the tree in the current quantum
 * (havethetime() is true), to toggle the stashed state
 * of this flag.
 *
 * The qsumflagmatch routine determines if the FV_QFLAG
 * of the current file matches the qsumflag value,
 * and the qsumflagtoggle routine flips FV_QFLAG.
 */

static enum {OFF=0, ON=1} qsumflag;	/* select which files to qsum this quantum */

void qsumfenvtreewalk (ino64_t dirino) {
	inod_t *p, *q;
	dent_t *ep;
	int i;
	char *c1, *endofname, *fname;
	size_t namelen;

	if ( ! havethetime())
		return;
	p = PINO(&mh,dirino);
	if (p->i_mode == 0) {
		warn ("qsumfenvtreewalk bogus inode", Pathname);
		return;
	}
	if (p->i_xnlink == 0 && dirino != rootino)
		warn ("qsumfenvtreewalk zero nlink", Pathname);

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

		q = PINO(&mh,ep->e_ino);

		c1 = endofname;
		*c1++ = '/';
		if (namelen + 1 + strlen(fname) + 1 >= PATH_MAX)
			error ("fsdump: pathname too long");
		(void) strcpy (c1, fname);

		if (S_ISREG(q->i_mode) && havethetime() && qsumflagmatch(q)) {
			char *oldsum;		/* the FV_QSUM value in the dump */
			char *newsum;		/* the freshly computed FV_QSUM value */
			int fchgd;		/* set if file stat changed since inode scan */
			int oldsumok;		/* set if old sum in fenv is valid */
			int newsumok;		/* set if new quicksum is valid */
			int samesum;		/* set if old/new sums valid and same */
			int diffsum;		/* set if old/new sums valid, but not same */

			oldsum = getfenv (q, FV_QSUM);
			newsum = quicksum(Pathname);

			fchgd  = fchanged (q, Pathname, NULL);
			oldsumok = (oldsum != NULL && strcmp (oldsum, "0") != 0);
			newsumok = (newsum != NULL && strcmp (newsum, "0") != 0);
			samesum  = (oldsumok && newsumok && strcmp(oldsum,newsum)==0);
			diffsum  = (oldsumok && newsumok && strcmp(oldsum,newsum)!=0);

#if 0
printf("fchgd %d, oldok %d, newok %d, same %d, diff %d",fchgd,oldsumok,newsumok,samesum,diffsum);
printf(" old %10s new %10s %.24s\n", oldsum, newsum, Pathname);
#endif

			if ( ! fchgd && diffsum ) {
				extern int errno;

				errno = 0;
				fprintf(stderr,"oldsum %s, newsum %s ",oldsum,newsum);
				warn (">>>>>> File contents corrupted !! :", Pathname);
			}

			if (fchgd && oldsumok)
				unsetfenv (q, FV_QSUM);

			if ( ! fchgd && ! samesum && newsumok)
				putfenv (q, FV_QSUM, newsum);

			qsumflagtoggle(q);
		}

		if ( ! (S_ISDIR(q->i_mode) || S_ISLNK(q->i_mode)) )
			continue;

		if (S_ISLNK(q->i_mode))
			continue;
		if (ismounted (Pathname))
			continue;
		qsumfenvtreewalk (ep->e_ino);
	}
	*endofname = 0;
}

void initqsumflag(void){
	char *flagp = getfenv (PINO(&mh, rootino), FV_QFLAG);
	if (flagp != NULL && (strcmp (flagp, "1") == 0 || strcmp (flagp, "on") == 0))
		qsumflag = ON;
	else
		qsumflag = OFF;
}

void dumpqsumflag(void){
	if (havethetime()) {
		qsumflagtoggle (PINO (&mh, rootino));
#if 1
		printf("Toggled qsumflag\n");
#endif
	}
}

int qsumflagmatch (inod_t *p) {
	char *flagp = getfenv (p, FV_QFLAG);
	if (flagp != NULL && (strcmp (flagp, "1") == 0 || strcmp (flagp, "on") == 0))
		return qsumflag == ON;
	else
		return qsumflag == OFF;
}

void qsumflagtoggle(inod_t *p) {
	char *flagp = getfenv (p, FV_QFLAG);
	if (flagp != NULL && (strcmp (flagp, "1") == 0 || strcmp (flagp, "on") == 0))
		putfenv (p, FV_QFLAG, "0");
	else
		putfenv (p, FV_QFLAG, "1");
}
