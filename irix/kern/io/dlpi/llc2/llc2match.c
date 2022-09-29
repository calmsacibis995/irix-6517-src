/******************************************************************
 *
 *  SpiderX25 - LLC2 Multiplexer
 *
 *  Copyright 1993 Spider Systems Limited
 *
 *  LLC2MATCH.C
 *
 *    Ethernet and LLC1 matching code
 *
 ******************************************************************/

/*
 *	 /net/redknee/projects/common/PBRAIN/SCCS/pbrainF/dev/sys/llc2/0/s.llc2match.c
 *	@(#)llc2match.c	1.5
 *
 *	Last delta created	11:20:34 5/25/93
 *	This file extracted	16:55:33 5/28/93
 *
 */

#ident "@(#)llc2match.c	1.5 (Spider) 5/28/93"

#ifdef DEBUG
extern int      llc2_debug;

#define LLC2_PRINTF(str, arg)   if (llc2_debug) printf(str, arg);

#else
#define LLC2_PRINTF(str, arg)

#endif /* DEBUG */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/snet/uint.h>
#include <sys/stream.h>
#include "sys/snet/llc2match.h"
#include "sys/snet/llc2match_if.h"
#include <sys/dlsap_register.h>

static void add_mask _P((struct match *to, struct match *from));
static void recalc_mask _P((struct match *to));
static struct match *add_match _P((caddr_t handle, queue_t *q, UINT8 flags,
	uint8 *start, UINT16 length));
static int delete_match _P((caddr_t handle, struct match *current));
static int do_match _P((struct match *current, uint8 *start1, UINT16 length1,
	uint8 *start2, UINT16 length2));
static struct match *lookup_match _P((caddr_t handle, uint8 *start1,
	UINT16 length1, uint8 *start2, UINT16 length2));
static struct match *find_match _P((caddr_t handle, caddr_t *nextp,
	queue_t *q, UINT8 flags, uint8 *start, UINT16 length));
#ifdef SGI
static int llc_memcmp _P((uint8 *s1, uint8 *s2, int n));
#else
static int memcmp _P((char *s1, char *s2, int n));
#endif
#ifdef DEBUG
static void print_match_info _P((struct match_info *info));
static void print_match _P((struct match *current));
#endif
extern void send_down_unreg(queue_t *, mblk_t *, queue_t *, uint16, uint16);

/*
 * The function should be called to initialise a new set-up on which 
 * matching is to be performed. The handle returned should be provided
 * as a parameter to all calls relating to the same matching setup.
 * If resources cannot be allocated, then NULL is returned.
 */
caddr_t
init_match()
{
	mblk_t *mp = allocb(sizeof(struct match_info), BPRI_MED);
	struct match_info *info;
	caddr_t handle;

	if (mp == (mblk_t *)0)		/* failed to allocate block */
	{
		LLC2_PRINTF("init_match: failed to allocate block of size %d\n",
			sizeof(struct match_info));
		return ((caddr_t ) 0);
	}

	mp->b_wptr += sizeof(struct match_info);

	info = (struct match_info *)mp->b_rptr;
	handle = (caddr_t) info;

	info->my_mp = mp;
	info->root = info->hash_start = (struct match *)0;


	/* add an empty string as the root of the system */
	if (add_match(handle, (queue_t *)0, ROOT, (uint8 *)0, 0) ==
		(struct match *)0)
	{
		end_match(handle);
		return ((caddr_t) 0);
	}

	return (handle);
}

/*
 * This function should be called to free the data used by a match set-up.
 */
void
end_match(handle)
caddr_t handle;
{
	struct match_info *info = (struct match_info *) handle;
	caddr_t next = (caddr_t) 0;
	struct match *current;

	ASSERT(handle != (caddr_t)0);

	/* delete all except the root entry */
	while ((current = find_match(handle, &next, (queue_t *)0,
		ACTIVE | INACTIVE, (uint8 *)0, 0)) != (struct match *)0)
			(void) delete_match(handle, current);

	ASSERT(info->root->first_child == (struct match *)0);
	ASSERT(info->root->hash_prev == (struct match *)0);
	ASSERT(info->root->hash_next == (struct match *)0);

	/* delete the root entry */
	if (info->root)
		(void) delete_match(handle, info->root);


	freemsg(info->my_mp);
}

