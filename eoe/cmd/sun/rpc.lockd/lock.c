/* Copyright 1991 Silicon Graphics, Inc. All rights reserved. */

#if 0
static char sccsid[] = "@(#)prot_lock.c	1.6 91/06/25 NFSSRC4.1 Copyr 1990 Sun Micro";
#endif

	/*
	 * Copyright (c) 1988 by Sun Microsystems, Inc.
	 */

	/*
	 * prot_lock.c consists of low level routines that
	 * manipulates lock entries;
	 * place where real locking codes reside;
	 * it is (in most cases) independent of network code
	 */

#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <bstring.h>
#include <assert.h>
#include <sys/file.h>
#include <sys/param.h>
#include "prot_lock.h"
#include "priv_prot.h"
#include <rpcsvc/sm_inter.h>
#include "sm_res.h"

#define same_proc(x, y) (obj_cmp(&x->lck.oh, &y->lck.oh))

static int init_reclock ( struct reclock *to , struct reclock *from );
void rem_process_lock ( struct data_lock *a );
static int add_process_lock ( struct data_lock *a );
static int mond ( char *site , int proc , int i );
static int contact_monitor ( int proc , struct lm_vnode *new);

static struct process_locks *proc_locks;


extern int	LockID;
extern int	pid;		/* used by status monitor */
extern int 	debug;
extern char	hostname[];


/*
 * Function merge_lock(l, list)
 *
 * This function merges the lock 'l' into the list. The validity of this
 * operation has already been checked by the higher levels of the locking
 * code. The merge operation can cause one or more locks to become coalesced
 * into one lock.
 *
 * This function handles all three cases :
 *	The lock is completely within an existing lock (easy)
 *	The lock is touching one or more other locks (coalesce)
 *	The lock is a completely new lock. (not too tough)
 */
void
merge_lock(req, list)
	struct reclock	*req;	/* The lock in question */
	struct reclocklist *list;  /* List to add it too   */
{
	struct data_lock	*a;
	struct data_lock	*n;
	struct data_lock	*l = &req->lck.lox;
	struct reclock		*tmp_req, *t;
	struct reclocklist	spanlist; /* Temporary list of spanned locks */
	struct process_locks	*p;
	int			done_flag = FALSE;

	if (debug > 1)
		printf("enter merge_lock(req %x list %x)\n", req, list);

	assert(l->magic);

	for (p = proc_locks; p && p->pid != l->pid; p = p->next)
		continue;

	if (p != NULL) {
		INIT_RECLIST(spanlist);

		for (a = p->lox; a; a = n) {
		    assert(a->magic);

		    /* Cache the next pointer */
		    if ((n = a->Next) == p->lox)
			    done_flag = TRUE;
	
		    if (obj_cmp(&(req->lck.fh), &(a->req->lck.fh)) &&
			SAMEOWNER(l, a) && (l->type == a->type)) {

			/* Case one, it is completely within another lock */
			if (WITHIN(l, a)) {
				req->rel = 1;	/* not saved, can be freed */
				return; /* merge successful */
			}
	
			/* Case two, it completely surrounds one or more locks*/
#ifdef LOCK_DEBUG
			printf("MERGE_LOCK : END(l)=%d TOUCHING(l, a)=%d\n",
				END(l), TOUCHING(l, a));
#endif
			if (TOUCHING(l, a) || ADJACENT(l, a)) {
				/* remove this lock from this list */
				rem_process_lock(a);
				if ((tmp_req = del_req_from_list(a, list))
			    		!= NULL)
					/* Add it to the spanned list */
					add_req_to_list(tmp_req, &spanlist);
			}
		    }
		    if (done_flag)
			    break;
		}
	
		/*
 		 * Case two continued, merge the spanned locks.  Expand l
 		 * to include the furthest "left" extent of all locks on
 		 * the span list -- and to include the furthest "right" extent
 		 * as well.  Remove all of the spanned locks.
 		 */
		for (tmp_req = spanlist.rnext;
		     tmp_req != (struct reclock *)&spanlist; tmp_req = t) {

			n = &tmp_req->lck.lox;
			if (n->base < l->base) {
				if (END(l) >= 0)
					l->length += l->base - n->base;
				l->base = n->base;
			}
			if (END(l) >= 0 && END(n) >= 0)
				l->length = (END(l) < END(n) ? 
					END(n) : END(l)) + 1 - l->base;
			else
				l->length = LOCK_TO_EOF;

			t = tmp_req->rnext;
			rm_req(tmp_req);
			tmp_req->rel = 1;
			release_reclock(tmp_req);
		}
	}
	
