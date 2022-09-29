/*
 * debug_block.h - definition of symmon debugger information block
 */

#ifndef _ARCS_DEBUG_BLOCK_H
#define _ARCS_DEBUG_BLOCK_H

#ident "$Revision: 1.12 $"

#if	defined(_LANGUAGE_C)

#include <sys/types.h>
#include <sys/cpumask.h>

typedef	cpumask_t	db_flush_t;

typedef struct debug_block {
    unsigned int	db_magic;	/* DB_MAGIC */
    void		(*db_bpaddr)();	/* Breakpoint handler in debugger */
    __scunsigned_t	db_idbgbase;	/* Idbg table address in client */
    int 		(*db_printf)();	/* Debugger console printf routine */
    __scunsigned_t	db_brkpttbl;	/* breakpoints list for client use */
    db_flush_t		db_flush;	/* MP cache invalidate requests */
    __scunsigned_t	db_nametab;
    __scunsigned_t	db_symtab;
    int			db_symmax;	/* max # of symbols */
    int			(*db_conslock)();	/* kernel/symmon console lock*/
    void		(*db_excaddr)(); /* exception handler in debugger */
} db_t;

#endif	/* _LANGUAGE_C */

#define DB_MAGIC	0xfeeddead
#if (_MIPS_SZPTR == 32)
#define DB_BPOFF	4		/* offset to bp handler address */
#elif (_MIPS_SZPTR == 64)
#define DB_BPOFF	8		/* offset to bp handler address */
#define DB_EXCOFF	80		/* offset to bp handler address */
#endif

#if DEBUG
/* Define a hardware-only lock to share console access with symmon on
 * debug kernels. When symmon is present, we must coordinate hardware
 * access with symmon otherwise symmon may access the uart at the same
 * time as the kernel and cause console problems. If symmon hooks in a
 * console lock function vector in the SPB, call it in all places in
 * the kernel where we cannot tolerate symmon poking the console
 * hardware. This lock should not be used to protect per-port data
 * structures, nor should it be held for any longer than absolutely
 * necessary.
 * This lock function is called by the lower layer since only the
 * lower layer knows when the hardware needs to be protected.
 */
#define CONSLOCK_EXISTS						\
	(SPB->Signature == SPBMAGIC &&				\
	 SPB->DebugBlock &&					\
	 ((db_t*)SPB->DebugBlock)->db_magic == DB_MAGIC &&	\
	 ((db_t*)SPB->DebugBlock)->db_conslock)


/* debuglock_ospl is a global defined in the file where this macro is
 * being used. Since it is a global, we don't modify it until the lock
 * is acquired
 */
#define CONS_HW_LOCK(isconsole)				\
{							\
    if ((isconsole) && CONSLOCK_EXISTS) {		\
	int tmpospl = spl7();				\
	((db_t*)SPB->DebugBlock)->db_conslock(0);	\
	debuglock_ospl = tmpospl;			\
    }							\
}

#define CONS_HW_UNLOCK(isconsole)			\
{							\
    if ((isconsole) && CONSLOCK_EXISTS) {		\
	int tmpospl = debuglock_ospl;			\
	((db_t*)SPB->DebugBlock)->db_conslock(1);	\
	splx(tmpospl);					\
    }							\
}

#else
#define CONS_HW_LOCK(x)
#define CONS_HW_UNLOCK(x)
#endif

#endif
