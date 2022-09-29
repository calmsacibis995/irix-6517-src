
#ifndef __SYS_IDBGENTRY_H__
#define __SYS_IDBGENTRY_H__

union rval;
struct idbgcomm;
struct idbgfssw;
struct mrlock_s;
struct pfdat;
extern int idbg_addfunc(char *, void (*)());
extern void idbg_delfunc(void (*)());
extern int idbg_copytab(caddr_t, int, union rval *);
extern void idbg_tablesize(union rval *);
extern int idbg_dofunc(struct idbgcomm *, union rval *);
extern void idbg_switch(int, int);
extern void qprintf(char *, ...);
extern void _prvn(void *, int);
extern char *fetch_kname(void *, void *);
extern __psunsigned_t roundpc(inst_t *pc);
extern void idbg_addfssw(struct idbgfssw *);
extern void idbg_delfssw(struct idbgfssw *);
extern void printflags(uint64_t, char **, char *);
extern char *padstr(char *, int);
extern void pmrlock(struct mrlock_s *);
extern void prsymoff(void *, char *, char *);
extern void idbg_btrace(int);
extern void idbg_dopfdat1(struct pfdat *);
extern unsigned long long strtoull(const char *, char **, int);

/*
 * idbg_doto_uthread: call pfunc on all uthreads associated with 'p1'.
 * If p1 is uthread address, calls pfunc on that uthread;
 * If p1 is -1, calls pfunc on curuthread (if it exists);
 * If p1 is a valid pid, calls pfunc on all that proc's uthreads.
 * Otherwise, fail.
 *
 * All: stop after first uthread if p1 is pid;
 * Must: call pfunc(p1) anyway if it doesn't match uthread/pid/-1
 */
struct uthread_s;

int
idbg_doto_uthread(__psint_t p1, void (*pfunc)(struct uthread_s *), int all, int must);

/* some other functions that various idbg modules might want to share */
struct region;
struct proc;
union pde;
struct anon;
struct pm;
struct k_mrmeter;
extern void shortproc(struct proc *, int);
extern void idbg_get_stack_bytes(struct uthread_s *, void*, void*, int);
extern void idbg_region(__psint_t);
extern void idbg_asonswap(struct proc *);
extern void idbg_aspmfind(struct proc *, struct pm *);
extern int  idbg_procscan(int (*)(struct proc *, void *, int), void *);
#define DUMP_PFDAT	0x1
extern int idbg_dopde(union pde *, off_t, int);
extern struct anon *find_anonroot(struct anon *);
extern void print_aswaptree(struct anon *, struct region *);
extern struct k_mrmeter *idbg_mrmeter(struct mrlock_s *);
extern void idbg_mrlock(struct mrlock_s *);

#endif /* __SYS_IDBGENTRY_H__ */