	/* Case three, it's a new lock with no siblings */
	if (add_process_lock(l))
		add_req_to_list(req, list);
}

void
lm_unlock_region(req, list)
	struct reclock		*req; 	/* The region to unlock */
	struct reclocklist	*list;	/* The list we are unlocking from */
{
	struct data_lock 	*t,	/* Some temporaries */
				*n,
				*x;
	struct reclock		*tmp_req;
	struct reclock		*tmp_p;
	struct data_lock	*l = &req->lck.lox;
	struct process_locks	*p;
	int			done_flag = FALSE;

	if (debug > 1)
		printf("enter lm_unlock_region(req %x list %x)\n",
			req, list);

	for (p = proc_locks; p && p->pid != l->pid; p = p->next)
		continue;

	if (p != NULL) {
	    for (t = p->lox; t; t = n) {
		assert(t->magic);

		if ((n = t->Next) == p->lox)
			done_flag = TRUE;

		if (obj_cmp(&(req->lck.fh), &(t->req->lck.fh)) &&
		    (((l->type && SAMEOWNER(t, l)) || 
		     (!l->type && l->pid == t->pid)) && TOUCHING(l, t))) {

		     if (WITHIN(t, l)) {
			/* Remove blocking references */
out:
			if ((tmp_req = del_req_from_list(t, list)) != NULL) {
				rem_process_lock(t);
				tmp_req->rel = 1;
				release_reclock(tmp_req);
			}
		     /* case one region 'a' is to the left of 't' */
		     } else if (l->base <= t->base) {
			if (END(t) >= 0 && END(l) >= 0) {
				t->length = END(t) - END(l);
				t->base = END(l) + 1;
			} else if (END(t) < 0 && END(l) >= 0) {
				t->length = LOCK_TO_EOF;
				t->base = END(l) + 1;
			} else if ((END(t) >= 0 && END(l) < 0) ||
				(END(t) < 0 && END(l) < 0)) {
				goto out;
			}
		     /* Case two new lock is to the right of 't' */
		     } else if (((END(l) >= 0) && (END(t) >= 0) &&
				(END(l) >= END(t))) || (END(l) < 0)) {
			t->length = l->base - t->base;
		     /* Case three, new lock is in the middle of 't' */
		     } else {
			/*
			 * This part can fail if there isn't another
			 * lock entry and 'n' lock_refs available to
			 * build its dependencies.
			 */
			 if ((tmp_req = get_reclock()) != NULL) {
				FOREACH_RECLIST(list, tmp_p) {
					if (&tmp_p->lck.lox == t)
						break;
				}
				if (tmp_p == (struct reclock *)list) {
					tmp_req->rel = 1;
					release_reclock(tmp_req);
					return;
				}

				/* Copy the original via structure assignment */
				*tmp_req = *tmp_p;

				if (init_reclock(tmp_req, tmp_p)) {
				    x = &tmp_req->lck.lox;	
				    /* give it a fresh id */
				    x->LockID = LockID++;	
				    t->length = l->base - t->base;
				    if (END(x) >= 0 && END(l) >= 0) {
					    x->length = END(x) - END(l);
					    x->base = END(l) + 1;
				    } else if (END(x) < 0 && END(l) >= 0) {
					    x->length = LOCK_TO_EOF;
					    x->base = END(l) + 1;
				    }
				    if (add_process_lock(x))
					    add_req_to_list(tmp_req, list);
				    else {
					    tmp_req->rel = 1;
					    release_reclock(tmp_req);
				    }
				} else {
				    tmp_req->rel = 1;
				    release_reclock(tmp_req);
				}
			}
		     }
		}
		if (done_flag)
			break;
	    }
	}
	return;
}

