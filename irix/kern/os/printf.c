
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1996, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 3.217 $"

#include "sys/param.h"
#include "sys/systm.h"
#include "sys/types.h"
#include "sys/sysmacros.h"
#include "sys/uadmin.h"
#include "sys/conf.h"
#include "ksys/vproc.h"
#include "sys/proc.h"
#include "sys/cmn_err.h"
#include "sys/reg.h"
#include "sys/pda.h"
#include "sys/pfdat.h"
#include "sys/page.h"
#include "sys/kabi.h"
#include "sys/klog.h"
#include "sys/debug.h"
#include "sys/splock.h"
#include "sys/stdnames.h"
#include "sys/timers.h"
#include "sys/dump.h"
#include "sys/kopt.h"
#ifdef CELL_IRIX
#include "ksys/cell/service.h"
#include "host/cell/dhost.h"
#include "invk_dshost_stubs.h"
#include "I_dshost_stubs.h"
#endif
#include "stdarg.h"
#include "string.h"
#ifdef SN0
#include "sys/SN/SN0/ip27log.h"
#endif

extern	char	*putbuf;	/* buffer where cmn_err output can be saved */
extern	int	putbufsz;	/* size of putbuf buffer */
unsigned int	putbufndx = 0;	/* index to next character in putbuf */
#ifdef EVEREST
/* For NVRAM error logging on Challenge boxes */
extern  char    fru_nvbuff[];
extern  char    nvtmpbuf[];
extern void set_fru_nvram(char* error_state, int len);
#endif
extern	char	*conbuf;	/* buffer where console output is accumulated */
extern	int	conbufsz;	/* size of console buffer */
int		constrlen = 0;	/* index to next character in putbuf */
#if SN0
static int	conbuf_tout;	/* conbuf_flush timeout pending */
#endif

extern	char	*errbuf;	/* buffer where error dump output is saved */
extern	int	errbufsz;	/* size of errbuf buffer */
unsigned int	errbufndx = 0;	/* index to next character in errbuf */



lock_t		putbuflck;	/* lock to single thread output */
volatile uint 	putbuflck_gen = 0; /* putbuf lock generation number. */
int		haspblock = -1;	/* cpuid of CPU that has lock */
int		pblock_nest = 0; /* nesting level of putbuflck */

int		ecc_panic_cpu = -1;	/* set to cpuid of cpu in ecc_panic */
lock_t		panic_putbuflck;	/* lock to single thread output */
int		panic_haspblock = -1;	/* cpuid of CPU that has lock */
volatile int	in_con_message = 0;

extern int	power_off_flag;

void panicspin(void);
static void errprintf(int, char **, register char *, va_list);
static void errvprintf(int, char **, register char *, ...);
static void printL(int, char **, __uint64_t, int);
static void printn(int, char **, __scunsigned_t, int);
static void dopromreset(int);
static void syncreboot(void);
static void mprboot(void (*)(), char *);
static void do_mprboot(char *);
static void _putstr(int, char **, char *);
void icmn_err(int, char *, va_list);
void icmn_err_tag(int, int, char *, va_list);

void	silence_all_audio(void);
long	arcs_write(long, void *, long, long *);
void	cn_write(char *, int);
void	cn_flush(void);
void	prom_reinit(void);
void	prom_autoboot(void);
void	dumptlb(int);
void	checkAllTlbsDumped(void);

#ifdef IP30
void	clear_cpu_intr(cpuid_t);
#endif	/* IP30 */

#define	isdigit(c)	(((c) >= '0') && ((c) <= '9'))

/* Codes for where output should go. (internal to cmn_err code) */
#define	PRW_BUF		0x01	/* Output to putbuf.		*/
#define	PRW_CONS	0x02	/* Output to console via sprom.	*/
#define PRW_ABUF	0x04	/* Output to arbitrary buffer.	*/
#define PRW_ARCS	0x08	/* Output via ARCS prom. */
#define PRW_KLOG	0x10	/* Output to /dev/klog. */
#define PRW_RPC		0x20	/* Identifies messages from rpc path */
#define PRW_FLAG	0x80	/* Identifies character as PRW flag */
#if defined(SN0) || defined(EVEREST)
#define	PRW_LOGNVRAM	0x100	/* Output to ip27log too.	*/
#endif
#define PRW_ERR		0x200	/* Output to error dump buffer */
#define PRW_NEWLINE	0x400	/* Add newline if missing from message */

int	panic_level = 0;
char	*panicstr;
char	panicbuf[DUMP_PANIC_LEN * 3];

klogmsgs_t	klogmsgs;	/* holds associated logging buffer info  */

/*
 * cmn_err - the common error handling routine for the kernel
 *
 * Used to print diagnostic information directly on console tty.
 * Since it is not interrupt driven, all system activities are pretty
 * much suspended.
 * Should not be used for chit-chat.
 * Kernel printfs are single threaded here to prevent garbled output.
 */
void
cmn_err(register int level, char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	icmn_err_tag(0, level, fmt, ap);
	va_end(ap);
}

void
cmn_err_tag(int seqnumber, register int level, char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	icmn_err_tag(seqnumber, level, fmt, ap);
	va_end(ap);
}

extern void dprintf(char *, ...);

#define D1ARG va_arg(ap,__psint_t)
#define D2ARG D1ARG,D1ARG
#define D4ARG D2ARG,D2ARG
#define D8ARG D4ARG,D4ARG
#define D16ARG D8ARG,D8ARG

#define ADD_CPU_ID(_buf)						\
	if (dophysid) {							\
		if (_buf) 		 				\
			errvprintf(PRW_ABUF, _buf, "%s: ",		\
				   private.p_cpunamestr);		\
		else 							\
			errvprintf(prt_where, NULL, "%s: ",		\
				   private.p_cpunamestr);		\
	}								\
	else if (docpuid) {						\
		if (_buf)						\
			errvprintf(PRW_ABUF, _buf, "CPU %d: ", cpuid()); \
		else							\
			errvprintf(prt_where, NULL, "CPU %d: ", cpuid()); \
	}


#ifdef CELL_IRIX
#define CELL_CONBUFSZ	16384
char 	cell_console_buf[CELL_CONBUFSZ];
int	cell_conbuf_start;
int	cell_conbuf_flushed;
int	cell_console_overflow;
volatile cell_t	last_con_cellid;
sema_t	cellconssema;		/* cell_console_thread() synchronization */

#define SECS_PER_DAY    86400           /* # secs per day       */
#define SECS_PER_HOUR   3600            /* # secs per hour      */
#define SECS_PER_MIN    60              /* # secs per minute    */

#define IS_GOLDEN_CELL	(cellid() == CELL_GOLDEN)

/* Define functions placed around console rpc to detect recursive calls */
int 	in_con_rpc = -1;
#define START_CON_RPC()	in_con_rpc = cpuid()
#define END_CON_RPC()	in_con_rpc = -1;

/* Define routine to identify and timestamp console messages from cells that
 * are in the console buffer.
 */

static void
start_con_msg(void)
{
	register int secs, hrs, min;

        secs = time % SECS_PER_DAY;
        hrs = secs / SECS_PER_HOUR;
        secs %= SECS_PER_HOUR;
        min = secs / SECS_PER_MIN;
        secs %= SECS_PER_MIN;
        errvprintf(PRW_BUF | PRW_KLOG, NULL, "(%d:%d:%d) ", hrs, min, secs);
}
		
#define ADD_CELL_ID     						\
	if (!in_con_message || last_con_cellid != cellid()) {		\
		start_con_msg();					\
		if (IS_GOLDEN_CELL) {					\
			errvprintf(prt_where & ~PRW_BUF, NULL, "(CELL:%d) ", \
				CELL_GOLDEN); \
		}							\
	}								\
	last_con_cellid = cellid();					
#else	/* !CELL_IRIX */
#define ADD_CELL_ID     
#endif

/*
 * Add an action to the SYSLOG for Availmon to pickup and act upon.
 * This routine is called by icmn_err to add some extra input to the
 * syslog which will later be pulled out and acted upon by availmon.
 */
