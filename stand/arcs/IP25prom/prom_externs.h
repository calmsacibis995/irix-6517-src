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

struct flag_struct;

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
extern	void	pod_loop(int, int);
extern	void	loprintf(char*, ...);
extern	int	*logets(int*, int);
extern	void 	send_int(int, int, int);
extern	int	jump_addr(__psunsigned_t, uint, uint, struct flag_struct *);
extern	void	xlate_ertoip(evreg_t);

extern	int	dumpPrimaryInstructionLine(int, int, int);
extern	int	dumpPrimaryDataLine(int, int, int);
extern	void	dumpSecondaryLine(int, int, int);
extern	int	dumpPrimaryInstructionLineAddr(__uint64_t, int, int);
extern	int	dumpPrimaryDataLineAddr(__uint64_t, int, int);
extern	void	dumpSecondaryLineAddr(__uint64_t, int, int);
extern	void	dumpPrimaryCache(int);
extern	void	dumpSecondaryCache(int);
extern	void	compareSecondaryTags(int, int);
extern	void	dumpSlice(int, int);
extern	void	dumpSlot(int);
extern	void	dumpAll(void);
extern	void	setSecondaryECC(__uint64_t);

/*
 * Functions from pod_mem.c
 */

extern	int	disable_slot_bank(__uint64_t, __uint64_t, evbnkcfg_t **, int, 
				  uint);

/* Version information */
char	*getversion(void); 

ulong 	sysctlr_getdebug(void);
void	sysctlr_message(char *);
void	sysctlr_bootstat(char *, int);

int	setfault(jmp_buf, unsigned **);
void	restorefault(unsigned *);

uint	get_BSR(void);
void	set_BSR(int);
uint	getendian(void);

void	delay(int);
void	prom_abdicate(int);

void	ccloprintf(char *, ...);

char	pod_getc(void);
void	pod_putc(char);
void	pod_flush(void);
void	pod_clear_ints(void);
void	pod_handler(char *, uint);
int	pod_check_mem(uint);
int	pod_check_rawmem(int, evbnkcfg_t *[]);
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

/* libasm.s routines */

extern	int	initCPUSpeed(void);
extern	void	set_ERTOIP(__uint64_t);
extern	__uint64_t get_ERTOIP(void);

/* ccio.s routines */

extern	char	ccuart_getc(void);
extern	int	ccuart_putc(char);
extern	int	ccuart_puts(char *);

/* podasm.s routines */

extern	void	clearIP25State(void);
extern	void	podMode(unsigned char, const char *);
extern	void	flushTlb(void);
extern  __uint64_t readCP0(__uint64_t);
extern  void 	writeCP0(__uint64_t, __uint64_t);

/* pod_cache.s routines */

extern	int	sCacheSize(void);
extern	int	iCacheSize(void);
extern	int	dCacheSize(void);
extern	int	testIcache(void);
extern	int	testDcache(void);
extern	int	testScache(void);
extern	int	invalidateIcache(void);
extern	int	invalidateDcache(void);
extern	int	invalidateIDcache(void);
extern	int	invalidateScache(void);
extern	int	testCCtags(void);
extern	void	testCCtags_END(void);
extern	void	initDcacheStack(void);
extern	void	*copyToICache(void *, __uint64_t);
extern	void	cacheFlush(void);
extern	void	invalidateCCtags(void);

char	*get_diag_string(uint);
void	decode_diagval(char *, int);
uint	read_reg(uint, uint);

__scunsigned_t	_sp(int, __scunsigned_t);
__scunsigned_t	_sr(int, __scunsigned_t);
__scunsigned_t	_epc(int, __scunsigned_t);
__scunsigned_t	_cause(int, __scunsigned_t);
__scunsigned_t	_config(int, __scunsigned_t);
__scunsigned_t	_badvaddr(int, __scunsigned_t);

#endif  /* _PROM_EXTERNS_H_ */
