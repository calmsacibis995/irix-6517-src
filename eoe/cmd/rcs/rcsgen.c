/* Generate RCS revisions.  */

/* Copyright 1982, 1988, 1989 Walter Tichy
   Copyright 1990, 1991, 1992, 1993, 1994, 1995 Paul Eggert
   Distributed under license by the Free Software Foundation, Inc.

This file is part of RCS.

RCS is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

RCS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RCS; see the file COPYING.
If not, write to the Free Software Foundation,
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

Report problems and direct all questions to:

    rcs-bugs@cs.purdue.edu

*/

/*
 * $Log: rcsgen.c,v $
 * Revision 1.10  1997/03/31 21:28:42  msf
 * Upgrade RCS from 5.6 GNU release to 5.7.
 * Include additional hooks for future programmatic (API) interfaces
 * (some code is bracketed with #ifdef RCSLIBRARY).
 *
 * Revision 1.2  1997/01/27  22:10:37  msf
 * Add copyrevision() - like buildrevision(), copies deltas into archive file,
 * but does not actually build the specified revision.
 * Add enterrevision() - enters the top revision of an archive file into
 * the internal edit buffer, so that diff scripts may be applied.
 * Add arg to editstring() - tells editstring whether edit script is a
 * string from an archive (,v) file, or is an input file ("mod").
 *
 * Revision 1.1  1996/10/03  17:34:24  pj
 * TAKE ptools now has a copy of kudzu RCS.
 * * This take puts a complete copy of kudzu's irix/cmd/rcs in
 *   the ptools source.  Eventually, when ptools is building
 *   against a kudzu based ROOT, we can remove this copy,
 *   and use the RCS library that will ship with kudzu.
 *   But that will be a while -- ptools currently builds with
 *   an IRIX 5.3 ROOT, in order to provide support for the
 *   widest practical audience.
 * * This is just an exact copy of the kudzu RCS source.
 *   It doesn't yet build against an IRIX 5.3 ROOT.
 *   Fixes for that problem will be forthcoming.
 *
 * Revision 1.6.1.3  1996/08/12  22:58:01  msf
 * Do not truncate whitespace from end of log message lines when parsing
 * existing RCS files (affects $Log expansion; also ci would otherwise
 * change the log message when writing it back into the RCS file)
 *
 * Revision 1.6.1.2  1996/06/28  23:59:02  msf
 * RCS library changes:
 * 1) Add putnamesp() - to emit namespace fields into RCS file
 * 2) Add reference to rcsNoStdin (do not attempt to read from stdin; ret EOF)
 * 3) Add reference to finform (use finform instead of stderr)
 *
 * Revision 1.6.1.1  1996/06/25  23:35:46  msf
 * Create branch for RCS library, based on version 5.7.
 *
 * Revision 5.16  1995/06/16 06:19:24  eggert
 * Update FSF address.
 *
 * Revision 5.15  1995/06/01 16:23:43  eggert
 * (putadmin): Open RCS file with FOPEN_WB.
 *
 * Revision 5.14  1994/03/17 14:05:48  eggert
 * Work around SVR4 stdio performance bug.
 * Flush stderr after prompt.  Remove lint.
 *
 * Revision 5.13  1993/11/03 17:42:27  eggert
 * Don't discard ignored phrases.  Improve quality of diagnostics.
 *
 * Revision 5.12  1992/07/28  16:12:44  eggert
 * Statement macro names now end in _.
 * Be consistent about pathnames vs filenames.
 *
 * Revision 5.11  1992/01/24  18:44:19  eggert
 * Move put routines here from rcssyn.c.
 * Add support for bad_creat0.
 *
 * Revision 5.10  1991/10/07  17:32:46  eggert
 * Fix log bugs, e.g. ci -t/dev/null when has_mmap.
 *
 * Revision 5.9  1991/09/10  22:15:46  eggert
 * Fix test for redirected stdin.
 *
 * Revision 5.8  1991/08/19  03:13:55  eggert
 * Add piece tables.  Tune.
 *
 * Revision 5.7  1991/04/21  11:58:24  eggert
 * Add MS-DOS support.
 *
 * Revision 5.6  1990/12/27  19:54:26  eggert
 * Fix bug: rcs -t inserted \n, making RCS file grow.
 *
 * Revision 5.5  1990/12/04  05:18:45  eggert
 * Use -I for prompts and -q for diagnostics.
 *
 * Revision 5.4  1990/11/01  05:03:47  eggert
 * Add -I and new -t behavior.  Permit arbitrary data in logs.
 *
 * Revision 5.3  1990/09/21  06:12:43  hammer
 * made putdesc() treat stdin the same whether or not it was from a terminal
 * by making it recognize that a single '.' was then end of the
 * description always
 *
 * Revision 5.2  1990/09/04  08:02:25  eggert
 * Fix `co -p1.1 -ko' bug.  Standardize yes-or-no procedure.
 *
 * Revision 5.1  1990/08/29  07:14:01  eggert
 * Clean old log messages too.
 *
 * Revision 5.0  1990/08/22  08:12:52  eggert
 * Remove compile-time limits; use malloc instead.
 * Ansify and Posixate.
 *
 * Revision 4.7  89/05/01  15:12:49  narten
 * changed copyright header to reflect current distribution rules
 * 
 * Revision 4.6  88/08/28  14:59:10  eggert
 * Shrink stdio code size; allow cc -R; remove lint; isatty() -> ttystdin()
 * 
 * Revision 4.5  87/12/18  11:43:25  narten
 * additional lint cleanups, and a bug fix from the 4.3BSD version that
 * keeps "ci" from sticking a '\377' into the description if you run it
 * with a zero-length file as the description. (Guy Harris)
 * 
 * Revision 4.4  87/10/18  10:35:10  narten
 * Updating version numbers. Changes relative to 1.1 actually relative to
 * 4.2
 * 
 * Revision 1.3  87/09/24  13:59:51  narten
 * Sources now pass through lint (if you ignore printf/sprintf/fprintf 
 * warnings)
 * 
 * Revision 1.2  87/03/27  14:22:27  jenkins
 * Port to suns
 * 
 * Revision 4.2  83/12/02  23:01:39  wft
 * merged 4.1 and 3.3.1.1 (clearerr(stdin)).
 * 
 * Revision 4.1  83/05/10  16:03:33  wft
 * Changed putamin() to abort if trying to reread redirected stdin.
 * Fixed getdesc() to output a prompt on initial newline.
 * 
 * Revision 3.3.1.1  83/10/19  04:21:51  lepreau
 * Added clearerr(stdin) for re-reading description from stdin.
 * 
 * Revision 3.3  82/11/28  21:36:49  wft
 * 4.2 prerelease
 * 
 * Revision 3.3  82/11/28  21:36:49  wft
 * Replaced ferror() followed by fclose() with ffclose().
 * Putdesc() now suppresses the prompts if stdin
 * is not a terminal. A pointer to the current log message is now
 * inserted into the corresponding delta, rather than leaving it in a
 * global variable.
 *
 * Revision 3.2  82/10/18  21:11:26  wft
 * I added checks for write errors during editing, and improved
 * the prompt on putdesc().
 *
 * Revision 3.1  82/10/13  15:55:09  wft
 * corrected type of variables assigned to by getc (char --> int)
 */




