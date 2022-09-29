/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * parity.h
 * Header code related to software fixups after detecting a parity error.
 */

/* syssgi calls */
#define PGUN_PHYS	1   /* Write bad parity to phys address */
#define PGUN_VIRT	2   /* Write bad parity to user virtual address */
#define PGUN_PID_VIRT	3   /* Same, but to a different process. */
#define PGUN_TEST_LOAD	4   /* See if load goes through w/parity error. */
#define PGUN_KTEXT	5   /* Test kernel text recovery. */
#define PGUN_CACHE_NOEE	6   /* test cache errs w/o EE set, kernel mode */
#define PGUN_TEXT_NOEE  7   /* execute code w/o EE set, kernel mode */

/* syssgi structure to pass data/cmds. */
typedef struct {
    unsigned long   addr;   /* Phys or virtual address to write bad parity */
    int		    bitmask;/* Which bit(s) in the byte to blast.  (0=just parity) */
    pid_t	    pid;  /* If virtual address in a given PID. */
} PgunData;

#ifdef _MEM_PARITY_WAR

/* Function prototypes for functions used across files. */
extern int ecc_perr_is_forced(void);
extern int perr_save_info(eframe_t *, k_machreg_t *, uint, k_machreg_t, int);
extern int ecc_exception_recovery(eframe_t *);
extern int dwong_patch(eframe_t *);

/* Globals used across files for parity error recovery. */
extern k_machreg_t *eccf_addr;
extern int no_parity_workaround;

#endif /* _MEM_PARITY_WAR */
