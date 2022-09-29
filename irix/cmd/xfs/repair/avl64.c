/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1993 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/* to allow use by user-level utilities */

#ifdef STAND_ALONE_DEBUG
#define AVL_USER_MODE
#endif

#if defined(STAND_ALONE_DEBUG) || defined(AVL_USER_MODE_DEBUG)
#define AVL_DEBUG
#endif

#ifndef AVL_USER_MODE
#include "sys/param.h"
#include "sys/debug.h"
#include "sys/sysmacros.h"
#include "sys/cmn_err.h"
#else
#include <stdio.h>
#include <assert.h>
#define ASSERT assert
#endif


#include "avl64.h"

#define CERT	ASSERT

#ifdef AVL_DEBUG

static void
avl64_checknode(
	register avl64tree_desc_t *tree,
	register avl64node_t *np)
{
	register avl64node_t *back = np->avl_back;
	register avl64node_t *forw = np->avl_forw;
	register avl64node_t *nextino = np->avl_nextino;
	register int bal = np->avl_balance;

	ASSERT(bal != AVL_BALANCE || (!back && !forw) || (back && forw));
	ASSERT(bal != AVL_FORW || forw);
	ASSERT(bal != AVL_BACK || back);

	if (forw) {
		ASSERT(AVL_START(tree, np) < AVL_START(tree, forw));
		ASSERT(np->avl_forw->avl_parent == np);
		ASSERT(back || bal == AVL_FORW);
	} else {
		ASSERT(bal != AVL_FORW);
		ASSERT(bal == AVL_BALANCE || back);
		ASSERT(bal == AVL_BACK || !back);
	}

	if (back) {
		ASSERT(AVL_START(tree, np) > AVL_START(tree, back));
		ASSERT(np->avl_back->avl_parent == np);
		ASSERT(forw || bal == AVL_BACK);
	} else {
		ASSERT(bal != AVL_BACK);
		ASSERT(bal == AVL_BALANCE || forw);
		ASSERT(bal == AVL_FORW || !forw);
	}

	if (nextino == NULL)
		ASSERT(forw == NULL);
	else
		ASSERT(AVL_END(tree, np) <= AVL_START(tree, nextino));
}

static void
avl64_checktree(
	register avl64tree_desc_t *tree,
	register avl64node_t *root)
{
	register avl64node_t *nlast, *nnext, *np;
	__uint64_t offset = 0;
	__uint64_t end;

	nlast = nnext = root;

	ASSERT(!nnext || nnext->avl_parent == NULL);

	while (nnext) {

		avl64_checknode(tree, nnext);
		end = AVL_END(tree, nnext);

		if (end <= offset) {
			if ((np = nnext->avl_forw) && np != nlast) {
				nlast = nnext;
				nnext = np;
			} else {
				nlast = nnext;
				nnext = nnext->avl_parent;
			}
			continue;
		}

		nlast = nnext;
		if (np = nnext->avl_back) {
			if (AVL_END(tree, np) > offset) {
				nnext = np;
				continue;
			}
		}

		np = nnext;
		nnext = nnext->avl_forw;
		if (!nnext)
			nnext = np->avl_parent;

		offset = end;
	}
}
#else	/* ! AVL_DEBUG */
#define avl64_checktree(t,x)
#endif	/* AVL_DEBUG */


/*
 * Reset balance for np up through tree.
 * ``direction'' is the way that np's balance
 * is headed after the deletion of one of its children --
 * e.g., deleting a avl_forw child sends avl_balance toward AVL_BACK.
 * Called only when deleting a node from the tree.
 */
