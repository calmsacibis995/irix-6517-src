#ifndef _COORD_H_
#define _COORD_H_

/* From brkpt.c */
extern void		_pdbx_clean(int);

/* From rdebug.c */
extern volatile int	netrdebug;
extern volatile int	singlestepping;
extern volatile int	kprinting;
extern volatile int 	shouldexit;
extern volatile int	nwkopen;
extern volatile int 	keepnwkdown;

/* From idbg.c */
extern unsigned long 	upcall(char *fname, char *);
extern int		isidbg(void);

/* From coord.c */ 
extern void 		enter_rdebug_loop(int);
extern void		leave_rdebug_loop(int);
extern volatile int	ncpus;

/* From parser.c */
extern void 		execute_command(struct cmd_table *, char *);

#ifdef MULTIPROCESSOR
/* From rdebug.c */
extern volatile int 	inrdebug; /* if in rdebug, then interacting with user */
extern int 		protocol_debug;

/* From coord.c */
extern volatile int 	symmon_owner;
extern lock_t 		symmon_owner_lock;
extern inst_t 		*brkptaddr[];
extern volatile CpuState cpustate[];
extern void 		setcpustate(int cpuid, CpuState s);
extern CpuState 	getcpustate(int);
extern char 		*itocs(CpuState s);
extern int 		waitedtoolong(register int *count, int limit);
extern int 		safely_check_zingbp(void);
#endif /* MULTIPROCESSOR */

#endif /* _COORD_H_ */
