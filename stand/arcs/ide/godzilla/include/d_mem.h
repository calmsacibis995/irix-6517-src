#ident	"ide/godzilla/include/d_mem.h:  $Revision: 1.15 $"
/*	
 * ide memory tests header
 *
 * All memory tests have the following interface: the arguments are
 *	struct range	*ra;		// range of memory to test
 *	enum bitsense	sense;		// whether to invert bit-sense of test
 *	enum runflag	till;		// whether to continue after an error
 *	void		(*errfun)();	// error status notifier
 * Return code is TRUE for success, FALSE otherwise; the user is notified of
 * failure via (*errfun)(address, expected, actual, status) where the bad
 * address, expected value, actual value, and status code are passed.
 */
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

#ifndef __MEM_H__
#define	__MEM_H__

#include "uif.h"
#include "sys/RACER/heart.h"

/* external entry points to memory tests */
bool_t memaddruniq(struct range *, enum bitsense, enum runflag,
		   void (*)(char *, __psunsigned_t, __psunsigned_t, int));
bool_t memwalkingbit(struct range *, enum bitsense, enum runflag,
		     void (*)(char *, __psunsigned_t, __psunsigned_t, int));
int marchX(__psunsigned_t, __psunsigned_t);
int marchY(__psunsigned_t, __psunsigned_t);
int khdouble_drv(__psunsigned_t first, __psunsigned_t last);
int Altaccess_l(__psunsigned_t, __psunsigned_t);
int March_l(__psunsigned_t, __psunsigned_t);
int Movi_l(__psunsigned_t, __psunsigned_t);
int Mats_l(__psunsigned_t, __psunsigned_t);
int Kh_l(__psunsigned_t, __psunsigned_t);
int Butterfly_l(__psunsigned_t, __psunsigned_t );
bool_t Fastmemtest_l(__psunsigned_t, __psunsigned_t,
		void (*)(char *, __psunsigned_t, __psunsigned_t, int) );
bool_t Patterns_test_l(__psunsigned_t, __psunsigned_t,
		void (*)(char *, __psunsigned_t, __psunsigned_t, int) );

/* misc functions */
void fillmemW(__psunsigned_t *, __psunsigned_t *, __psunsigned_t, __psunsigned_t);
int mem_setup(int, char *, char *, struct range *);
void blkwrt(u_int *, u_int *, u_int, u_int);
void blkrd(u_int *, u_int *, u_int);
int convert_chars (char **);
void delay_lp(int intervals);

struct tstvar;
int three_ll(__psunsigned_t first,__psunsigned_t last);
int three_l(__psunsigned_t first,__psunsigned_t last);

int dram(__psunsigned_t, __psunsigned_t, int);

#if _MIPS_SIM == _ABI64
#define FILL_1		0x1111111111111111
#define FILL_2		0x2222222222222222
#define FILL_3		0x3333333333333333
#define FILL_4		0x4444444444444444
#define FILL_5		0x5555555555555555
#define FILL_6		0x6666666666666666
#define FILL_7		0x7777777777777777
#define FILL_8		0x8888888888888888
#define FILL_9		0x9999999999999999
#define FILL_A		0xaaaaaaaaaaaaaaaa
#define FILL_B		0xbbbbbbbbbbbbbbbb
#define FILL_C		0xcccccccccccccccc
#define FILL_D		0xdddddddddddddddd
#define FILL_E		0xeeeeeeeeeeeeeeee
#define FILL_F		0xffffffffffffffff
#else
#define FILL_1		0x11111111
#define FILL_2		0x22222222
#define FILL_3		0x33333333
#define FILL_4		0x44444444
#define FILL_5		0x55555555
#define FILL_6		0x66666666
#define FILL_7		0x77777777
#define FILL_8		0x88888888
#define FILL_9		0x99999999
#define FILL_A		0xaaaaaaaa
#define FILL_B		0xbbbbbbbb
#define FILL_C		0xcccccccc
#define FILL_D		0xdddddddd
#define FILL_E		0xeeeeeeee
#define FILL_F		0xffffffff
#endif

#define WORD_ADDR_MASK		(-sizeof(unsigned int))
#if _MIPS_SIM == _ABI64
#define DWORD_ADDR_MASK		(-sizeof(long))
#endif

/* busy(1) spinner code -- much quicker for PiE (super slow) and a bit
 * quicker for teton as uncached memory access is quite slow.
 */
#if EMULATION || SABLE
#define	BUSY_COUNT	0x8000
#else
#define	BUSY_COUNT	0x200000
#endif

/* maximum number of memtest */
#define	MAX_MEMTESTNUM	11	

/* masks to isolate BX's when there are bad bits */
#define	BX1	0x000000000003ffff
#define	BX2	0x00000000fffc0000
#define	BX3	0x0003ffff00000000
#define	BX4	0xfffc000000000000

/* maximum number of fastmemtest data patterns  */
#define FASTMEMTEST_PATTERN_MAX	6
#define NO_PATTERNS	6
#define EVERY_4K	0x200
#define EVERY_1M	0x20000	/* for ptr incr: div by 8 */	

#endif	/* __MEM_H__ */