static int
init_reclock(to, from)
	struct reclock	*to;
	struct reclock	*from;
{
	to->rprev = to->rnext = NULL;

	if (from->cookie.n_len) {
		if (obj_copy(&to->cookie, &from->cookie) == -1)
			return(FALSE);
	} else
		to->cookie.n_bytes= 0;

	if ((to->alock.caller_name = copy_str(from->alock.caller_name)) 
		== NULL)
		return(FALSE);
	if (from->alock.clnt_name == from->alock.caller_name)
		to->alock.clnt_name = to->alock.caller_name;
	else {
		if ((to->alock.clnt_name = copy_str(from->alock.clnt_name)) 
			== NULL)
		return(FALSE);
	}
	if (from->alock.server_name == from->alock.caller_name)
		to->alock.server_name = to->alock.caller_name;
	else {
		if ((to->alock.server_name = copy_str(from->alock.server_name)) 
			== NULL)
		return(FALSE);
	}

	if (from->alock.fh.n_len) {
		if (obj_copy(&to->alock.fh, &from->alock.fh) == -1)
			return(FALSE);
	} else
		to->alock.fh.n_len = 0;
	if (from->alock.oh.n_len) {
		if (obj_copy(&to->alock.oh, &from->alock.oh) == -1)
			return(FALSE);
	} else
		to->alock.oh.n_len = 0;

	to->lck.lox.Next = to->lck.lox.Prev = NULL;
	to->lck.lox.req = to; /* set the data_lock back ptr to req */
	to->lck.lox.magic = 1;
	return (TRUE);
}


void
rem_process_lock(a)
	struct data_lock *a;
{
	struct data_lock *t;

	if (debug > 1)
		printf("enter rem_process_lock\n");

	if ((t = a->MyProc->lox) != NULL) {
		assert(t->magic);
		while (t != a) {
			if (t->Next == a->MyProc->lox)
				return;
			else
				t = t->Next;
		}
		t->Prev->Next = t->Next;
		t->Next->Prev = t->Prev;
		if (t == a->MyProc->lox) {
			if (t->Next == t)
				a->MyProc->lox = NULL;
			else
				a->MyProc->lox = t->Next;
		}
	}
}

static int
add_process_lock(a)
	struct data_lock *a;
{
	struct process_locks	*p;
	struct data_lock *t;

	/*
	 * Go through table and match pid
	 */
	if (debug > 1)
		printf("enter add_process_lock\n");

	for (p = proc_locks; p && p->pid != a->pid; p = p->next)
		continue;

	if (p == NULL) {
		if ((p = get_proc_list()) == NULL) {
			return (FALSE);
		}
		p->pid = a->pid;
		p->next = proc_locks;
		proc_locks = p;
		p->lox = NULL;
	}

	if (debug > 1)
		printf(" PID : %d\n", p->pid);
	if ((t = p->lox) == NULL)
		p->lox = a->Prev = a->Next = a;
	else {
		assert(t->magic);
		t->Prev->Next = a;
		a->Next = t;
		a->Prev = t->Prev;
		t->Prev = a;
	}
	a->MyProc = p;
	return (TRUE);
}



bool_t
obj_cmp(a, b)
	struct netobj *a, *b;
{
	if (a->n_len != b->n_len)
		return (FALSE);
	return bcmp(&a->n_bytes[0], &b->n_bytes[0], a->n_len) == 0;
}

/*
 * duplicate b in a;
 * return -1, if malloc error;
 * returen 0, otherwise;
 */
int
obj_alloc(a, b, n)
	netobj *a;
	char *b;
	u_int n;
{
	a->n_len = n;
	if (n > 0) {
		if ((a->n_bytes = xmalloc(n)) == NULL) {
			return (-1);
		}
		bcopy(b, a->n_bytes, a->n_len);
	} else
		a->n_bytes = NULL;
	return (0);
}

/*
 * copy b into a
 * returns 0, if succeeds
 * return -1 upon error
 */
int
obj_copy(a, b)
	netobj *a, *b;
{
	if (b == NULL) {
		/* trust a is already NULL */
		if (debug > 1)
			printf(" obj_copy(a = %x, b = NULL)\n", a);
		return (0);
	}
	return (obj_alloc(a, b->n_bytes, b->n_len));
}

