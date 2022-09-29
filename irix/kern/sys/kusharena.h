/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992-1995 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef _KUSHARENA_H
#define _KUSHARENA_H

#include "values.h"
#include "reg.h"
#include "ucontext.h"

/* Macros shamelessly stolen from netman/macros.h */
#define HOWMANY(n, u)   (((n)+(u)-1)/(u))
#define bitsizeof(t)    (sizeof(t) * BITSPERBYTE)

/* Currently the entire kusharena must fit on one page, so we limit
 * the number of vps.  Good enough for prototyping.
 */
#define NT_MAXNIDS	1024 	/* maximum number of NIDs allowed 	*/
#define NT_MAXRSAS	1024 	/* maximum number of RSAs allowed 	*/

/*
 * a0 == upcall code
 * a1 == nid, if applicable
 * a2 == rsa, if applicable
 * a3 reserved for future use
 */
#define RSAupcall_no_context	  4 /* Matched to stolen nid case */
#define RSAupcall_new_context	  5 /* Uthread allocator creates new thread */
#define RSAupcall_inval_nid	  6 /* Invalid nid passed.  Not really
				       necessary.  Code treats this as
				       not nanothreaded */
#define RSAupcall_inval_rsaid	  7 /* Invalid rsa for nid. */
#define RSAupcall_dropped_context 8 /* Attempting to repatriate
				       contexts. */
#define RSAupcall_return_gp	  1040 /* Used for to make C handlers
					  more convenient. */

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)

/* Register Set Area (RSA) values used by pthread libraries (libnanothread) */
#define NULL_NID                0
#define NULL_RSA                0
#define ANY_NID                 -2      /* search for runnable RSA */

typedef struct rsa {
	/* Use existing definition for machine context.
	 * May not setup all of the fields since the user can't modify
	 * them anyway.
	 * Currently do NOT plan to setup:
	 *	R0, K0, K1, CAUSE, SR
	 */
	mcontext_t	rsa_context;
} rsa_t;

/* NOTE: pad out so structure takes 6 cachelines.  Code in ml/LOCORE/intrnorm.s
 * knows the exact size of this structure and code must be changed if the
 * number of cachelines changes.
 */
typedef union {
	rsa_t	rsa;
	char	filler[640];
} padded_rsa_t;

#define rsa_gregs rsa.rsa_context.gregs

typedef struct kusharena {
	volatile uint_t	nrequested;	/* No. of processors requested 	*/
	volatile uint_t	nallocated;	/* No. actually allocted now	*/

	/* filler is here to pad out to cacheline boundary for RSAs.
	 * Must be computed by hand.
	 * Current size of the above fields (with NT_MAXNIDS = NT_MAXRSAS =128)
	 * is 0x908 bytes (you can double check value of KUS_RSA used by
	 * assembly code to make sure alignment is correct).
	 */
	char		cacheline_filler[120];

	/* runnable, but not running NIDs (suspended due to interrupt) */
	volatile uint64_t	rbits[HOWMANY(NT_MAXNIDS, bitsizeof(uint64_t))];

	volatile uint16_t	nid_to_rsaid[NT_MAXNIDS];

	/* available (unallocated) RSAs  */
	volatile uint64_t	fbits[HOWMANY(NT_MAXRSAS, bitsizeof(uint64_t))];

	padded_rsa_t	rsa[NT_MAXRSAS];
} kusharena_t;

#endif /* defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS) */

#endif /* !_KUSHARENA_H */
