#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <fcntl.h>
#include <ctype.h>

/*
 * check [-l | -d] [rcsdir]:
 *	- print out in a nice format, all the files that a person
 *	  has checked out in the RCS directory
 *	- if the "-l" flag is on, just print the real name of the
 *	  file (used the same as egrep -l is used)
 *	- if the "-d" flag is on, also print the date when the file was 
 *	  checked out.
 *
 * Written by: Kipp Hickman
 */

char	filename[2000];			/* current file name */
char	line[2000];			/* rcs file line being parsed */
char	owner[50];			/* owner of file co'd */
char	revision[20];			/* revision of file co'd */
short	lflag;				/* like egrep -l, print just names */
short	dflag;				/* print the date when the file was
					 * checked out */ 

char	*rcsdir;			/* place to look in */

#define	RCSDIR_DEFAULT	"RCS"

char	*realname();
int	readrcs(char *, struct stat *);

extern char *ctime();

main(argc, argv)
	int argc;
	char *argv[];
{
	register struct dirent *np;
	register short i;
	register char *truename;
	register DIR *d;
	int found;
	struct stat statBuf;

	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
			  case 'l':
				lflag++;
				break;
			  case 'd':
				dflag++;
				break;
			  default:
				goto usage;
			}
		} else {
			if (rcsdir == NULL)
				rcsdir = argv[i];
			else {
usage:				fprintf(stderr, "Usage:\n%s [-dl] [rcsdir]\n",
						argv[0]);
				exit(2);
			}
		}
	}

	if (rcsdir == NULL) {
		rcsdir = RCSDIR_DEFAULT;
		if (!exists(rcsdir)) {
			rcsdir = ".";
			if (!exists(rcsdir))
				goto no_rcsdir;
		}
	} else {
		if (!exists(rcsdir))
			goto no_rcsdir;
	}

    /* read in the contents of the directory */

	if ((d = opendir(rcsdir)) == NULL) {
no_rcsdir:
		fprintf(stderr, "%s: unable to open \"%s\"\n",
				argv[0], rcsdir);
		exit(2);
	}

	found = 0;
	while (np = readdir(d)) {
		sprintf(filename, "%s/%s", rcsdir, np->d_name);
		if (readrcs (filename, &statBuf)) {
			truename = realname(filename);
			if (lflag)
				printf("%s\n", truename);
			else if (dflag)
				printf("%-20s\t%s\t%s    %s",
					    truename, owner, revision,
					    ctime(&statBuf.st_mtime));
			else
				printf("%-20s\t%s revision %s\n",
					    truename, owner, revision);
			found++;
		}
	}
	if (found)
		exit(1);
	else
		exit(0);
}

/*
 * exists:
 *	- see if "name" exists (is stat'able)
 */
exists(name)
	char *name;
{
	struct stat sbuf;

	if (stat(name, &sbuf) == -1)
		return 0;
	if ((sbuf.st_mode & S_IFMT) == S_IFDIR)
		return 1;
	return 0;
}

/*
 * realname:
 *	- given an rcs name, rip its lips off, and leave just the
 *	  actual file name (no path in front, no ,v in back)
 */
char *
realname(s)
	char *s;
{
	register char *cp;

	if (cp = strrchr(s, '/'))
		s = cp + 1;
	if (cp = strrchr(s, ','))
		*cp = 0;
	return s;
}

static char *fname;
static char *end_rcs_header;

static char * skipblanks(cp)
char *cp;
{
	while (isspace(*cp)) cp++;
	return cp;
}

char *strndup (const char *s1, const size_t sz)
{
	char * s2;

	if ((s2 = malloc((unsigned) sz+1)) == NULL) {
		fprintf(stderr,"check: no memory\n");
		exit(2);
	}
	strncpy (s2, s1, sz);
	s2[sz] = '\0';
	return s2;
}

void
warn (char *s, char *f){
#if 0
	fprintf(stderr,"check warning: %s: %s\n", f, s);
#endif
}
extern char *strndup (const char *s1, const size_t sz);

static check (char *p, char *msg)
{
	if (p == NULL || p > end_rcs_header)
		{ warn (msg, fname); return -1; }
	return 0;
}

int readrcs (char *f, struct stat *statPtr)
{
	int fd = -1;
	char *owner_str, *revision_str;
	char *buf = (char *)(-1), *p, *q;
	char *lockp;

	fname = f;
	if ((fd = open (fname, O_RDONLY)) < 0)
		{ warn ("open:", fname); return 0; }
	if (fstat (fd, statPtr) < 0)
		{ warn ("stat:", fname); return 0; }
	if (statPtr->st_size == 0)
		{ warn ("zero length file:", fname); return 0; }
	buf = (char *)mmap((void *)0, statPtr->st_size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (buf == (char *)(-1))
		{ warn ("mmap:", fname); return 0; }

	if (strncmp (buf, "head", sizeof("head")-1) != 0)
		{ warn ("missing head:", fname); return 0; }

	/*
	 * The strstr calls presume that there is at least one nul character
	 * following all the valid data.  The mmap gives us this, except in
	 * the case that the mapped file ends exactly on a page boundary.
	 * In that case we overwrite the last character in the file with a nul.
	 *
	 * This slight disregard for the file's data is actually user visible:
	 * Truncate an RCS ,v file so that it ends with "\ndesc\n".  If the file
	 * is any size other than a page multiple, this code will think the header
	 * is fine, ignore the truncation, and find what it wants.  If the file
	 * happens to end up an exact page multiple, this code will write a nul
	 * over the final "\n", then be unable to find end_rcs_header, and complain
	 * that the file has "no desc line".  Who cares ...
	 */
	if ( (statPtr->st_size % getpagesize()) == 0 )
		buf[statPtr->st_size-1] = '\0';

	if ((end_rcs_header = strstr (buf, "\ndesc\n")) == NULL)
		{ warn ("no desc line:", fname); return 0; }

	p = buf + sizeof("head")-1;
	p = skipblanks (p);		{ if (check (p, "invalid head line:")) return 0; }
	q = strchr (p, ';');		{ if (check (q, "invalid head format:")) return 0; }

	p = strstr (p, "\nlocks");	{ if (check (p, "no locks:")) return 0; }
	p += sizeof("\nlocks")-1;
	p = skipblanks (p);		{ if (check (p, "invalid locks:")) return 0; }
	q = strchr (p, ';');		{ if (check (q, "invalid locks format:")) return 0; }
	lockp = strndup (p, q - p);
	if (buf != (char *)(-1)) {
		if (munmap (buf, statPtr->st_size) < 0) {
			fprintf (stderr,"check: munmap failed\n");
			exit(2);
		}
	}
	if (fd >= 0)
		close (fd);

	owner[0] = 0;
	revision[0] = 0;

	owner_str = strtok (lockp, ":");
	revision_str = strtok (NULL, ";\n\t");

	if (owner_str == NULL || revision_str == NULL)
	    return 0;

	strncpy (owner, owner_str, sizeof(owner));
	strncpy (revision, revision_str, sizeof(revision));
	owner[sizeof(owner)-1] = 0;
	revision[sizeof(revision)-1] = 0;

	return 1;
}
