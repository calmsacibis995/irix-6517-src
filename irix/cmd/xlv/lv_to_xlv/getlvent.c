
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

/*
 * functions for parsing /etc/lvtab
 *
 * This module is copied from libc/src/gen/getlvent.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <bstring.h>
#include <ctype.h>
#include <string.h>
#include "sys/lvtab.h"

#include <pathnames.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <diskinfo.h>
#include <sys/dvh.h>

#define LVBUFSIZ 200

#if 0
/* streq() is already defined in <pathnames.h> */
#define streq(x,y) (!strcmp(x,y))
#endif


int
_findtrksize(char *path)
{
	struct volume_header	vh;
	int	vfd;
	int	size;
	char	*vh_pathname;

	/* path must be a fully qualified pathname */

	vh_pathname = pathtovhpath(path);
	vfd = open(vh_pathname, O_RDWR | O_SYNC);

	if ((vfd < 0) || (getdiskheader(vfd, &vh) < 0)) {
		size = 0;
	} else {
		size = vh.vh_dp._dp_sect;
	}
	
	return(size);
}

void
xlv_freelvent(struct lvtabent *tabent)
{
	free(tabent->devname);
	free((caddr_t) tabent);
}


/* getlvent(): gets the next entry from the given file, which is expected
 * to contain entries of appropriate to an lvtab. Blank lines 
 * and lines whose first nonblank character is "#" are ignored.
 * It is assumed that on each call to getlvent, the file is positioned to a 
 * correct boundary: either start of file or the end of the previous entry.
 *
 * Some simple checks are done: the volume name must be < 80 chars;
 * only certain keywords of the form "stripes=" are recognized and must
 * be followed immediately by a numeric parameter (except "devs=" which
 * must be followed by comma-separated device pathnames and must come
 * after any numeric keywords).
 * A bogus entry causes ndevs to be coerced to 0.
 *
 * If a <comment_stream> is provided, any comments found are written
 * out to the stream.
 */

static char *_getlvtabline(FILE *lvtabp, FILE *comment_stream);

