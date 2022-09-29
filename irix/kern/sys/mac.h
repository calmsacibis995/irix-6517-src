
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef	__MAC_
#define	__MAC_

#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 1.7 $"

struct mac_label;
/*
 * Data types required by POSIX P1003.1eD15
 */
typedef struct mac_label * mac_t;

/*
 * Functions required by POSIX P1003.1eD15
 */
extern int mac_dominate(mac_t, mac_t);
extern int mac_equal(mac_t, mac_t);
extern int mac_free(void *);
extern mac_t mac_from_text(const char *);
extern mac_t mac_get_fd(int);
extern mac_t mac_get_file(const char *);
extern mac_t mac_get_proc(void);
extern mac_t mac_glb(mac_t, mac_t);
extern mac_t mac_lub(mac_t, mac_t);
extern int mac_set_fd(int, mac_t);
extern int mac_set_file(const char *, mac_t);
extern int mac_set_proc(mac_t);
extern ssize_t mac_size(mac_t);
extern char *mac_to_text(mac_t, size_t *);
extern char *mac_to_text_long(mac_t, size_t *);
extern int mac_valid(mac_t);

/*
 *  Label component data types.
 */
struct mac_b_label;
typedef struct mac_b_label * msen_t;
typedef struct mac_b_label * mint_t;

/*
 *  Functions for label components.
 */
extern mac_t mac_from_msen_mint(msen_t, mint_t);
extern mac_t mac_from_msen(msen_t);
extern mac_t mac_from_mint(mint_t);

extern msen_t msen_from_text(const char *); 
extern char * msen_to_text(msen_t, size_t *);
extern msen_t msen_from_mac(mac_t);
extern int msen_free(void *);
extern int msen_equal(msen_t, msen_t);
extern int msen_valid(msen_t);
extern int msen_dom(msen_t, msen_t);
extern ssize_t msen_size(msen_t);
extern msen_t msen_dup(msen_t lp);

extern mint_t mint_from_text(const char *); 
extern char * mint_to_text(mint_t, size_t *);
extern mint_t mint_from_mac(mac_t);
extern int mint_free(void *);
extern int mint_equal(mint_t, mint_t);
extern int mint_valid(mint_t);
extern int mint_dom(mint_t, mint_t);
extern ssize_t mint_size(mint_t);
extern mint_t mint_dup(mint_t lp);

#ifdef _KERNEL
extern msen_t msen_add_label(msen_t);
extern int msen_copyin_label( msen_t, msen_t * );
extern int mint_copyin_label( mint_t, mint_t * );
extern mint_t mint_add_label(mint_t);
#endif

#ifdef __cplusplus
}
#endif

#endif	/* __MAC_ */
