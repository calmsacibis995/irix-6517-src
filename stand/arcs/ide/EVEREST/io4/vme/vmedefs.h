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

#ifndef __IDE_VMEDEFS_H__
#define __IDE_VMEDEFS_H__

#ident "arcs/ide/IP19/io/io4/vme $Revision: 1.14 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/fault.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/addrs.h>
#include <sys/EVEREST/evmp.h>
#include <sys/EVEREST/io4.h>
#include <sys/iotlb.h>
#include <sys/EVEREST/fchip.h>
#include <sys/EVEREST/vmecc.h>
#include <sys/EVEREST/evintr.h>
#include <setjmp.h>
#include <everr_hints.h>
#include <fault.h>		/* arcs/include/fault.h */
#include <ide_msg.h>

#define SUCCESS         0
#define FAILURE         (-1)

#define ALL_5S          0x55555555
#define ALL_AS          0xAAAAAAAA
#define ALL_1S          0xFFFFFFFF
#define ALL_0S          0x00000000

#define	VMEIDE_ILEVEL	4
#define	IDE_GRP_MASK	0x44

/* 
 * Few data structures used by the cpus's in a multi-cpu configuration
 * use this parameter. Increase this depending on the no of processors
 * you want to use in the diagnostics
 */

#if _MIPS_SIM != _ABI64
#define A32_ADDR        (unsigned)0xa0000000
#define	A32M_ADDR	(unsigned)0x60000000
#define A24_ADDR        (unsigned)0x00a00000

#define L2MAP_A32       (unsigned)0xa8000000
#define L2MAP_A24       (unsigned)0x00a80000
#else
#define A32_ADDR        (__uint32_t)0xa0000000
#define	A32M_ADDR	(__uint32_t)0x60000000
#define A24_ADDR        (__uint32_t)0x00a00000

#define L2MAP_A32       (__uint32_t)0xa8000000
#define L2MAP_A24       (__uint32_t)0x00a80000
#endif

#define A24_NPAM	0x39
#define	A24_NPBAM	0x3b
#define	A24_SUAM	0x3d
#define	A24_SUBAM	0x3f

#define A32_NPAM 	0x9
#define	A32_NPBAM	0x0b
#define	A32_SUAM	0x0d
#define	A32_SUBAM	0x0f



#define	SLAVE_DATA	128
#define VMEDATA         (32*1024)       /* Should be > 8k and < 2M */


#if	_LANGUAGE_C

#define	WORDSWAP(x)  ((x << 24)|((x&0xff00) << 8)|((x>>8)&(0xff00))| ((unsigned)(x) >> 24))

#define	WINDOW(x)	((x >> 19) & 0x7)
#define	ANUM(x)		((x >> 16) & 0x7)


#define	RD_REG		EV_GET_REG
#define	WR_REG 		EV_SET_REG

#define	Return(x)	return ((x > 0) ? FAILURE : SUCCESS)

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

#define	ms_delay(i)	us_delay(i*1000)

/* Chip related definitions not provided for in the fchip.h file	*/
#define	FCHIP_VERNO	0x1
#define	FCHIP2_VERNO	0x2
#define FCHIP3_VERNO	0x3


#define	DIS_INT		set_SR(get_SR() & (unsigned)0xfffffffe)
#define	ENA_INT		set_SR(get_SR() | 0x1)
#define	CLR_INT		set_Cause(get_Cause() & (unsigned)0xffff00ff)

/* Forward declaraion of all the functions in vme */

/* Globally accessed variables */
extern volatile struct CDSIO_port *port;

#if _MIPS_SIM != _ABI64
extern volatile unsigned char *command_port;
extern volatile unsigned char *cdsio_base_addr;
extern volatile unsigned char *vme_base_addr;

extern volatile unsigned char *status_port;
extern volatile unsigned short *version_number;

extern volatile unsigned vmecc_addr;
#else
extern volatile paddr_t command_port;
extern volatile paddr_t cdsio_base_addr;
extern volatile paddr_t vme_base_addr;

extern volatile paddr_t status_port;
extern volatile paddr_t version_number;

extern volatile paddr_t vmecc_addr;
#endif

extern	jmp_buf	vmebuf;

#if _MIPS_SIM != _ABI64
int     iaram_rdwr(caddr_t);
int	iaram_addr(caddr_t);
int  	iaram_walk1s(caddr_t); 

unsigned get_vmebase(int slot, int anum);
void clear_vme_intr(unsigned vmebase);
int init_fchip(unsigned base_addr);
int init_vmecc(unsigned base_addr);
#else
int     iaram_rdwr(paddr_t);
int	iaram_addr(paddr_t);
int  	iaram_walk1s(paddr_t); 

paddr_t get_vmebase(int slot, int anum);
void clear_vme_intr(paddr_t vmebase);
int init_fchip(paddr_t base_addr);
int init_vmecc(paddr_t base_addr);
#endif /* TFP */

#endif /* _LANGUAGE_C */

#endif /* __IDE_VMEDEFS_H__ */
