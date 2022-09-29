#ident "symmon/fault.c: $Revision: 1.32 $"

/*
 * Copyright 1985 by MIPS Computer Systems, Inc.
 */

/*
 * fault.c -- saio/dbgmon fault handling routines
 */

#include "dbgmon.h"
#include "mp.h"
#ifdef NETDBX
extern int netrdebug;
#endif /* NETDBX */

/*
 * symmon_exchandler -- saio library fault handler
 */
/* ARGSUSED */
void
symmon_exchandler(k_machreg_t *ep)
{
	jmpbufptr jbp;

	if (private.dbg_exc != EXCEPT_NMI) {
		_check_bp();		/* give debug monitor a shot at it */
		/* if catching - let u try to fix by calling kernel
		 * K2SEG tlbmiss handler
		 * If it returns then it couldn't so just longjmp
		 */
		if ( private.dbg_modesav == MODE_IDBG || 
		     private.dbg_modesav == MODE_DBGMON ) {
			trykfix();
		}
		if (private.pda_nofault) {
#ifdef IP30
			cpu_clearnofault();
#endif
			jbp = private.pda_nofault;
			private.pda_nofault = 0;
			longjmp(jbp, 1);
		}
	}
#ifdef NETDBX
	if (private.dbg_exc == EXCEPT_NMI)
		netrdebug = 0;
#endif
	clear_nofault();
	private.pda_nofault = 0;
	symmon_fault();

#ifdef NETDBX
#ifdef MULTIPROCESSOR
	if (netrdebug) do_command_parser();
#endif /* MULTIPROCESSOR */
#endif /* NETDBX */

	_restart();
}

/* Private nofault to keep out libsk exception handlers.
 */
void
clear_nofault(void)
{
	register struct generic_pda_s *gp = (struct generic_pda_s *)
			&GEN_PDA_TAB(cpuid());

	_init_alarm();
	gp->pda_nofault = 0;
	gp->notfirst = 0;
}