#include "rcsbase.h"

libId(genId, "$Id: rcsgen.c,v 1.10 1997/03/31 21:28:42 msf Exp $")

int interactiveflag;  /* Should we act as if stdin is a tty?  */
int rcsNoStdin = 0;   /* Should all stdin requests return EOF? */
struct buf curlogbuf;  /* buffer for current log message */

enum stringwork { enter, copy, edit, expand, edit_expand };

static void putdelta P((struct hshentry const*,FILE*));
static void scandeltatext P((struct hshentry*,enum stringwork,int));

static void putnamesp P((struct namespace *, FILE *));



	char const *
buildrevision(deltas, target, outfile, expandflag)
	struct hshentries const *deltas;
	struct hshentry *target;
	FILE *outfile;
	int expandflag;
/* Function: Generates the revision given by target
 * by retrieving all deltas given by parameter deltas and combining them.
 * If outfile is set, the revision is output to it,
 * otherwise written into a temporary file.
 * Temporary files are allocated by maketemp().
 * if expandflag is set, keyword expansion is performed.
 * Return 0 if outfile is set, the name of the temporary file otherwise.
 *
 * Algorithm: Copy initial revision unchanged.  Then edit all revisions but
 * the last one into it, alternating input and output files (resultname and
 * editname). The last revision is then edited in, performing simultaneous
 * keyword substitution (this saves one extra pass).
 * All this simplifies if only one revision needs to be generated,
 * or no keyword expansion is necessary, or if output goes to stdout.
 */
{
	if (deltas->first == target) {
                /* only latest revision to generate */
		openfcopy(outfile);
		scandeltatext(target, expandflag?expand:copy, true);
		if (outfile)
			return 0;
		else {
			Ozclose(&fcopy);
			return resultname;
		}
        } else {
                /* several revisions to generate */
		/* Get initial revision without keyword expansion.  */
		scandeltatext(deltas->first, enter, false);
		while ((deltas=deltas->rest)->rest) {
                        /* do all deltas except last one */
			scandeltatext(deltas->first, edit, false);
                }
		if (expandflag || outfile) {
                        /* first, get to beginning of file*/
			finishedit((struct hshentry*)0, outfile, false);
                }
		scandeltatext(target, expandflag?edit_expand:edit, true);
		finishedit(
			expandflag ? target : (struct hshentry*)0,
			outfile, true
		);
		if (outfile)
			return 0;
		Ozclose(&fcopy);
		return resultname;
        }
}



	void