void availmon_action(int newlevel, int prt_where)
{
  newlevel &= CE_AVAILMONALL;

  if(newlevel){
    errvprintf(prt_where, NULL, "(");

    if(newlevel & CE_TOOKACTIONS){
      errvprintf(prt_where, NULL, "TOOK-ACTION");
      newlevel &= ~CE_TOOKACTIONS; /* clr*/
      if(newlevel)
	errvprintf(prt_where, NULL, ":"); /* still more bits */
    }

    if(newlevel & CE_RUNNINGPOOR){
      errvprintf(prt_where, NULL, "SYS-DEGRADED");
      newlevel &= ~CE_RUNNINGPOOR; /* clr*/
      if(newlevel)
	errvprintf(prt_where, NULL, ":");
    }
    
    if(newlevel & CE_MAINTENANCE){
      errvprintf(prt_where, NULL, "MAINT-NEEDED");
      newlevel &= ~CE_MAINTENANCE; /* clr*/
      if(newlevel)
	errvprintf(prt_where, NULL, ":");     
    }

    if(newlevel & CE_CONFIGERROR){
      errvprintf(prt_where, NULL, "CONFIG-ISSUE");
      newlevel &= ~CE_CONFIGERROR;  /* clr*/
      if(newlevel)
	errvprintf(prt_where, NULL, ":");
    }

    errvprintf(prt_where,NULL, "):");
  }
}

void
icmn_err_tag(int seqnum, register int level, char *fmt, va_list ap)
{
	int docpuid, dophysid, i, s, alevel;
	cpuid_t CpuId = getcpuid();
	int prt_where;
#if !IP30
	int sync;
#endif
	/* Extract and mask out the bits for availmon levels */
	alevel = (level & CE_SUBTASKMASK);

	/* Extract standard cmn_err bits */
	level &= CE_PRIOLEVELMASK; 

	/*
	 * Extract and mask off the bit indicating whether
	 * to prepend the CPU id to the output string
	 */
	docpuid = (level & CE_CPUID) && (maxcpus > 1);
	dophysid = (level & CE_PHYSID) && (maxcpus > 1);
#if !IP30
	sync = (level & CE_SYNC);
#endif
	/* Now, we can extract priority-level bits .... */
	level &= CE_LEVELMASK;


	/*
	 * Catch early system panics. Avoid cpuid() reference
	 * to PDA below.
	 */
	if (pdaindr[CpuId].CpuId != CpuId
#if MP && !SABLE
	    || !(spinlock_initialized(&putbuflck))
#endif
	) {
		if (level == CE_PANIC)
			dprintf("PANIC: ");
		dprintf(fmt, D16ARG);
		if (level == CE_PANIC)
			prom_reboot();
		return;
	}



	if (dophysid && !private.p_cpunamestr) {
		docpuid = 1;
		dophysid = 0;
	}

	/*
	 * Make sure intrs are disabled, while handling a panic
	 * (locking/unlocking putbuflck below still keeps us at
	 * the desired spl).
	 */
	if (level == CE_PANIC || level == CE_PBPANIC) {
		if (!dophysid && (maxcpus > 1))
			docpuid = 1;
		(void) spl7();
		/*
		 * enter_panic_mode() will allow us to break the putbuf lock 
		 * if necessary and will only allow 1 cpu to proceed in the 
		 * case where multiple cpus panic at roughly the same time. 
		 */
		enter_panic_mode();
	}

	/* if we already have the lock, then this is probably a
	 * BAD situation, we certainly don't want to try again!
	 * So just ignore it.
	 */
	PUTBUF_LOCK(s);

	/*
	 * Set up to print to putbuf or both console and putbuf,
	 * as indicated by the first character of the
	 * format.
	 */
	if (*fmt == '!') {
		prt_where = PRW_BUF | PRW_KLOG;
		fmt++;
	} else if (*fmt == '^') {
		prt_where = PRW_CONS | PRW_KLOG;
		fmt++;
	} else if (*fmt == '\\') {
		/* currently used only for the boot time banner in main.c,
		 * where we don't want the escape sequence in syslog */
		prt_where = PRW_CONS;
		fmt++;
	} else if (*fmt == '#') {
		prt_where = PRW_ERR;
		fmt++;
	} else
		prt_where = PRW_BUF | PRW_CONS | PRW_KLOG;

	ADD_CELL_ID;	/* This has to be first */
	
	/* In order to handle availmon actions, we clear the bits for these
	 * then we will pass level to availmon_action() for processing
	 * at which point the availmon actions will be added to the output stream.
	 */
	switch (level) { 

		case CE_DEBUG:
			errvprintf(prt_where & ~PRW_CONS, NULL, "<%d>", CE_DEBUG);
                        if(seqnum && (prt_where & PRW_KLOG) != 0)
				errvprintf(PRW_KLOG, NULL, "|$(0x%X)", seqnum);

			availmon_action(alevel, prt_where);

#if defined(SN0) || defined(EVEREST)
			if (in_panic_mode())
				prt_where |= PRW_LOGNVRAM;
#endif
			ADD_CPU_ID(NULL);
			errprintf(prt_where, NULL, fmt, ap);
			break;
		case CE_CONT:
			/* PV #168252, send in pri string before actual msg */
			errvprintf(prt_where & ~PRW_CONS, NULL, "<%d>", CE_CONT);

			availmon_action(alevel, prt_where);

#if defined(SN0) || defined(EVEREST)
			if (in_panic_mode())
				prt_where |= PRW_LOGNVRAM;
#endif
			ADD_CPU_ID(NULL);
			errprintf(prt_where, NULL, fmt, ap);
			break;

		case CE_NOTE:
			errvprintf(prt_where & ~PRW_CONS, NULL, "<%d>", CE_NOTE);
                        if(seqnum && (prt_where & PRW_KLOG) != 0)
				errvprintf(PRW_KLOG, NULL, "|$(0x%X)",seqnum);

#if defined(SN0) || defined(EVEREST)
			if (in_panic_mode())
				prt_where |= PRW_LOGNVRAM;
#endif
			errvprintf(prt_where, NULL, "NOTICE: ") ;

			availmon_action(alevel, prt_where);

			ADD_CPU_ID(NULL);
			errprintf(prt_where | PRW_NEWLINE, NULL, fmt, ap);
			break;

		case CE_WARN:
			errvprintf(prt_where & ~PRW_CONS, NULL, "<%d>", CE_WARN);
                        if(seqnum && (prt_where & PRW_KLOG) != 0)
				errvprintf(PRW_KLOG, NULL, "|$(0x%X)", seqnum);

#if defined(SN0) || defined (EVEREST)
			if (in_panic_mode())
				prt_where |= PRW_LOGNVRAM;
#endif
			errvprintf(prt_where, NULL, "WARNING: ");

			availmon_action(alevel, prt_where);

			ADD_CPU_ID(NULL);
			errprintf(prt_where | PRW_NEWLINE, NULL, fmt, ap);
			break;

		case CE_ALERT:
			errvprintf(prt_where & ~PRW_CONS, NULL, "<%d>", CE_ALERT);

                        if(seqnum && (prt_where & PRW_KLOG) != 0)
				errvprintf(PRW_KLOG, NULL, "|$(0x%X)", seqnum);

#if defined(SN0) || defined(EVEREST)
			if (in_panic_mode())
				prt_where |= PRW_LOGNVRAM;
#endif
			errvprintf(prt_where, NULL, "ALERT: ");
			availmon_action(alevel, prt_where);

			ADD_CPU_ID(NULL);
			errprintf(prt_where | PRW_NEWLINE, NULL, fmt, ap);
			break;

		case CE_PANIC:
			/* Fall through to common code */

		case CE_PBPANIC:

			switch (panic_level++) {
			case 0:
			{
				int 		total_number_of_cpus;
				cpuid_t		id;

				clkreld();

				/*
				 * Tell all of the other processors we're
				 * panic'ing.  They'll just sit and spin,
				 * only performing cpuaction() requests.
				 */

				total_number_of_cpus = MAX_NUMBER_OF_CPUS();

				for (i = 0; i < total_number_of_cpus; i++) {
					if (((id = pdaindr[i].CpuId) != -1) &&
		  			     (id != cpuid())) {
						SEND_VA_PANICSPIN(id);
#ifdef	IP30
						/*
						 * need to clear the ipi
						 * here, otherwise, slave
						 * processor will see it
						 * as soon as it gets to the
						 * PROM and may not respond
						 * to the master processor
						 * later, this causes a reset
						 * during reboot-on-panic
						 */
						clear_cpu_intr(id);
#endif	/* IP30 */

					}
				}

				/*
				 * Reset the prom port to talk through the
				 * proms and also reset the prom text window.
				 * Passing in a 1 means this is a panic.
				 */
				dopromreset(1);

				prt_where = PRW_CONS | PRW_BUF;

				/* make sure anything sent to cmn_err
				 * before the panic, appears before
				 * the panic on the console output.
				 */
				conbuf_flush(CONBUF_LOCKED|CONBUF_DRAIN);

                                /*
                                 * Panicstr is used by the debugger to find
                                 * the beginning of the panic output string.
				 * We save the message there first before
				 * putting it in the console buffer.
                                 */
				
				errvprintf(prt_where & ~PRW_CONS, NULL,
					 "\n<%d>", CE_PANIC);


#if defined(SN0) || defined(EVEREST)
				if (in_panic_mode())
					prt_where |= PRW_LOGNVRAM;
#endif
				panicstr = panicbuf;
				errvprintf(PRW_ABUF, &panicstr, "PANIC: ");

				availmon_action(alevel, prt_where);

				panicstr = panicbuf + strlen(panicbuf);
				if (maxcpus > 1) {
					ADD_CPU_ID(&panicstr);
					panicstr = panicbuf + strlen(panicbuf);
				}

				errprintf(PRW_ABUF | PRW_NEWLINE,
					  &panicstr, fmt, ap);
				panicstr = panicbuf;

				/* Put the panicstr in the console buffer 
				 * and also in the error buffer
				 */

				errvprintf(prt_where | PRW_ERR, NULL, panicstr);

				/* make sure the panic message actually
				 * gets sent all the way out.
				 */
				conbuf_flush(CONBUF_LOCKED|CONBUF_DRAIN);
				PUTBUF_UNLOCK(s);

				dumptlb(cpuid());

				/* Never let the cpu with the ECC error try to
				 * produce a dump.
				 */
				if ((maxcpus>1) && (ecc_panic_cpu == cpuid()))
					panicspin();

				silence_all_audio();
				DELAY(500000);	/* delay .5 second */
				checkAllTlbsDumped();
				syncreboot();
				/* NOTREACHED */
			}

			case 1 :
				clkreld();
				prt_where = PRW_CONS | PRW_BUF;
				if (maxcpus > 1)
					errvprintf(PRW_CONS | PRW_BUF,
					  NULL,
					  "\nDOUBLE PANIC: CPU %d: ", cpuid());
				else
					errvprintf(PRW_CONS | PRW_BUF,
					  NULL,
				 	  "\nDOUBLE PANIC: ");

				availmon_action(alevel, prt_where);

				errprintf(PRW_CONS | PRW_BUF | PRW_NEWLINE,
					  NULL, fmt, ap);

				conbuf_flush(CONBUF_LOCKED|CONBUF_DRAIN);

				PUTBUF_UNLOCK(s);

				if (kdebug) debug("ring");
				mdboot(AD_HALT, NULL);
				/*NOTREACHED*/
				for (;;)
					;

			case 2:
				if (kdebug) debug("ring");
				mdboot(AD_HALT, NULL);
				/* fall through */
			default:
				/* if more than 3, something is wrong with
				   symmon or the halt code, so just spin! */
				for (;;)
					;
			}
			/* NOTREACHED */
		default:
			cmn_err(CE_PANIC,
			  "unknown level in cmn_err (level=%d, msg=\"%s\")",
			  level, fmt, ap);
	}

#if !IP30
	/* The master processor must perform the actual dumping to the console.
	 * Therefore, send a message to the master processor if necessary;
	 * otherwise, do it now.
	 */

	if (prt_where & PRW_CONS)
		conbuf_flush(CONBUF_LOCKED |
			     ((sync || in_panic_mode()) ? CONBUF_DRAIN : 0));
#endif	/* !IP30 */

	/*
	 * Dont call klogwakeup() unless we were at spl0. We could be called 
	 * from error interrupts and already holding locks at >spl0. Doing
	 * klogwakeup (except at spl0) could lead to deadlocks.
	 */
	if (isspl0(s))
		klogwakeup();
	else
		klog_need_action++;

	PUTBUF_UNLOCK(s);
}

