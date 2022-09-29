/******************************************************************************
 *
 * Title:       ct_turbo.c
 * Author:      Mike Galles
 * Date:        3/7/96
 *
 * This program is designed to be called from a supervisor program 
 * which provides parameters for operation.  The basic idea of thies
 * program is to create a variety of system conditions which stress
 * cache coherency and memory traffic.  Conflict cases, false sharing, 
 * and unusual system interface combinations are encouraged.
 *
 *****************************************************************************/

#include <sys/types.h>
#include <libsc.h>
#include <uif.h>

#include "d_ct.h"

    /* UNROLL defines can be 1, 4, 8, or 16 ONLY! */
#if ORIG
#define READ_UNROLL 16
#define WRITE_UNROLL 16
#else
#define READ_UNROLL 8
#define WRITE_UNROLL 8
#endif

#define MAX_LOCAL_ADDR     4

#define OLD_KEY 1

/********************************************
 * MAKE_KEY created unique data to write to
 * a given address.  This data is unique to
 * the address, CPU ID, iteration number, and
 * stride.  If a failure is detected, you
 * can see where and when the failed data 
 * came from.
 ********************************************/

#define MAKE_KEY(_iter,_id,_l,_stride,_array)	(((_id & 0xf)     <<28)+ \
						 ((_iter & 0xf)   <<24)+ \
						 ((_stride & 0xf) <<20)+ \
						 ((_array & 0xf)  <<16)+ \
						 ((_l & 0xffff)))
#if 0
#define MAKE_KEY(_iter,_id,_l,_stride,_array)	(_id?0xffffffff:0xeeeeeeee)
#endif

#define PRINT_KEY(key) { msg_printf(DBG, "id=0x%X iter=0x%X stride=0x%X array=0x%X l=0x%X",(key&0xf0000000)>>28,(key&0x0f000000)>>24,(key&0x00f00000)>>20,(key&0x000f0000)>>16,(key&0x0000ffff)); }

extern int err_code;

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
	      )