copyrevision(target)
	struct hshentry *target;
/* Function: Copy the deltas in the archive file, up to and including
 * the revision given by target.  Like buildrevision, except that
 * the deltas are merely copied; the target revision is not actually
 * constructed from the copied deltas.
 */
{
		/* just a kludgy way to get the string of the target
		 * delta copied.  Scandeltatext automatically copies
		 * everything up to (but not including) the specified
		 * target revision, and then applies the selected function
		 * (here, "copystring") to the target's string.   Since
		 * we want that copied to foutptr (a side effect), and
		 * no main action, we perform a copystring to /dev/null.
		 * (copystring copies the string to fcopy).
		 */
		FILE *tmp = fcopy;
		fcopy = fopen("/dev/null", "w");
		scandeltatext(target, copy, false);
		fclose(fcopy);
		fcopy = tmp;
}

	void
enterrevision(target)
	struct hshentry *target;
/* Function: Enter the current revision, indicated by target, into the
 * edit buffer.   This reads from finptr, so finptr must point to the target
 * rev.
 *
 * Also, copy the current revision (after compressing double '@'s but not
 * performing keyword expansion) to the file indicated by foutptr.
 */
{
	FILE *fp = foutptr;
	foutptr = NULL;			/* So that scandeltatext won't
					 * dump unprocessed revision there
					 */
	scandeltatext(target, enter, true);	/*  enter into edit buffer */

	foutptr = fp;
	finishedit(NULL, foutptr, true);	/* copy to foutptr, remove
						 * double '@'s; makes fcopy
						 * a copy of foutptr
						 */
	fcopy = NULL;				/* Leave foutptr open,
						 * clean up dup file ptr
						 */

}



	static void
scandeltatext(delta, func, needlog)
	struct hshentry *delta;
	enum stringwork func;
	int needlog;
/* Function: Scans delta text nodes up to and including the one given
 * by delta. For the one given by delta, the log message is saved into
 * delta->log if needlog is set; func specifies how to handle the text.
 * Similarly, if needlog, delta->igtext is set to the ignored phrases.
 * Assumes the initial lexeme must be read in first.
 * Does not advance nexttok after it is finished.
 */
{
	struct hshentry const *nextdelta;

        for (;;) {
		if (eoflex())
		    fatserror("can't find delta for revision %s", delta->num);
                nextlex();
                if (!(nextdelta=getnum())) {
		    fatserror("delta number corrupted");
                }
		getkeystring(Klog);
		if (needlog && delta==nextdelta) {
			delta->log = savestring(&curlogbuf);
			trunclogmsg(&delta->log, '\n');
			nextlex();
			delta->igtext = getphrases(Ktext);
                } else {readstring();
			ignorephrases(Ktext);
                }
		getkeystring(Ktext);

		if (delta==nextdelta)
			break;
		readstring(); /* skip over it */

	}
	switch (func) {
		case enter: enterstring(); break;
		case copy: copystring(); break;
		case expand: xpandstring(delta); break;
		case edit: editstring((struct hshentry *)0, true); break;
		case edit_expand: editstring(delta,true); break;
	}
}

	struct cbuf
cleanlogmsg(m, s)
	char *m;
	size_t s;