void
icmn_err(register int level, char *fmt, va_list ap)
{ icmn_err_tag(0, level, fmt, ap);
}


/* The following does a printf to an arbitrary string. */
void
vsprintf(char *buf, char *fmt, va_list ap)
{
	errprintf(PRW_ABUF, &buf, fmt, ap);
}

void
sprintf(char *buf, char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);
	va_end(ap);
}

/*
 * Versions of sprintf and vsprintf that don't try to lock anything.  
 * These can be used on MP platforms in early startup code before 
 * locks have been initialized.  (MP ARCS machines need this in 
 * order to set up startpc in the environment.  See idbg.c)
 */
void
lo_vsprintf(char *buf, char *fmt, va_list ap)
{
	errprintf(PRW_ABUF, &buf, fmt, ap);
}

void
lo_sprintf(char *buf, char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	lo_vsprintf(buf, fmt, ap);
	va_end(ap);
}

void
printf(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	icmn_err_tag(0, CE_CONT, fmt, ap);
	va_end(ap);
}

void
sync_printf(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	icmn_err_tag(0, CE_SYNC | CE_CONT, fmt, ap);
	va_end(ap);
}

/* The following is an interface routine for the drivers. */
void
dri_printf(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	icmn_err_tag(0, CE_CONT, fmt, ap);
	va_end(ap);
}

/*
 * prdev prints a warning message.
 * dev is a block special device argument.
 * This routine now accepts a variable number of arguments and
 * coverts it into a string for use by the device specific print routine.
 * NOTE: no error checking that the generated string fits in the buffer is
 *	 done.
 */
void
prdev(char *fmt, int dev, ...)	/* a dev_t dev makes stdarg unhappy */
{
	va_list ap;
	char	prdev_buf[PRDEV_MAXBUFSZ];
	struct bdevsw *my_bdevsw;

	va_start(ap, dev);
	vsprintf(prdev_buf, fmt, ap);
	va_end(ap);

	my_bdevsw = get_bdevsw(dev);
	if (bdhold(my_bdevsw)) {
		cmn_err_tag(291,CE_WARN,"%s on bad dev 0x%x\n", prdev_buf, dev);
		return;
	}
	bdprint(my_bdevsw, dev, prdev_buf);
	bdrele(my_bdevsw);
}

/*
 * stdname
 *   This routine takes the string and dev arguments and combines
 *   them together to produce a standard nameformat for reporting device errors.
 */
char *
stdname(
	char	*str,
	char	*buf,
	int	ctlr,
	int	unit,
	int	fs)
{
	char formedstr[STDN_MAXNAMESZ];

	strncpy(formedstr, str, 3);	/* insure that name is only 3 chars */
	formedstr[ 3 ] = '\0';
	strcat(formedstr, STDN_CTLR_FMT);
	if (unit != STDN_NULL_UNIT) {
		strcat(formedstr, STDN_UNIT_FMT);
		if (fs != STDN_NULL_FS) {
			if (fs == STDN_VH_FS)
				strcat(formedstr, STDN_VH_FMT);
			else
				strcat(formedstr, STDN_FS_FMT);
		}
	}
	strcat(formedstr, STDN_END_FMT);

	sprintf(buf, formedstr, ctlr, unit, fs);
	return(buf);
}

/* Berkeley style panic() routine for compatibility */
void
panic(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	icmn_err_tag(0, CE_PANIC, fmt, ap);
	va_end(ap);
}

/* was a macro; as a function we use about 5K less memory... */
static void
output(int where, char **buf, uchar_t c)
{
#ifdef CELL_IRIX
	static void	cell_output(int, uchar_t);
#endif

	/* Keep track of current message state */

	if (c == '\n' || c == '\0') {
		in_con_message = 0;
	}
	else if (!in_con_message) {
		in_con_message = 1;
	}

	if (where & PRW_ABUF)
		*(*buf)++ = c;
#ifdef CELL_IRIX
	else if (!IS_GOLDEN_CELL) {
		/* Save in buffer to be flushed later to golden cell */

		cell_output(where, c);
	}
#endif
	else {
		if ((where & PRW_KLOG) && c != '\0')
			klogmsgs.klm_buffer[ klogmsgs.klm_writeloc++ & KLM_BUFMASK] = c;
		if (where & PRW_BUF) {
			*(putbuf+(putbufndx & (putbufsz-1))) = c;
			putbufndx++;
		}
		if (where & PRW_ARCS) {
			long a;
			char oc = c;
			if (c == '\n') arcs_write(1,"\r\n",2,&a);
			else arcs_write (1, &oc, 1, &a);
		}
		if (where & PRW_CONS) {
			if (constrlen < conbufsz) {
				*(conbuf+constrlen) = c;
				constrlen ++;
			}
		}
		if (where & PRW_ERR) {
			if (errbufndx < errbufsz) {
				*(errbuf+errbufndx) = c;
				errbufndx++;
			}
		}
	}
}