/*
 * This function updates the masks used by the parent, which are used
 * to improve performance in searching for an entry.
 */
static void
add_mask(to, from)
struct match *to;
struct match *from;
{
	uint8 value;

	ASSERT(from->length > to->length);

	/* get the value to use in the masks */
	value = from->start[to->length];

	to->ones_mask &= value;
	to->zeros_mask &= ~value;
}

/*
 * This function recalculates from scratch the masks used by a parent.
 * This is necessary when a child have been (re)moved.
 */
static void
recalc_mask(to)
struct match *to;
{
	struct match *child;

	to->ones_mask = to->zeros_mask = 0xFF;

	for (child = to->first_child; child != (struct match *)0;
		child = child->rsib)
			add_mask(to, child);
}

static struct match *
add_match(handle, q, flags, start, length)
caddr_t handle;
queue_t *q;
uint8 flags;
uint8 *start;
uint16 length;
{
	struct match_info *info = (struct match_info *) handle;
	int size;
	struct match *new, *current;
	mblk_t *mp;
	int diff;

	ASSERT(handle != (caddr_t)0);

	size = (int)sizeof(struct match) - 1 + length;
	mp = allocb(size, BPRI_MED);
	if (mp == (mblk_t *)0)
	{
		LLC2_PRINTF("add_match: failed to allocate block of size %d\n",
			size);
		return ((struct match *)0);
	}
	
	mp->b_wptr += size;

	/* initialise the structure */
	new = (struct match *)mp->b_rptr;
	new->my_mp = mp;
	new->lsib = new->rsib = new->parent = (struct match *)0;
	new->first_child = new->last_child = (struct match *)0;

	new->q = q;
	new->ones_mask = 0xFF;
	new->zeros_mask = 0xFF;
	new->flags = flags;
	new->length = length;
	if (length)
		bcopy((char *) start, (char *)new->start, length);

	current = info->root;

	/*
	 * Note: INACTIVE entries are not added to the binary tree.
	 * They are only in the hash list.
	 */

	if (flags & ACTIVE)
	{
		for (;;)
		{
			/* find the minimum of the two lengths */
			length = ((current->length < new->length)?
				current->length : new->length);

			diff = llc_memcmp(new->start, current->start,
					length);

			if (diff == 0)
			{
				/* they are the same up to the minimum length */

				if (current->length < new->length)
				{
					/*
					 * The current entry is an ancestor of
					 * the new one.
					 */

					if (current->first_child ==
						(struct match *)0)
					{
						/* new entry is first child */
						current->first_child =
							current->last_child =
								new;
						new->parent = current;
						add_mask(current, new);
						break;
					}

					/*
					 * Try again with the current entry's
					 * first child.
					 */
					current = current->first_child;
					continue;
				}
				else if (current->length > new->length)
				{
					/*
					 * The new entry is the parent of the
					 * current one, so replace the current
					 * one with the new one, and make the
					 * current entry a child of the new
					 * one. There may also be other
					 * siblings to the right of the current
					 * one which the new entry wishes to
					 * adopt.
					 */
					new->parent = current->parent;
					new->first_child = current;

					/* update the left sibling list */
					if (current->lsib)
						current->lsib->rsib = new;
					else
						current->parent->first_child =
							new;
					new->lsib = current->lsib;
					current->lsib = (struct match *) 0;
					current->parent = new;

					/* update the right sibling list */
					for (; current->rsib !=
						(struct match *)0;
							current = current->rsib)
					{
						if (llc_memcmp(current->rsib->start,
						    new->start, length) == 0)
						{
							/*
							 * adopted by new
							 * entry
							 */
							current->rsib->parent =
								new;
						}
						else
							break;	
					}

					new->last_child = current;
					new->rsib = current->rsib;
					if (current->rsib)
					{
						current->rsib->lsib = new;
						current->rsib =
							(struct match *)0;
					}
					else
					{
						/* all siblings transferred */
						new->parent->last_child = new;
					}

					recalc_mask(new);
					break;
				}
				else /* current->length == new->length */
				{
					/*
					 * An attempt is being made to add an
					 * ACTIVE entry with exactly the same
					 * data. This is failed as there can
					 * only be one ACTIVE entry at a time.
					 */

					freeb(mp);
					return ((struct match *)0);
				}
			}

			/*
			 * It is not related to the current sibling, so
			 * see if we are in the correct place to insert
			 * the entry.
			 */

			if (diff < 0)	/* new < current, so correct place */
			{
				/* insert the entry before the current one */

				/* update the sibling list */
				if (current->lsib)
					current->lsib->rsib = new;
				else
					current->parent->first_child = new;

				new->parent = current->parent;
				new->rsib = current;
				new->lsib = current->lsib;
				current->lsib = new;
				add_mask(new->parent, new);
				break;
			}

			if (current->rsib == (struct match *)0)
			{
				/* no more siblings, so add after current one */
				current->parent->last_child = new;
				new->parent = current->parent;
				new->lsib = current;
				new->rsib = (struct match *)0;
				current->rsib = new;
				add_mask(new->parent, new);
				break;
			}

			/* try the next sibling */
			current = current->rsib;
		}
	} else if (flags & ROOT)
		info->root = new;

	/* add to start of hash list */
	if (info->hash_start)
	{
		info->hash_start->hash_prev = new;
		new->hash_next = info->hash_start;
		info->hash_start = new;
	}
	else
	{
		info->hash_start = new;
		new->hash_next = (struct match *)0;
	}
	new->hash_prev = (struct match *)0;

	return (new);
}

