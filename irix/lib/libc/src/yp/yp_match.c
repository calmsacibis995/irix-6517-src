/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

#ifdef __STDC__
	#pragma weak yp_match = _yp_match
#endif
#include "synonyms.h"

#define NULL 0
#include <sys/time.h>
#include <rpc/rpc.h>
#include <string.h>		/* prototype for strlen() */
#include <bstring.h>		/* prototype for bzero() */
#include <unistd.h>		/* prototype for sleep() */
#include "yp_prot.h"
#include "ypclnt.h"
#include "yp_extern.h"

#undef gettimeofday	/* gets rid of warning about gettimeofday previously defined */

#define gettimeofday	BSDgettimeofday

struct cache {
	struct cache *next;
	unsigned int birth;
	char *domain;
	char *map;
	char *key;
	int  keylen;
	char *val;
	int  vallen;
};

static int v2domatch(const char *, const char *, const char *, int, struct dom_binding *,
		struct timeval, char **, int  *);
static void freenode(struct cache *);
static void detachnode(struct cache *, struct cache *);
static struct cache *makenode(const char *, const char *, int, int);

static struct cache *head _INITBSS;
#define	CACHESZ 16
#define	CACHETO 60*2

static void
detachnode(register struct cache *prev, register struct cache *n)
{

	if (prev == 0) {	
		/* assertion: n is head */
		head = n->next;
	} else {
		prev->next = n->next;
	}
	n->next = 0;
}

static void
freenode(register struct cache *n)
{

	if (n->val != 0)
	    free(n->val);
	if (n->key != 0)
	    free(n->key);
	if (n->map != 0)
	    free(n->map);
	if (n->domain != 0)
	    free(n->domain);
	bzero((char *) n, sizeof(*n));
	free((char *) n);
}

static struct cache *
makenode(const char *domain, const char *map, int keylen, int vallen)
{
	register struct cache *n =
	    (struct cache *) malloc(sizeof(struct cache));

	if (n == 0)
	    return (0);
	bzero((char *) n, sizeof(*n));
	for (;;) {
		if ((n->domain = malloc(1 + strlen(domain))) == 0)
		    break;
		if ((n->map = malloc(1 + strlen(map))) == 0)
		    break;
		if ((n->key = malloc((size_t)keylen)) == 0)
		    break;
		if ((n->val = malloc((size_t)vallen)) == 0)
		    break;
		return (n);
	}
	freenode(n);
	return (0);
}

/*
 * Requests the NIS server associated with a given domain to attempt to match
 * the passed key datum in the named map, and to return the associated value
 * datum. This part does parameter checking, and implements the "infinite"
 * (until success) sleep loop.
 */