/*
 * Scaled down version of C Library printf, called by cmn_err() and sprintf().
 * NOTE: putbuflck should be locked when called and will remain locked.
 *
 * One additional format: %b is supported to decode error registers.
 * Usage is:
 *	printf("reg=%b\n", regval, "<base><arg>*");
 * Where <base> is the output base expressed as a control character,
 * e.g. \10 gives octal; \20 gives hex.  Each arg is a sequence of
 * characters, the first of which gives the bit number to be inspected
 * (origin 1), and the next characters (up to a control character, i.e.
 * a character <= 32), give the name of the register.  Thus
 *	printf("reg=%b\n", 3, "\10\2BITTWO\1BITONE\n");
 * would produce output:
 *	reg=3<BITTWO,BITONE>
 *
 * For convenience, in 64 bit mode, all the standard %{x,d,u,o} all print
 *	64 bit quantities and thus one must specify something special
 *	to print a 32 bit quantity (note that this really only matters
 *	when one has more args than arg registers , and one tries to
 *	print a 32 bit quantity - one gets garbage in the high bits ...)
 * We should (1/10/95) add %p and %w32 formats to symmon to
 *	provide a complete set.
 */
static void
errvprintf(int where, char **buf, char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	errprintf(where, buf, fmt, ap);
	va_end(ap);
}

/* Return true if it is ok to print to the system console through
 * the console driver (i.e. the driver init routine has been called
 * and normal console output is now possible)
 */
static int
console_ready(void)
{
    int ready;

    ready = cn_is_inited();

#if (IP27 || IP30 || IP32) && !defined(SABLE)
    {
	extern int console_device_attached;
	extern int console_is_tport;

	/* XXXmdf If we're running textport, we want to get panic
	 * messages on the graphical console.  Also, we don't care if
	 * the serial console has been discovered yet.
	 */
	if (ready && !console_is_tport &&
#ifdef IP30
	    /*
	     * want non-panic processor's output to go through poll mode
	     * instead of DMA mode since the ioc3 DMA engine was shut down
	     * by the panic processor the first time it did a printf
	     */
	    (panicstr || !console_device_attached)
#else
	    (in_panic_mode() || !console_device_attached)
#endif	/* IP30 */
	    )
	    ready = 0;
    }
#endif
#if EVEREST
    {
	extern int console_device_attached;
	extern int console_is_tport;

	if (!console_is_tport &&
	    (in_panic_mode() || !console_device_attached))
	    ready = 0;
    }
#endif
    return(ready);
}

static void errprintf_V(int where, char **buf, dev_t arg);

static void
errprintf(int where, char **buf, register char *fmt, va_list ap)
{
	int i, any;
	__scunsigned_t b;
	register int c;
	register char *s;
	int Lflag, Iflag;
	register int base;
	int skipping_fill;
	char *fillchar;		/* pointer to supposed fill character */

	if ((where & PRW_CONS) && !console_ready())
		where = (where & ~PRW_CONS) | PRW_ARCS;

#ifdef SN0
	if (where & PRW_LOGNVRAM)
		ip27log_panic_printf(IP27LOG_FATAL, fmt, ap);
#endif
#ifdef EVEREST
        if (where & PRW_LOGNVRAM){
	        vsprintf(nvtmpbuf, fmt, ap);
		strcat(fru_nvbuff, nvtmpbuf);
                /* We have everything now, so store the buffer in NVRAM */
            	/* Log the FRU information into the NVRAM */
	       set_fru_nvram(fru_nvbuff, strlen(fru_nvbuff)+1);
         }
#endif
                
loop:
	skipping_fill = 0;
	Lflag = Iflag = 0;

	while ((c = *fmt++) != '%')
		if (c)
			output(where, buf, c);
		else
			goto done;

inner_loop:
	if ((c = *fmt++) == 'l' ) {
		if ((c = *fmt++) == 'l') {
			c = *fmt++;
			Lflag++;
		}
		/* silently support %lx */
	} else if (c == 'w') {
		if (*fmt == '3' && *(fmt+1) == '2') {
			Iflag = 1;
			fmt += 2;
			c = *fmt++;
		}
	}

	switch (c) {
	case 'b':
		b = va_arg(ap, __scunsigned_t); /* register value */
		s = va_arg(ap, char *);
		printn(where, buf, b, *s++);
		any = 0;
		if (b) {
			output(where, buf, '<');
			while (i = *s++) {
				if (b & (1 << (i-1))) {
					if (any)
						output(where, buf, ',');
					any = 1;
					for (; (c = *s) > 32; s++)
						output(where, buf, c);
				} else
					for (; *s > 32; s++)
						;
			}
			output(where, buf, '>');
		}
		break;
	case 'D':
	case 'd':
		base = -10;
		goto number;
	case 'u':
		base = 10;
		goto number;
	case 'p':
		/* generic pointer */
		base = 16;
		goto number;
	case 'X':
	case 'x':
		base = 16;
		goto number;
	case 'o':
		base = 8;
number:
		if (Lflag)
			printL(where, buf, va_arg(ap, __uint64_t), base);
		else if (Iflag)
			printn(where, buf, va_arg(ap, uint_t), base);
		else
			printn(where, buf, va_arg(ap, __scunsigned_t), base);
		break;
	case 's':
		s = va_arg(ap, char *);
		while (c = *s++) {
			output(where, buf, c);
		}
		break;
	case 'c':
		c = va_arg(ap, int);
		output(where, buf, c);
		break;

	case 'r':
	case 'R':
		b = va_arg(ap, __scunsigned_t);	/* register value */
		s = va_arg(ap, char *);	/* addr of descriptor struct */
		if (c == 'R') {
			_putstr(where, buf, "0x");
			printn(where, buf, b, 16);
		}
		if (c == 'r' || b) {
			register struct reg_desc *rd;
			register struct reg_values *rv;
			__psunsigned_t field;

			output(where, buf, '<');
			any = 0;
			for (rd = (struct reg_desc *)s; rd->rd_mask; rd++) {
				field = b & rd->rd_mask;
				field = (rd->rd_shift > 0)
				    ? field << rd->rd_shift
				    : field >> -rd->rd_shift;
				if (any &&
				      (rd->rd_format || rd->rd_values
				         || (rd->rd_name && field)
				      )
				)
					output(where, buf, ',');
				if (rd->rd_name) {
					if (rd->rd_format || rd->rd_values
					    || field) {
						_putstr(where, buf, rd->rd_name);
						any = 1;
					}
					if (rd->rd_format || rd->rd_values) {
						output(where, buf, '=');
						any = 1;
					}
				}
				if (rd->rd_format) {
					errprintf(where, buf, rd->rd_format,
					    (va_list)&field);
					any = 1;
					if (rd->rd_values)
						output(where, buf, ':');
				}
				if (rd->rd_values) {
					any = 1;
					for (rv = rd->rd_values;
					    rv->rv_name;
					    rv++) {
						if (field == rv->rv_value) {
							_putstr(where, buf, rv->rv_name);
							break;
						}
					}
					if (rv->rv_name == NULL)
						_putstr(where, buf, "???");
				}
			}
			output(where, buf, '>');
		}
		break;

		/* Printing of HWG dev_t directly.  Done in a routine to
		 * save huge local stack space of MAXDEVNAME bytes needed
		 * for conversion.
		 */
	case 'v':		/* printing HWG vertex_hdl_t */
		errprintf_V(where,buf,vhdl_to_dev(va_arg(ap, vertex_hdl_t)));
		break;
	case 'V':		/* printing HWG dev_t */
		errprintf_V(where,buf,va_arg(ap, dev_t));
		break;

	case '\0':
		goto done;

	default:
		/*
		 *  A fancy C printf supports "fill" characters, e.g. %08x
		 *  which says to display at least 8 digits in hex, using "0"
		 *  as a leading fill character if the value is otherwise less
		 *  than 8 nonzero digits.  We don't go to the trouble of
		 *  supporting "fill" formats in the kernel, but we will at
		 *  least recognize the "fill" digits and ignore them, so %08x
		 *  gets treated like %x.
		 */
		if (skipping_fill) {
			if (isdigit(c)) {
				/* still skipping "fill" */
				goto inner_loop;
			} else {
				/*
				 * Not a digit (and hence not fill), and
				 * not a known format type -- that wasn't
				 * fill!  Backtrack and try again.
				 */
				fmt = fillchar-1;
				goto loop;
			}
		} else {
			if (isdigit(c)) {
				/* may be "fill" formatting */
				fillchar = fmt;	 /* in case have to backtrack */
				skipping_fill = 1;
				goto inner_loop;
			}
		}
		output(where, buf, c);
		break;
	}
	goto loop;

done:
	if ((where & PRW_NEWLINE) && in_con_message)
		output(where, buf, '\n');
	if (where & PRW_ABUF)
		output(where, buf, '\0');
}

