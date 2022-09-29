/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef	_KSYS_CELL_H_
#define	_KSYS_CELL_H_	1
#ident "$Id: cell.h,v 1.42 1997/08/01 22:58:29 pjr Exp $"

/*
 * Definitions relating to cells and their identities.
 *
 * Lots of stuff is for UNIKERNEL - running simulated cells
 * within a single kernel. A UNIKERNEL system can partition memory
 * enough to provide administrative shutdown.
 * In UNIKERNEL mode, threads 'shuttle' between cells - they need
 *	not actually context switch, they simply need to change
 *	their cellid.
 *
 * Dynamic memory all defaults to allocating virtual space from a
 * particular cell and physical space from the same cell/NUMA-node.
 */

#include <ksys/kern_heap.h>
#include <stdarg.h>

/*
 * This is the maximum number of cells this kernel can connect to -
 * There are various things that contribute to this maximum - partitioning
 * of 32 bit id's (pids, shm, etc), token data structures, etc.
 */
#define MAX_CELLS	64

/*
 * This is the maximum cell number - not to be confused with the number of
 * cells. The max cell number might be substantially larger the number of
 * cells.
 */
#define MAX_CELL_NUMBER	(MAX_CELLS-1)

/*
 * Externs.
 */
extern cell_t 	golden_cell;		/* golden cell - can't be shutdown */
extern cell_t	my_cellid;		/* returned by cellid() */

extern void	cell_backoff(int);	
extern void 	cell_cpu_init(void);

#if CELL
#define PARTID_TO_CELLID(p)	((cell_t)(p))
#define CELLID_TO_PARTID(c)	((partid_t)(c))

extern void	cell_down(partid_t);
extern void	cell_up(partid_t);
extern int	cell_connect(cell_t);
extern void	cell_disconnect(cell_t);
extern void	cell_recovery(cell_t);
#endif

/*
 * Well-known cells.
 */
#define CELL_GOLDEN	golden_cell	/* the golden cll */
#define CELL_LOCAL	cellid()	/* this cell */
#define CELL_NONE	-1		/* no cell */
#define CELL_ANY	-2		/* any one cell */
#define CELL_ALL	-3		/* every cell */

#endif	/* _KSYS_CELL_H_ */
