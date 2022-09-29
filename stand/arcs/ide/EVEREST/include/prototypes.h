#ifndef __IDE_EVEREST_IP19_H__
#define __IDE_EVEREST_IP19_H__
#endif

#include <pod_cache.h>
#include <sys/EVEREST/s1chip.h>
#include <setjmp.h>
#include <sys/newscsi.h>
#include <sys/wd95a.h>
#include <sys/wd95a_struct.h>
#include <sys/EVEREST/s1chip.h>
#include <sys/mtio.h>
#include <sys/wd95a_wcs.h>

/* #include <sys/dksc.h> */

/**************************************************************************
	ugly hack, I'm so sorry, I just want this stuff to compile...grw
	this has to be inline before the include of sgidefs.h
**************************************************************************/
#ifndef _MIPS_SZLONG		/* only define if not already defined */
#define _MIPS_SZLONG 64
#define _MIPS_SZPTR 64
#define	_LONGLONG
#endif

#include <sgidefs.h>



/*
 * /include/prototype.h
 *
 *	-- defines, addresses, and configs for IP19, IP21.
 *
 *
 * Copyright 1991,1992, 1993 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

extern unsigned int  	calc_bustag_size(void);
extern void 		msg_printf(int, char *, ...);
extern int 		printf(const char *, ...);
int			sprintf(char *, const char *, ...);
void			_errputs(char *);
extern unsigned int  	get_bustag_mask(void);
extern void setconfig_info(void);
extern void date_stamp(void);
extern char * atobu(const char *, unsigned int *);
extern uint pi_fill(__psunsigned_t);

/***** called from taghi.c *****************************/


void eprintf( char *, ...);
int err_msg(uint, void *,  ...);
void	store_double_hi(long long *, long long);

/* load_double prototyped in everest.h */
long long	load_double_los(long long *);
long long	load_double_bloc(long long *);

/***** I don't know where these come from *********************/
/*
int     bcmp(char *, char *, int );
*/
uint    firstblock_ofpart(int, int, uint *);
int 	_dkipiforce (int, int, int);
void 	_dkipiclear(int);
int _dkipdmatst(int, __psunsigned_t, __psunsigned_t, __psunsigned_t);
void	xdebug(int);


/***** called from EVERESTlib.c *****************************/
void	parse_config(void);
void	parse_prid(void);
void	parse_status(void);
void	parse_cause(void);
void	desc_cause(void);
void	desc_status(void);
void	 r4kregs(void);
void	mc3_error_clear(int);
void	mc3_error_show(int);
void	mc3_mem_error_log(int);
void	mc3_ebus_error_log(int);
void	io4_error_clear(int);
void	io4_error_show(int);
void	io4_error_log(int);
void	adap_error_show(int,int);
void	adap_error_log(int,int);
void	adap_error_clear(int, int);
void	cpu_err_show(void);
void	cpu_err_log(void);
void	set_pgmask(int);
int	check_cpuereg(void);
void	dump_important(int);

/**** misc functions *********************************************
***************************************************************/
void	error(void);
#define  BREAK1(x) 	x = 0xdead
#define  BREAK2(x) 	x = 0xd00d
#define  BREAK3(x) 	x = 0xbeef
#define  BREAK4(x) 	x = 0xbabe
#define	 HERE(x)	set_cc_leds(x);

/**** pod_cache.s *********************************************
	returns 0 for pass,
	nonzero = fail      
***************************************************************/
 
long	dcache_data(long addr,long data,long size,long line);
long	dcache_addr(long addr , long size, long line);		
long	dcache_tag(void);

long	scache_data(long addr, long data, long size ,long line);		
long	scache_addr(long addr, long size, long line);	
long	scache_tag(void);	
long	check_scachesize(void);


void	pon_fix_dcache_parity(void);
long	read_scachesize(void);
long	pod_check_scache0(void);

void	clear_dcache(void);
void	clear_scache(void);

void	flush_tlb(int);
void	write_reg(unsigned long ,long);
void	pon_invalidate_IDcaches(void);
void	pon_invalidate_dcache(void);
void	flush_SAQueue(void);
void	_write_tag_only(int, int, int);
void	move_to_KP(long, long);

/**** ledprom.s ***********************************************/
void	ledloop(void);
void	set_cc_leds(long);

/**** ledprom.s ***********************************************/
void	scprint(char *);
void	sysctlr_message(char *);

/**** cachetest.c***********************************************/
long	address_space(void);

