#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = 	"@(#)getgrent.c	1.7 88/05/11 4.0NFSSRC SMI"; /* from UCB 5.2 3/9/86 SMI 1.23 */
#endif

#ifndef DSHLIB
#ifdef __STDC__
	#pragma weak fgetgrent = _fgetgrent
	#pragma weak getgrgid_r = _getgrgid_r
	#pragma weak getgrnam_r = _getgrnam_r
#endif
#endif
#include "synonyms.h"

#include <ctype.h>
#include <grp.h>
#include <stdlib.h>
#include <errno.h>
#include <paths.h>
#include <stdio.h>
#include <string.h>
#include <bstring.h>
#include <values.h>
#include <rpc/rpc.h>			/* for prototyping */
#include <rpcsvc/yp_prot.h>		/* for prototyping */
#include <rpcsvc/ypclnt.h>
#include "gen_extern.h"

typedef struct mgp {
	struct group gp;
	char *memlist;		/* actual member names */
	int nmem;		/* # of members */
} mgp_t;

static FILE *grf _INITBSS;	/* pointer into /etc/group */
static char *yp _INITBSS;	/* pointer into NIS */
static int yplen _INITBSS;
static char *line _INITBSS;
static int linelen _INITBSS;
static char *oldyp _INITBSS;
static int oldyplen _INITBSS;
static mgp_t *rgp _INITBSS;	/* the group struct that's returned */
char *__grpfilename = _PATH_GROUP;	/* changeable for debugging */

static struct list {		/* list of - items */
	char *name;
	struct list *nxt;
} *minuslist _INITBSS;

static mgp_t *interpret(char *val, int len);
static mgp_t *getnamefromyellow(const char *name, mgp_t *localgp);
static mgp_t *getgidfromyellow(gid_t gid, mgp_t *localgp);
static void freemgp(mgp_t *);
static void overlaylocal(mgp_t *gp, mgp_t *lgp);

static int matchname(char, mgp_t *, mgp_t **, const char *);
static int matchgid(char, mgp_t *, mgp_t **, gid_t);
static void addtominuslist(char *name);
static void freeminuslist(void);
static int onminuslist(char *);
static void getfirstfromyellow(void);
static void getnextfromyellow(void);
static gid_t gidof(const char *name);

struct group 	*__files_getgrgid(gid_t);
struct group 	*__files_getgrnam(const char *);
struct group 	*__files_getgrent(void);
void 		__files_setgrent(void);
void 		__files_endgrent(void);

#include <di_group.h>
#include <ssdi.h>


_SSDI_VOIDFUNC __files_group_ssdi_funcs[] = {
	(_SSDI_VOIDFUNC) __files_getgrnam,
	(_SSDI_VOIDFUNC) __files_getgrgid,
	(_SSDI_VOIDFUNC) __files_setgrent,
	(_SSDI_VOIDFUNC) __files_getgrent,
	(_SSDI_VOIDFUNC) __files_endgrent,
	(_SSDI_VOIDFUNC) NULL		/* No getbymember supported */
};


struct group *
__files_getgrgid(gid_t gid)
{
	int rv;

	if (rgp) {
		freemgp(rgp);
		rgp = NULL;
	}
	__files_setgrent();
	if (_getpwent_no_yp || !_yp_is_bound) {
		struct group *p;

		/* getgrent sets rgp ... */
		while( (p = __files_getgrent()) && p->gr_gid != gid );
		__files_endgrent();
		return(p);
	}
	if (!grf)
		return NULL;
	while ((line = __getline(grf, line, &linelen)) != NULL) {
		mgp_t *p;
		auto mgp_t *p1;

		if ((p = interpret(line, (int)strlen(line))) == NULL)
			continue;
		rv = matchgid(line[0], p, &p1, gid);
		if (rv == 2) {
			__files_endgrent();
			/* on minus list - return NULL */
			freemgp(p);
			return NULL;
		} else if (rv == 1) {
			/* found it - either got new gp from NIS or the
			 * the one from the file matched
			 */
			__files_endgrent();
			if (p1) {
				freemgp(p);
				rgp = p1;
			} else
				rgp = p;
			return &rgp->gp;
		}
		freemgp(p);
	}
	__files_endgrent();
	return NULL;
}

