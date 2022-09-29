/* ct_sweep.c */
int cache_thrasher(int argc, char** argv);
void ct_go(void);
int my_random(int min, int max, int *seed);

/* ct_turbo.c */
int ct_turbo (
	      int master_pid,   /* pid for master process */
	      int pid,		/* pid for this process */
	      int master_quiet, /* shut down master read/write */
	      int num_cpus,	/* total number of cpus participating */
	      int num_arrays,	/* number of arrays used */
	      int num_bte_arrays, /* number of BTE arrays */
	      int array_size,	/* sizeof arrays used, in words */
	      int * array_ptr[], /* pointer to start of each array */
	      int * uncached_ptr, /* points to uncached array above others */
	      long * fetch_op_ptr, /* points to fetch&op array */
	      int fetch_op_stride, /* fetch & op stride */
	      int use_fetch_op,  /* use fetch & op space */
	      long long * indirect_ptr, /* points to indirect array */
	      int * bte_ptr[],  /* pointer to start of each bte array */
	      int stride,	/* stride between adj. reads (in words) */
	      int max_iter,	/* number of iterations through the array */
    
	      int force_wb,	/* force writebacks at this frequency */
	      int write_text,	/* invalidate text in other CPUs at freq. */
	      int use_uncached, /* use uncached array */
	      int stripe_arrays, /* stripe arrays during reads */
	      int reverse_odd,  /* odd PID's read/write arrays last to first */
	      int use_sc,       /* use store conditional during writer */
	      int indirect,	/* use indirection to access arrays */
	      int indirect_inv,	/* invalidate indirect array */
	      int use_float,	/* access array with float ld/st */
	      int write_local,  /* write local registers during loop */
	      int read_local,   /* read local registers during loop */
	      int read_vector,  /* read vector during loop */

	      int debug_level,	/* type and freq. of debug messages */
	      int print_freq,	/* frequency each process prints it iter */
	      int led_freq,     /* frequency each CPU flashes LEDs */

	      int * brakes,	/* global flag to stop due to error */
	      int * bar_busy,	/* shared barrier busy flag */
	      int * bar_cnt,	/* shared barrier count */
	      int * printlock   /* semaphore for protecting print buf */
	      );
int writer(int iter, int pid, int *ptr, int proc_stride, int nlocs,
	   int array, int force_wb, int write_text, int use_sc,
	   int use_float, int write_local, int read_local, int read_vector,
	   int use_fetch_op, long * fetch_op_ptr,
	   int debug_level, int * printlock, int *brakes,
	   int num_cpus, int * bar_busy, int * bar_cnt);
int reader(int iter, int pid, int *ptr, int proc_stride, 
	   int nlocs, int array, int indirect, long long * indirect_ptr, 
	   int indirect_inv,
	   int use_fetch_op, long * fetch_op_ptr,
	   int *brakes, int * printlock);

/* ct_misc_lib.c */
int dummy (int i);
void ct_delay(int delay);
void ct_exit(void);
void setup_heart_trigger(void);
void heart_trigger(int);
void print_heart_exception_cause_reg(void);

/* ct_libmp.s */
void barrier(int,void*,void*);
void store_sc(int*ptr, int data);

/* ct_lock.s */
int Lock(int *);
int Unlock(int *);
