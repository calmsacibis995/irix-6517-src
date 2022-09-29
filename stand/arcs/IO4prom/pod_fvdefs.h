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
 * pod_fvdefs.h -- Defines all undefined parameters for diagnostics.
 */

#ifndef __POD_FVDEFS_H__
#define __POD_FVDEFS_H__

#ident "arcs/IO4prom/pod_fvdefs.h $Revision: 1.13 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <fault.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/addrs.h>
#include <sys/EVEREST/evmp.h>
#include <sys/EVEREST/io4.h>
#include <sys/iotlb.h>
#include <sys/EVEREST/fchip.h>
#include <sys/EVEREST/vmecc.h>
#include <sys/EVEREST/everror.h>
#include <setjmp.h>
#include <sys/vmereg.h>
#include "pod_fvmsgs.h"

#define SUCCESS         0
#define FAILURE         (-1)

#define ALL_5S          0x55555555
#define ALL_AS          0xAAAAAAAA
#define ALL_1S          0xFFFFFFFF
#define ALL_0S          0x00000000

#define	DIAG_INT_LEVEL	4
#define	DIAG_GRP_MASK	0x44

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

#define	RD_REG			EV_GET_REG
#define	WR_REG			EV_SET_REG

#define SET_REG 	IO4_SETCONF_REG
#define GET_REG     	IO4_GETCONF_REG


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
void	mk_msg(char *,  ...);
void	message(char *, ...); 
#endif

/* Forward declarations for all the functions		*/
extern  int		intr_vector;
extern  int		pslot;
extern  int		proc_no;
extern	int		IO4_slot;
extern	int		Window;
extern	int		Ioa_num;
extern	int		Nlevel;
extern	unsigned 	Chip_base;
extern	volatile unsigned Start_slave;
extern  volatile unsigned Proc2_start;
extern  volatile unsigned Tlb_sema;

#define	VMECC_SETUP	0xabcd	/* Some unused value	*/
#define	PATRN_SIZE 	4
extern  unsigned 	patterns[PATRN_SIZE];
extern	caddr_t		Mapram;
extern  jmp_buf		*Sys_jmpbuf;

/* utilities functions			*/
extern	int     rw_reg(int, unsigned);
extern	int     rw_reg_hi(int, unsigned);
extern  int     u64lw(unsigned, unsigned);
extern  void    u64sw(unsigned, unsigned, int);
extern	int	get_word(int, int, int);
extern	void	put_word(int, int, int, int);
extern  void	kxset(int);
extern  int	get_Cause();
extern  void	set_Cause(unsigned);
extern	caddr_t tlbentry(int, int, unsigned);
extern	int	tlbrmentry(caddr_t);

extern void	analyze_error(int);
extern void	clear_ereg(int);
extern void	setup_globals(int, int, int);
extern int	check_regbits(__psunsigned_t, unsigned, int);


/* iaram related functions 				*/
int	check_iaram(unsigned, int);
int	iaram_rdwr(caddr_t);
int	iaram_addr(caddr_t);
int	iaram_walk1s(caddr_t);

/* fchip related functions				*/

int 	promfchip_init (unsigned, unsigned );
int	check_fchip(int, int, int);
int	fchip_chkver(__psunsigned_t, int, int);
int	fchip_regs_test(__psunsigned_t);
int	fchip_intr_test(__psunsigned_t);
int	fchip_errclr_test(__psunsigned_t);
int	fchip_addrln_test(__psunsigned_t);
int	fchip_tlb_flush(__psunsigned_t);
int	fchip_errbit_test(__psunsigned_t);

/* vmecc related functions				*/

int	vmecc_regtest(__psunsigned_t);
int	vmecc_errtest(__psunsigned_t);
int	vmecc_addr_test(__psunsigned_t);
int	vmecc_intrmask(__psunsigned_t);
int	vmecc_intrtest(__psunsigned_t);
int	vmecc_pio(__psunsigned_t, int, int);
int	vmecc_rmwop(__psunsigned_t);
int	vmecc_lpbk(__psunsigned_t, int, int);
int	vmecc_rd_probe(__psunsigned_t, int, int );
unsigned a32_rmw_chk(__psunsigned_t, uint, uint, uint, uint);
int	vmecc_rmw_lpbk(__psunsigned_t, uint *);
int	vmecc_dmavmeRd(__psunsigned_t);
int	vmecc_dmavmeWr(__psunsigned_t);
int	vmecc_sl_dmard(__psunsigned_t, unsigned *, caddr_t);
int	vmecc_sl_dmawr(__psunsigned_t, unsigned *, caddr_t);
int	vmecc_sl_dmaop(__psunsigned_t);


/* Loopback related functins				*/
int	setup_lpbk(__psunsigned_t, caddr_t, unsigned *);
int	vmecc_piolpbk(__psunsigned_t, int, int);
int	pod_vmelpbk(int, int, int);
int	vmecc_ftlbchk(__psunsigned_t, int, int);

int	p2vmecc_lpbk(__psunsigned_t, int, int);
int	vmep2_sl_dmaop(__psunsigned_t);
/* iaid related functions				*/

int	check_io4(unsigned, unsigned);
int 	io4_check_regs(int);
int	io4_check_errreg(int);
int	io4_check_errpio(int, int);
void	min_io4config(int,int);

#endif
#endif

