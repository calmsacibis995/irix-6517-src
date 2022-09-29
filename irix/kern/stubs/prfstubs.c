
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/par.h>
#include <sys/kthread.h>

/*
 * Fawlty Stubs
 */

prfinstalled(){ return 0; }
void prfinit(void){}
/* ARGSUSED */
void prfintr(__psunsigned_t pc, __psunsigned_t sp,
	     __psunsigned_t ra, k_machreg_t sr) {}
/* ARGSUSED */
unsigned int
prfstacktrace(__psunsigned_t pc, __psunsigned_t sp, __psunsigned_t ra,
	      __psunsigned_t *stack, unsigned int nsp)
{
	stack[0] = pc;
	return 1;
}

/* ARGSUSED */
void fawlty_sys(short callno, struct sysent *callp, sysarg_t *argp){}
/* ARGSUSED */
void fawlty_sysend(short callno, struct sysent *callp,  sysarg_t *argp, __int64_t retvalue, __int64_t ret2, int error){}
/* ARGSUSED */
void fawlty_exec(const char *name){}
/* ARGSUSED */
void fawlty_resched(kthread_t* kt, int flags){}
/* ARGSUSED */
void fawlty_disk(int event,struct buf *bp){}
/* ARGSUSED */
void fawlty_fork(int64_t pkid, int64_t ckid, const char* fname) {}
