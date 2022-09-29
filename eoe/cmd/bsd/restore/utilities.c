#ident "$Revision: 1.11 $"

/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 * static char sccsid[] = "@(#)utilities.c	5.2 (Berkeley) 8/5/85";
 */

#include "restore.h"

/*
 * Insure that all the components of a pathname exist.
 */
void
pathcheck(char *name)
{
	char *cp;
	struct entry *ep;
	char *start;

	start = index(name, '/');
	if (start == 0)
		return;
	for (cp = start; *cp != '\0'; cp++) {
		if (*cp != '/')
			continue;
		*cp = '\0';
		ep = lookupname(name);
		if (ep == NIL) {
			ep = addentry(name, psearch(name), NODE);
			newnode(ep);
		}
		ep->e_flags |= NEW|KEEP;
		*cp = '/';
	}
}

/*
 * Change a name to a unique temporary name.
 */
void
mktempname(struct entry *ep)
{
	char oldname[MAXPATHLEN];

	if (ep->e_flags & TMPNAME)
		badentry(ep, "mktempname: called with TMPNAME");
	ep->e_flags |= TMPNAME;
	(void) strcpy(oldname, myname(ep));
	freename(ep->e_name);
	ep->e_name = savename(gentempname(ep));
	ep->e_namlen = strlen(ep->e_name);
	renameit(oldname, myname(ep));
}

/*
 * Generate a temporary name for an entry.
 */
char *
gentempname(struct entry *ep)
{
	static char name[MAXPATHLEN];
	struct entry *np;
	long i = 0;

	for (np = lookupino(ep->e_ino); np != NIL && np != ep; np = np->e_links)
		i++;
	if (np == NIL)
		badentry(ep, "not on ino list");
	(void) sprintf(name, "%s%d%d", TMPHDR, i, ep->e_ino);
	return (name);
}

/*
 * Rename a file or directory.
 */
void
renameit(char *from, char *to)
{
	if (!Nflag && rename(from, to) < 0) {
		fprintf(stderr, "Warning: cannot rename %s to ", from);
		(void) fflush(stderr);
		perror(to);
		return;
	}
	Vprintf(stdout, "rename %s to %s\n", from, to);
}

/*
 * Create a new node (directory).
 * If something exists in the place of the directory we are
 * creating and neither of the -e or -E flags were set,
 * try to remove it.
 */
void
newnode(struct entry *np)
{
	char *cp;
	int tryagain = 1;

	if (np->e_type != NODE)
		badentry(np, "newnode: not a node");
	cp = myname(np);
retry:
	if (!Nflag && mkdir(cp, 0777) < 0) {
		struct stat64 st;
		/* check if it is already a dir, as it is unfriendly to simply
		 * unlink a directory!  BSD net-2 release never does the unlink...
		 */
		if(errno == EEXIST && (justcreate||newest|| 
			(lstat64(cp, &st) == 0 && S_ISDIR(st.st_mode)))) {
			np->e_flags |= EXISTED;
			return;
		}
		if (tryagain) {
			tryagain = 0;
			(void) unlink(cp);
			goto retry;
		}
		np->e_flags |= EXISTED;
		fprintf(stderr, "Warning: ");
		(void) fflush(stderr);
		perror(cp);
		return;
	}
	if (justcreate || newest) {
		Vprintf(stdout, "Make directory %s (old ino %lu)\n", cp, np->e_ino);
		np->e_flags |= EXTRACTED;
	} else
		Vprintf(stdout, "Make directory %s\n", cp);
}

/*
 * Remove an old node (directory).
 */
void
removenode(struct entry *ep)
{
	char *cp;

	if (ep->e_type != NODE)
		badentry(ep, "removenode: not a node");
	if (ep->e_entries != NIL)
		badentry(ep, "removenode: non-empty directory");
	ep->e_flags |= REMOVED;
	ep->e_flags &= ~TMPNAME;
	cp = myname(ep);
	if (!Nflag && rmdir(cp) < 0) {
		fprintf(stderr, "Warning: ");
		(void) fflush(stderr);
		perror(cp);
		return;
	}
	Vprintf(stdout, "Remove node %s\n", cp);
}

/*
 * Remove a leaf.
 */
void
removeleaf(struct entry *ep)
{
	char *cp;

	if (ep->e_type != LEAF)
		badentry(ep, "removeleaf: not a leaf");
	ep->e_flags |= REMOVED;
	ep->e_flags &= ~TMPNAME;
	cp = myname(ep);
	if (!Nflag && unlink(cp) < 0) {
		fprintf(stderr, "Warning: ");
		(void) fflush(stderr);
		perror(cp);
		return;
	}
	Vprintf(stdout, "Remove leaf %s\n", cp);
}

/*
 * Create a link.
 */