static void
retreat(
	avl64tree_desc_t *tree,
	register avl64node_t *np,
	register int direction)
{
	register avl64node_t **rootp = &tree->avl_root;
	register avl64node_t *parent;
	register avl64node_t *child;
	register avl64node_t *tmp;
	register int	bal;

	do {
		ASSERT(direction == AVL_BACK || direction == AVL_FORW);

		if (np->avl_balance == AVL_BALANCE) {
			np->avl_balance = direction;
			return;
		}

		parent = np->avl_parent;

		/*
		 * If balance is being restored, no local node
		 * reorganization is necessary, but may be at
		 * a higher node.  Reset direction and continue.
		 */
		if (direction != np->avl_balance) {
			np->avl_balance = AVL_BALANCE;
			if (parent) {
				if (parent->avl_forw == np)
					direction = AVL_BACK;
				else
					direction = AVL_FORW;

				np = parent;
				continue;
			}
			return;
		}

		/*
		 * Imbalance.  If a avl_forw node was removed, direction
		 * (and, by reduction, np->avl_balance) is/was AVL_BACK.
		 */
		if (np->avl_balance == AVL_BACK) {

			ASSERT(direction == AVL_BACK);
			child = np->avl_back;
			bal = child->avl_balance;

			if (bal != AVL_FORW) /* single LL */ {
				/*
				 * np gets pushed down to lesser child's
				 * avl_forw branch.
				 *
				 *  np->    -D 		    +B
				 *	    / \		    / \
				 * child-> B   deleted	   A  -D
				 *	  / \		      /
				 *	 A   C		     C
				cmn_err(CE_CONT, "!LL delete b 0x%x c 0x%x\n",
					np, child);
				 */

				np->avl_back = child->avl_forw;
				if (child->avl_forw)
					child->avl_forw->avl_parent = np;
				child->avl_forw = np;

				if (parent) {
					if (parent->avl_forw == np) {
						parent->avl_forw = child;
						direction = AVL_BACK;
					} else {
						ASSERT(parent->avl_back == np);
						parent->avl_back = child;
						direction = AVL_FORW;
					}
				} else {
					ASSERT(*rootp == np);
					*rootp = child;
				}
				np->avl_parent = child;
				child->avl_parent = parent;

				if (bal == AVL_BALANCE) {
					np->avl_balance = AVL_BACK;
					child->avl_balance = AVL_FORW;
					return;
				} else {
					np->avl_balance = AVL_BALANCE;
					child->avl_balance = AVL_BALANCE;
					np = parent;
					avl64_checktree(tree, *rootp);
					continue;
				}
			}

			/* child->avl_balance == AVL_FORW  double LR rotation
			 *
			 * child's avl_forw node gets promoted up, along with
			 * its avl_forw subtree
			 *
			 *  np->     -G 		  C
			 *	     / \		 / \
			 * child-> +B   H	       -B   G
			 *	   / \   \	       /   / \
			 *	  A  +C   deleted     A   D   H
			 *	       \
			 *	        D
			cmn_err(CE_CONT, "!LR delete b 0x%x c 0x%x t 0x%x\n",
				np, child, child->avl_forw);
			 */

			tmp = child->avl_forw;
			bal = tmp->avl_balance;

			child->avl_forw = tmp->avl_back;
			if (tmp->avl_back)
				tmp->avl_back->avl_parent = child;

			tmp->avl_back = child;
			child->avl_parent = tmp;

			np->avl_back = tmp->avl_forw;
			if (tmp->avl_forw)
				tmp->avl_forw->avl_parent = np;
			tmp->avl_forw = np;

			if (bal == AVL_FORW)
				child->avl_balance = AVL_BACK;
			else
				child->avl_balance = AVL_BALANCE;

			if (bal == AVL_BACK)
				np->avl_balance = AVL_FORW;
			else
				np->avl_balance = AVL_BALANCE;

			goto next;
		}

		ASSERT(np->avl_balance == AVL_FORW && direction == AVL_FORW);

		child = np->avl_forw;
		bal = child->avl_balance;

		if (bal != AVL_BACK) /* single RR */ {
			/*
			 * np gets pushed down to greater child's
			 * avl_back branch.
			 *
			 *  np->    +B 		     -D
			 *	    / \		     / \
			 *   deleted   D <-child   +B   E
			 *	      / \	     \
			 *	     C   E	      C
			cmn_err(CE_CONT, "!RR delete b 0x%x c 0x%x\n",
				np, child);
			 */

			np->avl_forw = child->avl_back;
			if (child->avl_back)
				child->avl_back->avl_parent = np;
			child->avl_back = np;

			if (parent) {
				if (parent->avl_forw == np) {
					parent->avl_forw = child;
					direction = AVL_BACK;
				} else {
					ASSERT(parent->avl_back == np);
					parent->avl_back = child;
					direction = AVL_FORW;
				}
			} else {
				ASSERT(*rootp == np);
				*rootp = child;
			}
			np->avl_parent = child;
			child->avl_parent = parent;

			if (bal == AVL_BALANCE) {
				np->avl_balance = AVL_FORW;
				child->avl_balance = AVL_BACK;
				return;
			} else {
				np->avl_balance = AVL_BALANCE;
				child->avl_balance = AVL_BALANCE;
				np = parent;
				avl64_checktree(tree, *rootp);
				continue;
			}
		}

		/* child->avl_balance == AVL_BACK  double RL rotation
		cmn_err(CE_CONT, "!RL delete b 0x%x c 0x%x t 0x%x\n",
			np, child, child->avl_back);
		*/

		tmp = child->avl_back;
		bal = tmp->avl_balance;

		child->avl_back = tmp->avl_forw;
		if (tmp->avl_forw)
			tmp->avl_forw->avl_parent = child;

		tmp->avl_forw = child;
		child->avl_parent = tmp;

		np->avl_forw = tmp->avl_back;
		if (tmp->avl_back)
			tmp->avl_back->avl_parent = np;
		tmp->avl_back = np;

		if (bal == AVL_BACK)
			child->avl_balance = AVL_FORW;
		else
			child->avl_balance = AVL_BALANCE;

		if (bal == AVL_FORW)
			np->avl_balance = AVL_BACK;
		else
			np->avl_balance = AVL_BALANCE;
next:
		np->avl_parent = tmp;
		tmp->avl_balance = AVL_BALANCE;
		tmp->avl_parent = parent;

		if (parent) {
			if (parent->avl_forw == np) {
				parent->avl_forw = tmp;
				direction = AVL_BACK;
			} else {
				ASSERT(parent->avl_back == np);
				parent->avl_back = tmp;
				direction = AVL_FORW;
			}
		} else {
			ASSERT(*rootp == np);
			*rootp = tmp;
			return;
		}

		np = parent;
		avl64_checktree(tree, *rootp);
	} while (np);
}

