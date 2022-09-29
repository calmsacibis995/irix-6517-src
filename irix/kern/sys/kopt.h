/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990-1992 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*
 * Kernel options.
 * These kernel options are accesible by the user via the sgikopt() system
 * call.
 *
 */
#ifndef __SYS_KOPT_H__
#define __SYS_KOPT_H__

#ident "$Revision: 3.47 $"

#ifdef	_KERNEL

#include <sys/types.h>

#define TRUE_VALUE	"istrue"
#define FALSE_VALUE	"isfalse"

extern int is_true(char *);
extern int is_specified(char *);
extern int is_enabled(char *);
extern char *kopt_find(char *);
extern int kopt_index(int, char **, char **);
extern int kopt_set(char *, char *);
extern void getargs(int, __promptr_t *,  __promptr_t *);

extern char arg_root[];
extern char arg_swap[];
extern char arg_showconfig[];
extern char arg_initfile[];
extern char arg_initstate[];
extern char arg_swplo[];
extern char arg_nswap[];
extern char arg_console[];
extern char arg_gfx[];
extern char arg_keybd[];
extern char arg_cpufreq[];
extern char arg_monitor[];
extern char arg_sync_on_green[];
extern char arg_pagecolor[];		/* IP20/IP22/IP26/IP28/IP30 only */
extern char arg_screencolor[];
extern char arg_logocolor[];
extern char arg_netaddr[];
extern char arg_srvaddr[];
extern char arg_tapedevice[];
extern char arg_dlserver[];
extern char arg_dlgate[];
extern char arg_maxpmem[];
extern char arg_debug_bigmem[];
#if IP20 || IP22 || IP26 || IP28 || IP30 || IPMHSIM
extern char arg_eaddr[];
#endif
extern char arg_diagmode[];
extern char arg_scsiretry[];
extern char arg_scsihostid[];
extern char arg_syspart[];
extern char arg_ospart[];
extern char arg_osloader[];
extern char arg_osfile[];
extern char arg_osopts[];
extern char arg_autoload[];
extern char arg_timezone[];
extern char arg_sgilogo[];
extern char arg_nogui[];
extern char arg_kernname[];
#ifdef _SYSTEM_SIMULATION
extern char arg_sablediskpart[];
extern char arg_sableexectrace[];
extern char arg_sableio[];
#endif
#ifdef TRITON
extern char arg_triton_invall[];
#endif /* TRITON */
#ifdef IP32
extern char arg_bump_leds[];
extern char arg_reset_dump[];
extern char arg_ecc_enabled[];
extern char arg_ip32nokp[];
#endif
#if defined(EVEREST) && defined(MULTIKERNEL)
extern char arg_initmkcell_serial[];
extern char arg_evmk_numcells[];
#endif
#if CELL_IRIX
extern char arg_golden_cell[];
extern char arg_firewall[];
#endif
#if CELL
extern char arg_num_cells[];
#endif

#if defined (CELL_IRIX) && defined (LOGICAL_CELLS)
extern char arg_subcells[];
extern char arg_max_subcells[];
#endif /*  (CELL_IRIX) && (LOGICAL_CELLS) */

#ifdef _R5000_CVT_WAR
extern char arg_r5000_cvt_war[];
#endif /* _R5000_CVT_WAR */

#ifdef _R5000_BADVADDR_WAR
extern char arg_rm5271_badvaddr_war[];
#endif /* _R5000_BADVADDR_WAR */

extern char arg_mrmode[];
extern char arg_mrconfig[];
extern int showconfig;

/*
 * Kernel argument table. described in detail in master.d/kopt.
 */
struct	kernargs {
	char	*name;
	char	*str_ptr;		/* Pointer to local storage  */
	int	strlen;			/* Size of local storage */
	int	readonly;
};

#endif /* _KERNEL */

#endif /* __SYS_KOPT_H__ */