static void
errprintf_V(int where, char **buf, dev_t arg)
{
	char devnm[MAXDEVNAME];
	char *s;
	int c;

	/* dev_to_name() never returns NULL.  Use dev_t version so old
	 * names continue to work.
	 */
	s = dev_to_name(arg,devnm,MAXDEVNAME);
	while (c = *s++)
		output(where,buf,c);
}

/*
 * convert a va_list to a set of args - mostly for passing to prom printf
 * routines.
 */
#define MAXVARGS 16
void
aptoargs(char *fmt, va_list ap, __psint_t args[])
{
	int argnum = 0;
	register int c;
	int Iflag, Lflag;
	int skipping_fill;
	char *fillchar;		/* pointer to supposed fill character */

loop:
	skipping_fill = 0;
	Iflag = Lflag = 0;
	while ((c = *fmt++) != '%') {
		if (c == '\0')
			return;
	}

inner_loop:
	if ((c = *fmt++) == 'l' ) {
		if ((c = *fmt++) == 'l') {
			c = *fmt++;
			Lflag++;
		}
		/* silently support %lx */
	} else if (c == 'w') {
		if (*fmt == '3' && *(fmt+1) == '2') {
			Iflag = 1;
			fmt += 2;
			c = *fmt++;
		}
	}

	switch (c) {
	case 'b':
		args[argnum++] = (__psint_t)va_arg(ap, __scunsigned_t); /* register value */
		args[argnum++] = (__psint_t)va_arg(ap, char *);
		break;
	case 'D':
	case 'd':
	case 'u':
	case 'X':
	case 'x':
	case 'o':
	case 'p':
		if (Lflag) {
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
			/* keep long long pair in calling sequence */
			if ((argnum & 1) == 0) {
				args[argnum++] = (__psint_t)va_arg(ap, uint_t);
			}
                        args[argnum++] = (__psint_t)va_arg(ap, uint_t);
                        args[argnum++] = (__psint_t)va_arg(ap, uint_t);
#else
			args[argnum++] = (__psint_t)va_arg(ap, __uint64_t);
#endif
                }
		else if (Iflag)
			args[argnum++] = (__psint_t)va_arg(ap, uint_t);
		else
			args[argnum++] = (__psint_t)va_arg(ap, __scunsigned_t);
		break;
	case 's':
		args[argnum++] = (__psint_t)va_arg(ap, char *);
		break;
	case 'c':
		args[argnum++] = (__psint_t)va_arg(ap, int);
		break;

	case 'r':
	case 'R':
		args[argnum++] = (__psint_t)va_arg(ap, __scunsigned_t);	/* register value */
		args[argnum++] = (__psint_t)va_arg(ap, char *);	/* addr of descriptor struct */
		break;

	case '\0':
		return;

	default:
		/*
		 *  A fancy C printf supports "fill" characters, e.g. %08x
		 *  which says to display at least 8 digits in hex, using "0"
		 *  as a leading fill character if the value is otherwise less
		 *  than 8 nonzero digits.  We don't go to the trouble of
		 *  supporting "fill" formats in the kernel, but we will at
		 *  least recognize the "fill" digits and ignore them, so %08x
		 *  gets treated like %x.
		 */
		if (skipping_fill) {
			if (isdigit(c)) {
				/* still skipping "fill" */
				goto inner_loop;
			} else {
				/*
				 * Not a digit (and hence not fill), and
				 * not a known format type -- that wasn't
				 * fill!  Backtrack and try again.
				 */
				fmt = fillchar-1;
				goto loop;
			}
		} else {
			if (isdigit(c)) {
				/* may be "fill" formatting */
				fillchar = fmt;	 /* in case have to backtrack */
				skipping_fill = 1;
				goto inner_loop;
			}
		}
		break;
	}
	goto loop;
}


static void
_putstr(int where, char **buf, char *cp)
{
	char c;

	while (c = *cp++)
		output(where, buf, c);
}

static void
printL(int where, char **buf, register __uint64_t n, register int b)
{
	char prbuf[65];	/* paranoia: large enough for base 2 numbers */
	register char *cp;

	if (b < 0) {
		b = -b;
		if ((__int64_t)n < 0) {
			output(where, buf, '-');
			n = (__uint64_t)(-(__int64_t)n);
		}
	}

	cp = prbuf;
	do {
		*cp++ = "0123456789abcdef"[n%b];
		n /= b;
	} while (n);
	for (--cp; cp >= prbuf; --cp)
		output(where, buf, *cp);
}

static void
printn(int where, char **buf, register __scunsigned_t n, register int b)
{
#if (_MIPS_SZLONG == 64)
	printL(where, buf, n, b);
#else
	char prbuf[33];	/* paranoia: large enough for base 2 numbers */
	register char *cp;

	if (b < 0) {
		b = -b;
		if ((__scint_t)n < 0) {
			output(where, buf, '-');
			n = (__scunsigned_t)(-(__scint_t)n);
		}
	}

	cp = prbuf;
	do {
		*cp++ = "0123456789abcdef"[n%b];
		n /= b;
	} while (n);
	for (--cp; cp >= prbuf; --cp)
		output(where, buf, *cp);
#endif	/* _MIPS_SZLONG == 64 */
}

void
printregs(eframe_t *ep, void (*f)(char *, ...))
{

#define PFMT3	"%s0x%llx 0x%llx 0x%llx\n"
#define PFMT4	"%s0x%llx 0x%llx 0x%llx 0x%llx\n"

#if _MIPS_SIM == _ABI64 || _MIPS_SIM == _ABIN32
	f(PFMT3, "at/v0/v1:\t", ep->ef_at, ep->ef_v0, ep->ef_v1);
	f(PFMT4, "a0-a3:\t\t", ep->ef_a0, ep->ef_a1, ep->ef_a2, ep->ef_a3);
	f(PFMT4, "a4-a7 (t0-t3):\t",
				ep->ef_a4, ep->ef_a5, ep->ef_a6, ep->ef_a7);
	f(PFMT4, "t0-t3 (t4-t7):\t",
				ep->ef_t0, ep->ef_t1, ep->ef_t2, ep->ef_t3);
	f(PFMT4, "s0-s3:\t\t", ep->ef_s0, ep->ef_s1, ep->ef_s2, ep->ef_s3);
	f(PFMT4, "s4-s7:\t\t", ep->ef_s4, ep->ef_s5, ep->ef_s6, ep->ef_s7);
	f(PFMT4, "t8/t9/k0/k1:\t", ep->ef_t8, ep->ef_t9, ep->ef_k0, ep->ef_k1);
	f(PFMT4, "gp/sp/fp/ra:\t", ep->ef_gp, ep->ef_sp, ep->ef_fp, ep->ef_ra);
	f(PFMT3, "mdlo/mdhi/sr:\t", ep->ef_mdlo, ep->ef_mdhi, ep->ef_sr);
	f(PFMT3, "cause/epc/badva:\t", ep->ef_cause, ep->ef_epc, ep->ef_badvaddr);

#else

	f(PFMT3, "at/v0/v1:\t", ep->ef_at, ep->ef_v0, ep->ef_v1);
	f(PFMT4, "a0-a3:\t\t", ep->ef_a0, ep->ef_a1, ep->ef_a2, ep->ef_a3);
	f(PFMT4, "t0-t3:\t\t", ep->ef_t0, ep->ef_t1, ep->ef_t2, ep->ef_t3);
	f(PFMT4, "t4-t7:\t\t", ep->ef_t4, ep->ef_t5, ep->ef_t6, ep->ef_t7);
	f(PFMT4, "s0-s3:\t\t", ep->ef_s0, ep->ef_s1, ep->ef_s2, ep->ef_s3);
	f(PFMT4, "s4-s7:\t\t", ep->ef_s4, ep->ef_s5, ep->ef_s6, ep->ef_s7);
	f(PFMT4, "t8/t9/k0/k1:\t", ep->ef_t8, ep->ef_t9, ep->ef_k0, ep->ef_k1);
	f(PFMT4, "gp/sp/fp/ra:\t", ep->ef_gp, ep->ef_sp, ep->ef_fp, ep->ef_ra);
	f(PFMT3, "mdlo/mdhi/sr:\t", ep->ef_mdlo, ep->ef_mdhi, ep->ef_sr);
	f(PFMT3, "cause/epc/badva:\t", ep->ef_cause, ep->ef_epc, ep->ef_badvaddr);
#endif

#undef PFMT3
#undef PFMT4
}


