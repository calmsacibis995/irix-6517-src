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

#ident "$Revision: 1.27 $"

#ifndef	_LIBASM_H_
#define	_LIBASM_H_

#if _LANGUAGE_C
#include <ksys/elsc.h>
#endif /* _LANGUAGE_C */

#define JINV_ICACHE	0x01
#define JINV_DCACHE	0x02
#define JINV_SCACHE	0x04

#if _LANGUAGE_ASSEMBLY
/*
 * JUMP_TABLE macro does a PC-relative jump into a table without
 * referencing any labels.  Offreg should be a register containing
 * the offset from the beginning of the table, which should begin
 * directly after the JUMP_TABLE macro.  Uses k0, k1.
 */

#define JUMP_TABLE(offreg)	move	k1, ra;			\
				bgezal	zero, 99f;		\
				 nop;				\
			99:	move	k0, ra;			\
				move	ra, k1;			\
				daddu	k0, offreg;		\
				daddiu	k0, 24;			\
				jr	k0;			\
				 nop
#endif /* _LANGUAGE_ASSEMBLY */

#if _LANGUAGE_C

ulong	parity(ulong x);
ulong	bitcount(ulong x);
ulong	firstone(ulong x);
ulong	qs128(ulong addr);
ulong	gcd(ulong x, ulong y);
int	get_cpu_irr(void);
uint	get_fpu_irr(void);
void	set_BSR(ulong value);
ulong	get_BSR(void);
void	set_libc_device(ulong base);
ulong	get_libc_device(void);
void	set_libc_verbosity(int);
int	get_libc_verbosity(void);
void	set_ioc3uart_base(ulong base);
ulong	get_ioc3uart_base(void);
int	get_ioc3uart_baud(void);
void	set_elsc(elsc_t *e);
elsc_t *get_elsc(void);
void	set_sp(ulong addr);
ulong	get_sp(void);
void	set_gp(ulong addr);
ulong	get_gp(void);
ulong	get_pc(void);
ulong	get_cop0(ulong num);
void	set_cop0(ulong num, ulong value);
ulong	get_cop1(ulong num);
void	set_cop1(ulong num, ulong value);
void	delay(ulong loops);
void	halt(void);
void	sync_instr(void);
int	get_endian(void);	/* big = 0, little = 1 */
ulong	randnum(void);

ulong	jump_inval(ulong addr, ulong parm1, ulong parm2,
		   int jinv_flags, ulong new_sp);

uint	save_and_call(__uint64_t *save_addr,
		      uint (*func)(__scunsigned_t),
		      __scunsigned_t parm);
void	save_sregs(__uint64_t *addr);
void	restore_sregs(__uint64_t *addr);
void	save_gprs(__uint64_t *addr);

void	set_pi_eint_pend(ulong value);
void	set_pi_err_sts0_a(ulong value);
void	set_pi_err_sts1_a(ulong value);
void	set_pi_err_sts0_b(ulong value);
void	set_pi_err_sts1_b(ulong value);
void	set_md_dir_error(ulong value);
void	set_md_mem_error(ulong value);
void	set_md_proto_error(ulong value);
void	set_md_misc_error(ulong value);

ulong	get_pi_eint_pend(void);
ulong	get_pi_err_sts0_a(void);
ulong	get_pi_err_sts1_a(void);
ulong	get_pi_err_sts0_b(void);
ulong	get_pi_err_sts1_b(void);
ulong	get_md_dir_error(void);
ulong	get_md_mem_error(void);
ulong	get_md_proto_error(void);
ulong	get_md_misc_error(void);

#endif /* _LANGUAGE_C */

#endif /* _LIBASM_H_ */