/*
 *	Remove node from tree.
 *	avl_delete does the local tree manipulations,
 *	calls retreat() to rebalance tree up to its root.
 */
void
avl64_delete(
	register avl64tree_desc_t *tree,
	register avl64node_t *np)
{
	register avl64node_t *forw = np->avl_forw;
	register avl64node_t *back = np->avl_back;
	register avl64node_t *parent = np->avl_parent;
	register avl64node_t *nnext;


	if (np->avl_back) {
		/*
		 * a left child exits, then greatest left descendent's nextino
		 * is pointing to np; make it point to np->nextino.
		 */
		nnext = np->avl_back;
		while (nnext) {
			if (!nnext->avl_forw)
				break; /* can't find anything bigger */
			nnext = nnext->avl_forw;
		}
	} else
	if (np->avl_parent) {
		/*
		 * find nearest ancestor with lesser value. That ancestor's
		 * nextino is pointing to np; make it point to np->nextino
		 */
		 nnext = np->avl_parent;
		 while (nnext) {
			if (AVL_END(tree, nnext) <= AVL_END(tree, np))
				break;
			nnext = nnext->avl_parent;
		}
	} else
		nnext = NULL;

	if (nnext) {
		ASSERT(nnext->avl_nextino == np);
		nnext->avl_nextino = np->avl_nextino;
		/*
		 * 	Something preceeds np; np cannot be firstino.
		 */
		ASSERT(tree->avl_firstino != np);
	}
	else {
		/*
		 * 	Nothing preceeding np; after deletion, np's nextino
		 * 	is firstino of tree.
		 */
		ASSERT(tree->avl_firstino == np);
		tree->avl_firstino = np->avl_nextino;
	}
	

	/*
	 * Degenerate cases...
	 */
	if (forw == NULL) {
		forw = back;
		goto attach;
	}

	if (back == NULL) {
attach:
		if (forw)
			forw->avl_parent = parent;
		if (parent) {
			if (parent->avl_forw == np) {
				parent->avl_forw = forw;
				retreat(tree, parent, AVL_BACK);
			} else {
				ASSERT(parent->avl_back == np);
				parent->avl_back = forw;
				retreat(tree, parent, AVL_FORW);
			}
		} else {
			ASSERT(tree->avl_root == np);
			tree->avl_root = forw;
		}
		avl64_checktree(tree, tree->avl_root);
		return;
	}

	/*
	 * Harder case: children on both sides.
	 * If back's avl_forw pointer is null, just have back
	 * inherit np's avl_forw tree, remove np from the tree
	 * and adjust balance counters starting at back.
	 *
	 * np->	    xI		    xH	(befor retreat())
	 *	    / \		    / \
	 * back->  H   J	   G   J
	 *	  /   / \             / \
	 *       G   ?   ?           ?   ?
	 *      / \
	 *     ?   ?
	 */
	if ((forw = back->avl_forw) == NULL) {
		/*
		 * AVL_FORW retreat below will set back's
		 * balance to AVL_BACK.
		 */
		back->avl_balance = np->avl_balance;
		back->avl_forw = forw = np->avl_forw;
		forw->avl_parent = back;
		back->avl_parent = parent;
		
		if (parent) {
			if (parent->avl_forw == np)
				parent->avl_forw = back;
			else {
				ASSERT(parent->avl_back == np);
				parent->avl_back = back;
			}
		} else {
			ASSERT(tree->avl_root == np);
			tree->avl_root = back;
		}

		/*
		 * back is taking np's place in the tree, and
		 * has therefore lost a avl_back node (itself).
		 */
		retreat(tree, back, AVL_FORW);
		avl64_checktree(tree, tree->avl_root);
		return;
	}

	/*
	 * Hardest case: children on both sides, and back's
	 * avl_forw pointer isn't null.  Find the immediately
	 * inferior buffer by following back's avl_forw line
	 * to the end, then have it inherit np's avl_forw tree.
	 *
	 * np->	    xI			      xH
	 *	    / \			      / \
	 *         G   J	     back->  G   J   (before retreat())
	 *	  / \			    / \
	 *       F   ?...  		   F   ?1
	 *      /     \
	 *     ?       H  <-forw
	 *	      /
	 *	     ?1
	 */
	while (back = forw->avl_forw)
		forw = back;

	/*
	 * Will be adjusted by retreat() below.
	 */
	forw->avl_balance = np->avl_balance;
	
	/*
	 * forw inherits np's avl_forw...
	 */
	forw->avl_forw = np->avl_forw;
	np->avl_forw->avl_parent = forw;

	/*
	 * ... forw's parent gets forw's avl_back...
	 */
	back = forw->avl_parent;
	back->avl_forw = forw->avl_back;
	if (forw->avl_back)
		forw->avl_back->avl_parent = back;

	/*
	 * ... forw gets np's avl_back...
	 */
	forw->avl_back = np->avl_back;
	np->avl_back->avl_parent = forw;

	/*
	 * ... and forw gets np's parent.
	 */
	forw->avl_parent = parent;

	if (parent) {
		if (parent->avl_forw == np)
			parent->avl_forw = forw;
		else
			parent->avl_back = forw;
	} else {
		ASSERT(tree->avl_root == np);
		tree->avl_root = forw;
	}

	/*
	 * What used to be forw's parent is the starting
	 * point for rebalancing.  It has lost a avl_forw node.
	 */
	retreat(tree, back, AVL_BACK);
	avl64_checktree(tree, tree->avl_root);
}


