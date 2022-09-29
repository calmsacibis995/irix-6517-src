/*
 * char *getfenv (const inod_t *p, fenv_t ft) 
 *
 *	Return the file environment (fenv) string value
 *	of the fenv variable "ft" for inode "p".
 *
 * void putfenv (inod_t *p, fenv_t ft, char *val)
 *
 *	Set the fenv string value to "val"
 *	of the fenv variable "ft" for inode "p".
 *
 * void unsetfenv (inod_t *p, fenv_t ft)
 *
 *	Unset (delete) any (zero, one or more) instances
 *	of the fenv string variable "ft" for inode "p".
 */

#include <stdio.h>
#define NDEBUG
#include <assert.h>
#include "fsdump.h"
#include "fenv.h"

char *getfenv (const inod_t *p, fenv_t ft) {
	char *fp = PENV (mh, p);

	if (fp == NULL)
		return NULL;

	while (*fp > FV_MINVAL && *fp < FV_MAXVAL) {
		if (*fp == ft)
			return fp + 1;
		fp += strlen (fp) + 1;
	}
	if (*fp != FV_END)
		warn ("corrupt fenv", Pathname);
	return NULL;
}

/*
 * How many bytes in the fenv pointed to?
 */
static uint64_t fenvlen (char *fp) {
	char *ofp = fp;

	if (fp == NULL)
		return 0;

	while (*fp > FV_MINVAL && *fp < FV_MAXVAL)
		fp += strlen (fp) + 1;
	if (*fp != FV_END)
		warn ("corrupt fenv", Pathname);

	return fp - ofp + 1;
}

/*
 * Delete any (zero, one or more) copies of value "ft" from fenv at "fp",
 * by copying rest of fenv down, over deleted values.
 * Return pointer to ending FV_END char.
 */
static char *delfenv (char *ofp, fenv_t ft) {
	char *nfp = ofp;


	while (*ofp > FV_MINVAL && *ofp < FV_MAXVAL) {
		char *ofpnext = ofp + strlen(ofp) + 1;
		if (*ofp != ft) {
			strcpy (nfp, ofp);
			nfp += strlen (nfp) + 1;
		}
		ofp = ofpnext;
	}
	*nfp = FV_END;
	return nfp;
}

void putfenv (inod_t *p, fenv_t ft, char *val) {
	char *fp = PENV (mh, p);		/* pointer to fenv */
	char *fpend;				/* end of fenv - add val on here */
	uint64_t flen;				/* length of entire fenv */
	char *oldval;				/* previous value of "ft" */
	uint64_t oldvlen, newvlen;		/* length of old and new values of "ft" */
	uint64_t oldhlen, newhlen;		/* length old/new "ft" header+nul */
	uint64_t growth;				/* how many more bytes needed in fenv */

	oldval = getfenv (p, ft);
	if (oldval == NULL) {
		oldvlen = 0;
		oldhlen = 0;
	} else {
		oldvlen = strlen (oldval);
		oldhlen = 2;
	}
	newvlen = strlen (val);
	newhlen = 2;

	flen = fenvlen(fp);
	growth = newvlen + newhlen - oldvlen - oldhlen;
	if (flen == 0) growth++;		/* for FV_END */

	if (growth > 0) {
		p->i_xfenv = xheaprealloc (
			(void **)(&mh.hp_fenv), p->i_xfenv, flen, growth
		);
		fp = PENV (mh, p);
	}

	if (oldval == NULL) {
		if (flen > 0)
			fpend = fp + flen - 1;
		else
			fpend = fp;
	} else
		fpend = delfenv (fp, ft);

	*fpend++ = ft;
	strcpy (fpend, val);
	fpend += newvlen + 1;
	*fpend = FV_END;
}

void unsetfenv (inod_t *p, fenv_t ft) {
	char *fp = PENV (mh, p);

	(void) delfenv (fp, ft);
}

void fenvcat (inod_t *p, char *oldfenv) {
	while (*oldfenv > FV_MINVAL && *oldfenv < FV_MAXVAL) {
		if (getfenv (p, (fenv_t) *oldfenv) == NULL)
			putfenv (p, (fenv_t) *oldfenv, oldfenv + 1);
		oldfenv += strlen (oldfenv) + 1;
	}
}

#if 0
void dumpfenv(inod_t *p) {
	char *fp = PENV (mh, p);

	if (fp == NULL) {
		puts ("fenv NULL");
		return;
	}
	while (*fp > FV_MINVAL && *fp < FV_MAXVAL) {
		printf("%d: %s <Len: %d>\n", *fp, fp+1, strlen(fp+1) );
		fp += strlen (fp) + 1;
	}
	if (*fp != FV_END)
		warn ("corrupt fenv", Pathname);
}

fenvloop(){
	char buf[128];

	printf("fenvloop: enter PnVALUE, Gn or Dino (put val n, get n or dump ino)\n");

	while (gets (buf)) {
	switch (buf[0]) {
	case 'P':
		putfenv (PINO(&mh,rootino), atoi(buf+1), buf+2);
		break;
	case 'G':
		puts(getfenv(PINO(&mh,rootino), atoi(buf+1)));
		break;
	case 'D':
		dumpfenv(PINO(&mh,atoi(buf+1)));
		break;
	default:
		printf("fenvloop: enter PnVALUE, Gn or Dino (put val n, get n or dump)\n");
	}
	}
}
#endif