/* In-place copy of message, removing tabs and blanks immediately
 * preceeding newlines.  Also notes last non-whitespace character
 * in message (i.e. not blank, tab, or newline).
 *
 * Returns a cbuf, which points to the input string (post-modification),
 * whose length is the string up to the trailing whitespace (i.e.
 * does not include any trailing whtespace).
 *
 * For memory management: remember that the original memory is
 * returned.
 */
{
	register char *t = m;
	register char const *f = t;
	struct cbuf r;
	while (s) {
	    --s;
	    if ((*t++ = *f++) == '\n')
		while (m < --t)
		    if (t[-1]!=' ' && t[-1]!='\t') {
			*t++ = '\n';
			break;
		    }
	}
	while (m < t  &&  (t[-1]==' ' || t[-1]=='\t' || t[-1]=='\n'))
	    --t;
	r.string = m;
	r.size = t - m;
	return r;
}


int ttystdin()
{
	static int initialized;
	if (!initialized) {
		if (!interactiveflag)
			interactiveflag = isatty(STDIN_FILENO);
		initialized = true;
	}
	return interactiveflag;
}

	int
getcstdin()
{
	register FILE *in;
	register int c;

	in = stdin;
	if (feof(in) && ttystdin())
		clearerr(in);
	c = getc(in);
	if (c == EOF) {
		testIerror(in);
		if (feof(in) && ttystdin())
			afputc('\n',stderr);
	}
	return c;
}

#if has_prototypes
	int
yesorno(int default_answer, char const *question, ...)
#else
		/*VARARGS2*/ int
	yesorno(default_answer, question, va_alist)
		int default_answer; char const *question; va_dcl
#endif
{
	va_list args;
	register int c, r;
	if (!quietflag && (finform == stderr) && ttystdin()) {
		oflush();
		vararg_start(args, question);
		fvfprintf(stderr, question, args);
		va_end(args);
		eflush();
		r = c = getcstdin();
		while (c!='\n' && !feof(stdin))
			c = getcstdin();
		if (r=='y' || r=='Y')
			return true;
		if (r=='n' || r=='N')
			return false;
	}
	return default_answer;
}


	void
putdesc(textflag, textfile)
	int textflag;
	char *textfile;
/* Function: puts the descriptive text into file frewrite.
 * if finptr && !textflag, the text is copied from the old description.
 * Otherwise, if textfile, the text is read from that
 * file, or from stdin, if !textfile.
 * A textfile with a leading '-' is treated as a string, not a pathname.
 * If finptr, the old descriptive text is discarded.
 * Always clears foutptr.
 */
{
	static struct buf desc;
	static struct cbuf desclean;

	register FILE *txt;
	register int c;
	register FILE * frew;
	register char *p;
	register size_t s;
	char const *plim;

	frew = frewrite;
	if (finptr && !textflag) {
                /* copy old description */
		aprintf(frew, "\n\n%s%c", Kdesc, nextc);
		foutptr = frewrite;
		getdesc(false);
		foutptr = 0;
        } else {
		foutptr = 0;
                /* get new description */
		if (finptr) {
                        /*skip old description*/
			getdesc(false);
                }
		aprintf(frew,"\n\n%s\n%c",Kdesc,SDELIM);
		if (!textfile)
			desclean = getsstdin(
				"t-", "description",
				"NOTE: This is NOT the log message!\n", &desc
			);
		else if (!desclean.string) {
			if (*textfile == '-') {
				p = textfile + 1;
				s = strlen(p);
			} else {
				if (!(txt = fopenSafer(textfile, "r")))
					efaterror(textfile);
				bufalloc(&desc, 1);
				p = desc.string;
				plim = p + desc.size;
				for (;;) {
					if ((c=getc(txt)) == EOF) {
						testIerror(txt);
						if (feof(txt))
							break;
					}
					if (plim <= p)
					    p = bufenlarge(&desc, &plim);
					*p++ = c;
				}
				if (fclose(txt) != 0)
					Ierror();
				s = p - desc.string;
				p = desc.string;
			}
			desclean = cleanlogmsg(p, s);
		}
		putstring(frew, false, desclean, true);
		aputc_('\n', frew)
        }
}

	struct cbuf
getsstdin(option, name, note, buf)
	char const *option, *name, *note;
	struct buf *buf;
{
	register int c;
	register char *p;
	register size_t i;
	register int tty = ttystdin();

	static struct cbuf retval = {0, 0};
	if (rcsNoStdin) return retval;

	if (tty) {
	    aprintf(stderr,
		"enter %s, terminated with single '.' or end of file:\n%s>> ",
		name, note
	    );
	    eflush();
	} else if (feof(stdin))
	    rcsfaterror("can't reread redirected stdin for %s; use -%s<%s>",
		name, option, name
	    );
	
