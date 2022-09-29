/* sigLibP.h - private signal facility library header */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
02l,06jul94,rrr  added sc_resumption for I960KB.
02k,28jan94,kdl  marked sigEvtRtn pointer declaration extern.
02j,12jan94,kdl  added sigqueueInit() prototype.
02i,09jan94,c_s  applied rrr's patch for SPR #955.
02h,10dec93,smb  added event logging routine.
02g,22sep92,rrr  added support for c++
02f,22aug92,rrr  added some prototypes.
02e,30jul92,rrr  backed out 02d (we are now back to 02c)
02d,30jul92,kdl  backed out 02c changes pending rest of exc handling.
02c,29jul92,rrr  added fault table for exceptions
02b,08jul92,rrr  added all the _sigCtx* function prototypes.
02a,04jul92,jcf  cleaned up.
01b,26may92,rrr  the tree shuffle
01a,27apr92,rrr  written.
*/

#ifndef __INCsigLibPh
#define __INCsigLibPh

#ifdef __cplusplus
extern "C" {
#endif

#include "signal.h"
#include "exclib.h"
#include "private/funcbindp.h"
#include "regs.h"

#define RESTART		1	/* must be different from OK and ERROR */

#define SIG_TIMEOUT_RECALC(t)	(*_func_sigTimeoutRecalc)(t)

struct sigq			/* (Not posix) */
    {
    struct sigq		*sigq_next;
    struct sigq		*sigq_prev;
    };

struct sigstack			/* (Not posix) */
    {
    int			ss_onstack;
    void		*ss_sp;
    };

struct sigtcb			/* (Not posix) */
    {
    struct sigaction	sigt_vec[_NSIGS + 1];
    struct sigq		sigt_qhead[_NSIGS + 1];
    struct sigstack	sigt_stack;
    struct sigwait	*sigt_wait;
    sigset_t		sigt_kilsigs;
    sigset_t		sigt_blocked;
    sigset_t		sigt_pending;
    sigset_t		sigt_ignored;
    };

struct sigpend			/* (Not posix) */
    {
    struct sigq		sigp_q;
    struct siginfo	sigp_info;
    long		sigp_overruns;
    long		sigp_active_overruns;
    REG_SET		*sigp_pregs;
    struct sigtcb	*sigp_tcb;
    };

struct sigfaulttable
    {
    int sigf_fault;
    int sigf_signo;
    };

struct sigcontext		/* (Not posix) */
    {
    int			sc_onstack;
    int			sc_restart;
    sigset_t		sc_mask;
    struct siginfo	sc_info;
    REG_SET		sc_regs;
#if CPU_FAMILY==MC680X0
    unsigned short	sc_fformat;		/* only for 68k */
    unsigned short	sc_pad;			/* only for 68k */
#endif
#if CPU == I960KB
    struct {
	char		sc_data[16];		/* only for i960kb */
    } sc_resumption[1];
#endif
    REG_SET		*sc_pregs;
    };

extern VOIDFUNCPTR     sigEvtRtn;	/* windview - level 1 event logging */


#if defined(__STDC__) || defined(__cplusplus)
extern void _sigCtxRtnValSet (REG_SET *regs, int val);
extern void *_sigCtxStackEnd (const REG_SET *regs);
extern int _sigCtxSave (REG_SET *regs);
extern void _sigCtxLoad (const REG_SET *regs);
extern void _sigCtxSetup (REG_SET *regs, void *pStackBase, void (*taskEntry)(),
        int *args);

extern int	sigInit (void);
extern void	sigPendInit (struct sigpend *__pSigPend);
extern int	sigPendDestroy (struct sigpend *__pSigPend);
extern int	sigPendKill (int __tid, struct sigpend *__pSigPend);
extern int	sigqueueInit (int nQueues);
#else
extern void _sigCtxRtnValSet ();
extern void *_sigCtxStackEnd ();
extern int _sigCtxSave ();
extern void _sigCtxLoad ();
extern void _sigCtxSetup ();

extern int	sigInit ();
extern void	sigPendInit ();
extern int	sigPendDestroy ();
extern int	sigPendKill ();
extern int	sigqueueInit ();
#endif

#ifdef __cplusplus
}
#endif

#endif /* __INCsigLibPh */