struct lvtabent *
xlv_getlvent(FILE *lvtabp, FILE *comment_stream)
{
	register struct lvtabent *tabent;
	register char *line, *p;
	char keybuf[MAXLVKEYLEN + 1];
	int ndevs = 0;
	int mirrordevs = 0;
	int mirroring = 0;
	int linelen;
	int guessdevs;
	int stripes = 1;
	int gran = 0;
	int i, value;

	if (lvtabp == NULL)
		return (NULL);

	if ((line = _getlvtabline(lvtabp, comment_stream)) == NULL)
		return (NULL);
	linelen = strlen(line);

	/* In order to allocate the required lvtabent struct, we need to know
	 * how many dev entries there will be. Each one is expected to be
	 * of the form "/dev/dsk/ips0d0s0"; ie 17 chars. So, as a liberal
	 * estimate we'll divide the whole line by 17....
	 */

	guessdevs = linelen / 17;
	if ((tabent = (struct lvtabent *)malloc(sizeof (struct lvtabent) +
				(guessdevs + 1) * sizeof (char *))) == NULL)
	{
		free(line);
		return (NULL);
	}
	p = line;
	tabent->devname = line;

	/* The first colon-terminated field is the logical volume device
	 * name. A null device name is not legal, otherwise anything is
	 * accepted. More stringent checks should be done by the logical
	 * volume utilities.
	 */

	for (i = 0; i <= LVBUFSIZ; i++, p++)
		if (*p == ':') break;

	if ((*p != ':') || (i == 0))
		goto bogus;

	 *p++ = 0x00;

	/* Next, a free-form ascii name up to 80 chars, delineated by ':' */

	tabent->volname = p;
	for (i = 0; i < MAXVNAMELEN; i++, p++)
	{
		if (*p == ':') break;
	}
	if (*p != ':') goto bogus;
	*p++ = 0x00;
	if (i == 0) tabent->volname = NULL;  /* null name field */

	/* Now to parse for keywords of the form parm=XXXX. Note that all
	 * numerical parameters MUST come before the devs= specification.
	 */

	/*CONSTCOND*/
	for(;;)
	{
		while (isspace(*p)) 
			if ((++p - line) >= linelen) goto bogus;
		bzero(keybuf, MAXLVKEYLEN + 1);
		for (i = 0; i < MAXLVKEYLEN; i++, p++)
		{
			if (*p == '=') break;
			keybuf[i] = *p;
		}
		if (*p++ != '=') goto bogus;
		if (streq(keybuf, "devs")) break;
		value = 0;
		while(isdigit(*p)) value = (value * 10) + (*p++ - '0');
		if (!value) goto bogus;
		if (streq(keybuf, "stripes")) stripes = value;
		else if (streq(keybuf, "step")) gran = value;
		else goto bogus;
		while (isspace(*p)) 
			if ((++p - line) >= linelen) goto bogus;
		if (*p++ != ':') goto bogus;
	}

	/* Now we loop getting device pathnames; these are comma-separated.
	 * We will allow leading whitespace for legibility; the comma however
	 * must immediately follow the pathname it delimits.
	 */

	/*CONSTCOND*/
	for(;;)
	{
		while (isspace(*p)) 
			if ((++p - line) >= linelen) goto done;
		if (!strncmp(p, "mirrors=", 8))
			goto mirloop;
		tabent->pathnames[ndevs] = p;
		if (++ndevs > guessdevs)
		{
			ndevs = 0;
			goto bogus;
		}
		while (!isspace(*p) && (*p != ',')) p++;
		*p++ = 0x00;
		if ((p - line) >= linelen)
			goto done;
	}

mirloop:	/* There are mirror devices. Loop again to get them. */

	p += 8; /* step past "mirrors=" */
	mirroring = 1;
	/*CONSTCOND*/
	for(;;)
	{
		while (isspace(*p)) 
			if ((++p - line) >= linelen) goto done;
		tabent->pathnames[ndevs + mirrordevs] = p;
		mirrordevs++;
		if ((ndevs + mirrordevs) > guessdevs)
		{
			ndevs = 0;
			goto bogus;
		}
		while (!isspace(*p) && (*p != ',')) p++;
		*p++ = 0x00;
		if ((p - line) >= linelen)
			goto done;
	}

done:
	*p = 0x00;
	tabent->ndevs = ndevs;
	tabent->stripe = stripes;
	if (stripes && (gran == 0) && (ndevs > 0)) {
		/*
		 * Step size has not been explicitly set so
		 * default to the track size.
		 */
		gran = _findtrksize(tabent->pathnames[0]);
	}
	tabent->gran = gran;
	if (mirroring)
	{
		if (mirrordevs == ndevs)
			tabent->mindex = ndevs;
		else
			tabent->mindex = -1;  /* illegal # of mirrors. */
	}
	else
		tabent->mindex = 0;
	return (tabent);
	
bogus:
	*p = 0x00;
	tabent->ndevs = 0;
	tabent->stripe = 0;
	tabent->gran = 0;
	tabent->mindex = 0;
	return (tabent);
}

/* _getlvtabline(): the actual line-getter. It returns the line in
 * malloc'd memory. It implements backslash-continuation; skips blank
 * and comment lines.
 *
 * If a <comment_stream> is provided, any comments found are written
 * out to the stream.
 */

static char *
_getlvtabline(FILE *lvtabp, FILE *comment_stream)
{
	char *buf, *p;
	int offset = 0;
	int len;

nuline:
	if ((buf = malloc(LVBUFSIZ)) == NULL) return (NULL);
	p = buf;
moreline:
	if (fgets((p), LVBUFSIZ, lvtabp) == NULL) 
	{
		if (p != buf) return (buf);
		else
		{
			free(buf);
			return (NULL);
		}
	}
	len = strlen(buf);
	p = buf + (len - 1);
	while (isspace(*p) && (p != buf)) p--; /* skip trailing blanks */
	if ((p == buf) && isspace(*p))
		goto moreline;   /* line was blank! */
	if (*p == '\\')
	{
		offset = p - buf;
		if ((buf = realloc(buf, offset + LVBUFSIZ)) == NULL) 	
			return (NULL);
		p = buf + offset;
		goto moreline;
	}

	/* here we have got the complete line, possibly constructed of several
	 * backslash-continued lines in the file. We know also that it
	 * contains at least one nonblank char. Now check for comment: ie a
	 * leading '#'.
	 */

	p = buf;
	while(isspace(*p)) 
		p++; 		/* skip any leading blanks */
	if (*p == '#')
	{
		if (comment_stream)
			fprintf(comment_stream, "%s", buf);
		free(buf);
		goto nuline;
	}
	return (buf);
}