/**** ccio.s ***************************************************/
void	ccuart_putc(char);
void	ccuart_puts(char *);

/**** sysctlr.s ***************************************************/
void	sysctlr_message(char *);



/**** cachetest.c ***************************************************/
void	gcache_test(void);
void	gcache_size(void);

void 	loadtag(int, int);


/***** msg_printf.c *************************************************/


/***** io4_select.c *************************************************/
int		io4_select(int, int, char**);
int		io4_search(int, int *, int *);

/***** miscellaneous*************************************************/

int	cpuid(void);
int	console_is_gfx(void);
int	vsprintf(char *, const char *, char *);
int      vprintf(const char *, char *);

void	setup_err_intr(int, int);
void	ide_init_tlb(void);
void	show_fault(void);
void	clear_nofault(void);
int	badaddr(volatile void *, int );

/***** duart_lpbk.c*************************************************/
void configure_duart(u_char *, u_short, int);

/****** genasm.s **************************************************/
long long Getsr(void);

ulong get_CAUSE(void);
ulong GetCause(void);
uint getcause(void);

/***** ide_prims.c*************************************************/
int	continue_on_error(void);

/***** libsc/lib/libasm.s*******************************************/
void    bzero(void *, size_t);
void    bcopy(const void *, void *, size_t);
/***** libsc/lib/strstuff.c ****************************************/
char *	strcat( char *, const char *);
int 	strcmp( const char *, const char *);
char *	strcpy( char *, const char *);
size_t 	strlen( const char *);
char *	strdup( const char *);
char *	strstr( const char *, const char *);
int 	strncmp(const char *, const char *, size_t );
char * 	strncat(char *, const char *, size_t );
char * 	strncpy(char *, const char *, size_t );


/***** lib/stdio.c			 ****************************************/
char *	gets(char *);
int 	put(char *);


/***** libsk/ml/EVEREST.c ****************************************/
int		tlbrmentry(caddr_t);
caddr_t  	tlbentry(int, int , unsigned );
void		cpu_get_eaddr(u_char *);
int		flush_iocache(int);

/***** libsk/ml/delay.c *****************************************/
void	us_delay(uint);


/***** libsk/lib/fault.c ****************************************/
void	set_nofault(jmp_buf);

#ifdef NEVER
/***** lib/libsk/io/wd95.c **************************************/
int		wd95_present(char *);
struct scsi_target_info * wd95info(u_char, u_char, u_char);

/***** lib/libsk/io/wd95.c **************************************/
int	init_wd95a(char *, int, wd95ctlrinfo_t *);

/***** lib/libsk/net.if_ee.c*i ************************************/
int	ee_reset(void);
int	ee_init(void);

/***** ide/EVEREST/io4/scsi/scsilib.c ***************************/
int	scsi_setup(void);
int	senddiag(int, int, int);
void	s1infoprint(int);
int	scsi_setup(void);
int 	test_scsi(int , char**, int (*)()); 
int	ide_wd95busreset(wd95ctlrinfo_t *);
int	report_scsi_results (int, int *);
#endif

/***** ide/EVEREST/lib/ideio.c **********************************/
int	gopen(int, uint, uint, char *, int);
void	gclose(int);
int 	gread(daddr_t , void *, uint , int , int , int );
int	gwrite(daddr_t , void *, uint , int , int , int );

void	clearvh(void);
int	init_label(int);

int	scsi_dname(char *, int , int );

/***** ide/EVEREST/lib/vec.s **********************************/
void	DoErrorEret(void);

/***** ide/EVEREST/io4/enet/xmittest.c *****************************/
int	setup_walk(int);
int	setup_xmit(int, int);
int	setup_afivea(int);
int 	doxmit(int);
int 	verify_data(int);
int 	setup_fzerof(void);
int 	setup_incrse(int);

/***** ide/EVEREST/io4/enet/enetlib.c *****************************/
int	test_enet (int, char **, int());

/***** ide/EVEREST/io4/enet/readcollide.c *****************************/
int setup_xmit_ctr(int , int );
int doxmit_cntr(int);
void output_counters(void);
void setup_walk_ctr(int);
void setup_afivea_ctr(int);
void setup_fzerof_ctr(void);
void setup_incrse_ctr(int);





/***** lib/libsk/io/ioutils.c **************************************/
void	kern_free(void *);

/***** lib/libsk/ml/s1chip.c **************************************/
int	s1dma_flush(dmamap_t *);
/* int	s1_find_type(s1chip_t *);*/