/*
 * 	avl_findanyrange:
 *	
 *	Given range r [start, end), find any range which is contained in r.
 *	if checklen is non-zero, then only ranges of non-zero length are
 * 	considered in finding a match.
 */
avl64node_t *
avl64_findanyrange(
	register avl64tree_desc_t *tree,
	register __uint64_t start,
	register __uint64_t end,
	int 	checklen)
{
        register avl64node_t *np = tree->avl_root;

	/* np = avl64_findadjacent(tree, start, AVL_SUCCEED); */
	while (np) {
		if (start < AVL_START(tree, np)) {
			if (np->avl_back) {
				np = np->avl_back;
				continue;
			}
			/* if we were to add node with start, would
			 * have a growth of AVL_BACK
			 */
			/* if succeeding node is needed, this is it.
			 */
			break;
		}
		if (start >= AVL_END(tree, np)) {
			if (np->avl_forw) {
				np = np->avl_forw;
				continue;
			}
			/* if we were to add node with start, would
			 * have a growth of AVL_FORW; 
			 */
			/* we are looking for a succeeding node;
			 * this is nextino.
			 */
			np = np->avl_nextino;
			break;
		}
		/* AVL_START(tree, np) <= start < AVL_END(tree, np) */
		break;
	}
	if (np) {
		if (checklen == AVL_INCLUDE_ZEROLEN) {
			if (end <= AVL_START(tree, np)) {
				/* something follows start, but is
				 * is entierly after the range (end)
				 */
				return(NULL);
			}
			/* np may stradle [start, end) */
			return(np);
		}
		/*
		 * find non-zero length region 
		 */
		while (np && (AVL_END(tree, np) - AVL_START(tree, np) == 0)
			&& (AVL_START(tree, np)  < end))
				np = np->avl_nextino;

		if ((np == NULL) || (AVL_START(tree, np) >= end))
			return NULL;
		return(np);
	}
	/*
	 * nothing succeeds start, all existing ranges are before start.
	 */
	return NULL;
}


/*
 * Returns a pointer to range which contains value.
 */
avl64node_t *
avl64_findrange(
	register avl64tree_desc_t *tree,
	register __uint64_t value)
{
	register avl64node_t *np = tree->avl_root;

	while (np) {
		if (value < AVL_START(tree, np)) {
			np = np->avl_back;
			continue;
		}
		if (value >= AVL_END(tree, np)) {
			np = np->avl_forw;
			continue;
		}
		ASSERT(AVL_START(tree, np) <= value &&
		       value < AVL_END(tree, np));
		return np;
	}
	return NULL;
}


/*
 * Returns a pointer to node which contains exact value.
 */
avl64node_t *
avl64_find(
	register avl64tree_desc_t *tree,
	register __uint64_t value)
{
	register avl64node_t *np = tree->avl_root;
	register __uint64_t nvalue;

	while (np) {
		nvalue = AVL_START(tree, np);
		if (value < nvalue) {
			np = np->avl_back;
			continue;
		}
		if (value == nvalue) {
			return np;
		}
		np = np->avl_forw;
	}
	return NULL;
}


/*
 * Balance buffer AVL tree after attaching a new node to root.
 * Called only by avl_insert.
 */
