/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1996 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/***********************************************************************\
*       File:           checkclk.c                                      *
*                                                                       *
*	Routines to check the system clock against a outside standard   *
*       - the UART baud rate generator.					*
*                                                                       *
\***********************************************************************/

#ident  "$Revision: 1.5 $"

#include <sys/types.h>
#include <sys/ns16550.h>
#include <promgraph.h>
#include <rtc.h>
#include <sys/SN/launch.h>
#include <sys/SN/gda.h>
#include <sys/SN/SN0/ip27log.h>
#include <libkl.h>
#include <libsk.h>
#include <libsc.h>
#include "io6prom_private.h"

typedef unsigned int 		nas_slice_id_t ;
typedef unsigned short		sliceid_t ;
typedef __int64_t		mod_nas_id_t ;

extern rtc_time_t	rtc_remote_time(__psunsigned_t) ;

extern void 		slave_checkclk_local(mod_nas_id_t);
extern void   		synchronize_clocks(nasid_t, gda_t *, int) ;

extern int      	num_modules ;
extern struct mod_tab   module_table[] ;

#define MODNAS_MOD_SHFT		32
#define MODNAS_MASK		((__psunsigned_t)0xffffffff)
#define TO_MOD_NAS(m,n)		(((__psunsigned_t)m << MODNAS_MOD_SHFT) | n)
#define MOD_FROM_MODNAS(mn)	(mn >> MODNAS_MOD_SHFT)
#define NAS_FROM_MODNAS(mn)	(mn & MODNAS_MASK)

#define NULLCHAR		0
#define SLICEID_MASK		0xffff
#define SLICEID_SHFT		16

#define NASID_FROM_NS(_ns)	((nasid_t) (_ns & SLICEID_MASK))
#define SLICEID_FROM_NS(_ns)	((sliceid_t) (_ns >> SLICEID_SHFT) & SLICEID_MASK)
#define MK_NAS_SLICE_ID(_a, _b) (((nas_slice_id_t)_b << SLICEID_SHFT) | \
				 ((nas_slice_id_t)_a))
#define INVALID_NS		((nas_slice_id_t)INVALID_NASID)

#define LSTK_OFF		1024	/* Launch Stack Offset */
#define LAUNCH_STACK		(IP27PROM_STACK_A + LSTK_OFF)
#define LAUNCH_TIMEOUT		6000 	/* 6 seconds */

#define CHECKCLK_PASS		(drift < 2)
#define CHECKCLK_FAIL		(drift >= 2)
/*
 * WARNING: These values assume a BAUD RATE of 9600.
 * The ideal time taken is calculated as baud rate/10 
 */

#define MICROSEC		1000000
#define TEST_DURATION_SECS	3
#define NCHARS			960
#define IDEAL_TIME_US		TEST_DURATION_SECS * MICROSEC
#define NBYTES			TEST_DURATION_SECS * NCHARS

#define CHECK_CLOCK_SYNC	1
#define NO_CHECK_CLOCK_SYNC	0

void
print_checkclk_msg(moduleid_t modnum)
{
    printf("Checking Clock on Module %d ... ", modnum) ;
}

volatile uart_reg_t *
get_mst_uart_base(nasid_t n)
{
    volatile uart_reg_t *c = NULL ;

    c = (volatile uart_reg_t *)(KL_CONFIG_CH_CONS_INFO(n)->uart_base) ;
    c = (volatile uart_reg_t *) TO_NODE(n, (__psunsigned_t)c) ;

    return c ;
}

void
slv_putc(volatile uart_reg_t *cntrl, char c)
{
    if (!cntrl)
	return ;

    while (!(RD_REG(cntrl, LINE_STATUS_REG) &
        LSR_XMIT_BUF_EMPTY)) {
        rtc_sleep(20) ;
    }

    WR_REG(cntrl, XMIT_BUF_REG, c);
    rtc_sleep(10) ;
}

void
slv_puts(volatile uart_reg_t *cntrl, char *buf)
{
    if (!buf || !cntrl)
	return ;
    while (*buf)
	slv_putc(cntrl, *buf++) ;
}