struct group *
__files_getgrnam(const char *name)
{
	int rv;

	if (rgp) {
		freemgp(rgp);
		rgp = NULL;
	}
	__files_setgrent();
	if (_getpwent_no_yp || !_yp_is_bound) {
		struct group *p;

		/* getgrent sets rgp ... */
		while( (p = __files_getgrent()) && strcmp(p->gr_name,name) );
		__files_endgrent();
		return(p);
	}
	if (!grf)
		return NULL;
	while ((line = __getline(grf, line, &linelen)) != NULL) {
		mgp_t *p;
		auto mgp_t *p1;

		if ((p = interpret(line, (int)strlen(line))) == NULL)
			continue;
		rv = matchname(line[0], p, &p1, name);
		if (rv == 2) {
			__files_endgrent();
			/* on minus list - return NULL */
			freemgp(p);
			return NULL;
		} else if (rv == 1) {
			/* found it - either got new gp from NIS or the
			 * the one from the file matched
			 */
			__files_endgrent();
			if (p1) {
				freemgp(p);
				rgp = p1;
			} else
				rgp = p;
			return &rgp->gp;
		}
		freemgp(p);
	}
	__files_endgrent();
	return NULL;
}

void
__files_setgrent(void)
{
	if (!_getpwent_no_yp)
		_yellowup(1);            /* recheck whether NIS is up */
	if (!grf)
		grf = fopen(__grpfilename, "r");
	else
		rewind(grf);
	if (yp)
		free(yp);
	yp = NULL;
	freeminuslist();
}

void
__files_endgrent(void)
{
	if (grf) {
		(void) fclose(grf);
		grf = NULL;
	}
	if (yp)
		free(yp);
	yp = NULL;
	freeminuslist();
}

struct group *
fgetgrent(FILE *f)
{
	mgp_t *gp;

	if (rgp) {
		freemgp(rgp);
		rgp = NULL;
	}
	while ((line = __getline(f, line, &linelen)) != NULL) {
		gp = interpret(line, (int)strlen(line));
		if (gp) {
			rgp = gp;
			return &rgp->gp;
		}
	}
	return NULL;
}

static char *
grskip(char *p, int c)
{
	while(*p && *p != c && *p != '\n')
		p++;
	if (*p == '\n')
		*p = '\0';
	else if (*p != '\0')
		*p++ = '\0';
	return(p);
}

/* move out of function scope so we get a global symbol for use with data cording */
static mgp_t *savegp _INITBSS;

struct group *
__files_getgrent(void)
{
	mgp_t *gp = NULL;
	mgp_t *lgp;		/* gp from file */

	if (grf == NULL) {
		__files_setgrent();
		if (grf == NULL)
			return(NULL);
	}
	if (rgp) {
		freemgp(rgp);
		rgp = NULL;
	}
	/*
	 * XXX why do this???
	 * To pick up a failed binding or new binding during a scan??
	 * this is expensive - a getpid and a time() call per
	 * note that the setgrent above WILL check for yp - thus
	 * each time the caller calls endgrent, then getgrent, we'll check
	if (!_getpwent_no_yp)
		_yellowup(0);
	 */
again:
	/* if we have a gp - get rid of it */
	freemgp(gp);
	/*
	 * use NIS line - the way we get here is if we encounter a
	 * '+' in the file - that triggers the getfirstfromyellow code which
	 * sets the 'yp' buffer. From then on we will loop here until the NIS
	 * entries are exhausted.
	 */
	if (yp) {
		gp = interpret(yp, yplen);
		free(yp);
		yp = NULL;
		if (gp == NULL) {
			/* end of NIS entries and therefore end of file */
			freemgp(savegp);
			savegp = NULL;
			return(NULL);
		}
		overlaylocal(gp, savegp);
		getnextfromyellow();	/* prime the pump for next call */
		if (onminuslist(gp->gp.gr_name))
			goto again;
		goto done;
	}
	for (;;) {
		if ((line = __getline(grf, line, &linelen)) == NULL)
			return(NULL);
		gp = interpret(line, (int)strlen(line));
		if (gp)
			break;
	}
	if (_getpwent_no_yp || !_yp_is_bound) 
		goto done;
	switch(line[0]) {
	case '+':
		if (strcmp(gp->gp.gr_name, "+") == 0) {
			/*
			 * start sucking in entire NIS group list
			 * ALL NIS enties will have their passwd and group
			 * membership overlayed with the values from gp
			 */
			getfirstfromyellow();
			freemgp(savegp);	/* get rid of any previous savegp */
			savegp = gp;
			gp = NULL;
			goto again;
		}
		/* 
		 * else look up this entry in NIS
		 */
		lgp = gp;
		gp = getnamefromyellow(lgp->gp.gr_name+1, lgp);
		freemgp(lgp);
		if (gp == NULL || onminuslist(gp->gp.gr_name))
			goto again;
		break;
	case '-':
		addtominuslist(gp->gp.gr_name+1);
		goto again;
	default:
		if (onminuslist(gp->gp.gr_name))
			goto again;
	}
done:
	rgp = gp;
	return &rgp->gp;
}

/*
 * interpret ruins 'val' (except the first character).
 * It returns a pointer to a malloced group struct and all data is
 * saved in malloced buffers
 */