	for (
	   i = 0,  p = 0;
	   c = getcstdin(),  !feof(stdin);
	   bufrealloc(buf, i+1),  p = buf->string,  p[i++] = c
	)
		if (c == '\n')
			if (i  &&  p[i-1]=='.'  &&  (i==1 || p[i-2]=='\n')) {
				/* Remove trailing '.'.  */
				--i;
				break;
			} else if (tty) {
				aputs(">> ", stderr);
				eflush();
			}
	return cleanlogmsg(p, i);
}


	void
putadmin()
/* Output the admin node.  */
{
	register FILE *fout;
	struct assoc const *curassoc;
	struct rcslock const *curlock;
	struct access const *curaccess;

	if (!(fout = frewrite)) {
#		if bad_creat0
			ORCSclose();
			fout = fopenSafer(makedirtemp(0), FOPEN_WB);
#		else
			int fo = fdlock;
			fdlock = -1;
			fout = fdopen(fo, FOPEN_WB);
#		endif

		if (!(frewrite = fout))
			efaterror(RCSname);
	}

	/*
	* Output the first character with putc, not printf.
	* Otherwise, an SVR4 stdio bug buffers output inefficiently.
	*/
	aputc_(*Khead, fout)
	aprintf(fout, "%s\t%s;\n", Khead + 1, Head?Head->num:"");
	if (Dbranch && VERSION(4)<=RCSversion)
		aprintf(fout, "%s\t%s;\n", Kbranch, Dbranch);

	aputs(Kaccess, fout);
	curaccess = AccessList;
	while (curaccess) {
	       aprintf(fout, "\n\t%s", curaccess->login);
	       curaccess = curaccess->nextaccess;
	}
	aprintf(fout, ";\n%s", Ksymbols);
	curassoc = Symbols;
	while (curassoc) {
	       aprintf(fout, "\n\t%s:%s", curassoc->symbol, curassoc->num);
	       curassoc = curassoc->nextassoc;
	}
	aprintf(fout, ";\n%s", Klocks);
	curlock = Locks;
	while (curlock) {
	       aprintf(fout, "\n\t%s:%s", curlock->login, curlock->delta->num);
	       curlock = curlock->nextlock;
	}
	if (StrictLocks) aprintf(fout, "; %s", Kstrict);
	aprintf(fout, ";\n");
	if (Comment.size) {
		aprintf(fout, "%s\t", Kcomment);
		putstring(fout, true, Comment, false);
		aprintf(fout, ";\n");
	}
	if (Expand != KEYVAL_EXPAND)
		aprintf(fout, "%s\t%c%s%c;\n",
			Kexpand, SDELIM, expand_names[Expand], SDELIM
		);
	if (rcsNameSpace) putnamesp(rcsNameSpace, fout);
	awrite(Ignored.string, Ignored.size, fout);
	aputc_('\n', fout)
}


	static void
putdelta(node, fout)
	register struct hshentry const *node;
	register FILE * fout;
/* Output the delta NODE to FOUT.  */
{
	struct branchhead const *nextbranch;

	if (!node) return;

	aprintf(fout, "\n%s\n%s\t%s;\t%s %s;\t%s %s;\nbranches",
		node->num,
		Kdate, node->date,
		Kauthor, node->author,
		Kstate, node->state?node->state:""
	);
	nextbranch = node->branches;
	while (nextbranch) {
	       aprintf(fout, "\n\t%s", nextbranch->hsh->num);
	       nextbranch = nextbranch->nextbranch;
	}

	aprintf(fout, ";\n%s\t%s;\n", Knext, node->next?node->next->num:"");
	awrite(node->ig.string, node->ig.size, fout);
	putnamesp(node->namesp, fout);
}


	void
puttree(root, fout)
	struct hshentry const *root;
	register FILE *fout;
/* Output the delta tree with base ROOT in preorder to FOUT.  */
{
	struct branchhead const *nextbranch;

	if (!root) return;

	if (root->selector)
		putdelta(root, fout);

	puttree(root->next, fout);

	nextbranch = root->branches;
	while (nextbranch) {
	     puttree(nextbranch->hsh, fout);
	     nextbranch = nextbranch->nextbranch;
	}
}


	int
putdtext(delta, srcname, fout, diffmt)
	struct hshentry const *delta;
	char const *srcname;
	FILE *fout;
	int diffmt;
