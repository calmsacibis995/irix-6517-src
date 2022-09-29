/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * pod_iadefs.h -- Defines all undefined parameters for diagnostics.
 */

#ifndef __POD_IADEFS_H__
#define __POD_IADEFS_H__

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/EVEREST/addrs.h>
#include <sys/EVEREST/evmp.h>
#include <sys/EVEREST/io4.h>
#include <sys/iotlb.h>
#include <sys/EVEREST/vmecc.h>
#include <sys/EVEREST/everror.h>
#include <sys/EVEREST/evdiag.h>
#include <setjmp.h>
#include <sys/vmereg.h>

#define SUCCESS         0
#define FAILURE         (-1)

#define ALL_5S          0x55555555
#define ALL_AS          0xAAAAAAAA
#define ALL_1S          0xFFFFFFFF
#define ALL_0S          0x00000000

#define A32_ADDR        (unsigned)0xa0000000
#define	A32M_ADDR	(unsigned)0x60000000
/* In Simulation mode, don't use addresses 0x00900000 OR 0x00b00000
 * Since it has the bit-20 as 1, and you may be operating at a memory
 * address lower than 1MByte.
 */
#define A24_ADDR        (unsigned)0x00800000

#define L2MAP_A32       (unsigned)0xa8000000
#define L2MAP_A24       (unsigned)0x00a00000

#define	DMA_RDLEVEL	0x09
#define	DMA_WRLEVEL	0x08

#ifdef	NEWTSIM
#define	SLAVE_DATA	64	
#define	DMA_DONE	5
#else
#define	SLAVE_DATA	128
#endif

/* Following defines exist in csu.s also
 * Modification here should reflect there also
 */
#define	CPUM_SLOT	2
#define	CPUS_SLOT	3



#ifndef	LANGUAGE_ASSEMBLY

#define	WORDSWAP(x)  ((x << 24)|((x&0xff00) << 8)|((x>>8)&(0xff00))| ((unsigned)(x) >> 24))

#define	RD_REG		load_double_lo
#define	WR_REG		store_double_lo


#define SET_REG 	EV_SET_CONFIG
#define GET_REG		EV_GET_CONFIG


#define	RD_REG_HI(_reg)		load_double_hi(_reg)
#define	WR_REG_HI(_reg, _val)	store_double_hi(_reg,_val)

#ifdef	DEBUG_POD
#define Return(x)       print_hex((0xaa<<16)|x); return ( (x > 0) ? FAILURE : SUCCESS )
#else
#define	Return(x)	return ((x > 0) ? FAILURE : SUCCESS)
#endif

/* Chip related definitions not provided for in the fchip.h file	*/
#define	FCHIP_VERNO	0x1

struct	reg64bit_mask {
	int             reg_no;
	unsigned        bitmask0;
	unsigned        bitmask1;
};

struct	reg32bit_mask {
	int             reg_no;
	unsigned        bitmask0;
};


typedef	struct reg32bit_mask	IO4cf_regs;
typedef struct reg64bit_mask	Fchip_regs;
typedef struct reg32bit_mask	Vmecc_regs;

typedef struct iobrd {
int             slot;           /* Slot containing IO4 */
unchar          adapter[IO4_MAX_PADAPS];
}iobrd_t;


#ifdef	NEWTSIM
#define	mk_msg
#define	message
#define	STR		1
#else
unchar	mk_msg(char *,  ...);
int	message(char *, ...); 
#endif

/* Forward declarations for all the functions		*/

#define	VMECC_SETUP	0xabcd	/* Some unused value	*/
#define	PATRN_SIZE 	4

/* utilities functions			*/
extern	int     rw_reg(int, unsigned);
extern	int     rw_reg_hi(int, unsigned);
extern  int     u64lw(unsigned, unsigned);
extern  void    u64sw(unsigned, unsigned, int);
extern	int	get_word(int, int, int);
extern	void	put_word(int, int, int, int);
extern  void	kxset(int);
extern	caddr_t tlbentry(int, int, unsigned);
extern	int	tlbrmentry(caddr_t);

/* iaram related functions 				*/
uint	check_iaram(uint, int);
uint	iaram_rdwr(uint);
uint	iaram_addr(uint);
uint	iaram_walk1s(uint);
void	clear_iaram(uint);

uint	check_io4(unsigned, unsigned);
uint 	io4_check_regs(int);
unchar	io4_check_errreg(int);
uint	io4_check_errpio(int, int);
void	min_io4config(int,int);

#endif
#endif