/*
 * Deletes the given entry from the tree. Note that "match" must point
 * to a valid structure. Returns 0 on success, -1 on failure.
 */
static int
delete_match(handle, current)
caddr_t handle;
struct match *current;
{
	struct match_info *info = (struct match_info *) handle;

	ASSERT(handle != (caddr_t)0);
	ASSERT(current != (struct match *)0);

	if (current->parent == (struct match *)0)
	{
		/*
		 * This entry has no parent, i.e. it is the root or
		 * an INACTIVE entry.
		 * It can only be deleted if it has no children or
		 * siblings.
		 */

		if (current->first_child || current->lsib || current->rsib)
			return (-1);
	}

	/* remove the entry from the hash list */
	if (current->hash_prev)
		current->hash_prev->hash_next = current->hash_next;
	else
		info->hash_start = current->hash_next;

	if (current->hash_next)
		current->hash_next->hash_prev = current->hash_prev;

	if (current->parent == (struct match *)0)
	{
		freeb(current->my_mp);
		return (0);
	}

	if (current->first_child == (struct match *)0)
	{
		/* if no children, then remove from siblings */

		if (current->lsib)
			current->lsib->rsib = current->rsib;
		else
			current->parent->first_child = current->rsib;

		if (current->rsib)
			current->rsib->lsib = current->lsib;
		else
			current->parent->last_child = current->lsib;

	}
	else
	{
		/*
		 * The parent inherits our children. 
		 * Insert them in place of this one.
		 */
		struct match *child;

		for (child = current->first_child;
			child != (struct match *)0; child = child->rsib)
				child->parent = current->parent;

		if (current->lsib)
		{
			current->lsib->rsib = current->first_child;
			current->first_child->lsib = current->lsib;
		}
		else
			current->parent->first_child = current->first_child;

		if (current->rsib)
		{
			current->rsib->lsib = current->last_child;
			current->last_child->rsib = current->rsib;
		}
		else
			current->parent->last_child = current->last_child;
	}
	
	recalc_mask(current->parent);

	freeb(current->my_mp);
	return (0);
}