/*
 * Pass the current string to the console ("prom") output routines. Since
 * the conbuf is a circular buffer, it may be necessary to do two writes.
 *
 * The parameter is now flags instead of 'islocked'.  If CONBUF_UNLOCKED is
 * set we are being dumping on behalf of another processor and putbuflck
 * must be reacquired.  If CONBUF_DRAIN is set, after writing to the console
 * we call an additonal console routine so the output is actually flushed
 * to the output device.
 */
#if SN0

static void
conbuf_flush_timeout(register int flags)
{
	conbuf_tout = 0;
	conbuf_flush(flags);
}
#endif

void
conbuf_flush(register int flags)
{
	register int s;
#if SN0
	extern int printf_acquire_console(void);
	extern void printf_release_console(void);
#endif

#if defined(CELL_IRIX)
	/* Only the golden_cell flushes to the real console */
	if (cellid() != golden_cell) return;
#endif

#if SN0
	/* on SN0, all output pauses when the system controller daemon
	 * takes over the console port to write to the FFSC. We must
	 * not corrupt that data by spitting things to the
	 * console. Try again in a little while.
	 */
	if (!printf_acquire_console()) {
		/* we only need one timeout at a time. Make sure there
		 * isn't already one pending before requesting another
		 * one. 
		 */
		if (compare_and_swap_int(&conbuf_tout, 0, 1)) {
			if (timeout_nothrd((void(*)())conbuf_flush_timeout,
					   (void*)(long)CONBUF_UNLOCKED,
					   HZ / 4) == 0)
				/* force the compiler to generate an
				 * atomic store
				 */
				*(volatile int*)&conbuf_tout = 0;
		}
		return;
	}
#endif

	if (flags & CONBUF_UNLOCKED)
	    PUTBUF_LOCK(s);
	else
	    s = spl7();

	if (constrlen) {
	    cn_write(conbuf, constrlen);
	    constrlen = 0;
	}

	/* Make sure data is written all the way to the output display. */
	if (flags & CONBUF_DRAIN)
		cn_flush();

	if (flags & CONBUF_UNLOCKED)
	    PUTBUF_UNLOCK(s);
	else
	    splx(s);
#if SN0
	printf_release_console();
#endif
}

/*
 * Wrapper to prevent calling promreset twice
 */
static void
dopromreset(int arg)
{
	static int prom_is_reset = 0;

	if (!prom_is_reset) {
		cn_init(arg, 0);
		prom_is_reset = 1;
	}
}

/*
 * Sit and spin after a panic. The only thing we do is:
 *
 * 1. If we're the master processor, we need to flush console output.
 * 2. Check for cpuaction() requests to reboot.
 *
 * This is now a cpuvaction so we don't need to be able to get an action
 * block in order to panic.
 */
void
panicspin(void)
{
	clkreld();	/* turn off clock */
	
	spl7();
	dumptlb(cpuid());
	setjmp(private.p_panicregs_tbl);
	private.p_panicregs_valid = 1;

	/* If panicing due to ecc error on cpu, we don't want that cpu to
	 * initiate the dump.  Let master cpu do it instead, unless it is
	 * the one with the ecc error.  If the master cpu has the error, let
	 * the NEIGHBOR cpu do the syncreboot since the panic processor may
	 * have the highest number.
	 */
	if (ecc_panic_cpu != -1 && cpuid() != ecc_panic_cpu) {
		if (private.p_flags & PDAF_MASTER) {
			syncreboot();
		} else if ((pdaindr[ecc_panic_cpu].pda->p_flags &
			    PDAF_MASTER) &&
			   (cpuid() == (ecc_panic_cpu^1))) {
			syncreboot();
		}
	}

	for (;;) {
#if EVEREST || SN0 || IP30
		if (kdebug) {
			extern void chkdebug(void);
			chkdebug();
		}
#endif
		doactions();
		DELAY(500);
	}
}

static void
syncreboot(void)
{
	if (kdebug)
		debug("ring");

	/*
	 * Turning off lock bit ``initializes'' it without having
	 * to go through metering initialization.  We do this just
	 * in case... (in case it is already locked, I guess).
	 * Setting splockmeter to 0 ensures that we'll skip possible
	 * double-trips through metering code.
	 *
	 * spinlock_init(&putbuflck, "putbuf");
	 */

	splockmeter = 0;
#if MP
	putbuflck &= ~SPIN_LOCK;
	haspblock = -1;
	pblock_nest = 0;
#endif
	dumpsys();
	/*NOTREACHED*/
}

/*
 * rebooting routines
 */
typedef volatile struct mboot_s {
	int	cpunum;		/* # of CPU active in system to be rebooted */
	int	(*rboot_func)(char *);/* prom entry point */
	lock_t	mboot_lock;	/* spin lock */
} mboot_t;

static mboot_t mboot;

#define REBOOT_WAIT (5 * timer_freq)	/* 5 seconds */

/*
 * Machine dependent code to reboot
 */
void
mdboot(
	int fcn,
	char *mdep)
{
	
	/*
	 * Sync the tod clock with what the kernel is keeping
	 */
	wtodc();

	switch (fcn) {
	case AD_IBOOT:
		cmn_err_tag(292,CE_WARN,"Sorry, no interactive reboot.");
	case AD_HALT:
		mprboot(prom_reboot, 0);
		break;
	case AD_EXEC:
		cmn_err_tag(293,CE_WARN,"Sorry, no standalone exec.");
		mprboot(prom_reboot, 0);
		break;
	case AD_BOOT:
		mprboot(prom_autoboot, mdep);
		break;
	case AD_POWEROFF:
		power_off_flag = 1;
		/* call prom_reboot so we get the alarmclock auto poweron
		 * setup, if necessary. */
		mprboot(prom_reboot, 0);
		break;
	}
}

static void
mprboot(
	void (*func)(),
	char *mdep)
{
	int s, i, id, total_number_of_cpus;
	extern int (*io_halt[])(void);

	if (maxcpus > 1)
		cmn_err(CE_CONT,
			"%s started from CPU %d\n",
			power_off_flag ? "Power off" : "Reboot",
			cpuid());

	spinlock_init((lock_t *)&mboot.mboot_lock, "rbootlck");
	spl7();

	/*
 	 * Halt devices. allow devices to stop cleanly
	 * AFTER interrupts are shut off.
	 */

	for(i = 0; io_halt[i]; i++)
		(*io_halt[i])();

	s = mp_mutex_spinlock((lock_t *)&mboot.mboot_lock);
	mboot.rboot_func = (int (*)())func;
	mboot.cpunum = 1;

	total_number_of_cpus = MAX_NUMBER_OF_CPUS();
	for(i = 0; i < total_number_of_cpus; i++)
		if (((id = pdaindr[i].CpuId) != -1) &&
		    (id != private.p_cpuid)) {
			mboot.cpunum++;
			cpuaction(id, (cpuacfunc_t)do_mprboot, A_NOW);
		}
	mp_mutex_spinunlock((lock_t *)&mboot.mboot_lock, s);
	do_mprboot(mdep);
}

static void
do_mprboot(char *mdep)
{
	register int s;
	__uint64_t end_time;

	ASSERT(mboot.cpunum > 0);
	if (maxcpus > 1 && ! power_off_flag)
		cmn_err(CE_CONT, "CPU %d rebooting\n", cpuid());
	s = mp_mutex_spinlock((lock_t *)&mboot.mboot_lock);
	mboot.cpunum--;
	mp_mutex_spinunlock((lock_t *)&mboot.mboot_lock, s);
	if (cpuid() == masterpda->p_cpuid) {
		/*
		 * use the do construct such that conbuf_flush will be called
		 * even if the master processor comes here last
		 */
		end_time = absolute_rtc_current_time() + REBOOT_WAIT;
		do {
			if (*(volatile int *)&constrlen)
				conbuf_flush(CONBUF_UNLOCKED|CONBUF_DRAIN);
#ifdef IP19
			{
			/* No use waiting for the last cpu if we've died due
			 * to a cache error on one of the cpus.
			 */

			extern int ecc_panic_deadcpus(void);

			if ((mboot.cpunum == 1) && ecc_panic_deadcpus())
				mboot.cpunum = 0;
			}
#endif /* IP19 */		
			/* Put a time limit on the wait:  CPUs may be spinning
			 * with interrupts off (especially on an NMI)
			 */
			if (absolute_rtc_current_time() > end_time) {
				break;
			}

			us_delay(100000);	/* 0.1 second */

		} while (mboot.cpunum);
	}

	(*mboot.rboot_func)(mdep);
}