static void
avl64_balance(
	register avl64node_t **rootp,
	register avl64node_t *np,
	register int growth)
{
	/*
	 * At this point, np points to the node to which
	 * a new node has been attached.  All that remains is to
	 * propagate avl_balance up the tree.
	 */
	for ( ; ; ) {
		register avl64node_t *parent = np->avl_parent;
		register avl64node_t *child;

		CERT(growth == AVL_BACK || growth == AVL_FORW);

		/*
		 * If the buffer was already balanced, set avl_balance
		 * to the new direction.  Continue if there is a
		 * parent after setting growth to reflect np's
		 * relation to its parent.
		 */
		if (np->avl_balance == AVL_BALANCE) {
			np->avl_balance = growth;
			if (parent) {
				if (parent->avl_forw == np)
					growth = AVL_FORW;
				else {
					ASSERT(parent->avl_back == np);
					growth = AVL_BACK;
				}

				np = parent;
				continue;
			}
			break;
		}

		if (growth != np->avl_balance) {
			/*
			 * Subtree is now balanced -- no net effect
			 * in the size of the subtree, so leave.
			 */
			np->avl_balance = AVL_BALANCE;
			break;
		}

		if (growth == AVL_BACK) {

			child = np->avl_back;
			CERT(np->avl_balance == AVL_BACK && child);

			if (child->avl_balance == AVL_BACK) { /* single LL */
				/*
				 * ``A'' just got inserted;
				 * np points to ``E'', child to ``C'',
				 * and it is already AVL_BACK --
				 * child will get promoted to top of subtree.

				np->	     -E			C
					     / \	       / \
				child->	   -C   F	     -B   E
					   / \		     /   / \
					 -B   D		    A   D   F
					 /
					A

					Note that child->avl_parent and
					avl_balance get set in common code.
				 */
				np->avl_parent = child;
				np->avl_balance = AVL_BALANCE;
				np->avl_back = child->avl_forw;
				if (child->avl_forw)
					child->avl_forw->avl_parent = np;
				child->avl_forw = np;
			} else {
				/*
				 * double LR
				 *
				 * child's avl_forw node gets promoted to
				 * the top of the subtree.

				np->	     -E		      C
					     / \	     / \
				child->	   +B   F	   -B   E
					   / \		   /   / \
					  A  +C 	  A   D   F
					       \
						D

				 */
				register avl64node_t *tmp = child->avl_forw;

				CERT(child->avl_balance == AVL_FORW && tmp);

				child->avl_forw = tmp->avl_back;
				if (tmp->avl_back)
					tmp->avl_back->avl_parent = child;

				tmp->avl_back = child;
				child->avl_parent = tmp;

				np->avl_back = tmp->avl_forw;
				if (tmp->avl_forw)
					tmp->avl_forw->avl_parent = np;

				tmp->avl_forw = np;
				np->avl_parent = tmp;

				if (tmp->avl_balance == AVL_BACK)
					np->avl_balance = AVL_FORW;
				else
					np->avl_balance = AVL_BALANCE;

				if (tmp->avl_balance == AVL_FORW)
					child->avl_balance = AVL_BACK;
				else
					child->avl_balance = AVL_BALANCE;

				/*
				 * Set child to point to tmp since it is
				 * now the top of the subtree, and will
				 * get attached to the subtree parent in
				 * the common code below.
				 */
				child = tmp;
			}

		} else /* growth == AVL_BACK */ {

			/*
			 * This code is the mirror image of AVL_FORW above.
			 */

			child = np->avl_forw;
			CERT(np->avl_balance == AVL_FORW && child);

			if (child->avl_balance == AVL_FORW) { /* single RR */
				np->avl_parent = child;
				np->avl_balance = AVL_BALANCE;
				np->avl_forw = child->avl_back;
				if (child->avl_back)
					child->avl_back->avl_parent = np;
				child->avl_back = np;
			} else {
				/*
				 * double RL
				 */
				register avl64node_t *tmp = child->avl_back;

				ASSERT(child->avl_balance == AVL_BACK && tmp);

				child->avl_back = tmp->avl_forw;
				if (tmp->avl_forw)
					tmp->avl_forw->avl_parent = child;

				tmp->avl_forw = child;
				child->avl_parent = tmp;

				np->avl_forw = tmp->avl_back;
				if (tmp->avl_back)
					tmp->avl_back->avl_parent = np;

				tmp->avl_back = np;
				np->avl_parent = tmp;

				if (tmp->avl_balance == AVL_FORW)
					np->avl_balance = AVL_BACK;
				else
					np->avl_balance = AVL_BALANCE;

				if (tmp->avl_balance == AVL_BACK)
					child->avl_balance = AVL_FORW;
				else
					child->avl_balance = AVL_BALANCE;

				child = tmp;
			}
		}

		child->avl_parent = parent;
		child->avl_balance = AVL_BALANCE;

		if (parent) {
			if (parent->avl_back == np)
				parent->avl_back = child;
			else
				parent->avl_forw = child;
		} else {
			ASSERT(*rootp == np);
			*rootp = child;
		}

		break;
	}
}

