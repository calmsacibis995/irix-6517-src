/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident  "$Revision: 2.13 $"

/*******************************************************************

		PROPRIETARY NOTICE (Combined)

This source code is unpublished proprietary information
constituting, or derived under license from AT&T's UNIX(r) System V.
In addition, portions of such source code were derived from Berkeley
4.3 BSD under license from the Regents of the University of
California.

		Copyright Notice 

Notice of copyright on this source code product does not indicate 
publication.

	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
	          All rights reserved.
********************************************************************/ 

/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley Software License Agreement
 * specifies the terms and conditions for redistribution.
 */

/*
 * Tenex style file name recognition, .. and more.
 * History:
 *	Author: Ken Greer, Sept. 1975, CMU.
 *	Finally got around to adding to the Cshell., Ken Greer, Dec. 1981.
 */

#include <sys/types.h>
#include <widec.h>
#include <pwd.h>
#include <unistd.h>
#include <pfmt.h>
#include "sh.h"
#include "sh.wconst.h"

#define TRUE	1
#define FALSE	0
#define ON	1
#define OFF	0

#define ESC	'\033'
#define CNTL_D	'\004'

static char *BELL = "\07";

typedef enum {LIST, RECOGNIZE} COMMAND;

static jmp_buf osetexit;		/* saved setexit() state */
static struct termio  tty_save;		/* saved terminal state */
static struct termio  tty_new;		/* new terminal state */

static void	catn(wchar_t *, wchar_t *, int);
static void	copyn(wchar_t *, wchar_t *, int);
static wchar_t	*getentry(wchar_t *, DIR *, int, int *);
static bool	ignored(wchar_t *);
static int	is_prefix(wchar_t *, wchar_t *);
static int	recognize(wchar_t *, wchar_t *, int, int);
static wchar_t	*tilde(wchar_t *, wchar_t *);

extern int	tgetent(char *, char *);
extern char	*tgetstr(char *, char **);

static void
update_tty(struct termio *tp)
{
	struct termio tty_cur;

#ifdef TRACE
	tprintf("TRACE- update_tty()\n");
#endif
	(void) ioctl(SHIN, TCGETA,  (char *)&tty_cur);
	if (tty_cur.c_lflag & FLUSHO)
		tp->c_lflag |= FLUSHO;
	else
		tp->c_lflag &= ~FLUSHO;
}


static int
termchars(void)
{
	 char bp[1024];
	static char area[256];
	static int been_here = 0;
	 char *ap = area;
	register char *s;
	 char *term;

#ifdef TRACE
	tprintf("TRACE- termchars()\n");
#endif
	if (been_here)
		return;
	been_here = TRUE;

	if ((term = getenv("TERM")) == NULL)
		return;
	if (tgetent(bp, term) != 1)
		return;
	if (s = tgetstr("vb", &ap))		/* Visible Bell */
		BELL = s;
}

/*
 * Move back to beginning of current line
 */
static int
back_to_col_1(void)
{
	int omask;

#ifdef TRACE
	tprintf("TRACE- back_to_col_1()\n");
#endif
	omask = sigblock(sigmask(SIGINT));
	(void) write(SHOUT, "\r", 1);
	(void) sigsetmask(omask);
}

/*
 * Push string contents back into tty queue
 */
static int
pushback(wchar_t *string, int echoflag)
{
	register wchar_t *p;
	register int n, i;
	int omask;
	struct termio tty;
	char chbuf[MB_LEN_MAX];

#ifdef TRACE
	tprintf("TRACE- pushback()\n");
#endif
	omask = sigblock(sigmask(SIGINT));
	tty = tty_new;
	if( !echoflag)
	    tty.c_lflag &= ~ECHO;
	(void)ioctl(SHIN, TCSETAF, (char *)&tty);

	for(p = string; *p; p++) {
	    n = wctomb(chbuf, *p);
	    if(n < 0)
		continue;			/* error, but what else ? */
/*DDDD
	    for(i = n; i >= 0; i--)
*/
	    for(i = 0; i < n; i++)
		(void)ioctl(SHIN, TIOCSTI, chbuf + i);
	}
	if(tty.c_lflag != tty_new.c_lflag)
	    (void)ioctl(SHIN, TCSETA, (char *)&tty_new);
	(void)sigsetmask(omask);
}

/*
 * Concatenate src onto tail of des.
 * Des is a string whose maximum length is count.
 * Always null terminate.
 */
static void
catn(wchar_t *des, wchar_t *src, int count)
{
#ifdef TRACE
	tprintf("TRACE- catn()\n");
#endif
	while(--count >= 0 && *des)
	    des++;
	while(--count >= 0)
	    if((*des++ = *src++) == '\0')
		return;
	*des = '\0';
}

