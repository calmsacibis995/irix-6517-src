/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994 Silicon Graphics, Inc.	  	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.5 $"

#ifndef	_MTEST_H_
#define	_MTEST_H_

#include <sys/SN/agent.h>

#if _LANGUAGE_C
#include <sys/SN/error.h>
#endif /* _LANGUAGE_C */

/*
 * Internal
 */

#ifdef _LANGUAGE_ASSEMBLY

#define MTEST_FAIL_OFF_ADDR	0x00
#define MTEST_FAIL_OFF_MASK	0x08
#define MTEST_FAIL_OFF_EXPECT	0x10
#define MTEST_FAIL_OFF_ACTUAL	0x18
#define MTEST_FAIL_OFF_REREAD	0x20
#define MTEST_FAIL_OFF_MEM_ERR	0x28
#define MTEST_FAIL_OFF_DIR_ERR	0x30

#endif /* _LANGUAGE_ASSEMBLY */

#ifdef _LANGUAGE_C

typedef struct mtest_fail_s {
    __uint64_t		addr;			/* Address of mismatch      */
    __uint64_t		mask;			/* Mask of bits compared    */
    __uint64_t		expect;			/* Value that was written   */
    __uint64_t		actual;			/* Bad value that was read  */
    __uint64_t		reread;			/* Retried read result	    */
    hub_mderr_t		err;			/* Four error registers     */
} mtest_fail_t;

/*
 * Public
 */

int	mtest_basic(int premium, __uint64_t addr, int flag);

void	mtest_print_fail(mtest_fail_t *fail, int mderrs, int exception);

int	mtest_compare_prom(void *prom_addr,	/* mdir_asm.s */
			   void *mem_addr,
			   __uint64_t length,
			   mtest_fail_t *fail);

int	mtest(__uint64_t saddr,
	      __uint64_t eaddr,
	      int diag_mode,
	      mtest_fail_t *fail,
	      int maxerr);

int	mtest_dir(int premium,
		  __uint64_t saddr,
		  __uint64_t eaddr,
		  int diag_mode,
		  mtest_fail_t *fail,
		  int maxerr);

int	mtest_ecc(int premium,
		  __uint64_t saddr,
		  __uint64_t eaddr,
		  int diag_mode,
		  mtest_fail_t *fail,
		  int maxerr);

#endif /* _LANGUAGE_C */

#endif /* _MTEST_H_ */
