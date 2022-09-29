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

#ident "$Revision: 1.33 $"

#ifndef	_MDIR_H_
#define	_MDIR_H_

#ifdef _LANGUAGE_C

#include <sys/SN/error.h>

/*
 * Internal
 */

int	mdir_bddir_fill(__uint64_t sback,	/* mdir_asm.s */
			__uint64_t eback,
			__uint64_t dirhi,
			__uint64_t dirlo,
			__uint64_t prot);

void	mdir_bdecc_fill(__uint64_t sback,	/* mdir_asm.s */
			__uint64_t eback,
			uint value);

/*
 * Public
 */

int	mdir_xlate_syndrome(char *buf, int type, __uint64_t syn);

void	mdir_xlate_addr_mem(char *buf, __uint64_t address, int bit);
void	mdir_xlate_addr_pdir(char *buf, __uint64_t address, int bit,
			     int dimm0_sel);
void	mdir_xlate_addr_sdir(char *buf, __uint64_t address, int bit,
			     int dimm0_sel);

void	mdir_init(nasid_t nasid, __uint64_t freq_hub);


void	mdir_error_get_clear(nasid_t nasid, hub_mderr_t *err);
int	mdir_error_check(nasid_t nasid, hub_mderr_t *err);
int	mdir_error_decode(nasid_t nasid, hub_mderr_t *err);

void	mdir_copy_prom(void *prom_addr,		/* mdir_asm.s */
		       void *mem_addr,
		       __uint64_t length);

void	mdir_init_bddir(int premium,
			__uint64_t saddr,
			__uint64_t eaddr);


void	mdir_init_bdecc(int premium,
			__uint64_t saddr,
			__uint64_t eaddr);

void	mdir_config(nasid_t nasid, u_short *prem_mask);

void	mdir_bank_get(nasid_t nasid, int bank,
		      __uint64_t *phys_base, __uint64_t *length);

void	mdir_sz_get(nasid_t nasid, char *bank_sz);

void	mdir_dir_decode(char *buf, int premium,
			__uint64_t lo, __uint64_t hi);

char   *mdir_size_string(int size);

int	mdir_pstate(int, __psunsigned_t, __uint32_t);
int	mdir_sstate(int, __psunsigned_t, __uint32_t);
void	mdir_dirstate_usage(void);
void	mdir_disable_bank(nasid_t nasid, int bank);
__uint64_t	make_mask_from_str(char *str) ;
int 		is_node_memless(nasid_t n) ;

#define	MDIR_DIMM0_SEL(nasid)   \
	((REMOTE_HUB_L(nasid, MD_MEMORY_CONFIG)         \
	& MMC_DIMM0_SEL_MASK) >> MMC_DIMM0_SEL_SHFT)

#endif /* _LANGUAGE_C */

#endif /* _MDIR_H_ */