bool_t
same_fh(a, b)
	reclock *a, *b;
{
	return a && b && obj_cmp(&a->alock.fh, &b->alock.fh);
}


static bool_t
same_op(a, b)
	reclock *a, *b;
{
	return (a->block && b->block) || (!a->block && !b->block);
}

bool_t
same_bound(a, b)
	struct data_lock *a, *b;
{
	if (a && b && (a->base == b->base) &&
		(a->length == b->length))
		return (1);
	else
		return (0);
}

bool_t
same_type(a, b)
	struct data_lock *a, *b;
{
	if (a->type == b->type)
		return (1);
	else
		return (0);
}

bool_t
same_lock(a, b)
	reclock *a;
	struct data_lock *b;
{
	if (a && b && same_type(&(a->lck.lox), b) && 
		same_bound(&(a->lck.lox), b) &&
		SAMEOWNER(&(a->lck.lox), b))
		return (TRUE);
	else
		return (FALSE);
}


bool_t
simi_lock(a, b)
	reclock *a, *b;
{
	if (same_proc(a, b) && same_op(a, b) &&
	    WITHIN(&b->lck.lox, &a->lck.lox))
		return (TRUE);
	else
		return (FALSE);
}


/*
 * translate monitor calls into modifying monitor chains
 * returns 0, if success
 * returns -1, in case of error
 */
int
add_mon(a, i)
	reclock *a;
	int i;
{
	if (debug > 1) {
		printf("add_mon: server \"%s\" clnt \"%s\"\n",
			a->lck.server_name, a->lck.clnt_name);
	}
	if (strcasecmp(a->lck.server_name, a->lck.clnt_name) == 0)
		/* local case, no need for monitoring */
		return (0);
	if (strcasecmp(a->lck.server_name, hostname) != 0) { /* client */
		if (strlen(hostname) &&
			mond(hostname, PRIV_RECOVERY, i) == -1)
			return (-1);
		if (strlen(a->lck.server_name) &&
			mond(a->lck.server_name, PRIV_RECOVERY, i) == -1)
			return (-1);
	} else {			/* server */
		if (strlen(a->lck.clnt_name) &&
			mond(a->lck.clnt_name, PRIV_CRASH, i) == -1)
			return (-1);
	}
	return (0);
}

/*
 * mond set up the monitor ptr;
 * it return -1, if no more free mp entry is available when needed
 * or cannot contact status monitor
 */
static int
mond(site, proc, i)
	char *site;
	int proc;
	int i;
{
	struct lm_vnode *new;

	if (i == 1) {		/* insert! */
		if ((new = find_me(site)) == NULL) {
			if (( new = get_me()) == NULL)
				return (-1);
			if ((new->server_name = strdup(site)) == NULL) {
				release_me(new);
				return (-1);
			}

			if (contact_monitor(proc, new) == -1) {
				release_me(new);
				return (-1);
			}
			INIT_RECLIST(new->exclusive);
			INIT_RECLIST(new->shared);
			INIT_RECLIST(new->pending);
			insert_me(new);
		}
	} else { /* i== 0; delete! */
		(void) find_me(site);
	}
	return (0);
}

static int
contact_monitor(proc, new)
	int proc;
	struct lm_vnode *new;
{
	struct stat_res *resp;
	struct priv_struct priv;

	if (debug > 1)
		printf("enter contact_monitor: %d %s\n",
			proc, new->server_name);

	priv.pid = pid;
	priv.priv_ptr = (int *) new;

	if ( !strcasecmp(new->server_name, hostname) ) {
		return (0);
	}
	resp = stat_mon(new->server_name, hostname, PRIV_PROG, PRIV_VERS,
		proc, SM_MON, sizeof(priv), (char *)&priv);
	if (resp->res_stat == stat_succ) {
		if (resp->sm_stat == stat_succ) {
			return (0);
		}
		syslog(LOG_ERR,
		     "site %s does not have status monitor service", new->server_name);
		return (-1);
	}
	syslog(LOG_ERR, "rpc.lockd cannot contact local statd");
	return (-1);
}