static int
max(int a, int b)
{
	return (a > b ? a : b);
}

/*
 * Like strncpy but always leave room for trailing \0
 * and always null terminate.
 */
static void
copyn(wchar_t *des, wchar_t *src, int count)
{
#ifdef TRACE
	tprintf("TRACE- copyn()\n");
#endif
	while(--count >= 0)
	    if((*des++ = *src++) == '\0')
		return;
	*des = '\0';
}

/*
 * For qsort()
 */
static int
fcompare(const void *file1, const void *file2)
{
#ifdef TRACE
	tprintf("TRACE- fcompare()\n");
#endif
	return(wscmp(*((wchar_t**)file1), *((wchar_t**)file2)));
}

static wchar_t
filetype(wchar_t *dir, wchar_t *file, int nosym)
{
	struct stat statb;
	wchar_t path[MAXPATHLEN + 1];

#ifdef TRACE
	tprintf("TRACE- filetype()\n");
#endif
	if(dir) {
		catn(wscpy(path, dir), file, MAXPATHLEN);
		if (nosym) {
			if (stat_(path, &statb) < 0)
				return (' ');
		} else {
			if (lstat_(path, &statb) < 0)
				return (' ');
		}
		switch(statb.st_mode & S_IFMT) {
		    case S_IFDIR:
			return ('/');

		    case S_IFLNK:
			if (stat_(path, &statb) == 0 &&	    /* follow it out */
			   (statb.st_mode & S_IFMT) == S_IFDIR)
				return ('>');
			else
				return ('@');

		    case S_IFSOCK:
			return ('=');

		    default:
			if (statb.st_mode & 0111)
				return ('*');
		}
	}
	return (' ');
}

/*
 * Print sorted down columns
 */
static int
print_by_column(wchar_t *dir, wchar_t *items[], int count, int lookforcmd)
{
	register int i, rows, r, c, maxwidth = 0, columns;

#ifdef TRACE
	tprintf("TRACE- print_by_column()\n");
#endif
	for (i = 0; i < count; i++)
		maxwidth = max(maxwidth, tswidth(items[i]));

	/* for the file tag and space */
	maxwidth += lookforcmd? 1 : 2;
	columns = max(78 / maxwidth, 1);
	rows = (count + (columns - 1)) / columns;

	for (r = 0; r < rows; r++) {
		for (c = 0; c < columns; c++) {
			i = c * rows + r;
			if (i < count) {
				register int w;

				shprintf("%t", items[i]);
				w = tswidth(items[i]);
				/*
				 * Print filename followed by
				 * '@' or '/' or '*' or ' '
				 */
				if( !lookforcmd) {
				    shprintf("%T", filetype(dir, items[i], 0));
				    w++;
				}
				if (c < columns - 1)	/* last column? */
					for (; w < maxwidth; w++)
						shprintf(" ");
			}
		}
		shprintf("\n");
	}
}

/*
 * Expand file name with possible tilde usage
 *	~person/mumble
 * expands to
 *	home_directory_of_person/mumble
 *
 * ret: NULL if bad name or mbchar
 */
static wchar_t *person;

static wchar_t *
tilde(wchar_t *new, wchar_t *old)
{
	register wchar_t *p;
	register wchar_t *d;
	register char *s;
	register struct passwd *pw;
	int cflag;
	char cperson[CSHBUFSIZ];

	if (person == NULL) {
		person = (wchar_t *)calloc(1, sizeof (wchar_t) * CSHBUFSIZ);
	}

#ifdef TRACE
	tprintf("TRACE- tilde()\n");
#endif
	if(old[0] != '~')
	    return(wscpy(new, old));		/* no ~ */
	/*
	 * scan for path delimiter
	 * XXXX multibyte ?
	 */
	for(p = person, old++; *old && (*old != '/'); *p++ = *old++);
	*p = '\0';
	if(person[0] == '\0')
	    wscpy(new, value(S_home));		/* only ~/mumble */
	else {
	    s = tstostr(cperson, person, &cflag);
	    if(cflag)
		return(NULL);			/* illegal wchar = unknown */
	    if( ! (pw = getpwnam(s)))
		return(NULL);			/* bad login name */
	    d = strtots(NOSTR, pw->pw_dir, &cflag);
	    if(cflag)
		return(NULL);			/* bad chars = unknown user */
	    wscpy(new, d);
	    xfree(d);				/* free it */
	}
	wscat(new, old);			/* add mumble */
	return(new);
}