static
avl64node_t *
avl64_insert_find_growth(
		register avl64tree_desc_t *tree,
		register __uint64_t start, 	/* range start at start, */
		register __uint64_t end, 	/* exclusive */
		register int   *growthp) 	/* OUT */ 
{
	avl64node_t *root = tree->avl_root;
	register avl64node_t *np;

	np = root;
	ASSERT(np); /* caller ensures that there is atleast one node in tree */

	for ( ; ; ) {
		CERT(np->avl_parent || root == np);
		CERT(!np->avl_parent || root != np);
		CERT(!(np->avl_back) || np->avl_back->avl_parent == np);
		CERT(!(np->avl_forw) || np->avl_forw->avl_parent == np);
		CERT(np->avl_balance != AVL_FORW || np->avl_forw);
		CERT(np->avl_balance != AVL_BACK || np->avl_back);
		CERT(np->avl_balance != AVL_BALANCE ||
		     np->avl_back == NULL || np->avl_forw);
		CERT(np->avl_balance != AVL_BALANCE ||
		     np->avl_forw == NULL || np->avl_back);

		if (AVL_START(tree, np) >= end) {
			if (np->avl_back) {
				np = np->avl_back;
				continue;
			}
			*growthp = AVL_BACK;
			break;
		}

		if (AVL_END(tree, np) <= start) {
			if (np->avl_forw) {
				np = np->avl_forw;
				continue;
			}
			*growthp = AVL_FORW;
			break;
		}
		/* found exact match -- let caller decide if it is an error */
		return(NULL);
	}
	return(np);
}


static void
avl64_insert_grow(
	register avl64tree_desc_t *tree,
	register avl64node_t *parent,
	register avl64node_t *newnode,
	register int growth)
{
	register avl64node_t *nnext;
	register __uint64_t start = AVL_START(tree, newnode);

	if (growth == AVL_BACK) {

		parent->avl_back = newnode;
		/*
		 * we are growing to the left; previous in-order to newnode is
		 * closest ancestor with lesser value. Before this
		 * insertion, this ancestor will be pointing to
		 * newnode's parent. After insertion, next in-order to newnode
		 * is the parent.
		 */
		newnode->avl_nextino = parent;
		nnext = parent;
		while (nnext) {
			if (AVL_END(tree, nnext) <= start)
				break;
			nnext = nnext->avl_parent;
		}
		if (nnext)  {
			/*
			 * nnext will be null if newnode is
			 * the least element, and hence very first in the list.
			 */
			ASSERT(nnext->avl_nextino == parent);
			nnext->avl_nextino = newnode;
		}
	}
	else {
		parent->avl_forw = newnode;
		newnode->avl_nextino = parent->avl_nextino;
		parent->avl_nextino = newnode;
	}
}


avl64node_t *
avl64_insert(
	register avl64tree_desc_t *tree,
	register avl64node_t *newnode)
{
	register avl64node_t *np;
	register __uint64_t start = AVL_START(tree, newnode);
	register __uint64_t end = AVL_END(tree, newnode);
	int growth;

	ASSERT(newnode);
	/*
	 * Clean all pointers for sanity; some will be reset as necessary.
	 */
	newnode->avl_nextino = NULL;
	newnode->avl_parent = NULL;
	newnode->avl_forw = NULL;
	newnode->avl_back = NULL;
	newnode->avl_balance = AVL_BALANCE;

	if ((np = tree->avl_root) == NULL) { /* degenerate case... */
		tree->avl_root = newnode;
		tree->avl_firstino = newnode;
		return newnode;
	}

	if ((np = avl64_insert_find_growth(tree, start, end, &growth))
			== NULL) {
		if (start != end)  { /* non-zero length range */
#ifdef	AVL_USER_MODE
		printf("avl_insert: Warning! duplicate range [0x%llx,0x%llx)\n",
				start, end);
#else
			cmn_err(CE_CONT,
		"!avl_insert: Warning! duplicate range [0x%llx,0x%llx)\n",
				start, end);
#endif
		}
		return(NULL);
	}

	avl64_insert_grow(tree, np, newnode, growth);
	if (growth == AVL_BACK) {
		/*
		 * Growing to left. if np was firstino, newnode will be firstino
		 */
		 if (tree->avl_firstino == np)
			tree->avl_firstino = newnode;
	}
#ifdef notneeded
	else
	if (growth == AVL_FORW)
		/*
		 * Cannot possibly be firstino; there is somebody to our left.
		 */
		 ;
#endif

	newnode->avl_parent = np;
	CERT(np->avl_forw == newnode || np->avl_back == newnode);

	avl64_balance(&tree->avl_root, np, growth);

	avl64_checktree(tree, tree->avl_root);

	return newnode;
}

/*
 *
 * avl64_insert_immediate(tree, afterp, newnode):
 * 	insert newnode immediately into tree immediately after afterp.
 *	after insertion, newnode is right child of afterp.
 */