void
dump_result_plog(nasid_t n, moduleid_t m, int drift)
{
    char 	buf[64] ;

    sprintf(buf, "Module %d - ", m) ;
    strcat(buf, "RSLT: ") ;
    strcat(buf, ( CHECKCLK_PASS ? "PASS": "FAIL")) ;

    ip27log_setenv(n, CHECKCLK_RESULT, buf, 0) ;
}

void
dump_result_console(volatile uart_reg_t *cntrl, int drift)
{
    char 	buf[32] ;

    strcpy(buf, "RSLT: ") ;
    strcat(buf, (CHECKCLK_PASS ? "PASS": "FAIL")) ;
    strcat(buf, "\r\n") ;
    slv_puts(cntrl, buf) ;
}

#define GLOBAL		0
#define INTERNAL 	1

void
change_hub_clock(int flag)
{
    if (flag == INTERNAL)
     	*(LOCAL_HUB(PI_RT_LOCAL_CTRL)) |= 
		(PRLC_USE_INT|PRLC_GCLK_EN) ;
    else if (flag == GLOBAL)
	*(LOCAL_HUB(PI_RT_LOCAL_CTRL)) &= 
		~(PRLC_USE_INT|PRLC_GCLK_EN) ;
}

/*
 * checkclk_local:
 * 	This will be launched on a CPU in each module.
 *  	OR called by the global master.
 */

void
checkclk_local(mod_nas_id_t mn)
{
    int			mstnid 	= NAS_FROM_MODNAS(mn) ;
    moduleid_t		mod 	= MOD_FROM_MODNAS(mn) ;
    int 		cnt ;
    volatile uart_reg_t *cntrl = NULL ;
    unsigned long	ustm0, ustm1, ustime ;
    unsigned long 	a, b ;
    int			drift ;
    unsigned long	usrtm0, usrtm1 ;

    cntrl = get_mst_uart_base(mstnid) ;

    if (cntrl == NULL)
	return ;

    if (mstnid != get_nasid())
    	change_hub_clock(INTERNAL) ;

    ustm0       = rtc_time() ; 		/* start time */
    /* dump a known number of NULL chars to a tty */
    for (cnt = 0 ; cnt < NBYTES; cnt++) {
  	slv_putc(cntrl, NULLCHAR) ;
    }
    ustm1       = rtc_time() ; 		/* stop time */

    ustime      = ustm1 - ustm0 ;
    if (ustime < IDEAL_TIME_US) {
        a = IDEAL_TIME_US ;
        b = ustime ;
    } else {
        a = ustime ;
        b = IDEAL_TIME_US ;
    }
    drift = ((a - b) * 100)/a ;

    if (mstnid != get_nasid())
    	change_hub_clock(GLOBAL) ;

    /* Put the result on the console */

    dump_result_console(cntrl, drift) ;

    /* Put the result on the plog if the diag fails */

    if CHECKCLK_FAIL
    	dump_result_plog(get_nasid(), mod, drift) ;
}

/*
 * get_mod_nasid_vlb
 *     visit_lboard callback.
 *     Get the first available nasid + slice of a
 *     enabled CPU with the given modid.
 */

int
get_mod_nasid_vlb(lboard_t *lb, void *ap, void *rp)
{
    moduleid_t    	modnum = *(moduleid_t *)ap ;
    klcpu_t 		*kc ;

#if 0
printf("%d %d %d %d\n", lb->brd_module, lb->brd_type,
		     	lb->brd_nasid, modnum) ;
#endif

    if ((lb->brd_module == modnum) &&
        (lb->brd_type   == KLTYPE_IP27)) {
	kc = NULL ;
	do {
	    kc = (klcpu_t *)
	    find_component(lb, (klinfo_t *)kc, KLSTRUCT_CPU) ;
	    if ((kc) && KLCONFIG_INFO_ENABLED(&kc->cpu_info)) {
		*(nas_slice_id_t *)rp = MK_NAS_SLICE_ID(lb->brd_nasid,
						(kc->cpu_info.physid)) ;
                return 0 ;
	    }
 	} while (kc) ;
    }
    return 1 ;
}

