#ifndef __SYS_KABI_H__
#define __SYS_KABI_H__


typedef struct v_mapent {
        caddr_t start;
        caddr_t end;
        int     mode;   /* always map at 0? */
} v_mapent_t;

typedef struct proc_handl {
	int dummy;
} proc_handl_t;

typedef struct shaddr_handl {
	int dummy;
} shaddr_handl_t;

typedef struct thread_handle {
	int dummy;
} * thread_handle_t;
typedef struct thread_group_handle {
	int dummy;
} * thread_group_handle_t;


/* flag values for pid_signal */
#define PID_SIGNAL_NOSLEEP	0x1	/* no sleeping permitted - signal
					 * might be delayed in getting sent
					 */

/*
 * Macros and defines to find out the user's ABI
 * These have been moved here from proc.h so kernel modules which do
 * not include proc.h but include xlate.h, which needs these defines,
 * can compile.
 */
/* If you add new values, make sure they can fit in their field
 * in struct proc - currently at 8 bits! */
#define ABI_IRIX5	0x02		/* an IRIX5/SVR4 ABI binary */
#define ABI_IRIX5_64 	0x04		/* an IRIX5-64 bit binary */
#define ABI_IRIX5_N32 	0x08		/* an IRIX5-32 bit binary (new abi) */

#define ABI_IS(set,abi)		(((set) & (abi)) != 0)
#define ABI_IS_IRIX5(abi)	(ABI_IS(ABI_IRIX5, abi))
#define ABI_IS_IRIX5_N32(abi)	(ABI_IS(ABI_IRIX5_N32, abi))
#define ABI_IS_IRIX5_64(abi)	(ABI_IS(ABI_IRIX5_64, abi))
#define ABI_IS_64BIT(abi)	(ABI_IS(ABI_IRIX5_64, abi))
#define ABI_HAS_8_AREGS(abi)	(ABI_IS(ABI_IRIX5_N32|ABI_IRIX5_64, abi))
#define ABI_HAS_64BIT_REGS(abi)	(ABI_IS(ABI_IRIX5_N32|ABI_IRIX5_64, abi))

extern int curabi_is_irix5_64(void);

struct proc;
extern int procscan(int ((*)(struct proc *, void *, int)), void *);

struct k_siginfo;
extern char *get_current_name(void);
extern uint64_t get_thread_id(void);
extern pid_t proc_pid(proc_handl_t *);
extern char *proc_name(proc_handl_t *);
extern void sysinfo_outch_add(int);
extern void sysinfo_rawch_add(int);
extern void force_resched(void);
extern void set_triggersave(int);
extern void curthreadgroup_scan(int (*)(thread_handle_t, void *), void *);
extern thread_handle_t curthreadhandle(void);
extern thread_group_handle_t threadhandle_to_threadgrouphandle(thread_handle_t);
extern int threadgroup_refcnt(thread_handle_t thd);
extern proc_handl_t *threadhandle_to_proc(thread_handle_t);
extern proc_handl_t *curproc(void);
extern void *get_cursproc_shaddr(void);
extern int thread_issproc(thread_handle_t);
extern void setpdvec_curproc(int);
extern void clearpdvec_curproc(int);
extern int curproc_vtop(caddr_t, int, int *, int);
extern int curproc_vtopv(caddr_t, int, int *, int, int, int);
extern char curproc_abi(void);
extern void setgfxsched_thread(thread_handle_t);
extern void cleargfxsched_thread(thread_handle_t);
extern int processors_configured(void);
extern void gfx_waitc_on(void);
extern void gfx_waitc_off(void);
extern void gfx_waitf_on(void);
extern void gfx_waitf_off(void);
extern int curuthread_set_rtpri(int);
extern void psignal(proc_handl_t *, int);
extern void pid_signal(pid_t, int, int, struct k_siginfo *);
#ifdef R10000_SPECULATION_WAR
extern void unmaptlb_range(caddr_t, size_t);
#endif
extern int kabi_alloc_pages(paddr_t *, pgcnt_t, int, void *, int);
extern void  kabi_free_pages(paddr_t *, int);
extern pgno_t   kabi_num_lockable_pages(void);

/*
 * The following are all obsolete, and have been replaced with a new
 * interface defined in ddmap.h
 */
extern int v_mapkvsegment(void *, caddr_t, void *, pgno_t, int, __psunsigned_t);
extern int v_mapsegment(void *, caddr_t, unsigned, pgno_t, int, __psunsigned_t);
extern int chk_dmabuf_attr(char *, int, int);

#endif /* __SYS_KABI_H__ */