{
  int nlocs;			/* number of locations in each array */
  int iter;			/* current iteration number */
  int proc_stride;		/* stride used by each process */
  int array;			/* current array being used */
  int * ptr;			/* points to the start of array, this loop */
  long long * ind_ptr;
  int rtr_led_data;
  int i;
  int array_cnt;
  volatile int force_read;

  if ((num_arrays < 0) || (num_arrays > 50)) {
    Lock(printlock);
    msg_printf(DBG, "gaah! garbage, num_arrays is %d\n"
	   "spinning ...\n", num_arrays);
    Unlock(printlock);
    while(1);
  }

  proc_stride	= (num_cpus)*stride;
  nlocs	        = array_size/proc_stride/4;
  fetch_op_ptr  = fetch_op_ptr + fetch_op_stride * pid;

  if((debug_level >= 3)) {
    Lock(printlock);
    msg_printf(DBG, "Beginning ct_turbo loop with %d cpus.\n", num_cpus);
    msg_printf(DBG, "     array size = 0x%x\n", array_size);
    msg_printf(DBG, "     iterations = %d\n", max_iter);
    msg_printf(DBG, "     stride = %d\n", stride);
    msg_printf(DBG, "     nlocs = %d\n", nlocs);
    msg_printf(DBG, "     force_wb = %d\n", force_wb);
    msg_printf(DBG, "     write_text = %d\n", write_text);
    msg_printf(DBG, "     write_local = %d\n", write_local);
    msg_printf(DBG, "     read_local = %d\n", read_local);
    msg_printf(DBG, "     read_vector = %d\n", read_vector);
    msg_printf(DBG, "     stripe_arrays = %d\n", stripe_arrays);
    msg_printf(DBG, "     reverse_odd = %d\n", reverse_odd);
    msg_printf(DBG, "     use_sc = %d\n", use_sc);
    msg_printf(DBG, "     use_uncached = %d\n", use_uncached);
    msg_printf(DBG, "     reader unroll depth = %d\n", READ_UNROLL);
    msg_printf(DBG, "     writer unroll depth = %d\n", WRITE_UNROLL);
    msg_printf(DBG, "     print_freq is every %d iterations.\n", print_freq);
    msg_printf(DBG, "     led_freq is every %d iterations.\n", led_freq);
    msg_printf(DBG, "     Using %d arrays.\n", num_arrays);
    for(i=0; i<num_arrays; i++)
      msg_printf(DBG, "     array[%2d] begins at 0x%x\n", i, array_ptr[i]  + 
	     (pid * stride));
    for(i=0; i<num_bte_arrays; i++)
      msg_printf(DBG, "     bte_array[%2d] begins at 0x%x\n", i, bte_ptr[i]);
    if(use_uncached) msg_printf(DBG, "     uncached_array begins at 0x%x\n", 
			    uncached_ptr + (pid * stride));
    if(use_fetch_op) msg_printf(DBG, "     fetch_op array stride %d begins at 0x%x\n", 
			    fetch_op_stride, fetch_op_ptr);
    if(indirect) msg_printf(DBG, "     using indirect_array for array[0] at 0x%x\n",
			indirect_ptr + (pid * stride));
    if(indirect_inv) msg_printf(DBG, "     indirect invalidate = %d\n", indirect_inv);
    Unlock(printlock);
  }

  if(debug_level >= 5) {
    Lock(printlock);
    msg_printf(DBG, "PID %d calling ct_turbo barrier.\n", pid);
    Unlock(printlock);
  }

  barrier(num_cpus, bar_busy, bar_cnt); /* sync everybody up */

  if(debug_level >= 5) {
    Lock(printlock);
    msg_printf(DBG, "PID %d through ct_turbo barrier.\n", pid);
    Unlock(printlock);
  }

  if((pid == master_pid) && master_quiet) {
    Lock(printlock);
    msg_printf(DBG, "Master PID quiet, spin in MD uart to clog MD.\n");
    Unlock(printlock);
    /* spin in uncached space to clog memory: */
    while(*bar_cnt == 0) {
      ct_delay(1);
    }
  } else {
    
    if (debug_level >= 2) {
      Lock(printlock);
      msg_printf(DBG, "PID %d entering iter loop\n", pid);
      Unlock(printlock);
    }
  
    for(iter=0; iter<max_iter; iter++) {

      if ((iter % print_freq) == 0) {
	if (debug_level >= 2) {
	  Lock(printlock);
	  if(use_fetch_op) 
	    msg_printf(DBG, "pid %d: %d fetch_op: 0x%x\n", pid, iter, *fetch_op_ptr);
	  else
	    msg_printf(DBG, "pid %d: %d\n", pid, iter);
	  Unlock(printlock);
	}
      }

      /* change array access from read/write ro read+read/write+write */

      if(stripe_arrays) {	
	/***********************
	 * stripe arrays option:
	 ***********************/
	for(array_cnt = 0; array_cnt < (num_arrays + use_uncached); 
	    array_cnt++) {

	  if((reverse_odd && ((pid % 2) == 1))) /* traverse arrays forwards */
	    array = array_cnt;
	  else			/* traverse arrays backwards */
	    array = (num_arrays + use_uncached) - array_cnt - 1;

	  if(array == num_arrays) 
	    ptr = uncached_ptr +  (pid * stride);
	  else
	    ptr = array_ptr[array] +  (pid * stride);
	
	  ind_ptr = indirect_ptr + (pid * stride);

	  if(writer(iter, pid, ptr, proc_stride, nlocs, array,
		    force_wb, write_text, use_sc,
		    use_float, write_local, read_local, read_vector,
		    use_fetch_op, fetch_op_ptr,
		    debug_level, printlock, brakes,
		    num_cpus, bar_busy, bar_cnt) 
	     != 0) return 1;


	  if (use_fetch_op && ((iter % print_freq) == 0)) {
	    Lock(printlock);
	    msg_printf(DBG, "writer Fetch&Op value = 0x%x\n", *fetch_op_ptr);
	    Unlock(printlock);
	  }


	  if(reader(iter, pid, ptr, proc_stride, nlocs, array,
		    (indirect && (array == 0)), 
		    ind_ptr, indirect_inv,
		    use_fetch_op, fetch_op_ptr,
		    brakes, printlock) != 0) return 1;
	
	  if (use_fetch_op && ((iter % print_freq) == 0)) {
	    Lock(printlock);
	    msg_printf(DBG, "reader Fetch&Op value = 0x%x\n", *fetch_op_ptr);
	    Unlock(printlock);
	  }

	  if(*brakes != 0) {
	    Lock(printlock);
	    msg_printf(DBG, "Brakes observed by pid %d, exiting. (brakes are: %d)\n", pid, *brakes);
	    Unlock(printlock);
	    ct_exit();
	    return 1;
	  }
	}
      } else {
	/***********************
	 * don't stripe arrays:
	 ***********************/
	/* writer first: */
	for(array_cnt = 0; array_cnt < (num_arrays + use_uncached); 
	    array_cnt++) {

	  if((reverse_odd && ((pid % 2) == 1)))	/* traverse arrays forwards */
	    array = array_cnt;
	  else			/* traverse arrays backwards */
	    array = (num_arrays + use_uncached) - array_cnt - 1;

	  if(array == num_arrays)
	    ptr = uncached_ptr +  (pid * stride);
	  else
	    ptr = array_ptr[array] +  (pid * stride);
	  ind_ptr = indirect_ptr + (pid * stride);

	  if(writer(iter, pid, ptr, proc_stride, nlocs, array,
		    force_wb, write_text, use_sc,
		    use_float, write_local, read_local, read_vector,
		    use_fetch_op, fetch_op_ptr,
		    debug_level, printlock, brakes,
		    num_cpus, bar_busy, bar_cnt)
	     != 0) return 1;
	} /* now reader: */
	for(array_cnt = 0; array_cnt < (num_arrays + use_uncached); array_cnt++) {

	  if((reverse_odd && ((pid % 2) == 1)))	/* traverse arrays forwards */
	    array = array_cnt;
	  else			/* traverse arrays backwards */
	    array = (num_arrays + use_uncached) - array_cnt - 1;

	  if(array == num_arrays)
	    ptr = uncached_ptr +  (pid * stride);
	  else
	    ptr = array_ptr[array] +  (pid * stride);
	  if(reader(iter, pid, ptr, proc_stride, nlocs, array,
		    (indirect && (array == 0)), 
		    ind_ptr, indirect_inv,
		    use_fetch_op, fetch_op_ptr,
		    brakes, printlock) != 0) return 1;
	
	  if(*brakes != 0) {
	    Lock(printlock);
	    msg_printf(DBG, "Brakes observed by pid %d, exiting. (brakes are: %d)\n", pid, *brakes);
	    Unlock(printlock);
	    ct_exit();
	    return 1;
	  }
	}
      }
    }
  }
  
  barrier(num_cpus, bar_busy, bar_cnt); /* sync everybody up */
  
  if((pid == master_pid) & (debug_level >= 5)) {
    Lock(printlock);
    msg_printf(DBG, "ct_turbo done, pid=%d\n",pid);
    Unlock(printlock);
  }
  
  return 0;
}