int
linkit(char *existing, char *new, int type)
{
	int tryagain = 1;

retry:
	if (type == SYMLINK) {
		if (!Nflag && symlink(existing, new) < 0) {
			struct stat64 st;
			/* check if it is already a dir, as it is unfriendly to simply
			 * unlink a directory!  BSD net-2 release never does the unlink...
			 */
			if(errno == EEXIST && (justcreate||newest|| 
				(stat64(new, &st) == 0 && S_ISDIR(st.st_mode)))) {
				return FAIL;
			}
			if (tryagain) {
				tryagain = 0;
				(void) unlink(new);
				goto retry;
			}
			fprintf(stderr,
				"Warning: cannot create symbolic link %s->%s: ",
				new, existing);
			(void) fflush(stderr);
			perror("");
			return (FAIL);
		}
	} else if (type == HARDLINK) {
		if (!Nflag && link(existing, new) < 0) {
			/*
			 * Yuck! If "new" is a dangling symlink pointer then
			 * errno returned is ENOENT. The code below would
			 * blow away the existing "new" symlink, even if
			 * (justcreate||newest) is set.
			 *	- XXX
			 */
			if (errno == EEXIST && (justcreate||newest))
				return FAIL;
			if (tryagain) {
				tryagain = 0;
				(void) unlink(new);
				goto retry;
			}
			fprintf(stderr,
				"Warning: cannot create hard link %s->%s: ",
				new, existing);
			(void) fflush(stderr);
			perror("");
			return (FAIL);
		}
	} else {
		panic("linkit: unknown type %d\n", type);
		return (FAIL);
	}
	Vprintf(stdout, "Create %s link %s->%s\n",
		type == SYMLINK ? "symbolic" : "hard", new, existing);
	return (GOOD);
}

/*
 * find lowest number file (above "start") that needs to be extracted
 */
efs_ino_t
lowerbnd(efs_ino_t start)
{
	struct entry *ep;

	for ( ; start < maxino; start++) {
		ep = lookupino(start);
		if (ep == NIL || ep->e_type == NODE)
			continue;
		if (ep->e_flags & (NEW|EXTRACT))
			return (start);
	}
	return (start);
}

/*
 * find highest number file (below "start") that needs to be extracted
 */
efs_ino_t
upperbnd(efs_ino_t start)
{
	struct entry *ep;

	for ( ; start > ROOTINO; start--) {
		ep = lookupino(start);
		if (ep == NIL || ep->e_type == NODE)
			continue;
		if (ep->e_flags & (NEW|EXTRACT))
			return (start);
	}
	return (start);
}

/*
 * report on a badly formed entry
 */
void
badentry(struct entry *ep, char *msg)
{

	fprintf(stderr, "bad entry: %s\n", msg);
	fprintf(stderr, "name: %s\n", myname(ep));
	fprintf(stderr, "parent name %s\n", myname(ep->e_parent));
	if (ep->e_sibling != NIL)
		fprintf(stderr, "sibling name: %s\n", myname(ep->e_sibling));
	if (ep->e_entries != NIL)
		fprintf(stderr, "next entry name: %s\n", myname(ep->e_entries));
	if (ep->e_links != NIL)
		fprintf(stderr, "next link name: %s\n", myname(ep->e_links));
	if (ep->e_next != NIL)
		fprintf(stderr, "next hashchain name: %s\n", myname(ep->e_next));
	fprintf(stderr, "entry type: %s\n",
		ep->e_type == NODE ? "NODE" : "LEAF");
	fprintf(stderr, "inode number: %ld\n", ep->e_ino);
	panic("flags: %s (0x%x)\n", flagvalues(ep), ep->e_flags);
}

/*
 * Construct a string indicating the active flag bits of an entry.
 */
char *
flagvalues(struct entry *ep)
{
	static char flagbuf[BUFSIZ];

	(void) strcpy(flagbuf, "|NIL");
	flagbuf[0] = '\0';
	if (ep->e_flags & REMOVED)
		(void) strcat(flagbuf, "|REMOVED");
	if (ep->e_flags & TMPNAME)
		(void) strcat(flagbuf, "|TMPNAME");
	if (ep->e_flags & EXTRACT)
		(void) strcat(flagbuf, "|EXTRACT");
	if (ep->e_flags & NEW)
		(void) strcat(flagbuf, "|NEW");
	if (ep->e_flags & KEEP)
		(void) strcat(flagbuf, "|KEEP");
	if (ep->e_flags & EXISTED)
		(void) strcat(flagbuf, "|EXISTED");
	if (ep->e_flags & EXTRACTED)
		(void) strcat(flagbuf, "|EXTRACTED");
	return (&flagbuf[1]);
}

/*
 * Check to see if a name is on a dump tape.
 */
efs_ino_t
dirlookup(char *name)
{
	efs_ino_t ino;

	ino = psearch(name);
	if (ino == 0 || BIT(ino, dumpmap) == 0)
		fprintf(stderr, "%s is not on tape\n", name);
	return (ino);
}

/*
 * Elicit a reply.
 */
int
reply(char *question)
{
	char c;

	do	{
		fprintf(stderr, "%s? [yn] ", question);
		(void) fflush(stderr);
		c = getc(terminal);
		while (c != '\n' && getc(terminal) != '\n')
			if (feof(terminal) || ferror(terminal))
				return (FAIL);
	} while (c != 'y' && c != 'n');
	if (c == 'y')
		return (GOOD);
	return (FAIL);
}

/*
 * handle unexpected inconsistencies
 */
void
panic(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	if (reply("abort") == GOOD) {
		if (reply("dump core") == GOOD)
			abort();
		done(1);
	}
}
