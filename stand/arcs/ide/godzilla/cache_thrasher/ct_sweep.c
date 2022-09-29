/******************************************************************************
 *
 * Title:       ct_sweep.c
 * Author:      Mike Galles (ported to RACER by Georgeholio)
 * Date:        3/7/96
 *
 * This program calls ct_turbo with a variety of parameters.  
 * It sets up the array memories, forks all the processes,
 * and can itterate over several parameters.
 *
 * ct_turbo operates on one or more arrays.  ct_sweep sets up the
 * placement of those arrays.  They begin where alloc() returns, 
 * and continue contiguously up:
 *
 * +--------+ <-array_ptr[0]
 * |        |
 * |MAKE_KEY|
 * |        | <-array_size
 * |--------|
 * | unused |
 * +--------+ <-MAX_ARRAY_SIZE
 * +--------+ <-array_ptr[1]
 * |        |
 * |        |
 * |        |
 * |        |
 * |        |
 * +--------+
 * +--------+ <-array_ptr[2]
 * |        |
 * |        |
 * |        |
 * |        |
 * |        |
 * +--------+
 *     .
 *     .     
 *     .     
 * +--------+ <-array_ptr[MAX_ARRAYS-1]
 * |        |
 * |        |
 * |        |
 * |        |
 * |        |
 * +--------+
 * +--------+ <-uncached_ptr (just above last array_ptr)
 * |UNCACHED|
 * |        |
 * |        |
 * |        |
 * |        |
 * +--------+
 * +--------+ <-fetch_op_ptr (just above last uncached_ptr)
 * |FETCH&OP|
 * |        |
 * |        |
 * |        |
 * |        |
 * +--------+
 * +--------+ <-indirect_ptr (just above fetch_op_ptr)
 * |INDIRECT|
 * |        |
 * |        |
 * |        |
 * |        |
 * |        | (twice size of other arrays, since elements are 64 bit)
 * |        |
 * |        |
 * |        |
 * |        |
 * +--------+
 * +--------+ <-bte_ptr[0]
 * |        |
 * |        |
 * |        |
 * |        |
 * |        |
 * +--------+
 *     .
 *     .     
 *     .     
 * +--------+ <-bte_ptr[MAX_ARRAYS-1]
 * |        |
 * |        |
 * |        |
 * |        |
 * |        |
 * +--------+
 * +--------+ <- (stack limit just above last bte_ptr)
 * | STACK  |
 * | for    |
 * | Slave  |
 * |        |
 * |        | (stack_words sets size of this region)
 * |        |
 * |        |
 * |        |
 * +--------+ <- stack_ptr
 * 
 *****************************************************************************/

#include <sys/types.h>
#include <sys/immu.h>
#include <sys/cpu.h>
#include <limits.h>
#include <libsc.h>
#include <libsk.h>
#include <uif.h>

#include "d_ct.h"

/*#define OVERLAP_TEXT 1 <- ***uncomment to overlap text & array[0]****/

#ifdef SABLE /* smaller arrays for SABLE */
#define MAX_ARRAYS 10
#define MAX_ARRAY_SIZE 0x10000
#define MAX_BTE_ARRAYS 10
#define MAX_BTE_ARRAY_SIZE 0x1000
#else  /* real hardware: */
#define MAX_ARRAYS 50
#define MAX_ARRAY_SIZE 0x80000
#define MAX_BTE_ARRAYS 10
#define MAX_BTE_ARRAY_SIZE 0x1000
/*
#define MAX_ARRAYS 10          
#define MAX_ARRAY_SIZE 0x10000
#define MAX_BTE_ARRAYS 10
#define MAX_BTE_ARRAY_SIZE 0x1000
*/
#endif /* SABLE */

#define RANDOM_LOOPS 64
#define DEBUG_LEVEL 10
#define PRINT_FREQ 50

extern int date_cmd(int, char **);

extern int cpuid(void);

int num_cpus;     /* total number of cpus participating */
int master_pid;
int init_seed;
int err_code;

