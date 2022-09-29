/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __ATOMIC_OPS_H__
#define __ATOMIC_OPS_H__

/*
 * R4000 atomic operations based on link-load, store-conditional instructions.
 */

/*
 * Add, set or clear bits in an (four-byte) int.
 */
extern int atomicAddInt(int *, int);
extern int atomicSetInt(int *, int);
extern int atomicClearInt(int *, int);

extern int atomicAddUint(int *, int);
extern int atomicSetUint(int *, int);
extern int atomicClearUint(int *, int);

/*
 * Add, set or clear bits in a long -- works for native size of long.
 */
extern long atomicAddLong(long *, long);
extern long atomicSetLong(long *, long);
extern long atomicClearLong(long *, long);

extern long atomicAddUlong(long *, long);
extern long atomicSetUlong(long *, long);
extern long atomicClearUlong(long *, long);

#endif /* __ATOMIC_OPS_H__ */