/*
 * This routine compares the entry supplied in "current" with the data
 * supplied. It returns POSSIBLE_MATCH is it is a possible match, but
 * where there may be other entries which are a better match. It returns
 * DEFINITE_MATCH is the entry definitely matches. It it does not match,
 * then it returns STOP_MATCH if there are no more entries which can match,
 * or TRY_MATCH if the next entry should be tried. NB: STOP_MATCH should
 * be interpreted as TRY_MATCH is a linear search is being performed.
 *
 * It assumes that this data can be longer than the entry supplied.
 * The data may be split into two parts, with start1/length1 associated
 * with the first part and start2/length2 associated with second part.
 */
static int
do_match(current, start1, length1, start2, length2)
struct match *current;
uint8 *start1;
uint16 length1;
uint8 *start2;
uint16 length2;
{
	uint16 length = length1 + length2;
	uint8 byte;
	int ldiff = current->length - length;
	int cdiff;
	
	if ((current->flags & ACTIVE) == 0)
	{
		if (current->flags & INACTIVE)
			return (TRY_MATCH);

		/* must be root */
		byte = start1[0];

		if ((byte & current->ones_mask) != current->ones_mask ||
			(byte & current->zeros_mask) != 0)
			return (STOP_MATCH);

		return (POSSIBLE_MATCH);
	}


	if (ldiff > 0)	/* cannot match as data in entry is longer */
		return (STOP_MATCH);

	if (ldiff == 0)
	{
		cdiff = llc_memcmp(current->start, start1, length1);

		if (cdiff < 0)
			return (TRY_MATCH);
		else if (cdiff > 0)
			return (STOP_MATCH);

		if (length2 == 0)	/* no second field supplied */
			return (DEFINITE_MATCH);

		cdiff = llc_memcmp(current->start + length1, start2, length2);

		if (cdiff < 0)
			return (TRY_MATCH);
		else if (cdiff > 0)
			return (STOP_MATCH);

		return (DEFINITE_MATCH);
	}

	/* data is longer than the entry */

	ldiff = current->length - length1;

	if (ldiff <= 0)
	{
		/* entry fits into first part */
		cdiff = llc_memcmp(current->start, start1, current->length);

		if (cdiff < 0)
			return (TRY_MATCH);
		else if (cdiff > 0)
			return (STOP_MATCH);

		/*
		 * if we have no children, then it is a definite match
		 */
		if (current->first_child == (struct match *)0)
			return (DEFINITE_MATCH);

		if (ldiff == 0)
			byte = start2[0];
		else
			byte = start1[current->length];
	}
	else
	{
		/* entry does not fit into first part */
		cdiff = llc_memcmp(current->start, start1, length1);

		if (cdiff < 0)
			return (TRY_MATCH);
		else if (cdiff > 0)
			return (STOP_MATCH);

		cdiff = llc_memcmp(current->start + length1, start2, ldiff);

		if (cdiff < 0)
			return (TRY_MATCH);
		else if (cdiff > 0)
			return (STOP_MATCH);

		/*
		 * if we have no children, then it is a definite match
		 */
		if (current->first_child == (struct match *)0)
			return (DEFINITE_MATCH);

		byte = start2[ldiff];
	}

	/*
	 * Check the first byte after the match, to see one of the
	 * children might match.
	 */

	if ((byte & current->ones_mask) != current->ones_mask ||
		(byte & current->zeros_mask) != 0)
			return (DEFINITE_MATCH);

	return (POSSIBLE_MATCH);
}

/*
 * This is a fast lookup routine, which is used when trying to
 * just the registration field. It makes use of hashing, and a
 * binary tree for speed. It only looks for ACTIVE entries.
 */