/***** lib/libsk/io/dksc.c*****************************************/
/* void	drive_release(register struct dksoftc *); */

/***** lib/libsk/io/wd95.c*****************************************/
void    wd95intr(int, struct eframe_s *);
void    wd95timeout(wd95request_t *);
void    wd95a_do_cmd(wd95unitinfo_t *, wd95request_t *);
void    wd95clear(wd95ctlrinfo_t *, wd95unitinfo_t *, char *);
void    wd95_abort(register volatile char *);
void    wd95complete(wd95ctlrinfo_t *, wd95unitinfo_t *, wd95request_t *,
                             scsi_request_t *);
void    wd95start(wd95ctlrinfo_t *, wd95unitinfo_t *);
/* void	struct scsi_target_info *wd95info(u_char , u_char , u_char ); */
int     wd95busreset(register wd95ctlrinfo_t *);
void    wd_rm_wq(register wd95unitinfo_t *, register wd95request_t *);
int     wd95_earlyinit(int);
void    wd95_fin_reset(wd95ctlrinfo_t *);



/***** lib/everror.c  ****************************************/
void	everest_error_show(void);

/***** lib/libsc/lib/atob.c ***************************************/
int 	atoi(const char *);

/***** lib/libsc/lib/malloc.c ***************************************/
void *	kern_calloc(size_t, size_t);

/***** libsk/ml/EVERESTasm.s  ****************************************/
int u64lw(int , int );
int u64sw(int, int, int);

/***** lib/EVERESTlib.c  ****************************************/
int		cpu_intr_pending(void);
void 		setup_err_intr(int , int );
int		io_err_log(int, int);
int		io_err_show(int, int);
int		io_err_clear(int, int);
int		board_type(int);
void		ide_init_tlb(void);
int		clear_err_intr(int, int);
int 		fmaster(int, int);
int  		io4_window(int);
int		adap_slots(int);
int		adap_type(int, int);
int		clear_IP(void);
void		cpu_error_clear(void);
void		cpu_err_clear(void);
int 		report_check_error(int);
int 		check_ioereg(int , int );
int		mem_err_clear(int);
void		mem_err_log(void);
void		mem_err_show(void);
int 		banks_populated(int , int );
int 		board_type(int );
int 		hw_boardfound(int , int );
int 		hw_adapfound(int , int, int );
int		fmaster(int, int);
int		scsichip(int, int);
__psunsigned_t 	map_addr(__psunsigned_t , int , int );
int 		check_memereg(int );
int 		s1num_from_slotadap(int , int );



/***** EVEREST/mc3/<*>.c *********************************/
void memtest_init(void);

/***** mapram.c ***********************************************/
unchar		maptest(int);
unsigned	mapram_rdwr(__psunsigned_t, int);
unsigned	mapram_addr(__psunsigned_t, int);
unsigned	mapram_walk1s(__psunsigned_t, int);


/***** scsi_dmaintr.c ***********************************************/
int 		dmadoit(int, int, int, int);


/***** mc3/memutils.c ***********************************************/
int 	wcheck_mem(__psunsigned_t);
unsigned long long readconfig_reg(unsigned int , unsigned int );
uint	read_reg(uint, uint );
int 	bfail_mem(__psunsigned_t, unsigned char, unsigned char, int);
int 	chk_memereg(int);

/***** mc3/cachemem.c ***********************************************/
int memaddr(__psunsigned_t,__psunsigned_t, int, int);

