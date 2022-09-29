/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/* Local prototypes for kern/ml/RACER directory */

#ident "$Revision: 1.22 $"

int is_in_main_memory(paddr_t);

void load_nvram_tab(void);
char *log_dimmerr(paddr_t, int, char, __uint64_t, int);

void heartclock_intr(struct eframe_s *);
void fastick_maint(struct eframe_s *);

int cpu_mhz_rating(void);
uint findcpufreq_hz(void);

/* access to c0 regs */
uint _get_count(void);

/* soft power */
void cpu_clearpower(void);
void powerfail(intr_arg_t);
void powerintr(intr_arg_t);

/* non-kopt nvram variable access */
int ip30_set_an_nvram(int , int , char *);
int ip30_get_an_nvram(int , int , char *);

#ifdef HEART_INVALIDATE_WAR
void __heart_invalidate_war(void *, unsigned int len);
#endif

extern cpuid_t master_procid;			/* master processor's id */
extern lock_t ip30clocklock;			/* lock time of day clock */
