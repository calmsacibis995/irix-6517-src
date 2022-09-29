#if 0
static char sccsid[] = "@(#)hash.c	1.3 91/06/25 NFSSRC4.1 Copyr 1986 Sun Micro";
#endif

	/*
	 * Copyright (c) 1988 by Sun Microsystems, Inc.
	 */

	/*
	 * hash.c
	 * rotuines handle insertion, deletion of hashed monitor, file entries
	 */

#include <stdio.h>
#include <assert.h>
#include "prot_lock.h"

extern int debug;

typedef struct lm_vnode cache_me;

#define MAX_HASHSIZE 100
static cache_me *table_me[MAX_HASHSIZE];

#define isempty(l)	(l == (struct reclocklist*)l->rnext)

static void
print_reclist(struct reclocklist *rptr, char *str)
{
	struct reclock *tptr;
	int first = 1;

	if (rptr == NULL)
		return;
	FOREACH_RECLIST(rptr, tptr) {
		if (first) {
			printf("%s\n", str);
			first = 0;
		}
		printf(
		    "server %s, %sblk %s, type %d, base %d, len %d, pid %d\n",
			tptr->lck.server_name,
			tptr->block ? "" : "non",
			tptr->exclusive ? "write" : "read",
			tptr->lck.lox.type,
			tptr->lck.lox.base,
			tptr->lck.lox.length,
			tptr->lck.lox.pid);
	}
}

void
print_monlocks(void)
{
	cache_me *ptr;
	int i;

	printf("Monitored locks: ");
	for (i = 0; i < MAX_HASHSIZE; i++) {
		if ((ptr = table_me[i]) != NULL) {
			print_reclist(&ptr->exclusive, "Exclusive");
			print_reclist(&ptr->shared, "Shared");
			print_reclist(&ptr->pending, "Pending");
		}
	}
	putchar('\n');
}

/*
 * find_me returns the cached entry;
 * it returns NULL if not found;
 */
struct lm_vnode *
find_me(svr)
	char *svr;
{
	cache_me *cp;

	if (debug > 2)
		printf("find_me: %s\n", svr);

	cp = table_me[hash(svr)];
	while ( cp != NULL) {
		if (debug > 2)
			printf(" > %s\n", cp->server_name);
		if (strcasecmp(cp->server_name, svr) == 0) {
			/* found */
			return (cp);
		}
		cp = cp->next;
	}
	return (NULL);
}

/*
 * insert_me() -- add monitor entry to the hash table
 */
void
insert_me(mp)
	struct lm_vnode *mp;
{
	int h;

	h = hash(mp->server_name);
	mp->next = table_me[h];
	table_me[h] = mp;
}

/*
 * add_req_to_me() -- add lock request req to the monitor list
 */
void
add_req_to_me(req, status)
	struct reclock *req;
{
	struct lm_vnode *me;

	if (debug > 1)
		printf("enter add_req_to_me (reclock %x status %d) ...\n", 
			req, status);

	/* find the monitor entry for this server, it should be found */
	/* as the entry is created before the lock request is sent out*/
	/* to the server.					      */
	if ((me = find_me(req->lck.server_name)) == NULL) {
		if (debug)
			printf("rpc.lockd: monitor entry not found for \"%s\", request is not monitored.\n", req->lck.server_name);
		return;
	}

	switch (status) {
				/* add request to the appropriate list */
				/* exclusive or shared if the request  */
				/* is granted			       */
	case nlm_granted: 	if (req->exclusive) {
					merge_lock(req, &me->exclusive);
					/* if an upgrade lock, we rm the */
					/* the lock from the shared list */
					lm_unlock_region(req, &me->shared);
			  	} else {
					merge_lock(req, &me->shared);
					/* if downgrade of lock, we rm the */
					/* lock from the exclusive list    */
					lm_unlock_region(req, &me->exclusive);
				}
				if (debug) {
				    print_reclist(&me->exclusive, "Exclusive");
				    print_reclist(&me->shared, "Shared");
				}
				break;

				/* if request is queued at server, add	*/
				/* it to the pending list		*/
	case nlm_blocked: 	add_req_to_list(req, &me->pending);
				break;
	}
}

/*
 * upgrade_req_in_me() -- will move the lock request req that's in the 
 *			  monitor's pending list to exclusive/shared list
 */
void
upgrade_req_in_me(req)
	struct reclock *req;
{
	struct lm_vnode *me;

	if (debug)
		printf("enter upgrade_req_in_me (req %x%s) ...\n",
			req, req->exclusive ? " excl" : "");

	/* find the monitor entry for this server, it should be found */
	/* as the entry is created before the lock request is sent out*/
	/* to the server.					      */

	if ((me = find_me(req->lck.server_name)) == NULL) {
		if (debug)
			printf(
		"mon entry not found for \"%s\", request is not monitored.\n",
				req->lck.server_name);
		return;
	}