int
yp_match (const char *domain,
	const char *map,
	const char *key,
	register int  keylen,
	char **val,		/* returns value array */
	int  *vallen)		/* returns bytes in val */
{
	int domlen;
	int maplen;
	int reason;
	struct dom_binding *pdomb;
	register struct cache *c, *prev;
	int cnt, savesize;
	struct timeval now;
	struct timezone tz;

	if ( (map == NULL) || (domain == NULL) ) {
		return(YPERR_BADARGS);
	}
	
	domlen = (int)strlen(domain);
	maplen = (int)strlen(map);
	
	if ( (domlen == 0) || (domlen > (int)YPMAXDOMAIN) ||
	    (maplen == 0) || (maplen > (int)YPMAXMAP) ||
	    (key == NULL) || (keylen == 0) || (keylen > YPMAXRECORD)) {
		return(YPERR_BADARGS);
	}
	/* is it in our cache ? */
	for (prev=0, cnt=0, c=head; c != 0; prev=c, c=c->next, cnt++) {
		if ((c->keylen == keylen) &&
		    (bcmp(key, c->key, keylen) == 0) &&
		    (strcmp(map, c->map) == 0) &&
		    (strcmp(domain, c->domain) == 0)) {
			/* cache hit */
			(void) gettimeofday(&now, &tz);
			if (((unsigned long)now.tv_sec - c->birth) > CACHETO) {
				/* rats.  it it too old to use */
				detachnode(prev, c);
				freenode(c);
				break;
			} else {
				/* NB: Copy two extra bytes; see below */
				savesize = c->vallen + 2;
				*val = malloc((size_t)savesize);
				if (*val == 0) {
					return (YPERR_RESRC);
				}
				bcopy(c->val, *val, savesize);
				*vallen = c->vallen;

			    /* Reorganize the list to make this element 
			     * the head if it is already not the head.  
			     * This check fixes bug 1023528 (see below).
			     */

				if (prev != 0) {
					prev->next = c->next;
					c->next = head;
					head = c;
				}
				return (0);
			}
		}
		if (cnt >= CACHESZ) {
			detachnode(prev, c);
			freenode(c);
			break;
		}
	}

	for (;;) {
		
		if (reason = _yp_dobind(domain, &pdomb) ) {
			return(reason);
		}

		reason = v2domatch(domain, map, key, keylen, pdomb,
		    _ypserv_timeout, val, vallen);

		if (reason == YPERR_RPC) {
			yp_unbind(domain);
			(void) sleep(_ypsleeptime);
		} else {
			break;
		}
	}
	
	/* add to our cache */
	if (reason == 0) {
		/*
		 * NB: allocate and copy extract two bytes of the value;
		 * these two bytes are mandatory CR and NULL bytes.
		 */
		savesize = *vallen + 2;
		c = makenode(domain, map, keylen, savesize);
		if (c != 0) {
			(void) gettimeofday(&now, &tz);
			c->next = head;
			head = c;
			c->birth = (unsigned int)now.tv_sec;
			(void) strcpy(c->domain, domain);
			(void) strcpy(c->map, map);
			bcopy(key, c->key, c->keylen = keylen);
			bcopy(*val, c->val, savesize);
			c->vallen = *vallen;
		}
	}
	return(reason);
}

/*
 * This talks v2 protocol to ypserv
 */
static int
v2domatch (const char *domain,
	const char *map,
	const char *key,
	int  keylen,
	struct dom_binding *pdomb,
	struct timeval timeout,
	char **val,		/* return: value array */
	int  *vallen)		/* return: bytes in val */
{
	struct ypreq_key req;
	struct ypresp_val resp;
	unsigned int retval = 0;

	req.domain = (char *)domain;
	req.map = (char *)map;
	req.keydat.dptr = (char *)key;
	req.keydat.dsize = keylen;
	
	resp.valdat.dptr = NULL;
	resp.valdat.dsize = 0;

	/*
	 * Do the match request.  If the rpc call failed, return with status
	 * from this point.
	 */
	
	if(clnt_call(pdomb->dom_client, YPPROC_MATCH,
	    (xdrproc_t) xdr_ypreq_key, &req, (xdrproc_t) xdr_ypresp_val, &resp,
	    timeout) != RPC_SUCCESS) {
		return(YPERR_RPC);
	}

	/* See if the request succeeded */
	
	if (resp.status != YP_TRUE) {
		retval = (unsigned int)ypprot_err((unsigned) resp.status);
	}

	/* Get some memory which the user can get rid of as he likes */

	if (!retval && ((*val = malloc((unsigned)
	    resp.valdat.dsize + 2)) == NULL)) {
		retval = YPERR_RESRC;
	}

	/* Copy the returned value byte string into the new memory */

	if (!retval) {
		*vallen = resp.valdat.dsize;
		bcopy(resp.valdat.dptr, *val, resp.valdat.dsize);
		(*val)[resp.valdat.dsize] = '\n';
		(*val)[resp.valdat.dsize + 1] = '\0';
	}

	CLNT_FREERES(pdomb->dom_client, (xdrproc_t) xdr_ypresp_val, &resp);
	return((int)retval);

}
