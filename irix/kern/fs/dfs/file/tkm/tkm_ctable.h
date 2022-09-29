/*
 *      Copyright (C) 1992 1993 1994 Transarc Corporation
 *      All rights reserved.
 *
 *	Routines that manipulate and access the token conflict table.
 *
 */

#ifndef TRANSARC_TKM_CONFLICTTABLE_H
#define TRANSARC_TKM_CONFLICTTABLE_H

/* see tkm_InitConflictTable in tkm_ctable.c to see how these
 * are initialized
 */
extern short tkm_ctable0[256];	/* conf table for first 8 bits */
extern short tkm_ctable1[256];	/* conf table for 2nd 8 bits */

#define TKM_TYPE_CONFLICTS(t) \
    (tkm_ctable0[(t) & 0xFF] |  tkm_ctable1[((t)>>8) & 0xFF])

#define TKM_TYPES_WITH_RANGE(t) (t & 0xF)
#define TKM_TYPES_WITHOUT_RANGE(t) (t & ~0xF)

/* initialize the data structures used to determine conflicts */
#ifdef SGIMIPS
extern void tkm_InitConflictTable(void);
#else
extern void tkm_InitConflictTable();
#endif

#endif /* TRANSARC_TKM_CONFLICTTABLE_H */








