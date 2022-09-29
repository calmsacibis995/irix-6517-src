#ident	"arcs/ide/IP19/uif/msg_printf.c:  $Revision: 1.15 $"

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include <sys/types.h>
#include <stdarg.h>
#include <genpda.h>
#include <ide_msg.h>
#include <alocks.h>
#include "saioctl.h"
#include "uif.h"
#include "ide.h"
#include "ide_mp.h"
#include "../include/prototypes.h"

/* 
#define PUTBUFSIZE	0xc00
#define MAXMSGLENGTH	160
*/

#if defined(MULTIPROCESSOR)	/* ...as determined by alocks.h */
#define _cpuid()	cpuid()
#else
#define _cpuid()	0
#endif

#define CIRCBUFSIZE	0x10000	/* 64K circular (shared) buffer */
static char circbuf[CIRCBUFSIZE];
static char *cb_head = &circbuf[0];

/*
 * We need a static output buffer until the PDAs are initialized. 
 * Since this initializing is done by the master before launching
 * any children, don't worry about MP problems.
 */
static char initpb[PUTBUFSIZE];


short msg_where;	/* output dest: buffer until initialized */
int failflag;

/*
 * IDE uses only the lower 16 bits of 'Reportlevel' for the integer
 * report level value.  The upper short is borrowed to use as a
 * bitfield, overriding the integer level for only the specific 
 * messages having one of the bits set.  If not masked-off, these bits
 * make 'Reportlevel' look huge, and msg_printf becomes very verbose.
 */
#undef Reportlevel	/* ide.h currently cooks 'Reportlevel' with a #define */

#define NORM_MLEVEL(_mlevel)    (int)(NORM_MMASK & _mlevel)

#define IDE_EMSG	(NORM_MLEVEL(msglevel) == IDE_ERR) 
#define DIAG_EMSG	(NORM_MLEVEL(msglevel) == ERR)
#define USER_EMSG	(NORM_MLEVEL(msglevel) == USER_ERR)
#define SPEC_EMSG	(IDE_EMSG || USER_EMSG)
#define ERROR_MSG	(SPEC_EMSG || DIAG_EMSG)
#define IDE_DBGMSG	(msglevel & IDE_DBGMMASK)
#define BEST_DOIT	(msglevel & PRINT_IT)

#define MSG_DEBUG	(0x10000000)
#define MSG_MSG		(MSG_DEBUG & (*Reportlevel))


/*
 * these variables monitor the master's stack.  Only the processor
 * running as IDE_MASTER interprets the script code (which burns 
 * stackspace), so only the master uses them: no need to lock or
 * put in PDAs.
 */
#if defined(STKDEBUG)
extern float ms_tos, ms_bos, ms_ssize;
extern float ms_curlen, ms_sp_biggest;
extern float ms_sp_now;
static int ms_pct_used = 0;
static char msp_msgbuf[240];
#endif


extern int get_mvid(void);
extern int _ide_initialized;

/*
 *  msg_printf - message printf
 *
 *  This function is intended to be the formatted print tool common to
 *  all diagnostic tests.  It is patterned after printf() but requires
 *  that a "message level" be provided with each call.  Actual output
 *  only occurs when this message level is less than or equal to the
 *  globally defined "Reportlevel".  Also provided is the ability to
 *  temporarily buffer messages when buffering mode is turned on via
 *  the externally defined variable "msg_where".
 *
 *  WARNING:  For message buffering to work messages must not be longer
 *            than MAXMSGLENGTH (defined in arcs/include/ide_msg.h).
 *
 *  When made for systems for which 'MULTIPROCESSOR' is defined, the
 *  printing resource must be locked to single-thread simultaneous 
 *  requests from slaves and master.  LOCK_USER_INTERFACE() and
 *  UNLOCK_USER_INTERFACE() make this lock easy to manipulate.
 */
