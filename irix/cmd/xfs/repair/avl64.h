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
#ifndef __XR_AVL64_H__
#define __XR_AVL64_H__

#include <sys/types.h>

typedef struct	avl64node {
	struct 	avl64node	*avl_forw;	/* pointer to right child  (> parent) */
	struct 	avl64node *avl_back;	/* pointer to left child  (< parent) */
	struct	avl64node *avl_parent;	/* parent pointer */
	struct	avl64node *avl_nextino;	/* next in-order; NULL terminated list*/
	char		 avl_balance;	/* tree balance */
} avl64node_t;

/*
 * avl-tree operations
 */
typedef struct avl64ops {
	__uint64_t	(*avl_start)(avl64node_t *);
	__uint64_t	(*avl_end)(avl64node_t *);
} avl64ops_t;

/*
 * avoid complaints about multiple def's since these are only used by
 * the avl code internally
 */
#ifndef AVL_START
#define	AVL_START(tree, n)	(*(tree)->avl_ops->avl_start)(n)
#define	AVL_END(tree, n)	(*(tree)->avl_ops->avl_end)(n)
#endif

/* 
 * tree descriptor:
 *	root points to the root of the tree.
 *	firstino points to the first in the ordered list.
 */
typedef struct avl64tree_desc {
	avl64node_t	*avl_root;
	avl64node_t	*avl_firstino;
	avl64ops_t	*avl_ops;
} avl64tree_desc_t;

/* possible values for avl_balance */

#define AVL_BACK	1
#define AVL_BALANCE	0
#define AVL_FORW	2

/*
 * 'Exported' avl tree routines
 */
avl64node_t
*avl64_insert(
	avl64tree_desc_t *tree,
	avl64node_t *newnode);

void
avl64_delete(
	avl64tree_desc_t *tree,
	avl64node_t *np);

void
avl64_insert_immediate(
	avl64tree_desc_t *tree,
	avl64node_t *afterp,
	avl64node_t *newnode);
	
void
avl64_init_tree(
	avl64tree_desc_t  *tree,
	avl64ops_t *ops);

avl64node_t *
avl64_findrange(
	avl64tree_desc_t *tree,
	__uint64_t value);

avl64node_t *
avl64_find(
	avl64tree_desc_t *tree,
	__uint64_t value);

avl64node_t *
avl64_findanyrange(
	avl64tree_desc_t *tree,
	__uint64_t	start,
	__uint64_t	end,
	int     checklen);


avl64node_t *
avl64_findadjacent(
	avl64tree_desc_t *tree,
	__uint64_t	value,
	int		dir);

#ifdef AVL_FUTURE_ENHANCEMENTS
void
avl64_findranges(
	register avl64tree_desc_t *tree,
	register __uint64_t	start,
	register __uint64_t	end,
	avl64node_t 	        **startp,
	avl64node_t		**endp);
#endif

/*
 * avoid complaints about multiple def's since these are only used by
 * the avl code internally
 */
#ifndef AVL_PRECEED
#define AVL_PRECEED	0x1
#define AVL_SUCCEED	0x2

#define AVL_INCLUDE_ZEROLEN	0x0000
#define AVL_EXCLUDE_ZEROLEN	0x0001
#endif

#endif /* __XR_AVL64_H__ */