static int
beep(void)
{

#ifdef TRACE
	tprintf("TRACE- beep()\n");
#endif
	if (adrof(S_nobeep /*"nobeep" */) == 0)
		(void) write(SHOUT, BELL, strlen(BELL));
}

/*
 * Erase that silly ^[ (if necessary) and print the recognized
 * part of the string.
 */
static void
precstuff(wchar_t *recpart)
{
	int unit;

#ifdef TRACE
	tprintf("TRACE- precstuff()\n");
#endif
	unit = didfds? 1 : SHOUT;
	flush();	
	if (!(tty_save.c_lflag & ECHOCTL)) { /* no ^[ was echoed */
		shprintf("%t", recpart);
		flush();	
		return;
	}
	switch(tswidth(recpart)) {
	    /*
	     * erase two characters: ^[
	     */
	    case 0:
		write(unit, "\010\010  \010\010", 6);
		break;
	    /* 
	     * overstrike the ^, erase the [
	     */
	    case 1:
		write(unit, "\010\010", 2);
		shprintf("%t", recpart);
		write(unit, "  \010\010", 4);
		break;
	    /*
	     * overstrike both characters ^[
	     */
	    default:
		write(unit, "\010\010", 2);
		shprintf("%t", recpart);
		break;
	}
	flush();
}

/*
 * Parse full path in file into 2 parts: directory and file names
 * Should leave final slash (/) at end of dir.
 */
static
extract_dir_and_name(wchar_t *path, wchar_t *dir, wchar_t *name)
{
	register wchar_t  *p;

#ifdef TRACE
	tprintf("TRACE- extract_dir_and_name()\n");
#endif
	p = wsrchr(path, '/');
	if (p == NOSTR) {
		copyn(name, path, MAXNAMLEN);
		dir[0] = '\0';
	} else {
		copyn(name, ++p, MAXNAMLEN);
		copyn(dir, path, p - path);
	}
}

static wchar_t *
getentry(wchar_t *wcp, DIR *dir_fd, int looklog, int *cflag)
{
	register struct passwd *pw;
	register struct dirent *dirp;

#ifdef TRACE
	tprintf("TRACE- getentry()\n");
#endif
	if(looklog) {
	    if( !(pw = getpwent()))
		return(NULL);
	    return(strtots(wcp, pw->pw_name, cflag));
	}
	if( !(dirp = readdir(dir_fd)))
	    return(NULL);
	strtots(wcp, dirp->d_name, cflag);
	if(*cflag)
	    err_fntruncated(dirp->d_name);
	return(wcp);
}

static int
free_items(wchar_t **items)
{
	register int i;

#ifdef TRACE
	tprintf("TRACE- free_items()\n");
#endif
	for(i = 0; items[i]; i++)
	    free(items[i]);
	free((char *)items);
}

#define FREE_ITEMS(items) { \
	int omask;\
\
	omask = sigblock(sigmask(SIGINT));\
	free_items(items);\
	items = NULL;\
	(void) sigsetmask(omask);\
}

/*
 * Perform a RECOGNIZE or LIST command on string "word".
 */
static wchar_t **items = NULL;