static struct match *
lookup_match(handle, start1, length1, start2, length2)
caddr_t handle;
uint8 *start1;
uint16 length1;
uint8 *start2;
uint16 length2;
{
	struct match_info *info = (struct match_info *) handle;
	struct match *current, *best = (struct match *)0;
	int type = TRY_MATCH;
#if HASH_MAX > 0
	uint16 count = 0;
#endif
#if HASH_MOVE > 0
	uint16 best_count = 0;
#endif


#if HASH_MAX > 0
	/* start by looking through the hash table */
	for (count = 0, current = info->hash_start;
		current != (struct match *) 0 && count < HASH_MAX;
			current = current->hash_next, count++)
	{
		type = do_match(current, start1, length1, start2, length2);

		if (type == TRY_MATCH || type == STOP_MATCH)
			continue;

		/* a possible or definite match */
		if (best == (struct match *)0 ||
			current->length > best->length)
		{
			/* found a better match */
			best = current;
#if HASH_MOVE > 0
			best_count = count;
#endif
		}

		if (type == DEFINITE_MATCH)
			break;

		/* it's only a POSSIBLE_MATCH, so keep looking */
	}
#else /* HASH_MAX > 0 */
	current = info->root;
#endif

	if (type != DEFINITE_MATCH && current)
	{
		/*
		 * The entry was not found exactly in the first HASH_MAX
		 * entries, so start searching the binary tree.
		 *
		 * If "best" is set, then that is the entry to start
		 * with, otherwise start at the root. We start at the
		 * first child of the entry.
		 */
		current = (best? best : info->root)->first_child;

		for (;;)
		{
			if (current == (struct match *)0)
				/* no more entries to look for */
				break;

			type = do_match(current, start1, length1, start2,
				length2);

			if (type == STOP_MATCH)
				break;

			if (type == TRY_MATCH)
			{
				current = current->rsib;
				continue;
			}

			/* found a better match */
			best = current;
#if HASH_MOVE > 0
			best_count = HASH_MOVE;	/* force a hash move */
#endif

			if (type == DEFINITE_MATCH)
				break;

			/* a POSSIBLE_MATCH, so look at its children */

			current = current->first_child;
		}
	}

#if HASH_MOVE > 0 && HASH_MAX > 0
	if (best)		/* found a match */
	{
		/* if far down the list, then put it to the top */
		if (best_count >= HASH_MOVE)
		{
			ASSERT(best->hash_prev != (struct match *)0);

			best->hash_prev->hash_next = best->hash_next;

			if (best->hash_next)
				best->hash_next->hash_prev =
					best->hash_prev;

			best->hash_prev = (struct match *)0;
			best->hash_next = info->hash_start;
			info->hash_start->hash_prev = best;
			info->hash_start = best;
		}
	}
#endif /* HASH_MOVE > 0 && HASH_MAX > 0 */

	return (best);
}

/*
 * This routine performs a linear search through the table.
 * It looks for an entry matching the parameters "q", "flags"
 * "start" (of length "length"). If "q" is NULL it will search
 * for any queue. If "start" is NULL, then any registration string
 * will be accepted. If a match is found, then the a pointer to
 * the associated "match" structure is returned. If the end of the
 * table is reached without finding a match, then NULL is returned.
 *
 * On the initial call to the routine "nextp" should point to a
 * variable of type "caddr_t" which holds NULL. If there is more
 * than one entry to be found, then after receiving the first entry
 * "find_match" must be called again with "nextp" pointing to the
 * same value of the previous call.
 */
static struct match *
find_match(handle, nextp, q, flags, start, length)
caddr_t handle;
caddr_t *nextp;
queue_t *q;
uint8 flags;
uint8 *start;
uint16 length;
{
	struct match_info *info = (struct match_info *) handle;
	struct match *current;

	/*
	 * If *nextp is non-NULL, then we continue from *nextp.
	 * If it is NULL, we search from the start. If it is -1,
	 * we are at the end.
	 */
	if (nextp)
	{
		if (*nextp == (caddr_t) -1L)		/* at end */
			return ((struct match *)0);

		if (*nextp)
			current = (struct match *) *nextp;
		else
			current = info->hash_start;
	}
	else
		current = info->hash_start;

	for (; current != (struct match *) 0; current = current->hash_next)
	{
		if (q && current->q != q)
			continue;

		if ((flags & current->flags) == 0)
			continue;

		if (start)
		{
			if (length != current->length)
				continue;

			if (length &&
				llc_memcmp(start, current->start, length) != 0)
				continue;
		}

		/* we have found a match */
		if (nextp)
		{
			*nextp = (caddr_t)current->hash_next;

			/* if the last, set to -1 to indicate the end */
			if (*nextp == (caddr_t) 0)
				*nextp = (caddr_t) -1L;
		}

		return (current);
	}
	return ((struct match *)0);
}