/***************************************
 * ct_turbo write loop:
 ***************************************/

int writer(int iter, int pid, int *ptr, int proc_stride, int nlocs,
	   int array, int force_wb, int write_text, int use_sc,
	   int use_float, int write_local, int read_local, int read_vector,
	   int use_fetch_op, long * fetch_op_ptr,
	   int debug_level, int * printlock, int *brakes,
	   int num_cpus, int * bar_busy, int * bar_cnt)
     
{
  int l, index;
  int t1, t2, t3;
  int i;
  int *writer_addr;
  int  writer_data[20];
  int key[WRITE_UNROLL];
  int *write_ptr[WRITE_UNROLL];
  long * local_addr[MAX_LOCAL_ADDR];
  long vector_data;
  long pi_err_int;
  int local_pi_index;
  
  /* only use_sc iff cached */
  if((((long) ptr) & 0xff00000000000000UL) != 0xa800000000000000UL)
    use_sc = 0;
  
  if(write_text) {
    writer_addr = (int *) writer;
    for(i=0; i<20; i++)		/* load up data array of instructions */
      writer_data[i] = *(writer_addr + i*32);
  }
  
  l = 0;
  while (l < nlocs) {
    index = l * proc_stride;

    if(l+WRITE_UNROLL < nlocs) {
      for(i=0; i<WRITE_UNROLL; i++) { /* setup ptr array, key array: */
	write_ptr[i] = &(ptr[(l+i)*proc_stride]);

	if (debug_level >= 17) {
	  Lock(printlock);
	  msg_printf(DBG, "write_ptr[%d] = 0x%X\n",i,write_ptr[i]);
	  Unlock(printlock);
	}

#if OLD_KEY
	key[i] = MAKE_KEY(iter, pid, (l+i)*proc_stride, 
			  proc_stride,array);
#else
	key[i] = MAKE_KEY(iter, pid, (ulong) write_ptr[i], 
			  proc_stride,array);
#endif
      }
/*
#define WP(x) { if ((debug_level >= 15) && (l % 0x1 == 0)) { Lock(printlock); msg_printf(DBG, "Wrote 0x%X [",key[x]); PRINT_KEY(key[x]); msg_printf(DBG, "] at 0x%X\n",write_ptr[x]); Unlock(printlock); } }
*/
#define WP(x)
/*
#define WP(x) { if (debug_level >= 15) { msg_printf(DBG, "%d",pid); } }
*/

      if((use_sc > 0) && ((l % ((pid + use_sc))) == 0))
	store_sc(write_ptr[0], key[0]);
      else {
	*(write_ptr[0]) = key[0]; WP(0);
      }
#if WRITE_UNROLL > 1
      *(write_ptr[1]) = key[1];   WP(1);
      *(write_ptr[2]) = key[2];   WP(2);
      *(write_ptr[3]) = key[3];   WP(3);
#endif
#if WRITE_UNROLL > 4
      *(write_ptr[4]) = key[4];   WP(4);
      *(write_ptr[5]) = key[5];   WP(5);
      *(write_ptr[6]) = key[6];   WP(6);
      *(write_ptr[7]) = key[7];   WP(7);
#endif
#if WRITE_UNROLL > 8
      *(write_ptr[8]) = key[8];   WP(8);
      *(write_ptr[9]) = key[9];   WP(9);
      *(write_ptr[10]) = key[10]; WP(10);
      *(write_ptr[11]) = key[11]; WP(11);
      *(write_ptr[12]) = key[12]; WP(12);
      *(write_ptr[13]) = key[13]; WP(13);
      *(write_ptr[14]) = key[14]; WP(14);
      *(write_ptr[15]) = key[15]; WP(15);
#endif
      l += (WRITE_UNROLL - 1);
      if(use_fetch_op) /* increment fetch & op addr: */
	*(fetch_op_ptr + 1) = 0xbeefcafe;
    }
    else			/* end of array, no room to unroll: */
#if OLD_KEY
      ptr[index] = MAKE_KEY(iter, pid, index, 
			    proc_stride,array); 
#else
      ptr[index] = MAKE_KEY(iter, pid, (ulong) write_ptr[0], 
			    proc_stride,array); 
#endif

    if(force_wb > 0) {		/* force WB by reading at addr + .5M + 1M */
      if(((l/WRITE_UNROLL + pid) % (force_wb)) == 0) {
	t1 = (ptr[index + 0x20000]);
	t2 = (ptr[index + 0x40000]);
	t3 += t1 + t2;
      }
    }
	
    if(write_text) {
      if((((l/WRITE_UNROLL) + iter + pid) % write_text) == 0) {
	i = (iter + l + pid) % 20;
	*(writer_addr + i*32) = writer_data[i];
      }
    }

    l++;
  }
  dummy((int) t3);
  return 0;
}

