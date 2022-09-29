#ifndef _IP27PROM_POD_H_
#define _IP27PROM_POD_H_

#include <setjmp.h>

#if _LANGUAGE_C
#include "libc.h"
#endif

/* This is the status register value POD mode will load. */

#define POD_SR		((SR_IMASK & ~(SR_IBIT8|SR_IBIT1|SR_IBIT2)) | \
			 SR_FR | SR_CU1 | SR_BEV | SR_KX)

#define LINESIZE		192

#if _LANGUAGE_ASSEMBLY

/*
 * The following definitions control where we store GP registers in FP
 * registers.
 */

#define AT_FP			$f6
#define A0_FP			$f7
#define A1_FP			$f8
#define A2_FP			$f9
#define V0_FP			$f10
#define V1_FP			$f11
#define T0_FP			$f12
#define T1_FP			$f13
#define T2_FP			$f14
#define T3_FP			$f15
#define T4_FP			$f16
#define T5_FP			$f17
#define T6_FP			$f18
#define T7_FP			$f19
#define S0_FP			$f20
#define S1_FP			$f21
#define S2_FP			$f22
#define S3_FP			$f23
#define S4_FP			$f24
#define S5_FP			$f25
#define S6_FP			$f26
#define T8_FP			$f27
#define T9_FP			$f28
#define SP_FP			$f29
#define A3_FP			$f30
#define RA_FP			$f31

#endif	/* _LANGUAGE_ASSEMBLY */

/*
 * 23 GP registers are used by pod_mode before calling pod_loop.
 * This is the structure we use to access the stored GPRs after we
 * retrieve them from the FP registers.
 */

#define POD_MODE_DEX		0
#define POD_MODE_UNC		1
#define POD_MODE_CAC		2

/*  Get the reg_struct definition which identifies the layout
 *  of the nmi cpu register save area for sys/SN/nmi.h
 */

#include <sys/SN/nmi.h>

#if _LANGUAGE_C

struct flag_struct {
	int mode;		/* POD_MODE_xxx */
	int diagval;		/* Diagval that brought us to POD mode */
	int scroll_msg;		/* Scroll long diagnostic string? */
	char slot;		/* Where are we on the backplane? */
	char slice;		/* Where are we on the board? */
	char selected;		/* Who's selected now? */
	char silent;		/* Run in quiet mode. */
	int maxerr;		/* Max. errors to print in mem. tests */
	__uint64_t pod_ra;	/* Where pod_mode was called from */
	__uint64_t alt_regs;		/* show an alternate register set */
};

#endif

#if _LANGUAGE_C

int	hubprintf(const char *fmt, ...);

extern libc_device_t	dev_nulluart;
extern libc_device_t	dev_ioc3uart;
extern libc_device_t	dev_elscuart;
extern libc_device_t	dev_junkuart;
extern libc_device_t	dev_hubuart;

/*
 * Functions from pod.c
 */
extern	void	print_exception(struct reg_struct *gprs,
				struct flag_struct *flags);

extern	void	pod_console(void *console, char *dev_list, int cons_dipsw, int flag, uchar_t *);
extern	void	pod_mode(int, int, char *);
extern	void	pod_loop(int, int, char *, __uint64_t);
extern	void	pod_resume(void);

extern	int	dumpPrimaryInstructionLine(int, int, int);
extern	int	dumpPrimaryDataLine(int, int, int);
extern	void	dumpSecondaryLine(int, int, int);
extern	int	dumpPrimaryInstructionLineAddr(__uint64_t, int, int);
extern	int	dumpPrimaryDataLineAddr(__uint64_t, int, int);
extern	void	dumpSecondaryLineAddr(__uint64_t, int, int);
extern	void	dumpPrimaryCache(int);
extern	void	dumpSecondaryCache(int);

extern	void	dump_tlb(int);

extern	char   *cop0_names[];
extern	int	cop0_count;
extern	int	cop0_lookup(const char *name);

extern	char   *gpr_names[];

void	xlate_cause(__scunsigned_t value, char *buf);

/* Version information */
char	*getversion(void);

int	call_asm(uint (*function)(__scunsigned_t), __scunsigned_t);
int	sc_disp(unsigned char);
int	set_unit_enable(nasid_t nasid, int slice, int enable, int temp);
int	set_mem_enable(nasid_t nasid, char *mask, int enable);
void	reset_system(void);

/* pod_asm.s routines */

extern	void	pod_move_sp(__uint64_t);
char	*get_diag_string(uint);
void	decode_diagval(char *, int);

#define FLED_POD	(1 << 0)
#define FLED_CONT	(1 << 1)
#define FLED_NOTTY	(1 << 2)

void	fled_die(__uint64_t leds, int flags);
void	ip27_die(__uint64_t leds);

#define	IP27_INV_ABSENT		0
#define	IP27_INV_PRESENT	1
#define	IP27_INV_ENABLE		2

void	ip27_inventory(nasid_t nasid);

#endif /* _LANGUAGE_C */

#endif /* _IP27PROM_POD_H_ */