/*
 * This function returns the upper queue on which a message should be
 * placed. If no queue has registered for the types, then NULL is returned.
 */
queue_t *
lookup_sap(handle, lengthp, start1, length1, start2, length2)
caddr_t handle;
uint16 *lengthp;
uint8 *start1;
uint16 length1;
uint8 *start2;
uint16 length2;
{
	struct match *current;

	if (handle == (caddr_t )0)
		return ((queue_t *) 0);

	current = lookup_match(handle, start1, length1, start2, length2);

	if (current == (struct match *)0)
		return ((queue_t *) 0);

	if (lengthp)
		*lengthp = current->length;

	return (current->q);
}

/*
 * This function is used to register for a given SAP. It returns 0 on
 * success, and -1 on failure.
 */
int
register_sap(handle, q, start, length)
caddr_t handle;
queue_t *q;
uint8 *start;
uint16 length;
{
	caddr_t next = (caddr_t) 0;
	struct match *current, *new;
	int ldiff;


	/*
	 * Look for the entry, to ensure no duplicate
	 */
	if (find_match(handle, &next, q, INACTIVE | ACTIVE,
		start, length) != (struct match *)0)
	{
		/* entry found */
		return (-1);
	}

	new = add_match(handle, q, ACTIVE, start, length);

	if (new == (struct match *)0)
	{
		return (-1);
	}

	/*
	 * Check to see if there is a previous active entry,
	 * which this entry is a superset off. If there is, then it
	 * will need to be changed to be INACTIVE. If there is an entry
	 * which is a superset of the new one, then the new one becomes
	 * INACTIVE.
	 */

	next = (caddr_t) 0;

	while ((current = find_match(handle, &next, q,
		ACTIVE, (uint8 *)0, 0)) != (struct match *)0)
	{
		if (current == new)
			continue;

		ldiff = current->length - length;

		if (ldiff > 0)
		{
			if (llc_memcmp(current->start, start, length) != 0)
					continue;

			/*
			 * This entry is a superset of the new one.
			 * Delete the new entry and re-insert it as INACTIVE.
			 */
			(void) delete_match(handle, new);
			
			new = add_match(handle, q, INACTIVE, start, length);

			if (new == (struct match *)0)
			{
				return (-1);
			}
			break;
		}

		if (ldiff < 0)
		{
			struct match *current2;

			if (llc_memcmp(current->start, start,
				current->length) != 0)
					continue;

			/*
			 * The new entry is a superset of this one.
			 * Insert this one as INACTIVE, and delete the active one.
			 */
			
			current2 = add_match(handle, current->q, INACTIVE,
				current->start, current->length);

			if (current2 == (struct match *)0)
			{
				(void) delete_match(handle, new);
				return (-1);
			}

			(void) delete_match(handle, current);
			break;
		}
	}

	return (0);
}

/*
 * This function is used to deregister for a given SAP. It returns 0
 * on success and -1 on failure.
 */
int
deregister_sap(handle, q, start, length)
caddr_t handle;
queue_t *q;
uint8 *start;
uint16 length;
{
	struct match *current, *delete, *best = (struct match *)0;
	caddr_t next = (caddr_t) 0;
	int best_length = 0;

	/*
	 * Find the entry to delete
	 */
	delete = find_match(handle, &next, q, INACTIVE | ACTIVE,
		start, length);

	if (delete == (struct match *)0)
	{
		/* entry not found */
		return (-1);
	}

	if (delete->flags & ACTIVE)
	{
		/*
		 * Find the best inactive entry (if there is one)
		 * which the deleted entry is a superset off. If one is found,
		 * then it will need to be changed to be ACTIVE.
		 */

		next = (caddr_t) 0;
		while ((current = find_match(handle, &next, q,
			INACTIVE, (uint8 *)0, 0)) != (struct match *)0)
		{
			if (current == delete)
				continue;

			if (current->length >= length)
				continue;

			if (llc_memcmp(current->start, start, current->length) != 0)
				continue;

			if (best_length > current->length)
				continue;
				
			best_length = current->length;
			best = current;
		}

		if (best)
		{
			/* found an entry to make active */

			/* add the entry */
			current = add_match(handle, best->q, ACTIVE,
				best->start, best->length);

			/*
			 * if an active entry could not be added,
			 * then just leave the entry in as INACTIVE
			 */

			if (current)
				(void) delete_match(handle, best);
		}
	}

	if (delete_match(handle, delete) < 0)
	{
		/* failed to delete the entry */
		return (-1);
	}
	return (0);
}