/***************************************
 * ct_turbo read loop:
 ***************************************/

int reader(int iter, int pid, int *ptr, int proc_stride, 
	   int nlocs, int array, int indirect, long long * indirect_ptr, 
	   int indirect_inv,
	   int use_fetch_op, long * fetch_op_ptr,
	   int *brakes, int * printlock)
{
  int got[READ_UNROLL];
  int *got_ptr[READ_UNROLL];
  long long inv_data;
  int i, u;
  int *addr;
  int cl[100];
  ulong al[100];
  int l, index, checkit = 1;
  extern int major_loop;

  l = 0;
  while (l < nlocs) {

    index = l * proc_stride;

    for(u=0; u < READ_UNROLL; u++) {
      got_ptr[u] = &(ptr[index]);
      index += proc_stride;
    }

				/* if no indirection or near end of array: */
    if(!indirect | ((l + READ_UNROLL) > nlocs)) {
      got[0] = *(got_ptr[0]);
#if READ_UNROLL > 1
      got[1] = *(got_ptr[1]);
      got[2] = *(got_ptr[2]);
      got[3] = *(got_ptr[3]);
#endif
#if READ_UNROLL > 4
      got[4] = *(got_ptr[4]);
      got[5] = *(got_ptr[5]);
      got[6] = *(got_ptr[6]);
      got[7] = *(got_ptr[7]);
#endif
#if READ_UNROLL > 8
      got[8] = *(got_ptr[8]);
      got[9] = *(got_ptr[9]);
      got[10] = *(got_ptr[10]);
      got[11] = *(got_ptr[11]);
      got[12] = *(got_ptr[12]);
      got[13] = *(got_ptr[13]);
      got[14] = *(got_ptr[14]);
      got[15] = *(got_ptr[15]);
#endif
      if(use_fetch_op) /* decrement fetch & op addr: */
	*(fetch_op_ptr + 2) = 0xbabeface;
    }
    else {			/* use indirect_ptr */
      got[0] = *((int *) (indirect_ptr[(l + 0) * proc_stride]));
#if READ_UNROLL > 1
      got[1] = *((int *) (indirect_ptr[(l + 1) * proc_stride]));
      got[2] = *((int *) (indirect_ptr[(l + 2) * proc_stride]));
      got[3] = *((int *) (indirect_ptr[(l + 3) * proc_stride]));
#endif
#if READ_UNROLL > 4
      got[4] = *((int *) (indirect_ptr[(l + 4) * proc_stride]));
      got[5] = *((int *) (indirect_ptr[(l + 5) * proc_stride]));
      got[6] = *((int *) (indirect_ptr[(l + 6) * proc_stride]));
      got[7] = *((int *) (indirect_ptr[(l + 7) * proc_stride]));
#endif
#if READ_UNROLL > 8
      got[8] = *((int *) (indirect_ptr[(l + 8) * proc_stride]));
      got[9] = *((int *) (indirect_ptr[(l + 9) * proc_stride]));
      got[10] = *((int *) (indirect_ptr[(l + 10) * proc_stride]));
      got[11] = *((int *) (indirect_ptr[(l + 11) * proc_stride]));
      got[12] = *((int *) (indirect_ptr[(l + 12) * proc_stride]));
      got[13] = *((int *) (indirect_ptr[(l + 13) * proc_stride]));
      got[14] = *((int *) (indirect_ptr[(l + 14) * proc_stride]));
      got[15] = *((int *) (indirect_ptr[(l + 15) * proc_stride]));
#endif
    }
    
    for(u=0; u < READ_UNROLL; u++) {
#if OLD_KEY
      index = (l+u) * proc_stride;
#else
      if(!indirect | ((l + READ_UNROLL) > nlocs)) 
	index = (int) got_ptr[u];
      else
	index = (int) (int *) (indirect_ptr[(l + u) * proc_stride]);
#endif
      if ( checkit && (l+u < nlocs) &&
	  ( got[u] != MAKE_KEY(iter, pid, index, 
			       proc_stride, array))) {

	heart_trigger(got[u]);

	print_heart_exception_cause_reg();

	addr = ptr + index;
	*brakes = 1;
	for(i= -32; i<32; i++) {	/* save surrounding cache line */
	  cl[i+32] = *(addr+i);
	  al[i+32] = (ulong) (addr+i);
	}
	Lock(printlock);
	msg_printf(DBG, "Data mismatch for pid %d\n", pid);
	msg_printf(DBG, "Expected Data = 0x%x\n", 
	       MAKE_KEY(iter, pid, index, proc_stride, array));
	msg_printf(DBG, "Data Read     = 0x%x\n", got[u]);
	msg_printf(DBG, "DataAddress   = 0x%x\n", addr);
	msg_printf(DBG, "Loop #        = %d\n", major_loop);
	msg_printf(DBG, "Iteration #   = %d\n", iter);
	msg_printf(DBG, "l Count       = %d\n", l+u);
	msg_printf(DBG, "Dumping 32 previous and following data:");
	for(i= 0; i<64; i++) {
	  msg_printf(DBG, "%d 0x%X:     0x%x [", i, al[i], cl[i]);
	  PRINT_KEY(cl[i]);
	  msg_printf(DBG, "]\n");
	}
	Unlock(printlock);
	/* return to monitor */
	err_code = 1;
	return 1;
	/* ct_exit(); */
      }
    }

    if(indirect_inv != 0)	/* invalidate entry in indirect array */
      if((((l/READ_UNROLL) + iter + pid) % indirect_inv) == 0) {
	inv_data = ((volatile long long *) indirect_ptr)[l];
	((volatile long long *) indirect_ptr)[l] = inv_data;
      }
    
    l += u;
  }
  
  return 0;
}




