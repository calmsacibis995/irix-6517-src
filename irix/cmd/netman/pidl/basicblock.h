#ifndef BASICBLOCK_H
#define BASICBLOCK_H
/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 * Basic blocks are linked using a first-successor/next-sibling scheme,
 * with non-null previous-sibling links for all but the first successor.
 * The predecessor link points at the last sibling to which this block
 * is a successor.
 */

typedef struct basicblock BasicBlock;

struct basicblock {
	struct scope	*bb_scope;	/* closest enclosing scope */
	struct treenode	*bb_leader;	/* first node in block list */
	BasicBlock	*bb_next;	/* next sibling in DAG */
	BasicBlock	*bb_prev;	/* and previous sibling */
	BasicBlock	*bb_succ;	/* list of successor blocks */
	BasicBlock	*bb_pred;	/* and predecessor blocks */
	unsigned short	bb_prefcnt;	/* counts predecessor references */
	unsigned short	bb_dfnum;	/* depth-first number */
	unsigned short	bb_visited:1;	/* flag for depth-first search */
	unsigned short	bb_formal:1;	/* protocol formal argument bb */
	short		bb_width;	/* width of all fields in this bb */
	struct bitset	*bb_gen;	/* set of defs in bb that leave it */
	struct bitset	*bb_out;	/* set of all defs that leave bb */
	struct bitset	*bb_in;		/* set of defs that reach bb */
};

extern BasicBlock	*basicblock(struct context *, struct treenode *,
				    BasicBlock *, BasicBlock *);
extern int		checkblocks(BasicBlock *, struct context *);
extern void		destroyblocks(BasicBlock *);

#endif