#define MAX_CONLEN	256
#ifdef CELL_IRIX
static void
get_cell_conmsg(char *msg, int *pbi, int *cc)
{

	register int ci = 0;
	register char *cp;

	do {
		cp = &cell_console_buf[(*pbi)++];
		*pbi %= CELL_CONBUFSZ;
		if (*cp == '\0') {
			continue;
		}
		if (*cp & PRW_FLAG) {
			continue;
		}
		msg[ci++] = *cp;
		(*cc)--;
		if (*cp == '\n') {
			goto done;
		}
	} while (*pbi != cell_conbuf_start && ci < MAX_CONLEN && *cc != 0);

done:
	if (*pbi == cell_conbuf_start) {
		*cc = 0;
	}
	msg[ci] = '\0';
	return;
}
#endif

static void
get_conmsg(char *msg, int *pbi, int *cc)
{

	register int ci = 0;
	register char *cp;
	register int	putbufmask = putbufsz - 1;

	if (*cc == 0) {
		goto done;
	}

	do {
		cp = putbuf + (*pbi)++;
		if (*cp != '\0') {
			msg[ci++] = *cp;
			(*cc)--;
		}
		*pbi %= putbufsz;
		if (*cp == '\n') {
			goto done;
		}
	} while (*pbi != (putbufndx & putbufmask) && *cc != 0 && 
		ci < MAX_CONLEN);

done:
	if (*pbi == (putbufndx & putbufmask)) {
		*cc = 0;
	}
	msg[ci] = '\0';
	return;
}

void
printputbuf(
	int n,		/* if < 0 print entire thing; else that many bytes */
	void (*pfunc)(char *, ...))
{

	register int		lim;
	register int		opl;
	register int		putbufmask = putbufsz - 1;
	int			pbi;
	int			cc;
	char			msg[MAX_CONLEN];

	opl = spl7();

#ifdef CELL_IRIX
	pbi = cell_conbuf_start;
	cc = n;

	if (cc > 0) {
		if (cc > CELL_CONBUFSZ) {
			cc = CELL_CONBUFSZ;
		}
		pbi -= cc;
		if (pbi < 0) {
			pbi += CELL_CONBUFSZ;
		}
	}

	do {
		get_cell_conmsg(msg, &pbi, &cc);
		if (msg[0] != '\0') {
			(*pfunc)(msg);
		}
	} while (pbi != cell_conbuf_start && cc != 0);
#endif

	pbi = putbufndx & putbufmask;
	lim = putbufsz;
	cc = n;
	if (cc > 0) {
		if (cc > lim) {
			cc = lim;
		}
		pbi -= cc;
		if (pbi < 0) {
			pbi += lim;
		}
	}

	do {
		get_conmsg(msg, &pbi, &cc);
		if (msg[0] != '\0') {
			(*pfunc)(msg);
		}
	} while (pbi != (putbufndx & putbufmask) && cc != 0);

	splx(opl);
}

/*
 * Called by the ASSERT macro in debug.h when an
 * assertion fails.
 * All the pertinent registers have already been saved in assfail
 * in locore which calls this
 */
eframe_t _assregs;
int clrass = 0;
extern int console_is_tport;

void
_assfail(char *a, char *f, int l)
{
	int s;
	extern int panic_cpu;

	/* set so other cpus will stop pronto */
	s = spl7();

	/*
	 * panic_cpu is set in assfail() to the cpu which saved
	 * off it's registers so it should be the one to print
	 * them and subsequently panic.
	 */
	if (cpuid() == panic_cpu) {
		enter_panic_mode();

		if (console_is_tport)
			dopromreset(1);	/* since we're sort of panicking */

		printf("assertion failed cpu %d: %s, file: %s, line: %d\n",
				cpuid(), a, f, l);
		printregs(&_assregs, printf);
	}

	if (clrass == 2) {
		exit_panic_mode();
		splx(s);
		return;
	}

	if (kdebug) {
		/*
		 * Tell all of the other processors we're
		 * panicking.  They'll just sit and spin,
		 * only performing cpuaction() requests.
		 */
	 	if (cpuid() == panic_cpu) {
	                debug_stop_all_cpus((cpumask_t*)-1LL);
		}
		debug("ring");
	}

	if (clrass) {
		if (clrass != 2)
			clrass = 0;
		exit_panic_mode();
		splx(s);
		return;
	} 

	/* 
	 * enter_panic_mode() will only allow 1 cpu to proceed in the 
	 * case where multiple cpus assert at roughly the same time. 
	 */
	enter_panic_mode();

	cmn_err(CE_PANIC, "assertion failure!");
}


/*
 * Some (all?) proms will clear the graphics screen when restarted, not
 * leaving much time to read any messages.  Therefore, if the system is
 * panicking, just spin and wait for a hard reset.
 */
extern void dprintf(char *, ...);

void
prom_reboot(void)
{
	extern int reboot_on_panic;
	char *arg;

	if (panicstr) {
		int	old_value = reboot_on_panic;
		/*  Some people want to reboot on panic, and others want
		 * to spin so they can see the panic message, or avoid
		 * panic loops.  New proms have the env var 'rebound'
		 * to control this.  If 'rebound' is set it overrides
		 * the stune 'reboot_on_panic' variable.
	 	 */
		if ((arg = kopt_find("rebound")) && arg[0])
			reboot_on_panic = (arg[0] == 'y');

		/* If reboot_on_panic was set to 2, it indicates 
		 * someone wanted to force a hard reset as part of 
		 * reboot (Fix for some SN0 specific problem).
		 * So, if we are rebooting, check for this situation.
		 */
		if (reboot_on_panic && (old_value == 2))
			reboot_on_panic = 2;

#ifdef DEBUG
		/* Hope the textport/duart is working */
		if (kdebug) {
			static int panic_sync = 0;

			/*
			 * don't want symmon and IRIX outputting to the serial
			 * console at the same time
			 */
			if (cpuid() == masterpda->p_cpuid) {
				dprintf("\nexit debugger to reboot\n\n");
				panic_sync++;
			}
			else {
				while (!panic_sync)
					;
			}
			debug("ring");
			prom_reinit();
		}
#endif

		/* If we don't want the system to rebound, wait for a reset
		 */
		if (!reboot_on_panic) {
			extern void cpu_waitforreset(void);

			if (cpuid() == masterpda->p_cpuid) {
				dprintf("[Press reset to restart the machine.]");

				/* Wait for a reset or power-off */
				cpu_waitforreset();
				/* NOTREACHED */
			}
			else {
				/*
				 * IP30: don't want slave to play with ioc3
				 * and power management hardware when master
				 * may be outputting to the serial console
				 */
				while (1)
					;
				/* NOTREACHED */
			}
		}

		if (cpuid() == masterpda->p_cpuid)
			dprintf("Restarting the machine...\n");
		prom_autoboot();
	}

	/* Not rebounding or panicing, go back to the prom
	 */
	prom_reinit();
}

void
arcs_printf(char *fmt, ...)
{
	va_list ap;
	cpuid_t cpu = getcpuid();

	va_start(ap, fmt);

	if (cn_is_inited() && (pdaindr[cpu].CpuId == cpu))
		icmn_err_tag(0, CE_CONT,fmt,ap);
	else
		errprintf(PRW_ARCS, NULL, fmt, ap);
	va_end(ap);
}

/*
 * To prevent deadlocks in certain cases, locking on putbuflck is allowed
 * to succeed if the cpu already has the lock.  In the case that a CPU doesn't
 * release the putbuf lock for a long period of time, we'll break it.
 * In order to avoid puttiong yet anotehr lock around the code that breaks
 * the putbuf lock (which might well fail in the same way under the dire
 * circumstances where we'd break the lock in the first place), we've added
 * a delay based on the CPU id.  The theory here is that in a panic situation,
 * CPUs may all want to get to the putbuf at the same time while some other
 * CPU is holding the lock, never to release it.  By adding an offset based
 * on the CPU id, we minimize the chance that multiple CPUs will break the
 * lock simultaneously.
 *
 * The lock must go to spl7 here because an arbitrarily high level
 * interrupt may do a kernel printf.  However, since this may cause
 * the audio driver to miss its deadline, DSD platforms don't allow
 * kernel printfs from interrupt levels above splhi except during
 * a kernel panic.
 *
 * If we are in promlog mode, we are already holding the putbuf lock, so
 * don't worry
 */
#if IP20 || IP22 || IP26 || IP28 || IP30 || IP32
#define splputbuf splhi
#else
#define splputbuf spl7
#endif

int pbuf_nested;