int master_quiet;   /* master does not participate */
int * num_arrays;   /* number of arrays used */
int * num_bte_arrays; /* number of BTE arrays */
int * array_size;   /* sizeof arrays used, in words */
int * array_ptr[MAX_ARRAYS];  /* pointer to start of each array */
int * uncached_ptr; /* points to uncached array above arrap_ptr[MAX] */
long * fetch_op_ptr; /* points to fetch & op array above uncached array */
int * fetch_op_stride; /* fetch & op stride */
int * use_fetch_op; /* use fetch & op space */
int * bte_ptr[MAX_BTE_ARRAYS];      /* pointer to BTE arrays */
long long * indirect_ptr; /* points to indirect array above fetch&op */
void * stack_base; /* points to base of stack for slave */
int stack_words;    /* size of slave stack space, in words */
void * stack_ptr; /* initial "stack pointer" for slave */
int * stride;       /* stride between adj. reads (in words) */
int * max_iter;     /* number of iterations through the array */
int * force_wb;     /* force writebacks at this frequency */
int * write_text;   /* invalidate text in other CPUs at freq. */
int * use_uncached; /* use uncached array during loops */
int * stripe_arrays;/* stripe arrays during reads */
int * reverse_odd;  /* odd PIDs read/write arrays last to first */
int * use_sc;       /* use store conditional during writer */
int * indirect;     /* use indirection to access arrays */
int * indirect_inv; /* invalidate indirect array */
int * use_float;    /* access array with float ld/st */
int * write_local;  /* write to local hub registers during loop */
int * read_local;   /* read local hub registers during loop */
int * read_vector;  /* read vector register during loop */
int debug_level;    /* type and freq. of debug messages */
int * print_freq;   /* frequency each process prints it iter */
int * led_freq;     /* frequency each process flashes LED */
int * brakes;       /* global flag to stop due to error */
int * bar_busy;     /* shared barrier busy flag */
int * bar_cnt;      /* shared barrier count */

int * printlock;
int array_stride;
int bte_array_stride;
volatile int ip30_slave_release = 0;

#define	SHOW(v)		do { if (debug_level >= 9) msg_printf(DBG, "0x%X  " #v "\n", v); } while (0)
#define	SHOWA(v,i)	do { if (debug_level >= 9) msg_printf(DBG, "0x%X  " #v "[%d]\n", v[i], i); } while (0)

#define	UL(v)		((unsigned long)(v))
#define	ALIGNDN(t,m,b)	((t)(UL(m) &~ UL(b)))
#define	ALIGNUP(t,m,b)	ALIGNDN(t,UL(m) + UL(b), b)
#define	SPACE(t,s,p)	((t)((UL(s)<<56) | (UL(p) & 0x0000FFFFFFFFFFFFul)))

ip30_slave_wait(void)
{
  msg_printf(DBG, "cpu %d spinning in ip30_slave_wait\n",cpuid());

  while (!ip30_slave_release)
    /* spin */;

  msg_printf(DBG, "cpu %d done spinning, calling ct_go\n",cpuid());

  ct_go();
  return 0;
}


/* 
 * IDE entry pt
 */ 
