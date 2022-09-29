#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/include/RCS/trace.h,v 1.1 1999/05/25 19:19:20 tjm Exp $"

#define STACK_SEGMENTS 4

typedef struct function_rec_s {
	struct function_rec_s	*next;
	kaddr_t					 lowpc;
	int						 func_size;
	int						 frame_size;
} function_rec_t;

#define INCLUDE_REGINFO 1
/*
*/

#ifdef INCLUDE_REGINFO

#define IS_AREG(r) ((r >= 4) && (r <=  11))
#define IS_SREG(r) (((r >= 16) && (r <= 23)) || (r == 30))

#define REGVAL_UNKNOWN	0
#define REGVAL_VALID	1
#define REGVAL_SAVEREG	2  /* A-reg moved to S-reg (S-reg number in savereg) */
#define REGVAL_BAD	    4  /* Value loaded into register before it was saved */

/* Register record
 */
typedef struct reg_rec {
	uint	state;
	uint	savereg;
	uint64	value;	
} reg_rec_t;
#endif

/* Stack frame
 */
typedef struct sframe_rec {
	struct sframe_rec   *next;
	struct sframe_rec   *prev;
	int					 flag;
	int     			 level;
	char				*funcname;
	char				*srcfile;
	int					 line_no;
	kaddr_t    		 	 sp;
	kaddr_t    		 	 pc;
	kaddr_t    		 	 ra;
	uint			 	*asp;
	int     			 frame_size;
	kaddr_t				 ep;
	k_ptr_t    		 	 eframe;
	int					 ptr;
	k_uint_t			 error;
#ifdef INCLUDE_REGINFO
	reg_rec_t			 regs[32];
#endif
} sframe_t;

/* Stack segment structure
 */
struct stack_s {
	int		 	type;
	uint        size;
	kaddr_t		addr;
	uint        *ptr;
};

/* Stack trace header
 */
typedef struct trace_rec {
	int				flags;
	kaddr_t		 	kthread;
	k_ptr_t		 	ktp;
	kaddr_t			kthread2;	/* for when ithread jumps to other kthread */
	k_ptr_t			ktp2;
	kaddr_t			exception;
	k_ptr_t			exp;
	kaddr_t			u_eframe;
	k_ptr_t		 	pdap;
	kaddr_t		 	intstack;
	kaddr_t		 	bootstack;
	struct stack_s	stack[STACK_SEGMENTS];
	int			 	stackcnt;
	sframe_t 	   *frame;
	int			 	nframes;
#ifdef INCLUDE_REGINFO
	reg_rec_t		regs[32]; /* Beginning reg values (if can be dtermined) */
#endif
} trace_t;

/* Flags that can be set in trace_rec_s
 */
#define TF_TRACEREC_VALID  0x01 /* The trace_rec_s has been setup already!   */
#define TF_KTHREAD2_VALID  0x02 /* The trace_rec_s has second kthread added  */
#define TF_BAD_KTHREAD	   0x04	/* may be necessary when doing an NMI ctrace */
#define	TF_SUPPRESS_HEADER 0x08 /* Suppress header output from trace cmds    */
#ifdef INCLUDE_REGINFO
#define	TF_GET_REGVALS     0x10 /* Suppress header output from trace cmds    */
#endif

/* Stack types (and locations of eframes)
 */
#define U_EFRAME 		0
#define USERSTACK 		0
#define KERNELSTACK 	1
#define INTSTACK 		2
#define BOOTSTACK 		3
#define KEXTSTACK 		4
#define ITHREADSTACK	5
#define STHREADSTACK	6
#define XTHREADSTACK	7
#define RAW_STACK       8
#define UNKNOWN_STACK	9

/* Flags that determine how a trace record is setup
 */
#define KTHREAD_FLAG  	1
#define KTHREAD2_FLAG  	2
#define CPU_FLAG		3
#define RAW_FLAG		4

/* Stack frame updating macro
 */
#define UPDATE_FRAME(FUNCNAME, PC, RA, SP, ASP, SRCNAME, LINE_NO, SIZE) \
			curframe->funcname = FUNCNAME; \
			curframe->pc = PC; \
			curframe->sp = SP; \
			curframe->ra = RA; \
			curframe->asp = ASP; \
			curframe->srcfile = SRCNAME; \
			curframe->line_no = LINE_NO; \
			curframe->frame_size = SIZE; \
			curframe->ptr = curstkidx; \
			kl_enqueue((element_t **)&trace->frame, (element_t *)curframe); \
			trace->nframes++; \

#define BUMP_WORDP(W) \
		if (PTRSZ64(K)) { \
			(W) = (uint*)((uint)(W) + 8); \
		} \
		else { \
			(W)++; \
		}

#define IS_TEXT(addr) get_funcaddr(addr)

/* Trace function prototypes
 */
sframe_t *alloc_sframe(trace_t *);
void free_sframes(trace_t *);
trace_t *alloc_trace_rec();
void free_trace_rec(trace_t *);
void clean_trace_rec(trace_t *);
int setup_trace_rec(kaddr_t, kstruct_t *, int, trace_t *);
int setup_kthread_trace_rec(kaddr_t, int, trace_t *);
int setup_cpu_trace_rec(kaddr_t, trace_t *);
int stack_index(kaddr_t, trace_t *);
int add_stack(kaddr_t, int, int, trace_t *);
int stack_type(kaddr_t, trace_t *);
int stack_size(int, trace_t *);
kaddr_t stack_addr(kaddr_t, trace_t *);
kaddr_t kthread_stack(trace_t *, int *);
int is_kthreadstack(kaddr_t, trace_t *);
kaddr_t _get_kthread_stack(kaddr_t, int*);
short get_frame_size(kaddr_t, int);
int get_ra_offset(kaddr_t, int);
int is_exception(char *);
int find_trace(kaddr_t, kaddr_t, kaddr_t, kaddr_t, trace_t *, int);
int find_trace2(kaddr_t, kaddr_t, kaddr_t, kaddr_t, trace_t *, int);
int find_eframe(trace_t *, int);
int cpu_trace(int, int, FILE *);
int get_cpu_trace(int, trace_t *);
int print_valid_traces(kaddr_t, trace_t *, int, int, FILE *);
int do_list(kaddr_t, trace_t *, FILE *);
int eframe_trace(kaddr_t, trace_t *, int, FILE *);
int proc_trace(trace_t *, int, FILE *);
int proc_trace(trace_t *, int, FILE *);
int print_trace(trace_t *, int, FILE *);
void dump_stack_frame(trace_t *, sframe_t *, FILE *);
void print_eframe(k_ptr_t, FILE *);
void print_trace_error(trace_t *, FILE *);
int print_cpu_status(int, FILE *);