/*VARARGS2*/
void
msg_printf(int msglevel, char *fmt, ...)
{
    extern short msg_where;  /* indicates destination of output */
    int rlevel = NORM_MLEVEL(*Reportlevel);	/* keep only lower 16 bits */
    va_list ap;

    static char ide_emsg[] = "\nIDE INTERNAL ERROR: ";
    static char user_emsg[] = "\nUSER ERROR: ";
    char *emsgp=0;
    int estrsize, doit=0;
    int vid_me = cpuid();
    char *pb_ptr;
    int pb_index = 0;

    ASSERT(_ide_initialized == 0 || _ide_initialized == IDE_PDAMAGIC);

    if (!_ide_initialized) {
#ifdef NOHACKS
	pb_ptr = &initpb[0];
#else
	pb_ptr = cb_head;
#endif
    } else {
#ifdef NOHACKS
	pb_ptr = &(IDE_ME(putbuf[0]));
#else
	pb_ptr = cb_head;
#endif
    }
    pb_index = 0;

    /* only interested in ide_master's stack */
#if defined(NOOSTKDEBUG)
    if (_initialized && (vid_me == get_mvid())) {
	ms_sp_now = (float)get_sp();

	if (ms_sp_now < ms_sp_biggest) {
	    ms_sp_biggest = ms_sp_now;
	    ms_curlen = (ms_tos - ms_sp_biggest);
	    ms_pct_used = (ms_curlen/ms_ssize) * 100;
/* 	    LOCK_USER_INTERFACE();	*/
	    if (ms_curlen >= ms_ssize) {
		sprintf(msp_msgbuf, "MASTER STACK OVERFLOW!!!!\n");
		_errputs(msp_msgbuf);
		sprintf(msp_msgbuf,"    !!sp 0x%x (TOS 0x%x, BOS 0x%x)\n",
			(uint)ms_sp_biggest, (uint)ms_bos, (uint)ms_tos);
		_errputs(msp_msgbuf);
/*		UNLOCK_USER_INTERFACE();	*/
		ide_panic("\nmsg_printf: IDE MASTER STACK OVERFLOW");
	    }
	    sprintf(msp_msgbuf,"\n%0.f master stack: %d%% used (%0.f total)\n",
		ms_curlen, ms_pct_used, ms_ssize);
	    _errputs(msp_msgbuf);
	    sprintf(msp_msgbuf,"    tos %0.f; bos %0.f\n", ms_tos, ms_bos);
	    _errputs(msp_msgbuf);
	    _errputs("\n");
/*	    UNLOCK_USER_INTERFACE();	*/

	}
    }
#endif /* STKDEBUG */

    /*
     * Now decide whether this message warrants printing: ide-internal
     * and ide-user errors plus messages marked "PRINT_IT" are
     * displayed unconditionally; additionally the top 16 bits
     * of msglevel are reserved for bit-field specification of
     * ide-internal debugging levels; a match with Reportlevel
     * prints.  Failing all this, a Reportlevel of NO_REPORT is
     * respected, and messages with msglevels <= Reportlevel
     * are printed.
     */
    if ( (SPEC_EMSG || BEST_DOIT || (IDE_DBGMSG & (*Reportlevel))) || 
        (rlevel > 0 && msglevel <= rlevel) ) {
	doit = 1;
    } else {
#if MSG_DEBUG
	if (MSG_MSG || (rlevel > DBG)) {
	    if (MSG_MSG)
	    	_errputs(" NOT_D1 ");
	    printf("  <msg:r%d(R%d), m%d==0x%x:NO>\n", rlevel, (*Reportlevel), 
		msglevel, msglevel);
	}
#endif
    }
    
    msglevel &= NORM_MMASK;
    emsgp = 0;		/* if remains NULL, this isn't an errormsg */
    estrsize = 0;

    if (ERROR_MSG) {
#if MSG_DEBUG
	if (MSG_MSG || (rlevel > DBG)) {
	    if (MSG_MSG)
	 	_errputs(" E1 \n");
	    printf("msg:(r%d/R%d, msglev %d==0x%x) is ERROR_MSG\n",
		rlevel, (*Reportlevel), msglevel, msglevel);
	}
#endif
	/*
	 * if this is added for EVEREST, only flash when system is 
	 * executing in NON_COHERENT mode (_coherent == COHERENT).
	 */
	/* ip12_set_sickled(1); */
	/*
	 *  We use a failure flag to tell us whether the failure LED should
	 *  be on or off at any given time.  This way we can do things like
	 *  blink the LED during long tests and leave it in its proper state
	 *  at the end of the test.
	 */
	failflag = 1;

	if (IDE_EMSG) {
	    emsgp = &ide_emsg[0];
	    estrsize = sizeof(ide_emsg);
#ifdef NOTDEF
	} else if (DIAG_EMSG) {
	    emsgp = &diag_emsg[0];
	    estrsize = sizeof(diag_emsg);
#endif /* NOTDEF */
	} else if (USER_EMSG) {
	    emsgp = &user_emsg[0];
	    estrsize = sizeof(user_emsg);
	}
    }


#ifdef NOHACKS
xxx    if ( !(msg_where & PRW_BUF) && IDE_ME(putbufndx) )
    {
	/*  We're NOT in "buffering" mode and there's something in the
	 *  buffer, so print it out. 
	 */
	if (MSG_MSG)
	    _errputs(" P2 ");
xxx	printf("%s", IDE_ME(putbuf));
xxx	IDE_ME(putbufndx) = 0;
    }
#endif

    if (doit) {
	va_start(ap, fmt);
	if (msg_where & PRW_BUF)
	{
	    int charcnt = 0;
	    /*
	     *  We`re in "buffering" mode (we must be waiting for the
	     *  graphics console to be initialized).
	     */
	    if ((pb_index + estrsize) < (PUTBUFSIZE - MAXMSGLENGTH))
	    {
		if (emsgp)
		{
		    if (MSG_MSG)
			_errputs(" SP1 ");
		    charcnt = sprintf(pb_ptr,"%s", emsgp);
		    /*
		     * Move the buffer pointer & inc index to end of
		     * newly-added string.
		     */
		    pb_ptr += charcnt;
		    pb_index += charcnt;
		}

		if (MSG_MSG)
			_errputs(" VS1 ");
		charcnt = vsprintf(pb_ptr, fmt, ap);
		/*
		 *  Move the buffer pointer to end of newly added string.
		 *  NOTE: The pointer is left pointing at the terminating
		 *        NULL char so that it will be overwritten and the
		 *        buffer will end up containing one big string.
		 */
		if (MSG_MSG)
			_errputs(" APND1 ");
		pb_index += charcnt;
		pb_ptr += charcnt;

#if !defined(NOHACKS)
		_errputs(cb_head);
		cb_head = pb_ptr;
#endif
	    }
	}	/* unbuffered */
	else
	{
	    if (emsgp) {
		if (MSG_MSG)
		    _errputs(" E2 ");
		printf("%s", emsgp);
	    }
	    if (MSG_MSG)
		_errputs(" V2 ");
	    vprintf(fmt, ap);
	}
	va_end(ap);
    } /* doit */
#define DOCHECK 0
#if DOCHECK
    else {
	if (MSG_MSG)
	    _errputs(" P3 ");
	printf("\n!!doit wasn't true!\n");
    }
#endif

} /* msg_printf */


/*
 *  buffon - msg_printf buffering ON
 *
 *  If using the graphics console, turn on buffering within msg_printf().
 */
void
buffon(void)
{

    if (console_is_gfx())
	msg_where = PRW_BUF;  /* buffering is ON */
}


/*
 *  buffoff - msg_printf buffering OFF
 */
void
buffoff(void)
{
    int rlevel = NORM_MLEVEL(*Reportlevel);	/* keep only lower 16 bits */

    if (MSG_MSG)
	_errputs(" ZZ \n");
    msg_where = PRW_CONS;  /* buffering is OFF */
    msg_printf(VRB, "");   /* dummy msg_printf call to flush buffer */
}