int
cache_thrasher(int argc, char** argv) {

  int i;
  int * memory; /* points to our big chunk of memory */

  ip30_slave_release = 0;
  if (cpuid() > 0) {
    ip30_slave_wait();
    return 0;			/* should never exit */
  }

  debug_level = 0;		/* default: no debug */
  err_code = 0;			/* clear the error code */
  init_seed = 0x42424242;	/* default: standard prng seed */
  stack_words = 256 * 512;	/* default: 512kbyte slave stack */

#if 0
  /* run cached */
  run_cached();
#endif

#if ORIG_ARGC
  if (argc > 1)
    debug_level = atoi(argv[1]);
  if (argc > 2)
    atohex(argv[2],&init_seed);
  if (argc > 3)
    stack_words = 256 * atoi(argv[3]);
#else
  debug_level = atoi(argv[1]);
#endif

  msg_printf(SUM, "Cache Thrasher Sweep Test\n");
  msg_printf(DBG, "debug_level = %d\n", debug_level);

#if 0
  msg_printf("Running Cached\n");
#endif

  if (debug_level >= 14) 
    msg_printf(DBG, "Config Register = 0x%X\n",r4k_getconfig());

#ifdef SABLE
  msg_printf("***ct_sweep running under SABLE parameters!***\n");
#endif

/*******************
 * Allocate global memory:
 *******************/

SHOW(&memory);

  /* malloc to get low memory pointer, never free: */
  memory = (int *) K1_TO_K0( malloc(MAX_ARRAY_SIZE) );

SHOW(memory);

  /* address assignments for shared variables: */
  printlock		= memory++;
  brakes		= memory++;
  bar_busy		= memory++;
  bar_cnt		= memory++;
  num_arrays		= memory++;
  num_bte_arrays	= memory++;
  array_size		= memory++;
  stride		= memory++;
  max_iter		= memory++;
  force_wb		= memory++;
  write_text		= memory++;
  use_uncached		= memory++;
  fetch_op_stride	= memory++;
  use_fetch_op		= memory++;
  stripe_arrays		= memory++;
  reverse_odd		= memory++;
  use_sc		= memory++;
  indirect		= memory++;
  indirect_inv		= memory++;
  use_float		= memory++;
  write_local		= memory++;
  read_local		= memory++;
  read_vector		= memory++;
  print_freq		= memory++;
  led_freq		= memory++;

  *printlock = 1;    /* init shared locks */
  *bar_busy = 0;
  *bar_cnt = 0;
  *brakes = 0;

SHOW(bar_busy);
SHOW(bar_cnt);
SHOW(num_arrays);

#ifdef PHYS_CHECK_LO	/* XXX- except PHYS_CHECK_LO is undefined ... */
  memory = (int *)PHYS_TO_K0(PHYS_CHECK_LO);
SHOW(memory);
#elif IP30	/* XXX- should use PHYS_CHECK_LO if it were available */
  memory = (int *)PHYS_TO_K0(PHYS_RAMBASE + 0x00C00000);
SHOW(memory);
#else
  /* align to next cache line */
  memory += 32;	/* make sure we get to next cache line */
  memory = ALIGNUP(int *,memory,0x7F);
  /*
   * NOTE: You are likely to overwrite stuff that you
   * would rather not overwrite. On my
   * system-under-test, "memory" here points about half
   * a megabyte below the stack pointer, so this code
   * could overlap the stack with the test data area,
   * with less than pleasant results.
   */
#endif

#ifdef OVERLAP_TEXT 
  {
    unsigned long	ma = (unsigned long)memory;
    unsigned long	wa = (unsigned long)writer;
    if ((ma & 0x7FFFF) > (wa & 0x7FFFF)) ma += 0x80000;
    ma = ((~0x7FFFF & ma) | (0x7FFFF & wa));
    memory = (int *)ma;
  }
#endif /*OVERLAP_TEXT*/

  array_stride = (MAX_ARRAY_SIZE / 4); /* array_stride is int * */
  
  for(i=0; i<MAX_ARRAYS; i++) {	/* assign pointers to beginnings of arrays */
    array_ptr[i] = memory;
    memory += array_stride;
SHOWA(array_ptr,i);
  }

  /* place uncached array above last array */
  /* 0x96000000 00000000 is uncached */
  uncached_ptr = SPACE(int *, 0x96, memory);
  memory += array_stride;
SHOW(uncached_ptr);

  /* place fetch&op array above uncached array */
  /* 0x94000000 00000000 is uncached */
  fetch_op_ptr = SPACE(long *, 0x94, memory);
  memory += array_stride;

SHOW(fetch_op_ptr);


  /* place indirect pointer above fetch&op */
  /* aligned to doubleword */
  memory = ALIGNUP(int *, memory, 7);
  indirect_ptr = (long long *) memory;
  memory += 2 * array_stride;

SHOW(indirect_ptr);

  bte_array_stride = (MAX_BTE_ARRAY_SIZE / 4); 

  for(i=0; i<MAX_BTE_ARRAYS; i++) {
    bte_ptr[i] = memory;
    memory += bte_array_stride;
SHOWA(bte_ptr,i);
  }

  memory = ALIGNUP(int *, memory, 16383);
  stack_base = (void *) memory;
  memory += stack_words;
  stack_ptr = (void *) (memory - 32);

SHOW(stack_base);
SHOW(stack_ptr);

  num_cpus = cpu_probe();

  if (debug_level >= 5)
    msg_printf(DBG, "number of cpus = %d\n",num_cpus);

  msg_printf(DBG, "Setting up Heart Trigger\n");
  setup_heart_trigger();

  /* master pid == master cpu == 0*/
  master_pid = 0;

  /* launch slave */
  if (num_cpus > 1) {
    Lock(printlock);
    msg_printf(DBG, " ===========================================================\n");
    msg_printf(SUM, "launching slave ...\n");
    Unlock(printlock);
#ifdef NOTDEF_LAUNCH_SLAVE
    launch_slave(cpuid()+1,
		 (void (*)(void *))ct_go, NULL,
		 (void (*)(void *))ct_exit, NULL,
		 stack_ptr);
#endif
  } 
  
  /* master */
  Lock(printlock);
  msg_printf(SUM, "launching master ...\n");
  Unlock(printlock);
  ip30_slave_release = 1;
  ct_go();

  if (err_code) {
  	msg_printf(SUM, "\nCache Thrasher Sweep Test Failed\n");
  } else {
  	msg_printf(SUM, "\nCache Thrasher Sweep Test Passed\n");
  }
  return (err_code);
}