	/* remove the request from the pending list */
	(void) del_req_from_list(&req->lck.lox, &me->pending);

	/* add the request to the exclusive/shared list */
	if (req->exclusive)
		merge_lock(req, &me->exclusive);
	else
		merge_lock(req, &me->shared);
}

/*
 * remove_req_in_me() -- will remove the lock request req from the monitor's
 *			 pending, exclusive or shared list.
 *			 del_req() is used to free the memory of the lock
 *			 request.
 */
void
remove_req_in_me(req)
	struct reclock *req;
{
	struct lm_vnode *me;

	if (debug > 1)
		printf("enter remove_req_in_me (reclock %x) ...\n", req);

	/* find the monitor entry for this server, it should be found */
	/* as the entry is created before the lock request is sent out*/
	/* to the server.					      */
	if ((me = find_me(req->lck.server_name)) == NULL) {
		if (debug)
			printf("monitor entry not found for \"%s\", request is not monitored.\n", req->lck.server_name);
		return;
	}

	/* since we don't know which of the monitor list this request reside */
	/* we'll try pending, and then exclusive or shared list.	     */
	/* the memory of this lock request req will be freed by del_req()    */
	if (del_req(req, &me->pending))
		return;
	lm_unlock_region(req, &me->exclusive);
	lm_unlock_region(req, &me->shared);
}

/*
 * add_req_to_list() -- to add a lock request req to the monitor list listp
 */
void
add_req_to_list(req, list)
	struct reclock *req;
	struct reclocklist *list;
{
	struct reclock *tmp_p;

	if (debug)
		printf("enter add_req_to_list (req %x list %x %s) ...\n",
			req, list, isempty(list) ? "empty" : "");

	/* make sure the request is not in the list already, this is possible */
	/* due to retransmissions.					      */
	FOREACH_RECLIST(list, tmp_p) {
		if (debug)
			printf("  %x\n", tmp_p);
		if ((tmp_p == req) || 
		    (same_lock(req, &(tmp_p->lck.lox)) &&
		     obj_cmp(&(req->lck.fh), &(tmp_p->lck.fh)))) {
			req->rel = 1;	/* duplicate, can be freed */
			return;
		}
	}

	req->rprev = (struct reclock *)list;
	req->rnext = list->rnext;
	list->rnext->rprev = req;
	list->rnext = req;
}

void
rm_req(struct reclock *req)
{
	req->rprev->rnext = req->rnext;
	req->rnext->rprev = req->rprev;

	req->rnext = req->rprev = NULL;
}

/*
 * del_req_from_list() -- will remove the request of the data lock l from the 
 *			  doubly linked list listp
 *			  similar to del_req() except it doesn't free reclock
 *			  + arg is data_lock instead of reclock.
 *			  if the req is not found, return NULL; else return the
 *			  reclock.
 */
struct reclock *
del_req_from_list(l, list)
	struct data_lock *l;
	struct reclocklist *list;
{
	struct reclock *tmp_p;

	if (debug)
		printf("enter del_req_from_list (lox %x list %x %s) ...\n",
			l, list, isempty(list) ? "empty" : "");

	FOREACH_RECLIST(list, tmp_p) {
		if (debug)
			printf("  %x: %x\n", tmp_p, &tmp_p->lck.lox);
		if (&tmp_p->lck.lox == l) {
			rm_req(tmp_p);
			return (tmp_p);
		}
	}
	return (NULL);
}


/*
 * del_req() -- similar to del_req_from_list() in that it will remove the
 *		request req from the linked list listp.  difference in that
 *		del_req() will free the memory of req.
 *	  	difference is arg is reclock instead of data lock.
 *		if the req is not found, return FALSE; else TRUE.
 */
bool_t
del_req(req, list)
	struct reclock *req;
	struct reclocklist *list;
{
	struct reclock *tmp_p;

	if (debug)
		printf("enter del_req (reclock %x list %x %s) ...\n",
			req, list, isempty(list) ? "empty" : "");

	FOREACH_RECLIST(list, tmp_p) {
		if (debug)
			printf(" ? %x", tmp_p);
		if ((WITHIN(&(tmp_p->lck.lox), &(req->lck.lox)) ||
		     same_bound(&(req->lck.lox), &(tmp_p->lck.lox))) &&
		    SAMEOWNER(&(req->lck.lox), &(tmp_p->lck.lox)) &&
		    obj_cmp(&(req->lck.fh), &(tmp_p->lck.fh))) {

			tmp_p->rel = 1;
			rm_req(tmp_p);
			release_reclock(tmp_p);
			if (debug)
				printf(" deleted\n");
			return (TRUE);
		}
		if (debug)
			printf("\n");
	}
	return (FALSE);
}
