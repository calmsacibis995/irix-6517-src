/******************************************************************
 *
 *  SpiderX25 - LLC2 Multiplexer
 *
 *  Copyright 1993 Spider Systems Limited
 *
 *  LLC2MATCH.H
 *
 *    Ethernet and LLC1 matching code include file
 *
 ******************************************************************/

/*
 *	 /net/redknee/projects/common/PBRAIN/SCCS/pbrainF/dev/sys/llc2/0/s.llc2match.h
 *	@(#)llc2match.h	1.1
 *
 *	Last delta created	15:47:49 5/6/93
 *	This file extracted	16:55:34 5/28/93
 *
 */

/*
 * HASH_MAX indicates the number of hash entries to check before
 * giving up and using the binary tree.
 */
#define HASH_MAX		6

/*
 * If an entry found is not within the first HASH_MOVE entries in
 * the hash list, then it is moved into the first position.
 */
#define HASH_MOVE		3

/*
 * Defines returned by the "do_match" routine
 */
#define POSSIBLE_MATCH		1
#define DEFINITE_MATCH		2
#define TRY_MATCH		3
#define STOP_MATCH		4

/*
 * The "match" structure holds information on the current match value.
 */
struct match
{
	mblk_t *my_mp;			/* pointer to my message block */
	struct match *lsib;		/* left sibling */
	struct match *rsib;		/* right sibling */
	struct match *parent;		/* parent of this node */
	struct match *first_child;	/* first child of this node */
	struct match *last_child;	/* last child of this node */
	struct match *hash_prev;	/* previous in hash list */
	struct match *hash_next;	/* next in hash list */
	queue_t *q;			/* queue associated with this entry */
	uint8 ones_mask;		/* hash mask for 1's */
	uint8 zeros_mask;		/* hash mask for 0's */
	uint8 flags;			/* flags associated with this entry */
	uint16 length;			/* length of bytes to match */
	uint8 start[1];			/* start of array of bytes */
};

struct match_info
{
	mblk_t *my_mp;			/* pointer to my message block */
	struct match *root;		/* root of the matching tree */
	struct match *hash_start;	/* start of the hash list */
};


/*
 * values for "flags" field of "match" structure
 */

#define	ACTIVE		0x01

/*
 * The flag INACTIVE indicates that the entry is not currently active. This
 * happens when a registration has been performed for values which are a 
 * superset of a previous value.
 */
#define	INACTIVE	0x02

/*
 * This flag indicates that the entry is the root of the binary tree.
 * The entry cannot be deleted until it is the last in the tree.
 */
#define ROOT		0x04

/*
 * macro to check assertions 
 */
#ifdef ADEBUG
#define ASSERT(expr) \
	if (expr); else printf("Assertion failure: file %s, line %d\n", \
		__FILE__, __LINE__)
#else
#define ASSERT(expr)
#endif