int major_loop;
void 
ct_go()
{

  int fetch_op_index;
  int fetch_op_expect;
  int fetch_op_actual;
  int seed;


  if(debug_level >= 5) {
    Lock(printlock);
    msg_printf(DBG, "My PID is %d\n", cpuid());
    Unlock(printlock);
  }

/********************
 * randomize parameters
 ********************/

  if(cpuid() == master_pid) {
    seed = init_seed;
    Lock(printlock);
    msg_printf(DBG, "Pick initial random seed of 0x%x.\n", seed);
    Unlock(printlock);
#ifdef CT_SPINNER
    start_spinner( 1 );
#endif	/* CT_SPINNER */
  }


  for(major_loop=0; major_loop < RANDOM_LOOPS; ) {

    if(cpuid() == master_pid) {
	major_loop++;
    }

    if(cpuid() == master_pid) {
      if (debug_level >= 2) {
	Lock(printlock);
	msg_printf(DBG, "----------------------------------------------\n");
	msg_printf(DBG, "Randomizing ct_turbo parameters, loop %d of %d\n", 
	       major_loop,  RANDOM_LOOPS);
	/* date_cmd(1, NULL); */
	msg_printf(DBG, "using random seed of 0x%x.\n", seed);
	Unlock(printlock);
      }
#ifdef CT_SPINNER
      else {
	spinner();
      }
#endif	/* CT_SPINNER */

      *num_arrays    = my_random(1, 5, &seed);
      *array_size    = my_random(0x1000, 0x10000, &seed) & ~0x7f;
      *stride        = my_random(1, 21, &seed);
      *max_iter      = my_random(500, 1000, &seed) +
                                 (0x20000 / *array_size) / *num_arrays;
      *force_wb      = my_random(0, 1, &seed) * my_random(1, 21, &seed);
      *write_text    = my_random(0, 1, &seed) * my_random(5, 40, &seed);
      *use_uncached  = my_random(0, 0, &seed);
      *stripe_arrays = my_random(0, 0, &seed);
      *reverse_odd   = my_random(0, 1, &seed);
      *use_sc        = (my_random(0, 100, &seed) < 40);
      *use_fetch_op  = my_random(0, 0, &seed); /* fetch-op for now */
      *fetch_op_stride = my_random(1, 2, &seed) * 8;
      *write_local   = (my_random(0, 100, &seed) < 25);
      *read_local    = (my_random(0, 100, &seed) < 25);
      *read_vector   = (my_random(0, 100, &seed) < 50);
#if MASTER_QUIET_RAND
      master_quiet   = (my_random(0, 100, &seed) < 25);
#else
      master_quiet   = 0; /* never quiet */
#endif
				/* unfinished features: */
      *indirect      = 0;
      *indirect_inv  = 0;
      *use_float     = 0;
      *num_bte_arrays = 0;

      *print_freq = ((PRINT_FREQ / (*num_arrays + (*use_sc > 0))) * 
		     (*stride) *  (0x80000 / (*array_size)));
      *led_freq = *print_freq / 8;

#ifdef SABLE
      *print_freq = 1;
      *max_iter = 5;
      *array_size = 0x100;
#endif

      if(debug_level >= 5) {
	Lock(printlock);
	msg_printf(DBG, "print_freq = 0x%X\n",*print_freq);
	msg_printf(DBG, "max_iter = 0x%X\n",*max_iter);
	msg_printf(DBG, "array_size = 0x%X\n",*array_size);
	Unlock(printlock);
      }
    }

    if(*use_fetch_op) { /* initialize fetch & op space */
      fetch_op_index = *fetch_op_stride * cpuid();
      SPACE(long *, 0x96, fetch_op_ptr)[fetch_op_index] =
	0xFFFFFFFFFFFFFFFFul;
      if(*(fetch_op_stride) == 0) fetch_op_expect = 0xBABE;
      if(*(fetch_op_stride) != 0) fetch_op_expect = cpuid();
      fetch_op_ptr[fetch_op_index] = fetch_op_expect;
    }

    if(debug_level >= 5) {
      Lock(printlock);
      msg_printf(DBG, "PID %d of %d calling barrier.\n", cpuid(), num_cpus);
      Unlock(printlock);
    }

    barrier(num_cpus, bar_busy, bar_cnt); /* sync everybody up */

    if(debug_level >= 5) {
      Lock(printlock);
      msg_printf(DBG, "PID %d through barrier.\n", cpuid());
      Unlock(printlock);
    }

    if (
    ct_turbo (master_pid, cpuid(), master_quiet, num_cpus, 
	      *num_arrays, *num_bte_arrays, *array_size, 
	      array_ptr, uncached_ptr, fetch_op_ptr, 
	      *fetch_op_stride, *use_fetch_op,
	      indirect_ptr, bte_ptr,
	      *stride, *max_iter,
	      *force_wb, *write_text, *use_uncached,
	      *stripe_arrays, *reverse_odd, *use_sc,
	      *indirect, *indirect_inv, *use_float,
	      *write_local, *read_local, *read_vector,
	      debug_level, *print_freq, *led_freq,
	      brakes, 
	      bar_busy, bar_cnt, printlock)
    ) {
      printf("ct_turbo on cpu %d didn't return 0\n",cpuid());
      return;
    }

    barrier(num_cpus, bar_busy, bar_cnt); /* sync everybody up */

    if(*use_fetch_op) { /* check fetch & op space */
      fetch_op_actual = fetch_op_ptr[fetch_op_index];
      if(fetch_op_actual != fetch_op_expect) {
	Lock(printlock);
	msg_printf(DBG, "Fetch & Op did not return %d!  Read %d.\n", 
	       fetch_op_expect, fetch_op_actual);
	ct_exit();
	return;
      }
    }
  
    if(cpuid() == master_pid) {
      if (debug_level >= 1) {
	Lock(printlock);
	msg_printf(VRB, "%d ",major_loop); 
	busy(1);
	Unlock(printlock);
      }
      if (debug_level >= 2) {
	Lock(printlock);
	msg_printf(DBG, "Random parameter ct_turbo complete.\n");
	msg_printf(DBG, "----------------------------------------------\n");
	Unlock(printlock);
      }
    }
  }

#ifdef CT_SPINNER
  if(cpuid() == master_pid) {
    end_spinner();
  }
#endif	/* CT_SPINNER */

  Lock(printlock);
  msg_printf(DBG, "pid %d completing ct_sweep program\n", cpuid());
  Unlock(printlock);

}


/********************
 * my_random function:
 ********************/

int my_random(int min, int max, int *seed)
{
  int val;

  val = *seed;		/* make next random # */
  val = val << 1 | (val >> 30 ^ val >> 27) & 1;
  *seed = val;

  val = (val % (1 + max - min));

  if(val<0) val = val * -1;

  val = val + min;

  return (val);

}