/***** fpu/fpregs.c ***********************************************/
extern void SetFP0(uint);
extern void SetFP1(uint);
extern void SetFP2(uint);
extern void SetFP3(uint);
extern void SetFP4(uint);
extern void SetFP5(uint);
extern void SetFP6(uint);
extern void SetFP7(uint);
extern void SetFP8(uint);
extern void SetFP9(uint);
extern void SetFP10(uint);
extern void SetFP11(uint);
extern void SetFP12(uint);
extern void SetFP13(uint);
extern void SetFP14(uint);
extern void SetFP15(uint);
extern void SetFP16(uint);
extern void SetFP17(uint);
extern void SetFP18(uint);
extern void SetFP19(uint);
extern void SetFP20(uint);
extern void SetFP21(uint);
extern void SetFP22(uint);
extern void SetFP23(uint);
extern void SetFP24(uint);
extern void SetFP25(uint);
extern void SetFP26(uint);
extern void SetFP27(uint);
extern void SetFP28(uint);
extern void SetFP29(uint);
extern void SetFP30(uint);
extern void SetFP31(uint);
extern uint GetFP0(void);
extern uint GetFP1(void);
extern uint GetFP2(void);
extern uint GetFP3(void);
extern uint GetFP4(void);
extern uint GetFP5(void);
extern uint GetFP6(void);
extern uint GetFP7(void);
extern uint GetFP8(void);
extern uint GetFP9(void);
extern uint GetFP10(void);
extern uint GetFP11(void);
extern uint GetFP12(void);
extern uint GetFP13(void);
extern uint GetFP14(void);
extern uint GetFP15(void);
extern uint GetFP16(void);
extern uint GetFP17(void);
extern uint GetFP18(void);
extern uint GetFP19(void);
extern uint GetFP20(void);
extern uint GetFP21(void);
extern uint GetFP22(void);
extern uint GetFP23(void);
extern uint GetFP24(void);
extern uint GetFP25(void);
extern uint GetFP26(void);
extern uint GetFP27(void);
extern uint GetFP28(void);
extern uint GetFP29(void);
extern uint GetFP30(void);
extern uint GetFP31(void);


/***** ide/ide_prims.c ***********************************************/
int 	quick_mode(void);


/****** this was undefined.h *****************************************/

#ifdef TFP

#define SR_DE 0		/* since this is a "or" term, it shouldn't hurt */
#endif
#if defined(IP21) || defined(IP25)
#define SR_CE 0			
#define LWIN_PHYS(x,y)  (LWINDOW_BASE | (x <<  LWIN_REGIONSHIFT) | \
			(y  << LWIN_PADAPSHIFT))

/******** where are these at ???? *****************************************/
int		dkipdmatst(unsigned, unsigned, unsigned, unsigned);
int		adapt_slots(unsigned *);

#endif

extern void set_mem_locs(uint, uint, uint, uint);
extern uint check_tag(uint, uint, uint, uint, uint);
extern uint check_2ndary_val(uint, uint, uint);
extern void write_word(uint, uint, uint);
extern int flush2ndline(uint);
extern uint check_mem_val(uint, uint, uint);
extern uint read_word(uint, uint);
extern uint sd_HWB(__psunsigned_t);
extern void HitInv_D(__psunsigned_t, uint, uint);
extern void HitWB_D(__psunsigned_t, uint, uint);
extern void HitWB_SD(__psunsigned_t, uint, uint);
extern void HitWB_I(__psunsigned_t, uint, uint);
extern void HitInv_SD(__psunsigned_t, uint, uint);
extern void HitInv_I(__psunsigned_t, uint, uint);
extern void HitWBInv_SD(__psunsigned_t, uint, uint);
extern void InvCache_D(__psunsigned_t, uint, uint);
extern void InvCache_SD(__psunsigned_t, uint, uint);
extern void IndxStorTag_I(__psunsigned_t, uint);
extern uint IndxLoadTag_I(__psunsigned_t);
extern void IndxStorTag_D(__psunsigned_t, uint);
extern uint IndxLoadTag_D(__psunsigned_t);
extern void IndxStorTag_SD(__psunsigned_t, uint);
extern uint IndxLoadTag_SD(__psunsigned_t);
extern void IndxWBInv_D(__psunsigned_t, uint, uint);
extern uint ChkITag_CS(__psunsigned_t, uint, uint, uint);
extern uint ChkSdTag(__psunsigned_t, uint, uint, __psunsigned_t);
extern uint ChkPTag(__psunsigned_t, uint, uint, uint);
extern uint ChkDTag_CS(__psunsigned_t, uint, uint, uint);
extern uint GetTagLo(void);
extern void Fill_Icache(__psunsigned_t, uint, uint);
extern void CreateDE_SD(__psunsigned_t, uint, uint);
extern void fillmemW(__psunsigned_t, __psunsigned_t, uint, uint);
extern void filltagW_l(u_int *, u_int *, uint, uint);
extern uint GetECC(void);
extern void scacherr(uint);
extern uint GetPstate(uint);
extern void ide_invalidate_caches(void);

extern void set_compare(uint);
extern uint get_compare(void);
extern void set_count(uint);
extern uint get_count(void);

extern void EnableTimer(uint);
extern int RemoveHandler(int);
extern void StopTimer(void);
extern int InstallHandler(int, int (*)(void));
extern int TimerHandler(void);
extern uint mc3slots(void);
uint decode_address(unsigned long long, unsigned long long, int, int *);