nas_slice_id_t
get_mod_naslice(moduleid_t modnum)
{
    nas_slice_id_t	rns = INVALID_NS ;
    moduleid_t		m = modnum ;

    visit_lboard(get_mod_nasid_vlb, (void *)&m, (void *)&rns) ;
 
    return rns ;
}

/*
 * checkclk_ns:
 * 	Launch a process on the given CPU
 *	that checks for the local hub's clock.
 */

void
checkclk_ns(nasid_t n, sliceid_t s, moduleid_t m)
{
    mod_nas_id_t	mn ;

    mn = TO_MOD_NAS(m, get_nasid()) ;

    if (n == get_nasid()) {
	checkclk_local(mn) ;
	return ;
    }

    LAUNCH_SLAVE(n, s,
                 (launch_proc_t)PHYS_TO_K0(kvtophys
                     ((void *)slave_checkclk_local)),
                 mn,
                 (void *) TO_NODE(n, LAUNCH_STACK),
                 (void *)0) ;
    LAUNCH_WAIT(n, s, LAUNCH_TIMEOUT);
}

void
check_clocks(void)
{
    gda_t	*gdap ;
    int		maxn, i ;
    __uint64_t	*timval ;

    gdap = GDA ;
    maxn = cpu_get_max_nodes() ;
    timval = (__uint64_t  *)malloc((maxn+1) * sizeof(__uint64_t)) ;

    for (i=0; i<maxn; i++) {
  	timval[i] = *(REMOTE_HUB(gdap->g_nasidtable[i], PI_RT_COUNT)) ;
    }
    for (i=0; i<maxn; i++) {
	printf("%lx\n", timval[i]) ;
    }
    free(timval) ;
}

void
sync_clocks(int flag)
{
    /* defined in traverse_nodes.c */

    synchronize_clocks(get_nasid(), GDA, cpu_get_max_nodes()) ;
    if (flag & CHECK_CLOCK_SYNC)
        check_clocks() ;
}

/* #define TEST */

/* To test on a single module system, the launch mechanism.
 * Just runs the test on 2 cpus - nasids 0 and 1.
 */

/*
 * Run the checkclk program on all the modules.
 * This program checks if the CPU is running at
 * the speed that it is programmed to.
 */

void
checkclk_mod_all(void)
{
    int 		i ;
    nasid_t		n ;
    sliceid_t		s ;
    nas_slice_id_t	ns ;
    moduleid_t		modnum ;

#ifndef TEST
    for (i=1; i <= num_modules; i++) {
#else
    for (i=0; i < 2; i++) {
#endif

#ifndef TEST
	modnum = module_table[i].module_id ;
    	ns = get_mod_naslice(modnum) ;
    	if (ns == INVALID_NS)
	    continue ;
    	n = NASID_FROM_NS(ns) ;
    	s = SLICEID_FROM_NS(ns) ;
#else
	n = i ;
	s = 0 ;
	modnum = 0 ;
#endif

        print_checkclk_msg(modnum) ;

	checkclk_ns(n, s, modnum) ;
    }
    sync_clocks(NO_CHECK_CLOCK_SYNC) ;
}

void
checkclkmod(moduleid_t modnum)
{
    nasid_t		n ;
    sliceid_t  		s ;
    nas_slice_id_t      ns ;

    ns = get_mod_naslice(modnum) ;
    if (ns == INVALID_NS) {
	printf("checkclk: Invalid module number %d\n", modnum) ;
	return ;
    }

    print_checkclk_msg(modnum) ;
    n = NASID_FROM_NS(ns) ;
    s = SLICEID_FROM_NS(ns) ;
    checkclk_ns(n, s, modnum) ;
    sync_clocks(NO_CHECK_CLOCK_SYNC) ;
}

int
checkclkmod_cmd(int argc, char **argv)
{
    moduleid_t	modnum = -1 ;

    if (argc > 1)
	modnum = atoi(argv[1]) ;

    if (modnum >= 0)
        checkclkmod(modnum) ;
    else
        checkclk_mod_all() ;

    return 0 ;
}