static int
search2(wchar_t *word, COMMAND command, int max_word_length)
{
	register DIR *dir_fd;
	register nitems = 0, ignoring = TRUE, nignored = 0;
	register name_length, looklog;
	wchar_t *entry;
	int cflag;
	wchar_t tilded_dir[MAXPATHLEN + 1];
	wchar_t dir[MAXPATHLEN + 1];
	wchar_t name[MAXNAMLEN + 1];
	wchar_t extndname[MAXNAMLEN + 1];
	wchar_t ename[MAXNAMLEN + 1];

#define MAXITEMS 1024

#ifdef TRACE
	tprintf("TRACE- search2()\n");
#endif

	if(items)
	    FREE_ITEMS(items);

	looklog = (*word == '~') && !wschr(word, '/');
	if(looklog) {
	    (void)setpwent();
	    copyn(name, &word[1], MAXNAMLEN);		/* name sans ~ */
	} else {
	    extract_dir_and_name(word, dir, name);
	    if( !tilde(tilded_dir, dir))
		return(0);
	    dir_fd = opendir_(*tilded_dir ? tilded_dir : S_DOT);
	    if( !dir_fd)
		return(0);
	}
	/*
	 * search loop
	 */
again:
	name_length = wslen(name);
	for(nitems = 0; entry = getentry(ename, dir_fd, looklog, &cflag); ) {
	    if(cflag && looklog)
		continue;			/* ignore wrong logname */
	    if( !is_prefix(name, entry))
		continue;
	    /*
	     * Don't match . files on null prefix match
	     */
	    if( !name_length && (entry[0] == '.') && !looklog)
		continue;
	    if(command == LIST) {
		if(nitems >= MAXITEMS - 1) {
		    showstr(MM_ERROR,
			looklog?
			    gettxt(_SGI_DMMX_csh_2manynames,
				"\nYikes! Too many names in passwd!")
			:
			    gettxt(_SGI_DMMX_csh_2manyfiles,
				"\nToo many files: not all will be listed"), 0);
		    break;
		}
		if( !items)
		    items = (wchar_t **)salloc(MAXITEMS, sizeof(wchar_t **));
		items[nitems] = wcalloc(wslen(entry) + 1);
		copyn(items[nitems], entry, MAXNAMLEN);
		nitems++;
	    } else {			/* RECOGNIZE command */
		if(ignoring && ignored(entry))
		    nignored++;
		else {
		    if(recognize(extndname, entry, name_length, ++nitems))
			break;
		}
	    }
	}
	if(ignoring && !nitems && (nignored > 0)) {
	    ignoring = FALSE;
	    nignored = 0;
	    if(looklog)
		(void)setpwent();
	    else
		rewinddir(dir_fd);
	    goto again;
	}
	if(looklog)
	    (void) endpwent();			/* close passwd database */
	else
	    closedir(dir_fd);

	if(command == RECOGNIZE && nitems > 0) {
	    if(looklog)
		 copyn(word, S_TIL, 1);
	    else
		copyn(word, dir, max_word_length);	/* put back dir part */
	    catn(word, extndname, max_word_length);	/* add extended name */
	    return(nitems);
	}
	if(command == LIST) {
	    qsort( (wchar_t *)items, nitems, sizeof(items[1]), fcompare);
	    /*
	     * Never looking for commands in this version, so final
	     * argument forced to 0.  If command name completion is
	     * reinstated, this must change.
	     */
	    print_by_column(looklog ? NULL : tilded_dir,
		items, nitems, 0);
	    if(items)
		FREE_ITEMS(items);
	}
	return(0);
}

/*
 * Object: extend what user typed up to an ambiguity.
 * Algorithm:
 * On first match, copy full entry (assume it'll be the only match) 
 * On subsequent matches, shorten extndname to the first
 * character mismatch between extndname and entry.
 * If we shorten it back to the prefix length, stop searching.
 */
static int
recognize(wchar_t *extndname, wchar_t *entry, int name_length, int nitems)
{

#ifdef TRACE
	tprintf("TRACE- recognize()\n");
#endif
	if (nitems == 1)				/* 1st match */
		copyn(extndname, entry, MAXNAMLEN);
	else {					/* 2nd and subsequent matches */
		register wchar_t *x, *ent;
		register int len = 0;

		x = extndname;
		for (ent = entry; *x && *x == *ent++; x++, len++)
			;
		*x = '\0';			/* Shorten at 1st char diff */
		if (len == name_length)		/* Ambiguous to prefix? */
			return (-1);		/* So stop now and save time */
	}
	return (0);
}

/*
 * Return true if check items initial chars in template
 * This differs from PWB imatch in that if check is null
 * it items anything
 */
static int
is_prefix(wchar_t *check, wchar_t *template)
{
#ifdef TRACE
	tprintf("TRACE- is_prefix()\n");
#endif

	do {
		if (*check == 0)
			return (TRUE);
	} while (*check++ == *template++);
	return (FALSE);
}

/*
 *  Return true if the chars in template appear at the
 *  end of check, i.e., are its suffix.
 */
static int
is_suffix(wchar_t *check, wchar_t *template)
{
	register wchar_t *c, *t;

#ifdef TRACE
	tprintf("TRACE- is_suffix()\n");
#endif
	for(c = check; *c++;);
	for(t = template; *t++;);
	for(;;) {
	    if(t == template)
		return(TRUE);
	    if(c == check || *--t != *--c)
		return(FALSE);
	}
}

