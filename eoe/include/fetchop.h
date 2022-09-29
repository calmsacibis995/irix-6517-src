/*
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ifndef __FETCHOP_H__
#define __FETCHOP_H__

#define USE_DEFAULT_PM -1

typedef struct {
	uid_t	uid;	/* owner's user id */
	gid_t	gid;	/* owner's group id */
	mode_t	mode;	/* access modes */
} atomic_perm_t;

typedef int atomic_res_ident_t;
typedef __uint32_t atomic_var_ident_t;
typedef unsigned long atomic_var_t;
typedef struct atomic_maint_s *atomic_maint_t;
typedef struct atomic_var_reg_s *atomic_var_reg_t;
typedef struct atomic_reservoir_s *atomic_reservoir_t;

#define ATOMIC_GET_VADDR 1
#define ATOMIC_SET_VADDR 2

#if SN0
#define FETCHOP_VAR_SIZE 64 /* 64 byte per fetchop variable */
#define FETCHOP_LOAD 0
#define FETCHOP_INCREMENT 8
#define FETCHOP_DECREMENT 16
#define FETCHOP_CLEAR 24
#define FETCHOP_FLUSH 56
#define FETCHOP_STORE 0
#define FETCHOP_AND 24
#define FETCHOP_OR 32
#define FETCHOP_LOAD_OP(addr, op) (                                        \
         *(atomic_var_t *)((__psunsigned_t) (addr) + (op)))
#define FETCHOP_STORE_OP(addr, op, x) (                                    \
	 *(atomic_var_t *)((__psunsigned_t) (addr) + (op)) =              \
	        (fetchop_var_t) (x))
#else /* !SN0 */
#define FETCHOP_VAR_SIZE sizeof(fetchop_var_t)
#endif /* SN0 */

atomic_res_ident_t atomic_alloc_res_ident(size_t);
atomic_res_ident_t atomic_alloc_res_ident_addr(size_t);
/*
 * Initialize the atomic variable reservoir by the given policy module 
 * and from the reservoir in ident.  IF ident is NULL then get a new ident
 * If no particular policy modules are desired, USE_DEFAULT_PM is passed. 
 * Return: a pointer to the reservoir if successful
 *         NULL if failing to initialize
 */
atomic_reservoir_t
atomic_alloc_reservoir(pmo_handle_t, size_t, atomic_res_ident_t);
atomic_reservoir_t 
atomic_alloc_reservoir_addr(pmo_handle_t, size_t, 
			    atomic_res_ident_t, atomic_var_t *, uint);

/*
 * Returns a reserved place in the bitmap array and sets the bit
 * indicating that it has been reserved.  Returns 0 on failure.
 */
atomic_var_ident_t atomic_alloc_var_ident(atomic_reservoir_t);
/*
 * return the virtual address for a given variable identifier
 * If ident is 0 then first get a valid place in the 
 * array 
 * return NULL on failure
 */
atomic_var_t *atomic_alloc_variable(atomic_reservoir_t, atomic_var_ident_t);

/* set permissions for what other processes have access to this reservoir */
int atomic_set_perms(atomic_reservoir_t, atomic_perm_t);

/* Free a atomic variable */
void atomic_free_variable(atomic_reservoir_t, atomic_var_t *);

/* Free a atomic variable */
void atomic_free_var_ident(atomic_reservoir_t, atomic_var_ident_t);

/* remove the shared memory segments associated with this reservoir */
void atomic_free_reservoir(atomic_reservoir_t);

/* Store a value in the atomic variable */
void atomic_store(atomic_var_t *, atomic_var_t);

/* Logical OR operation */
void atomic_store_and_or(atomic_var_t *, atomic_var_t);

/* Logical AND operation */
void atomic_store_and_and(atomic_var_t *, atomic_var_t);

/* Load the value of the atomic variable */
atomic_var_t atomic_load(atomic_var_t *);

/*
 * Increment the variable
 * Return: the old value stored in the variable
 */
atomic_var_t atomic_fetch_and_increment(atomic_var_t *);

/*
 * Decrement the variable
 * Return: the old value stored in the variable
 */
atomic_var_t atomic_fetch_and_decrement(atomic_var_t *);

/*
 * Clear the variable
 * Return: the old value stored in the variable
 */
atomic_var_t atomic_clear(atomic_var_t *);


#define OLD_FETCHOP_COMPATIBILITY
#ifdef OLD_FETCHOP_COMPATIBILITY
#define fetchop_reservoir_id_t atomic_res_ident_t
#define fetchop_reservoir_t atomic_reservoir_t 
#define fetchop_var_t atomic_var_t

fetchop_reservoir_t fetchop_init(pmo_handle_t, size_t);
fetchop_var_t 	*fetchop_alloc(fetchop_reservoir_t);
void		fetchop_free(fetchop_reservoir_t, fetchop_var_t *);

fetchop_var_t 	fetchop_load(fetchop_var_t *);
fetchop_var_t 	fetchop_increment(fetchop_var_t *);
fetchop_var_t 	fetchop_decrement(fetchop_var_t *);
fetchop_var_t 	fetchop_clear(fetchop_var_t *);

void 		storeop_store(fetchop_var_t *, fetchop_var_t);
void		storeop_and(fetchop_var_t *, fetchop_var_t);
void		storeop_or(fetchop_var_t *, fetchop_var_t);

#endif 	/* OLD_FETCHOP_COMPATIBILITY */

#endif /* __FETCHOP_H__ */