int
putbuf_lock()
{
	int s, i;
	uint gen_number;

	if (haspblock == cpuid()) {
		s = 0;
		pblock_nest++;
		pbuf_nested++;
		if (!in_promlog_mode() && !in_panic_mode())
			debug("ring");
	} else if (in_panic_mode()) {
		gen_number = putbuflck_gen;
		/*
	 	 * We need to timeout the putbuf lock in this case.
		 * Wait PUTBUF_LOCK_USECS + 10 ms * cpuid.
		 */
		for (i = 0; i < (PUTBUF_LOCK_USECS +
				 (cpuid() * 10000)); i++) {
			if (s = mutex_spintrylock_spl(&putbuflck, splputbuf))
				break;
			/* Wait a microsecond */
			DELAY(1);
			/*
			 * If someone else has obtained and released the
			 * putbuf lock while we were holding it, don't break it.
			 */
			if (gen_number != putbuflck_gen)
				i = 0;
		}

		/* Break the lock.  Assume no one will release it now. */
		s = splhi();
		haspblock = cpuid();
		putbuflck_gen++;

	} else {
		s = mutex_spinlock_spl(&putbuflck, splputbuf);
		haspblock = cpuid();
	}
	
	return s;
}

int
putbuf_trylock()
{
	int		s;

	s = mutex_spintrylock_spl(&putbuflck, splputbuf);
	return(s);
}


void
putbuf_unlock(int s)
{
	if (pblock_nest) {
		pblock_nest--;
	}
	else {
		haspblock = -1;
		putbuflck_gen++;
		mutex_spinunlock(&putbuflck, s);
	}
	return;
}

#ifdef CELL_IRIX
#define ADD_CONBUF(_c)	cell_console_buf[cell_conbuf_start++] = (char)(_c); \
			cell_conbuf_start %= CELL_CONBUFSZ; 
static void
cell_output(int where, uchar_t c)
{

	int	newpos;
	int	buflen = 1;
	static int cur_where = 0;
	static int msg_startpos = 0;

	/*
	 * Save buf to circular console buffer so another thread
	 * can do the acutal write to the console since it may not
	 * be safe to do so now. 
	 */
	
	if (in_con_rpc == cpuid()) {					

		/* This message was triggered by an rpc of a previous message */

		where |= PRW_RPC;				
	}

	if (cur_where != where) {

		/* New message, we prepend an extra control character */

		buflen = 2;
		cur_where = where;
		msg_startpos = cell_conbuf_start;
	}

	if (!cell_console_overflow) {

		/* Figure out the next position in the buffer */

		newpos = (cell_conbuf_start + buflen) % CELL_CONBUFSZ; 
	}

	if (cell_console_overflow || 
			(newpos >= cell_conbuf_flushed) &&
			((cell_conbuf_start < cell_conbuf_flushed) ||
			 (newpos < cell_conbuf_start))) {

		/*
		 * This character would write over older messages that
		 * have not been flushed to the console yet so skip
		 * it. The overflow flag will be cleared once the
		 * buffer has been flushed. 
		 */

		if (!cell_console_overflow) {
			cell_console_overflow = 1;

			/* The current message overflowed so we delete it */

			cell_conbuf_start = msg_startpos;
			cell_console_buf[cell_conbuf_start] = '\0';

			/*
			 * Clear the "where" flag so that once the overflow
			 * flag is cleared a new message can be started.
			 */

			cur_where = 0;
		}

		if (c == '\n') {

			/* Count the messages lost */

			cell_console_overflow++;
		}
	}
	else {

		if (buflen > 1) {

			/* Put in control character and then message */

			ADD_CONBUF(where | PRW_FLAG);
		}
		ADD_CONBUF(c);
		if (c == '\n') {

			/* End of this message */

			cur_where = 0;
			cvsema(&cellconssema);
		}
	}
}

#define MAX_CONFLUSH	256

void
I_dshost_printf(cell_t client, char *msg, size_t len)
{
	int i, s;
	int where;
	char c, *cp;

	/* Process the printf from the remote cell. */

	PUTBUF_LOCK(s);
	if (!in_con_message || last_con_cellid != client) {

		/*
		 * We need to pre-pend the cell number first.
		 */

		errvprintf(PRW_KLOG | PRW_CONS, NULL, "(CELL:%d) ", client);
	}
	last_con_cellid = client;

	/* The printf may contain sections that don't all go to the
	 * console. A flag character should precede each one. We also
	 * clear the PRW_BUF bit because the printf is already in
	 * the console buffer of the remote cell.
	 */

	i = 0;
	while (i < len && msg[i] != '\0') {
		where = (int)msg[i++];
		ASSERT(where & PRW_FLAG);
		where &= ~(PRW_BUF | PRW_FLAG);
		if (where & PRW_ARCS) {

			/* Very early console output from remote cell */

			where &= ~PRW_ARCS;
			where |= PRW_CONS;
		}
		cp = &msg[i];
		while (i < len && !((int)msg[i] & PRW_FLAG) && msg[i] != '\0') {
			i++;
		}

		/* Found end of current section, print it. */

		c = msg[i]; 	/* Save the last (flag) character */
		msg[i] = '\0';	/* Terminate this section */
		errvprintf(where, NULL, cp);
		msg[i] = c;	/* Put the flag back */
	}
	PUTBUF_UNLOCK(s);
}

static void
flush_cell_console_buf(void)
{

	int i;
	char *cp, cur_msg[MAX_CONFLUSH];
	int s, buflen;
	service_t svc;
	/* REFERENCED */
	int msgerr;

	/* 
	 * For each message in the console buffer flush it to
	 * the golden cell so it can deal with it.
	 */

	while (cell_conbuf_flushed != *(volatile int *)&cell_conbuf_start) {

		/* Package up a message to flush. We look for '\n' to
		 * indicate where it ends.
		 */

		PUTBUF_LOCK(s);
		buflen = 0;
		while (buflen < (MAX_CONFLUSH - 1) &&
				(cell_conbuf_flushed != 
					*(volatile int *)&cell_conbuf_start)) {
			cp = &cell_console_buf[cell_conbuf_flushed++];
			cell_conbuf_flushed %= CELL_CONBUFSZ;
			if (!buflen && !(*cp & PRW_FLAG)) {

				/* Not the start of a message */

				continue;
			}
			cur_msg[buflen++] = *cp;
			if (*cp == '\n') {
				break;
			}
		}
		PUTBUF_UNLOCK(s);
		cur_msg[buflen++] = '\0';
		
		if (buflen > 1) {

			/* We have message to send */

			SERVICE_MAKE(svc, CELL_GOLDEN, SVC_HOST);
			ASSERT((int)cur_msg[0] & PRW_FLAG);
			if ((int)cur_msg[0] & PRW_RPC) {

				/* A previous rpc triggered this message.
				 * We skip it to prevent a recursive loop.
				 */

				continue;
			}

			START_CON_RPC();
			msgerr = invk_dshost_printf(svc, cellid(), cur_msg,
							buflen);
			ASSERT(!msgerr);
			END_CON_RPC();

		}
	}
	if (cell_console_overflow) {

		/*
		 * The buffer overflowed before we could be woken up. Clear
		 * the flag now that we have flushed out the buffer and send
		 * out a warning message that some were lost.
		 */

		i = cell_console_overflow;
		cell_console_overflow = 0;
		printf("WARNING: lost %d console messages\n", i);
	}
}

void
cell_console_thread(void)
{
	/*
	 * This thread just sleeps waiting for a 'printf' to
	 * wake it up when there is a message in the buffer to be
	 * flushed to the golden cell.
	 */

	initnsema(&cellconssema, 0, "cellcons");

	while (1) {
		flush_cell_console_buf();
		psema(&cellconssema, PSWP);
	}
}
#endif /* CELL_IRIX */
 
/*
 * This procedure is meant to be invoked from DBG: prompt as 
 * "call _symmon_dump" to take a core dump. Give it some time to do the
 * dump, as it might be doing its work silently.
 */
void
_symmon_dump()
{
	extern void _hook_exceptions(void);
 
	/* reset putbuflck to avoid problems with lock acquisition
	 * following an NMI.
	 */
#if MP
	panic_haspblock = haspblock;
	panic_putbuflck = putbuflck;
	putbuflck &= ~SPIN_LOCK;
	haspblock  = -1;
	pblock_nest = 0;
#endif
	
	/* Need to be able to process kernel K2 tlbmisses, so we can't use
	 * symmon's tlbmiss handler. Likewise need general exception vector
	 * set to kernel for second level tlbmiss processing.
	 */
	kdebug = 0;     /* no more symmon ! */
	panic_level = 0; 	/* arrange to call even from panics */
	private.p_utlbmisshndlr = utlbmiss_swtch[0].u_start;
	private.common_excnorm = get_except_norm();
	
#if 0
	/* may only be needed when entering from POD */
	_hook_exceptions();
#endif
	cmn_err(CE_PANIC, "symmon_dump routine invoked from symmon/POD");

}