static mgp_t *
interpret(char *val, int len)
{
	mgp_t *mgp;
	struct group *gp;
	register char *p, **q;
	char *end, *tp;
	size_t x;
	register int ypentry;
	int grcount = 0, grsize;

	/* allow # to be used as comment character */
	p = val;
	while (*p && isspace(*p))
		p++;
	if (*p == '\n' || *p == '#' || *p == '\0')
		return NULL;

	if ((mgp = calloc(1, sizeof(*mgp))) == NULL)
		return NULL;

	/*
	 * since most of a group entry is its member list - simply
	 * replicate the whole line
	 */
	if ((mgp->memlist = malloc((unsigned int)(len) + 1)) == NULL)
		goto cleanup;
	bcopy(val, mgp->memlist, len + 1); /* val is always NULL terminated */
	gp = &mgp->gp;
	p = mgp->memlist;

	/*
 	 * Set "ypentry" if this entry references the NIS;
	 * if so, null GIDs are allowed (because they will be filled in
	 * from the matching NIS entry).
	 */
	ypentry = (*p == '+') || (*p == '-');

	tp = p;
	p = grskip(p,':');
	gp->gr_name = strdup(tp);
	tp = p;
	p = grskip(p,':');
	gp->gr_passwd = strdup(tp);
	if (*p == ':' && !ypentry)
		/* check for non-null gid */
		goto cleanup;
	x = strtol(p, &end, 10);	
	p = end;
	if (*p++ != ':' && !ypentry)
		/* check for numeric value - must have stopped on the colon */
		goto cleanup;
	gp->gr_gid = (gid_t)x;
	(void) grskip(p,'\n');	/* replaces trailing newline with a null */

	grsize = 2;
	if ((gp->gr_mem = (char **)malloc((unsigned int)(grsize) * sizeof(char *))) == NULL) {
		goto cleanup;
	}
	q = gp->gr_mem;
	while (*p) {
		/* need 1 extra for NULL at end of list */
		if (grcount >= (grsize - 1)) {
			/* ran out of pointers */
			grsize *= 2;
			gp->gr_mem = (char **)realloc((char *)gp->gr_mem, (unsigned int)(grsize) * sizeof(char *));
			if (gp->gr_mem == NULL)
				goto cleanup;
			q = gp->gr_mem + grcount;
		}
		*q++ = p;
		grcount++;
		p = grskip(p,',');
	}
	*q = NULL;
	mgp->nmem = grcount;
	return mgp;

cleanup:
	freemgp(mgp);
	return NULL;
}

static void
freemgp(mgp_t *mgp)
{
	struct group *gp = &mgp->gp;
	if (mgp == NULL)
		return;
	if (gp->gr_name)
		free(gp->gr_name);
	if (gp->gr_passwd)
		free(gp->gr_passwd);
	if (gp->gr_mem)
		free((char *)gp->gr_mem);
	if (mgp->memlist)
		free(mgp->memlist);
	free(mgp);
}

static void
freeminuslist(void)
{
	struct list *ls;
	
	struct list *nxt;
	for (ls = minuslist; ls != NULL; ls = nxt) {
		nxt = ls->nxt;
		free(ls->name);
		free(ls);
	}
	minuslist = NULL;
}

static void
overlaylocal(mgp_t *gp, mgp_t *lgp)
{
	int i;

	/*
	 * overlay local info on NIS info
	 */
	if (lgp->gp.gr_passwd && *lgp->gp.gr_passwd) {
		free(gp->gp.gr_passwd);
		gp->gp.gr_passwd = strdup(lgp->gp.gr_passwd);
	}
	if (lgp->gp.gr_mem && *lgp->gp.gr_mem) {
		free(gp->gp.gr_mem);
		i = gp->nmem = lgp->nmem;
		gp->gp.gr_mem = (char **)malloc((unsigned int)(i + 1) * sizeof(char *));
		bcopy(lgp->gp.gr_mem, gp->gp.gr_mem, (i + 1) * (int)sizeof(char *));
	}
}

static int
onminuslist(char *nm)
{
	struct list *ls;

	for (ls = minuslist; ls != NULL; ls = ls->nxt)
		if (strcmp(ls->name, nm) == 0)
			return 1;
	return 0;
}

static void
getnextfromyellow(void)
{
	int reason;
	char *key = NULL;
	int keylen;
	
	reason = yp_next(_yp_domain, "group.byname", oldyp, oldyplen,
			     &key, &keylen, &yp, &yplen);
	if (reason) {
#ifdef DEBUG
fprintf(stderr, "reason yp_next failed is %d\n", reason);
#endif
		yp = NULL;
	}
	if (oldyp)
		free(oldyp);
	oldyp = key;
	oldyplen = keylen;
}

