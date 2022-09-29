/***********************************************************************\
*	File:		prom_externs.h					*
*									*
*	Contains external declarations of various C routines used in	*
*	the PROM.							*
*									*
\***********************************************************************/

#ifndef _PROM_EXTERNS_H_
#define _PROM_EXTERNS_H_

#include <sys/types.h>
#include <setjmp.h>
#include <sys/EVEREST/evconfig.h>

void	init_cfginfo(evcfginfo_t*);
int	init_inventory(evcfginfo_t*, uint);
void	dump_evconfig_entry(int);
void	configure_cpus(evcfginfo_t*);
void	init_mpconf(void);

/*
 * Functions from epc_config.c
 */
int		io4_initmaster(evcfginfo_t*);
__scunsigned_t	master_epc_adap(void);

/* 
 * Functions from mc3_config.c
 */
uint	mc3_config(evcfginfo_t*);

/*
 * Functions from nvram.c
 */
uint	get_nvreg(uint);
uint 	set_nvreg(uint, uint);
uint 	nvchecksum(void);
int	set_nvram(uint, uint, char*);
void	get_nvram(uint, uint, char*);
void	init_nvram(void);
uint	nvram_okay(void);

/*
 * Functions from pod.c 
 */
void	pod_loop(int, int);
void	loprintf(char*, ...);
int*	logets(int*, int);
void 	send_int(int, int, int);

/* Version information */
char	*getversion(void); 

uint	load_double_hi(long long *);
int	load_double_lo(long long *);
uint	load_double_los(long long *);
uint	load_double_hi_nowar(long long *);
uint	load_double_lo_nowar(long long *);
uint	load_double_los_nowar(long long *);
uint	load_double_bloc(long long *);

void	store_double_hi(long long *, long long);
void	store_double_lo(long long *, long long);

ulong 	sysctlr_getdebug(void);
void	sysctlr_message(char *);
void	sysctlr_bootstat(char *, int);

int	setfault(jmp_buf, unsigned **);
void	restorefault(unsigned *);

uint	get_BSR(void);
void	set_BSR(int);
uint	get_prid(void);
uint	getendian(void);

void	delay(int);
void	prom_abdicate(int);

void	null(char *, ...);		/* for DPRINTF macro */
void	ccloprintf(char *, ...);

char	pod_getc(void);
void	pod_putc(char);
void	pod_flush(void);
void	pod_clear_ints(void);
void	pod_handler(char *, uint);
int	pod_check_mem(uint);
int	pod_check_rawmem(int, evbnkcfg_t *[]);
int	pod_check_bustags(void);
int	pod_check_epc(int, int, int);
int	pod_poll(void);

void	set_epcuart_base(__psunsigned_t);
void	epcuart_puts(char *, uint);
void	init_epc_uart(void);

uint	any_io_boards(void);
uint	any_mc_boards(void);

void		lo_strcpy(char *, char *);
int		lo_strcmp(int *, char *);
__scunsigned_t	lo_atoh(int *);
uint		lo_atoi(int *);
int		lo_ishex(int);
int		*logets(int *, int);

uint	load_uhalf(void *);
uint	load_ubyte(void *);
uint	load_word(void *);

void	set_timeouts(evcfginfo_t *);
int	timed_out(unsigned int, unsigned int);

void	memory(int, int, __scunsigned_t, __scunsigned_t, int);
void	info(void);
int	call_asm(uint (*function)(__scunsigned_t), __scunsigned_t);
int	sc_disp(unsigned char);
char	get_char(unsigned char *);
int	prom_slave(void);
int	set_unit_enable(uint, uint, uint, uint);
void	set_cc_leds(int);

int	cpu_slots(void);
int	memory_slots(void);
int	occupied_slots(void);

void	save_sregs(int *);
void	restore_sregs(int *);

uint	pon_invalidate_IDcaches(__scunsigned_t);
uint	pon_invalidate_dcache(__scunsigned_t);
void	flush_entire_tlb(void);

char	*get_diag_string(uint);
void	decode_diagval(char *, int);
uint	read_reg(uint, uint);
uint	read_reg_nowar(uint, uint);

__scunsigned_t	_sp(int, __scunsigned_t);
__scunsigned_t	_sr(int, __scunsigned_t);
__scunsigned_t	_epc(int, __scunsigned_t);
__scunsigned_t	_cause(int, __scunsigned_t);
__scunsigned_t	_config(int, __scunsigned_t);
__scunsigned_t	_badvaddr(int, __scunsigned_t);

#endif  /* _PROM_EXTERNS_H_ */