/*
 * This function is used to deregister all SAPs associated with a queue.
 */
void
deregister_q(handle, q)
caddr_t handle;
queue_t *q;
{
	caddr_t next = (caddr_t) 0;
	struct match *current;

	/* delete all matching the queue */
	while ((current = find_match(handle, &next, q,
		ACTIVE | INACTIVE, (uint8 *)0, 0)) != (struct match *)0)
			(void) delete_match(handle, current);
}

#ifdef SGI
/*
 * This function is used to send deregister messages to the downstream
 * queue associated with a queue.
 */
void
unregister_q(handle, uq, dq, length)
caddr_t handle;
queue_t *uq;
queue_t *dq;
uint16 length;
{
	caddr_t next = (caddr_t) 0;
	struct match *current;
	uint16 sap;

	/* find all matching the upper queue */
	while ((current = find_match(handle, &next, uq,
		ACTIVE | INACTIVE, (uint8 *)0, 0)) != (struct match *)0)
	{
		if (current->length < length)	/* too short */
			continue;

		if (length == 1)
			sap = current->start[0];
		else	/* machine order */
			sap = current->start[1] | current->start[0] << 8;

		(void)send_down_unreg(uq, (mblk_t *) 0, dq, sap, DL_LLC_ENCAP);
	}
}
#endif /* SGI */

#ifdef DEBUG
void
print_match_all(handle)
caddr_t handle;
{
	struct match_info *info = (struct match_info *) handle;
	struct match *current;
	caddr_t next = 0;

	print_match_info(info);

	while ((current = find_match(handle, &next, (queue_t *)0,
		ACTIVE | INACTIVE | ROOT, (uint8 *)0, 0)) != (struct match *)0)
		print_match(current);
}

static void
print_match_info(info)
struct match_info *info;
{
	printf("Match info record: ptr 0x%lx\n", info);

	if (info == (struct match_info *)0)
		return;

	printf("my_mp: 0x%lx, root: 0x%lx, hash_start: 0x%lx\n\n",
		info->my_mp, info->root, info->hash_start);
}

static void
print_match(current)
struct match *current;
{
	int i;

	printf("Match record: ptr 0x%lx\n", current);

	if (current == (struct match *)0)
		return;

	printf("my_mp: 0x%lx, lsib: 0x%lx, rsib: 0x%lx\n", current->my_mp,
		current->lsib, current->rsib);
	printf("parent: 0x%lx, 1st child: 0x%lx, 2nd child: 0x%lx\n",
		current->parent, current->first_child, current->last_child);
	printf("hash_prev: 0x%lx, hash_next: 0x%lx, q: 0x%lx\n",
		current->hash_prev, current->hash_next, current->q);
	printf("ones_mask: 0x%x, zeros_mask: 0x%lx, flags: %s%s%s\n",
		current->ones_mask, current->zeros_mask,
		(current->flags & ACTIVE)? "active " : "",
		(current->flags & INACTIVE)? "inactive " : "",
		(current->flags & ROOT)? "root" : "");
	printf("length: %d, 0x", current->length);
	
	for (i = 0; i < current->length; i++)
		printf("%x-", current->start[i]);
	printf("\n\n");
}
#endif

static int
#ifdef SGI
llc_memcmp(s1, s2, n)
register uint8 *s1;
register uint8 *s2;
#else
memcmp(s1, s2, n)
register char *s1;
register char *s2;
#endif
register int n;
{
	while (n--)
		if (*s1++ != *s2++)
			return (s1[-1] - s2[-1]);

	return (0);
}