static void
getfirstfromyellow(void)
{
	int reason;
	char *key = NULL;
	int keylen;
	
	reason = yp_first(_yp_domain, "group.byname", &key, &keylen,
			       &yp, &yplen);
	if (reason) {
#ifdef DEBUG
fprintf(stderr, "reason yp_first failed is %d\n", reason);
#endif
		yp = NULL;
	}
	if (oldyp)
		free(oldyp);
	oldyp = key;
	oldyplen = keylen;
}

static mgp_t *
getnamefromyellow(const char *name, mgp_t *localgp)
{
	mgp_t *gp;
	int reason;
	char *val;
	int vallen;
	
	reason = yp_match(_yp_domain, "group.byname", name, (int)strlen(name),
			      &val, &vallen);
	if (reason) {
#ifdef DEBUG
fprintf(stderr, "reason yp_next failed is %d\n", reason);
#endif
		return NULL;
	}
	gp = interpret(val, vallen);
	free(val);
	if (gp == NULL)
		return NULL;
	if (localgp)
		overlaylocal(gp, localgp);
	return gp;
}

static void
addtominuslist(char *name)
{
	struct list *ls;
	char *buf;
	
	ls = (struct list *)malloc(sizeof(struct list));
	buf = (char *)malloc(strlen(name) + 1);
	if (ls == NULL || buf == NULL)
		return;
	(void) strcpy(buf, name);
	ls->name = buf;
	ls->nxt = minuslist;
	minuslist = ls;
}

static int
matchname(char line1, mgp_t *mgp, mgp_t **rgpp, const char *name)
{
	mgp_t *ngp;

	*rgpp = NULL;
	switch(line1) {
		case '+':
			if (strcmp(mgp->gp.gr_name, "+") == 0) {
				ngp = getnamefromyellow(name, mgp);
				if (ngp) {
					*rgpp = ngp;
					return 1;
				}
				else
					return 0;
			}
			if (strcmp(mgp->gp.gr_name+1, name) == 0) {
				ngp = getnamefromyellow(mgp->gp.gr_name+1, mgp);
				if (ngp) {
					*rgpp = ngp;
					return 1;
				} else
					return 0;
			}
			break;
		case '-':
			if (strcmp(mgp->gp.gr_name+1, name) == 0)
				return 2;
			break;
		default:
			if (strcmp(mgp->gp.gr_name, name) == 0)
				return 1;
	}
	return 0;
}

static int
matchgid(char line1, mgp_t *mgp, mgp_t **rgpp, gid_t gid)
{
	mgp_t *ngp;

	*rgpp = NULL;
	switch(line1) {
		case '+':
			if (strcmp(mgp->gp.gr_name, "+") == 0) {
				ngp = getgidfromyellow(gid, mgp);
				if (ngp) {
					*rgpp = ngp;
					return 1;
				} else
					return 0;
			}
			ngp = getnamefromyellow(mgp->gp.gr_name+1, mgp);
			if (ngp && ngp->gp.gr_gid == gid) {
				*rgpp = ngp;
				return 1;
			}
			break;
		case '-':
			if (gid == gidof(mgp->gp.gr_name+1))
				return 2;
			break;
		default:
			if (mgp->gp.gr_gid == gid)
				return 1;
	}
	return 0;
}

static gid_t
gidof(const char *name)
{
        mgp_t *mgp;
	gid_t gid;

        mgp = getnamefromyellow(name, NULL);
        if (mgp) {
		gid = mgp->gp.gr_gid;
		freemgp(mgp);
                return gid;
        } else
                return MAXINT;
}
 
static mgp_t *
getgidfromyellow(gid_t gid, mgp_t *localgp)
{
	mgp_t *gp;
	int reason;
	char *val;
	int vallen;
	char gidstr[20];
	
	sprintf(gidstr, "%d", gid);
	reason = yp_match(_yp_domain, "group.bygid", gidstr,
					(int)strlen(gidstr), &val, &vallen);
	if (reason) {
#ifdef DEBUG
fprintf(stderr, "reason yp_next failed is %d\n", reason);
#endif
		return NULL;
	}
	gp = interpret(val, vallen);
	free(val);
	if (gp == NULL)
		return NULL;
	overlaylocal(gp, localgp);
	return gp;
}

/* ARGSUSED */
int
getgrnam_r(
	const char *name,
	struct group *grp,
	char *buf,
	size_t bufsize,
	struct group **result)
{
	return ENOSYS;
}

/* ARGSUSED */
int
getgrgid_r(
	uid_t uid,
	struct group *grp,
	char *buf,
	size_t bufsize,
	struct group **result)
{
	return ENOSYS;
}
