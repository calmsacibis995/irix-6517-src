#ident "$Header: /proj/irix6.5.7m/isms/irix/lib/klib/libutil/src/RCS/htnode.c,v 1.1 1999/02/23 20:38:33 tjm Exp $"

#include <stdio.h>
#include <sys/types.h>
#include <klib/htnode.h>

htnode_t *
next_htnode(htnode_t *curhtp) 
{ 
	htnode_t *htp = curhtp;

	if (htp->children) {
		return(htp->children);
	}
again:
	if ((htp->next != htp) && (htp->next != htp->parent->children)) {
		return(htp->next);
	}
	if ((htp = htp->parent) && htp->level) {
		goto again;
	}
	return((htnode_t *)NULL);
}

htnode_t *
prev_htnode(htnode_t *curhtp)
{
	htnode_t *lasthtp, *htp = curhtp;

	if ((htp->prev == htp) || 
			(htp->parent && (htp->prev == htp->parent->children->prev))) {
		return(htp->parent);
	}
	else if (htp->prev->children) {
		/* We have to walk down this sub-tree to the last node and return
		 * that.
		 */
		lasthtp = htp->prev;
		while (htp = next_htnode(lasthtp)) {
			if (htp == curhtp) {
				return(lasthtp);
			}
			lasthtp = htp;
		}
	}
	else {
		return(htp->prev);
	}
	return((htnode_t *)NULL);
}

static void
insert_before(htnode_t *htp, htnode_t *new)
{
	htp->prev->next = new;
	new->prev = htp->prev;
	new->next = htp;
	htp->prev = new;
}

static void
insert_after(htnode_t *htp, htnode_t *new)
{
	htp->next->prev = new;
	new->next = htp->next;
	new->prev = htp;
	htp->next = new;
}

/*
 * ht_insert_pier()
 */
void
ht_insert_pier(htnode_t *htp, htnode_t *new, int flags)
{
	if (flags & HT_BEFORE) {
		insert_before(htp, new);
	}
	else {
		insert_after(htp, new);
	}
	new->parent = htp->parent;
	new->level = htp->level;
}

/*
 * ht_insert_child()
 */
void
ht_insert_child(htnode_t *htp, htnode_t *new, int flags)
{
	if (!htp->children) {
		htp->children = new;
		new->next = new->prev = new;
	}
	else {
		if (flags & HT_BEFORE) {
			insert_before(htp->children, new);
		}
		else {
			insert_after(htp->children, new);
		}
	}
	new->parent = htp;
	new->level = (htp->level + 1);
}

/*
 * ht_insert()
 */
int
ht_insert(htnode_t *htp, htnode_t *new, int flags)
{
	if (!htp || !new) {
		return(1);
	}
	if (flags & HT_CHILD) {
		ht_insert_child(htp, new, flags);
	}
	else {
		ht_insert_pier(htp, new, flags);
	}
	return(0);
}

/*
 * ht_insert_next_htnode()
 *
 * This function will add the next htnode to a htree. Note that in 
 * order for this function to work properly, it is necessary that 
 * the level value for each htnode already be set.
 */
void
ht_insert_next_htnode(htnode_t *current, htnode_t *next)
{
    if (next->level > current->level) {
        ht_insert_child(current, next, HT_AFTER);
    }
    else if (next->level == current->level) {
        ht_insert_pier(current, next, HT_AFTER);
    }
    else {
        while (current->level > next->level) {
            current = current->parent;
        }
        ht_insert_pier(current, next, HT_AFTER);
    }
}

