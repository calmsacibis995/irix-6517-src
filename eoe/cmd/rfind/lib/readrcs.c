#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <fcntl.h>
#include <ctype.h>
#include "externs.h"

static char *filename;
static char *end_rcs_header;

static char * skipblanks(char *cp)
{
	while (isspace(*cp)) cp++;
	return cp;
}


static int check (char *p, char *msg) {
	if (p == NULL || p > end_rcs_header)
		{ warn (msg, filename); return -1; }
	return 0;
}

static int pagesize = 0;

void readrcs (char *f, char **revpp, char **lockpp, char **datepp, char **authpp) {
	int fd = -1;
	struct stat64 sbuf;
	char *buf = (char *)MAP_FAILED, *p, *q;
	char *revbuf = NULL, *revp, *lockp, *datep, *authp;
	long revlen;

	if (!pagesize)
		pagesize = getpagesize();

	filename = f;
	if ((fd = open (filename, O_RDONLY)) < 0)
		{ warn ("open:", filename); goto fail1; }
	if (fstat64 (fd, &sbuf) < 0)
		{ warn ("stat64:", filename); goto fail1; }
	if (sbuf.st_size == 0)
		{ warn ("zero length file:", filename); goto fail1; }
	buf = (char *)mmap((void *)0, sbuf.st_size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (buf == (char *)MAP_FAILED)
		{ warn ("mmap:", filename); goto fail1; }

	if (strncmp (buf, "head", sizeof("head")-1) != 0)
		{ warn ("missing head:", filename); goto fail1; }

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
	if ( (sbuf.st_size % pagesize) == 0 )
		buf[sbuf.st_size-1] = '\0';

	if ((end_rcs_header = strstr (buf, "\ndesc\n")) == NULL)
		{ warn ("no desc line:", filename); goto fail1; }

	p = buf + sizeof("head")-1;
	p = skipblanks (p);		{ if (check (p, "invalid head line:")) goto fail1; }
	q = strchr (p, ';');		{ if (check (q, "invalid head format:")) goto fail1; }
	revlen = q - p;
	revp = strndup (p, revlen);

	p = strstr (p, "\nlocks");	{ if (check (p, "no locks:")) goto fail2; }
	p += sizeof("\nlocks")-1;
	p = skipblanks (p);		{ if (check (p, "invalid locks:")) goto fail2; }
	q = strchr (p, ';');		{ if (check (q, "invalid locks format:")) goto fail2; }
	lockp = strndup (p, q - p);

	if ((revbuf = malloc(sizeof("\n") + revlen + sizeof("\ndate"))) == NULL)
		error ("no memory");
	strcpy (revbuf, "\n");
	strcat (revbuf, revp);
	strcat (revbuf, "\ndate");
	p = strstr (p, revbuf);		{ if (check (p, "no rev description:")) goto fail3; }
	p += strlen (revbuf);
	p = skipblanks (p);		{ if (check (p, "invalid rev desc:")) goto fail3; }
	q = strchr (p, ';');		{ if (check (q, "no rev desc colon:")) goto fail3; }
	datep = strndup (p, q - p);

	p = skipblanks (q+1);
	if (strncmp (p, "author", sizeof("author")-1) != 0)
		{ warn ("no desc author", filename); goto fail4; }
	p += sizeof ("author")-1;
	p = skipblanks (p);		{ if (check (p, "no author:")) goto fail4; }
	q = strchr (p, ';');		{ if (check (q, "no author colon:")) goto fail4; }
	authp = strndup (p, q - p);
	goto finish;

fail1:
	revp = strdup("");
fail2:
	lockp = strdup("");
fail3:
	datep = strdup("");
fail4:
	authp = strdup("");
finish:
	*revpp = revp;
	*lockpp = lockp;
	*datepp = datep;
	*authpp = authp;
	if (revbuf != NULL)
		free ((void *)revbuf);
	if (buf != (char *)MAP_FAILED) {
		if (munmap (buf, sbuf.st_size) < 0)
			error ("munmap");
	}
	if (fd >= 0)
		close (fd);
}