int
tenex(wchar_t *iline, int ilsize)
{
	register int nitems, nread, should_retype;
	int i;
	int omask;

#ifdef TRACE
	tprintf("TRACE- tenex()\n");
#endif

	/*
	 * START of tty initialization block that was setup_tty(ON)
	 *
	 * The shell makes sure that the tty is not in some weird state
	 * and fixes it if it is.  But it should be noted that the
	 * tenex routine will not work correctly in CBREAK or RAW mode
	 * so this code below is, therefore, mandatory.
	 *
	 * have to do this all inline, or we violate requirements
	 * for setjmp, since we would longjmp back into a function
	 * that has already returned...  We've been lucky about this
	 * in the past, but got bit by it in ficus...
	 *
	 * Also, in order to recognize the ESC (filename-completion)
	 * character, set EOL to ESC.  This way, ESC will terminate
	 * the line, but still be in the input stream.
	 * EOT (filename list) will also terminate the line,
	 * but will not appear in the input stream.
	 */
	omask = sigblock(sigmask(SIGINT));
	(void) ioctl(SHIN, TCGETA,  (char *)&tty_save);
	getexit(osetexit);
	if (setjmp(reslab)) {
		update_tty(&tty_save);
		(void) ioctl(SHIN, TCSETAW,  (char *)&tty_save);
		resexit(osetexit);
		reset();
	}
	else {
		tty_new = tty_save;
		tty_new.c_cc[VEOL] = ESC;
#define CANONMODE       (ICANON|ECHO)
		if ((tty_new.c_lflag & CANONMODE) != CANONMODE)
			tty_new.c_lflag |= CANONMODE;
#undef  CANONMODE
		(void) ioctl(SHIN, TCSETAW,  (char *)&tty_new);
	}
	(void) sigsetmask(omask);
	/* END of tty initialization block that was setup_tty(ON) */

	termchars();
	nread = 0;
	should_retype = FALSE;

	while((i = read_(SHIN, iline + nread, ilsize - nread)) >= 0) {
	    register wchar_t *str_end, *word_start, lastc;
	    register int space_left;
	    struct termio tty;
	    COMMAND command;

	    nread += i;
	    iline[nread] = '\0';		/* terminate string */
	    lastc = iline[nread - 1] & TRIM;	/* last char */

	    /*  Check if nread is 0. If so, then lastc is meaningless as we
		may be comparing against garbage which may cause unpredictable
		behaviour. Moved the check for nread to the top. This should be
		done first.
	    */

	    if( !nread) {
		errno = 0;			/* but no error */
		break;
	    }

	    if((nread == ilsize) || (lastc == '\n'))
		break;

	    str_end = iline + nread;
	    if(lastc == ESC) {
		command = RECOGNIZE;
		*--str_end = '\0';		/* wipe out trailing ESC */
	    } else
		command = LIST;

	    tty = tty_new;			/* set tty */
	    tty.c_lflag &= ~ECHO;
	    (void) ioctl(SHIN, TCSETAF,  (char *)&tty);

	    if(command == LIST)
		shprintf("\n");

	    /*
	     * Find LAST occurence of a delimiter in the iline.
	     * The word start is one character past it.
	     */
	    for(word_start = str_end; word_start > iline; --word_start) {
		if(wschr(S_DELIM, word_start[-1]) || isauxsp(word_start[-1]))
		    break;
	    }
	    space_left = ilsize - (word_start - iline) - 1;
	    nitems = search2(word_start, command, space_left);

	    if(command == RECOGNIZE) {
		precstuff(str_end);		/* print from str_end on */
		if(nitems != 1)			/* Beep = No match/ambiguous */
		    beep();
	    }

	    /*
	     * Tabs in the input line cause trouble after a pushback.
	     * tty driver won't backspace over them because column
	     * positions are now incorrect. This is solved by retyping
	     * over current line.
	     */
	    if(wschr(iline, '\t')) {		/* tab in input line? */
		back_to_col_1();
		should_retype = TRUE;
	    }
	    if(command == LIST)			/* Always retype after a LIST */
		should_retype = TRUE;
	    if(should_retype)
		printprompt();
	    pushback(iline, should_retype);
	    nread = 0;				/* chars will be reread */
	    should_retype = FALSE;
	}

	/* START of tty initialization block that was setup_tty(OFF) */
	/* Reset terminal state to what user had when invoked */
	omask = sigblock(sigmask(SIGINT));
	update_tty(&tty_save);
	(void) ioctl(SHIN, TCSETAW,  (char *)&tty_save);
	resexit(osetexit);
	(void) sigsetmask(omask);
	/* END of tty initialization block that was setup_tty(OFF) */

	return(nread? nread : -1);
}

static bool
ignored(wchar_t *entry)
{
	struct varent *vp;
	register wchar_t **cp;

#ifdef TRACE
	tprintf("TRACE- ignored()\n");
#endif
	if( !(vp = adrof(S_fignore)) || !(cp = vp->vec))
	    return(FALSE);
	for(; *cp; cp++) {
	    if(is_suffix(entry, *cp))
		return(TRUE);
	}
	return(FALSE);
}