/*
 * Output a deltatext node with delta number DELTA->num, log message DELTA->log,
 * ignored phrases DELTA->igtext and text SRCNAME to FOUT.
 * Double up all SDELIMs in both the log and the text.
 * Make sure the log message ends in \n.
 * Return false on error.
 * If DIFFMT, also check that the text is valid diff -n output.
 */
{
	RILE *fin;
	if (!(fin = Iopen(srcname, "r", (struct stat*)0))) {
		eerror(srcname);
		return false;
	}
	putdftext(delta, fin, fout, diffmt ? -1 : 0);
	Ifclose(fin);
	return true;
}
	static void
putnamesp(nsptr, fout)
	struct namespace *nsptr;
	register FILE *fout;
/*
 * Output namespaces (name:value ...);  namespaces are linked together by nsptr
 */
{
	for(; nsptr; nsptr = nsptr->next) {
	    register struct nameval *nvptr;
	    aprintf(fout, "%s %s", Knamespace, nsptr->namesp);

	    for (nvptr = nsptr->nvhead; nvptr; nvptr = nvptr->next) {
		aprintf(fout, "\n\t%s:", nvptr->name);
		if (nvptr->val) {
		    struct cbuf cb;
		    cb.string = nvptr->val;
		    cb.size = strlen(cb.string);
		    putstring(fout, true, cb, false);
		}
	    }
	    aputs(";\n", fout);
	}
}

	void
putstring(out, delim, s, log)
	register FILE *out;
	struct cbuf s;
	int delim, log;
/*
 * Output to OUT one SDELIM if DELIM, then the string S with SDELIMs doubled.
 * If LOG is set then S is a log string; append a newline if S is nonempty.
 */
{
	register char const *sp;
	register size_t ss;

	if (delim)
		aputc_(SDELIM, out)
	sp = s.string;
	for (ss = s.size;  ss;  --ss) {
		if (*sp == SDELIM)
			aputc_(SDELIM, out)
		aputc_(*sp++, out)
	}
	if (s.size && log)
		aputc_('\n', out)
	aputc_(SDELIM, out)
}

	void
putdftext(delta, finfile, foutfile, nbytes)
	struct hshentry const *delta;
	RILE *finfile;
	FILE *foutfile;
	int nbytes;
/* like putdtext(), except the source file is already open */
/* also, nbytes == -1 means check diff fmt (read to EOF, or use 'a' byte counts)
 *       nbytes ==  0 means just copy to EOF
 *       nbytes >   0 means copy nbytes (not to EOF)
 */
{
	declarecache;
	register FILE *fout;
	register int c;
	register RILE *fin;
	register unsigned long l;
	int ed;
	struct diffcmd dc;

	fout = foutfile;
	aprintf(fout, DELNUMFORM, delta->num, Klog);

	/* put log */
	putstring(fout, true, delta->log, true);
	aputc_('\n', fout)

	/* put ignored phrases */
	awrite(delta->igtext.string, delta->igtext.size, fout);

	/* put text */
	aprintf(fout, "%s\n%c", Ktext, SDELIM);

	fin = finfile;
	setupcache(fin);
	if (!nbytes) {
	    /* Copy the file */
	    cache(fin);
	    for (;;) {
		cachegeteof_(c, break;)
		if (c==SDELIM) aputc_(SDELIM, fout) /*double up SDELIM*/
		aputc_(c, fout)
	    }
	    uncache(fin);
	}
	else if (nbytes > 0) {
	    cache(fin);
	    for (l = nbytes; l--;) {
		cachegeteof_(c, { unexpected_EOF(); })
		if (c==SDELIM) aputc_(SDELIM, fout) /* double up SDELIM*/
		aputc_(c, fout)
	    }
	    uncache(fin);
	}
	else {
	    initdiffcmd(&dc);
	    while (0  <=  (ed = getdiffcmd(fin, false, fout, &dc)))
		if (ed) {
		    cache(fin);		/* get local cache in sync w/fin */
		    if (l = dc.nbytes) for (l = dc.nbytes; l--;) 
		    {
			cachegeteof_(c, { unexpected_EOF(); })
			if (c == SDELIM)
			    aputc_(SDELIM, fout)
			aputc_(c, fout)
		    }
		    else while (dc.nlines--)
			do {
			    cachegeteof_(c, { if (!dc.nlines) goto OK_EOF; unexpected_EOF(); })
			    if (c == SDELIM)
				aputc_(SDELIM, fout)
			    aputc_(c, fout)
			} while (c != '\n');
		    uncache(fin);
		}
	    
	}
    OK_EOF:
	aprintf(fout, "%c\n", SDELIM);
}