void
avl64_insert_immediate(
		avl64tree_desc_t *tree,
		avl64node_t *afterp,
		avl64node_t *newnode)
{
	/*
	 * Clean all pointers for sanity; some will be reset as necessary.
	 */
	newnode->avl_nextino = NULL;
	newnode->avl_parent = NULL;
	newnode->avl_forw = NULL;
	newnode->avl_back = NULL;
	newnode->avl_balance = AVL_BALANCE;

	if (afterp == NULL) {
		tree->avl_root = newnode;
		tree->avl_firstino = newnode;
		return;
	}

	ASSERT(afterp->avl_forw == NULL);
	avl64_insert_grow(tree, afterp, newnode, AVL_FORW); /* grow to right */
	CERT(afterp->avl_forw == newnode);
	avl64_balance(&tree->avl_root, afterp, AVL_FORW);
	avl64_checktree(tree, tree->avl_root);
}


/*
 *	Returns first in order node
 */
avl64node_t *
avl64_firstino(register avl64node_t *root)
{
	register avl64node_t *np;

	if ((np = root) == NULL)
		return NULL;

	while (np->avl_back)
		np = np->avl_back;
	return np;
}

#ifdef AVL_USER_MODE
/*
 * leave this as a user-mode only routine until someone actually
 * needs it in the kernel
 */

/*
 *	Returns last in order node
 */
avl64node_t *
avl64_lastino(register avl64node_t *root)
{
	register avl64node_t *np;

	if ((np = root) == NULL)
		return NULL;

	while (np->avl_forw)
		np = np->avl_forw;
	return np;
}
#endif

void
avl64_init_tree(avl64tree_desc_t *tree, avl64ops_t *ops)
{
	tree->avl_root = NULL;
	tree->avl_firstino = NULL;
	tree->avl_ops = ops;
}

#ifdef AVL_DEBUG
static void
avl64_printnode(avl64tree_desc_t *tree, avl64node_t *np, int nl)
{
	printf("[%d-%d]%c", AVL_START(tree, np),
		(AVL_END(tree, np) - 1), nl ? '\n' : ' ');
}
#endif
#ifdef STAND_ALONE_DEBUG

struct avl_debug_node {
	avl64node_t	avl_node;
	off_t		avl_start;
	unsigned int	avl_size;
}

avl64ops_t avl_debug_ops = {
	avl_debug_start,
	avl_debug_end,
}

static __uint64_t
avl64_debug_start(avl64node_t *node)
{
	return (__uint64_t)(struct avl_debug_node *)node->avl_start;
}

static __uint64_t
avl64_debug_end(avl64node_t *node)
{
	return (__uint64_t)
		((struct avl_debug_node *)node->avl_start +
		 (struct avl_debug_node *)node->avl_size);
}

avl_debug_node 	freenodes[100];
avl_debug_node 	*freehead = &freenodes[0];

static avl64node_t *
alloc_avl64_debug_node()
{
	freehead->avl_balance = AVL_BALANCE;
	freehead->avl_parent = freehead->avl_forw = freehead->avl_back = NULL;
	return(freehead++);
}

static void
avl64_print(avl64tree_desc_t *tree, avl64node_t *root, int depth)
{
	int i;

	if (!root)
		return;
	if (root->avl_forw)
		avl64_print(tree, root->avl_forw, depth+5);
	for (i = 0; i < depth; i++)
		putchar((int) ' ');
	avl64_printnode(tree, root,1);
	if (root->avl_back)
		avl64_print(tree, root->avl_back, depth+5);
}

