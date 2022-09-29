/* Copyright (C) 1989-1995 Silicon Graphics, Inc. All rights reserved.  */
#ifndef _KUCONTEXT_H
#define _KUCONTEXT_H

#ident "$Revision: 1.16 $"

#include <sys/ucontext.h>
#include <sys/ksignal.h>

/*
 * Kernel internal versions of ucontext
 *
 * ucontext - used in /proc, sigsetjmp, signal handlers
 * NOTE: in 32 bit MIPS I  mode this structure is 128 bytes long - the same as
 * a SIGJMPBUF
 * Another version works for MIPS I/II/III and assumes all 64 bit
 * registers
 */

/*
 * IRIX5/SVR4 ABI version
 */
#define IRIX5_NGREG	36

/* To be useful on a 64 bit machine, irix5_greg_t has to
 * be sign-extended on assignment. Hence the signed type.
 */
typedef app32_int_t irix5_greg_t;

typedef irix5_greg_t irix5_gregset_t[IRIX5_NGREG];

typedef struct irix5_fpregset {
	union {
		double		fp_dregs[16];
		float		fp_fregs[32];
		app32_uint_t	fp_regs[32];
	} fp_r;
	app32_uint_t 	fp_csr;
	app32_uint_t	fp_pad;
} irix5_fpregset_t;

typedef struct {
	irix5_gregset_t		gregs;	/* general register set */
	irix5_fpregset_t 	fpregs;	/* floating point register set */
} irix5_mcontext_t;

/* irix5_extra_ucontext is used to save a pointer to extended
 * user context block. This is required to support mips3/mips4
 * o32 binaries. The idea is to save and restore full
 * 64 bits on signal delivery. Since the user context block 
 * has space for only 32-bit registers, full 64bit registers
 * are saved in this extended context block. Later when sigreturn
 * gets called, registers are restored from this block if user
 * modified them or from ucontext (and sign extended) if not.
 */
#define irix5_extra_ucontext uc_filler[36]

typedef struct irix5_ucontext {
	app32_ulong_t	uc_flags;
	app32_ptr_t	uc_link;
	sigset_t   	uc_sigmask;
	irix5_stack_t 	uc_stack;
	irix5_mcontext_t 	uc_mcontext;
	app32_long_t	uc_filler[47];
	/* SGI specific */
	app32_int_t	uc_triggersave;	/* state of graphic trigger */
} irix5_ucontext_t;

/*
 * IRIX5-64 or Extended mode version
 * In extended mode, include status register
 */
#define IRIX5_64_NGREG	37
typedef __uint64_t irix5_64_greg_t;

typedef irix5_64_greg_t irix5_64_gregset_t[IRIX5_64_NGREG];

typedef struct irix5_64_fpregset {
	union {
		double		fp_dregs[32];
#ifdef	_MIPSEB
		struct {
			__uint32_t	_fp_fill;
			float		_fp_fregs;
		} fp_fregs[32];
#else
		struct {
			float		_fp_fregs;
			__uint32_t	_fp_fill;
		} fp_fregs[32];
#endif
		__uint64_t	fp_regs[32];
	} fp_r;
	__uint32_t 	fp_csr;
	__uint32_t	fp_pad;
} irix5_64_fpregset_t;

typedef struct {
	irix5_64_gregset_t	gregs;	/* general register set */
	irix5_64_fpregset_t 	fpregs;	/* floating point register set */
} irix5_64_mcontext_t;

typedef struct irix5_64_ucontext {
	app64_ulong_t	uc_flags;
	app64_ptr_t	uc_link;
	sigset_t   	uc_sigmask;
	irix5_64_stack_t 	uc_stack;
	irix5_64_mcontext_t 	uc_mcontext;
	app64_long_t	uc_filler[49];
} irix5_64_ucontext_t;

typedef struct irix5_n32_ucontext {
	app32_ulong_t	uc_flags;
	app32_ptr_t	uc_link;
	sigset_t   	uc_sigmask;
	irix5_stack_t 	uc_stack;
	irix5_64_mcontext_t 	uc_mcontext;
	app32_long_t	uc_filler[49];
} irix5_n32_ucontext_t;

#endif
