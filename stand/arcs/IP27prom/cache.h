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

#ident "$Revision: 1.12 $"

#ifndef	_CACHE_H_
#define	_CACHE_H_

#if _LANGUAGE_C
/*
 * Structure to hold the registers at the time of cache test failure.
 * NOTE : This structure should always be kept in sync with STE_OFFSETs
 * defined below.
 */
typedef struct {
	__uint64_t	
	t0,
	t1,
	t2,
	t3,
	a0,
	a1,
	a2,
	a3,
        v0,
	v1;
} cache_test_eframe_t;

/*
 * Define:	cache_test_eframe_print
 * Purpose: 	Print the registers saved after the cache test failure
 * Parameters:	cache test eframe
 */

#define cache_test_eframe_print(ct_frame)				     \
	    printf("Cache Test Register State:\n"			     \
		   "t0 = %y	t1 = %y\n"				     \
		   "t2 = %y	t3 = %y\n"  				     \
		   "a0 = %y	a1 = %y\n"				     \
		   "a2 = %y	a3 = %y\n"  				     \
		   "v0 = %y	v1 = %y\n",			     	     \
		   ct_frame.t0,ct_frame.t1,				     \
		   ct_frame.t2,ct_frame.t3,	     			     \
		   ct_frame.a0,ct_frame.a1,				     \
		   ct_frame.a2,ct_frame.a3,	     			     \
		   ct_frame.v0,ct_frame.v1);


int	cache_size_s(void);
int	cache_size_i(void);
int	cache_size_d(void);

int	cache_test_i(void);	/* 0 for success, non-zero for failure */
int	cache_test_d(void);
int	cache_test_s(cache_test_eframe_t *);

void	cache_inval_i(void);
void	cache_inval_d(void);
void	cache_inval_s(void);

void	cache_flush(void);
void	cache_unflush(ulong vaddr, ulong size);

void	cache_dirty(ulong vaddr, ulong paddr, ulong size);

ulong	cache_get_i(ulong vaddr, void *data);
ulong	cache_get_d(ulong vaddr, void *data);
void	cache_get_s(ulong vaddr, void *tags, void *duptags, void *data);

ulong	cache_tag_ecc(ulong tag_value);

void	exec_sscache_asm(ulong base, ulong taglo, ulong taghi);
void	exec_sstag_asm(ulong base, ulong taglo, ulong taghi, ulong way);

#if 0
void   *cache_copy_pic(void *code, ulong len, ulong vaddr, ulong paddr);
#endif

#endif /* _LANGUAGE_C */

#if _LANGUAGE_ASSEMBLY

/*
 * NOTE : Maintain these offsets and the above cache_test_eframe structure 
 *	 in sync.
 */
#define CTE_OFFSET_t0	0	
#define CTE_OFFSET_t1	CTE_OFFSET_t0 + 8	
#define CTE_OFFSET_t2	CTE_OFFSET_t1 + 8	
#define CTE_OFFSET_t3	CTE_OFFSET_t2 + 8	
#define CTE_OFFSET_a0	CTE_OFFSET_t3 + 8	
#define CTE_OFFSET_a1	CTE_OFFSET_a0 + 8	
#define CTE_OFFSET_a2	CTE_OFFSET_a1 + 8	
#define CTE_OFFSET_a3	CTE_OFFSET_a2 + 8
#define CTE_OFFSET_v0	CTE_OFFSET_a3 + 8	
#define CTE_OFFSET_v1	CTE_OFFSET_v0 + 8	


/*
 * Define:	SCACHE_TEST_GPR_SAVE
 * Purpose: 	Save the general purpose regiCTEr  state when the scache test
 *		failed. Useful for additional debugging.
 * Parameters:	None
 * Notes:	Donot use any registers
 */

#define CACHE_TEST_GPR_SAVE		\
	sd	t0,CTE_OFFSET_t0(a5);	\
	sd 	t1,CTE_OFFSET_t1(a5);	\
	sd	t2,CTE_OFFSET_t2(a5);	\
	sd	t3,CTE_OFFSET_t3(a5);	\
	sd	a0,CTE_OFFSET_a0(a5);	\
	sd 	a1,CTE_OFFSET_a1(a5);	\
	sd	a2,CTE_OFFSET_a2(a5);	\
	sd	a3,CTE_OFFSET_a3(a5);	\
	sd	v0,CTE_OFFSET_v0(a5);	\
	sd	v1,CTE_OFFSET_v1(a5);	



#endif /* _LANGUAGE_ASSEMBLY */
 
#endif /* _CACHE_H_ */