main()
{
	int 		i, j;
	avl64node_t  	*np;
	avl64tree_desc_t	tree;
	char		linebuf[256], cmd[256];

	avl64_init_tree(&tree, &avl_debug_ops);

	for (i = 100; i > 0; i = i - 10)
	{	
		np = alloc__debug_avlnode();
		ASSERT(np);
		np->avl_start = i;
		np->avl_size = 10;
		avl64_insert(&tree, np);
	}
	avl64_print(&tree, tree.avl_root, 0);

	for (np = tree.avl_firstino; np != NULL; np = np->avl_nextino)
		avl64_printnode(&tree, np, 0);
	printf("\n");

	while (1) {
		printf("Command [fpdir] : ");
		fgets(linebuf, 256, stdin);
		if (feof(stdin)) break;
		cmd[0] = NULL;
		if (sscanf(linebuf, "%[fpdir]%d", cmd, &i) != 2)
			continue;
		switch (cmd[0]) {
		case 'd':
		case 'f':
			printf("end of range ? ");
			fgets(linebuf, 256, stdin);
			j = atoi(linebuf);

			if (i == j) j = i+1;
			np = avl64_findinrange(&tree,i,j);
			if (np) {
				avl64_printnode(&tree, np, 1);
				if (cmd[0] == 'd')
					avl64_delete(&tree, np);
			} else
				printf("Cannot find %d\n", i);
			break;
		case 'p':
			avl64_print(&tree, tree.avl_root, 0);
			for (np = tree.avl_firstino;
				np != NULL; np = np->avl_nextino)
					avl64_printnode(&tree, np, 0);
			printf("\n");
			break;
		case 'i':
			np = alloc_avlnode();
			ASSERT(np);
			np->avl_start = i;
			printf("size of range ? ");
			fgets(linebuf, 256, stdin);
			j = atoi(linebuf);

			np->avl_size = j;
			avl64_insert(&tree, np);
			break;
		case 'r': {
			avl64node_t 	*b, *e, *t;
			int		checklen;

			printf("End of range ? ");
			fgets(linebuf, 256, stdin);
			j = atoi(linebuf);

			printf("checklen 0/1 ? ");
			fgets(linebuf, 256, stdin);
			checklen = atoi(linebuf);


			b = avl64_findanyrange(&tree, i, j, checklen);
			if (b) {
				printf("Found something\n");
				t = b;
				while (t)  {
					if (t != b &&
					    AVL_START(&tree, t) >= j)
						break;
					avl64_printnode(&tree, t, 0);
					t = t->avl_nextino;
				}
				printf("\n");
			}
		     }
		}
	}
}
#endif

/*
 * 	Given a tree, find value; will find return range enclosing value,
 *	or range immediately succeeding value,
 * 	or range immediately preceeding value.
 */
avl64node_t *
avl64_findadjacent(
	register avl64tree_desc_t *tree,
	register __uint64_t value,
	register int		dir)
{
        register avl64node_t *np = tree->avl_root;

	while (np) {
		if (value < AVL_START(tree, np)) {
			if (np->avl_back) {
				np = np->avl_back;
				continue;
			}
			/* if we were to add node with value, would
			 * have a growth of AVL_BACK
			 */
			if (dir == AVL_SUCCEED) {
				/* if succeeding node is needed, this is it.
				 */
				return(np);
			}
			if (dir == AVL_PRECEED) {
				/*
				 * find nearest ancestor with lesser value.
				 */
				 np = np->avl_parent;
				 while (np) {
					if (AVL_END(tree, np) <= value)
						break;
					np = np->avl_parent;
				}
				return(np);
			}
			ASSERT(dir == AVL_SUCCEED || dir == AVL_PRECEED);
			break;
		}
		if (value >= AVL_END(tree, np)) {
			if (np->avl_forw) {
				np = np->avl_forw;
				continue;
			}
			/* if we were to add node with value, would
			 * have a growth of AVL_FORW; 
			 */
			if (dir == AVL_SUCCEED) {
				/* we are looking for a succeeding node;
				 * this is nextino.
				 */
				return(np->avl_nextino);
			}
			if (dir == AVL_PRECEED) {
				/* looking for a preceeding node; this is it. */
				return(np);
			}	
			ASSERT(dir == AVL_SUCCEED || dir == AVL_PRECEED);
		}
		/* AVL_START(tree, np) <= value < AVL_END(tree, np) */
		return(np);
	}
	return NULL;
}


#ifdef AVL_FUTURE_ENHANCEMENTS
/*
 *  	avl_findranges:
 *
 *	Given range r [start, end), find all ranges in tree which are contained
 *	in r. At return, startp and endp point to first and last of
 * 	a chain of elements which describe the contained ranges. Elements
 *	in startp ... endp are in sort order, and can be accessed by
 *	using avl_nextino.
 */

void
avl64_findranges(
	register avl64tree_desc_t *tree,
	register __uint64_t start,
	register __uint64_t end,
	avl64node_t 	        **startp,
	avl64node_t		**endp)
{
        register avl64node_t *np;

	np = avl64_findadjacent(tree, start, AVL_SUCCEED);
	if (np == NULL 				/* nothing succeding start */
		|| (np && (end <= AVL_START(tree, np))))
						/* something follows start,
						but... is entirely after end */
	{
		*startp = NULL;
		*endp = NULL;
		return;
	}

	*startp = np;

	/* see if end is in this region itself */
	if (end <= AVL_END(tree, np) ||
	    np->avl_nextino == NULL ||
	    (np->avl_nextino &&
	    (end <= AVL_START(tree, np->avl_nextino)))) {
		*endp = np;
		return;
	}
	/* have to munge for end */
	/*
	 * note: have to look for (end - 1), since
	 * findadjacent will look for exact value, and does not
	 * care about the fact that end is actually one more
	 * than the value actually being looked for; thus feed it one less.
	 */
	*endp = avl64_findadjacent(tree, (end-1), AVL_PRECEED);
	ASSERT(*endp);
}

#endif /* AVL_FUTURE_ENHANCEMENTS */
