#include <stdlib.h>
#include <string.h>
#include <mutex.h>
#include "counts.h"

/* globals, written only in init */
int		enable_ex_lvl_cnts;
int		enable_ker_lvl_cnts;
int		enable_sup_lvl_cnts;
int			gen_start;

/* global (lock protected) to accumulate total over threads */
evcnt_t acc_cnt0=0;
evcnt_t acc_cnt1=0;
evcnt_t acc_all[HWPERF_EVENTMAX];
volatile unsigned long global_lock = 0;

FILE* output_stream = stderr;

/* protos */
void __ateachexit(void ());

void
__adios(void){ fprintf(output_stream,"BDB> Parent exiting...\n"); fflush(output_stream);
             }

/* called at each thread exit */
/* don't call exit within this procedure! */
void
read_cntrs_thread(void){ 
  hwperf_cntr_t   counts;
  char *p;
  int e0,e1,gen_stop,count_all=0;
  pid_t pid = getpid();
  int fd,n,me,master_pid;
  char pfile[32];
  prpsinfo_t ps_info;

  me = pid;
#ifdef _BDB
  fprintf(output_stream,"global variables:\n");
  fprintf(output_stream,"%d>exception_level = %d\n",me, enable_ex_lvl_cnts);
  fprintf(output_stream,"%d>kernel_level = %d\n",me, enable_ker_lvl_cnts);
  fprintf(output_stream,"%d>supervisor_level = %d\n",me, enable_sup_lvl_cnts);
  fprintf(output_stream,"%d>gen_start = %d\n",	me, gen_start);
  fprintf(output_stream,"%d>output_stream = %d\n", me, output_stream);
#endif
  master_pid = atoi(getenv("__PERFEX_PIDC"));
  if(p=getenv("__PERFEX_COUNT_ALL")) count_all = atoi(p);
#ifdef _BDB
    fprintf(output_stream,"BDB>me: %d master: %d\n",me,master_pid);
    fflush(output_stream);
#endif
    e0 = atoi(getenv("T5_EVENT0"));
    e1 = atoi(getenv("T5_EVENT1"));
# if 0
    gen_start = atoi(getenv("__PERFEX_GENERATION"));
# endif
    sprintf(pfile, "/proc/%010d", pid);
    if ((fd = open(pfile, O_RDONLY)) < 0) {
      fprintf(output_stream,"Can't open /proc/%d",pid);
  }

    /* get process exec'ed basename */
    if( ioctl(fd, PIOCPSINFO, &ps_info) < 0 ) {
      perror("prioctl PIOCPSINFO returns error");
    } 

    /* retrieve the counts */
    if ((gen_stop = ioctl(fd, PIOCGETEVCTRS, (void *)&counts)) < 0) {
      perror("prioctl PIOCGETEVCTRS returns error");
    }

    /* release the counters */
    if (ioctl(fd, PIOCRELEVCTRS) < 0) {
      perror("prioctl PIOCRELEVCTRS returns error");
    }

    if(gen_start != gen_stop) {
        perror("lost counters; aborting\n");
#ifdef _BDB
    fprintf(output_stream,"BDB> generations: start=%d, read=%d\n",gen_start,gen_stop);
#endif        
        return;
      }

/*
 * print results ordered by pid.  
 * NB correctness depends on thread 0 calling __ateachexit 
 * after all children have returned from __ateachexit.  This 
 * is currently safe.
 */
    fflush(output_stream);
     while( ( (global_lock) != 0 ) ||
	   (test_and_set( &global_lock, 1 ) != 0 ) ) {}
    {
      char spid[128], thread_fname[128];
      FILE* tfp;
      if(getenv("__PERFEX_MP_INIT")) {
	if(p=getenv("__PERFEX_OUTPUT_FILE")) {
	/* redefine the output stream per thread */
	  sprintf(spid,"\.%010d",me);
	  strcpy(thread_fname,p);
	  strcat(thread_fname,spid);
	  tfp = fopen(thread_fname,"a");
	  if( !tfp ) {
	    fprintf(output_stream,"couldn't open %s for output\n",p);
	  } else {
	    if (fcntl(fileno(tfp) , F_SETFD, FD_CLOEXEC) == -1) {
	      fprintf(output_stream,
		 "libperfex: cannot open '%s' for writing\n",thread_fname);
	    } else {

#ifdef _BDB
	      fprintf(output_stream, 
                 "libperfex: pid %d output sent to %s on %llx\n",me,thread_fname,(long long) tfp);
#endif
	      output_stream = tfp;
#ifdef _BDB
	      fprintf(output_stream,
		      "BDB>libperfex (pid %d): tfp address = %llx ", me,(long long) &tfp);
#endif
	    }
	  }
	}
      }
      fflush(output_stream);
      if(count_all) {
	if(me!=master_pid) {  
          for(n=0;n<HWPERF_EVENTMAX;n++ ) acc_all[n] += counts.hwp_evctr[n];
	} else {
          for(n=0;n<HWPERF_EVENTMAX;n++ ) counts.hwp_evctr[n] -= acc_all[n];
	}
	PFX_print_counters_thread_all(me, ps_info.pr_fname, &counts);
      } else {
	if(me!=master_pid) {            
	  acc_cnt0 += counts.hwp_evctr[e0];
	  acc_cnt1 += counts.hwp_evctr[e1];
	} else {
	  counts.hwp_evctr[e0] -= acc_cnt0;
	  counts.hwp_evctr[e1] -= acc_cnt1;
	}
	PFX_print_counters_thread(me, ps_info.pr_fname, e0, (evcnt_t) (counts.hwp_evctr[e0]) \
           ,e1,(evcnt_t) (counts.hwp_evctr[e1]) );
      }
      fflush(output_stream);
      if ( output_stream != stderr )  fclose(output_stream);
    }
    test_and_set( &global_lock, 0); 
 }

/* this function is called at dso init time, before main. 
 *  Not thread-safe 
 */
void
__init_counters_thread(void)
{
  char *p;
  FILE* fp;
#ifdef _BDB
	atexit(__adios);
#endif
    if(getenv("__PERFEX_MP_INIT")) {
      __ateachexit(read_cntrs_thread);
    }
    /* get global args from environment */
    enable_ex_lvl_cnts = atoi(getenv("__PERFEX_XLCNTS"));
    enable_ker_lvl_cnts = atoi(getenv("__PERFEX_KLCNTS"));
    enable_sup_lvl_cnts = atoi(getenv("__PERFEX_SLCNTS"));
    gen_start = atoi(getenv("__PERFEX_GENERATION"));
    if(p=getenv("__PERFEX_OUTPUT_FILE")) {
      fp = fopen(p,"a");
      if( !fp ) {
	fprintf(output_stream,"couldn't open %s for output\n",p);
      } else {
	if (fcntl(fileno(fp) , F_SETFD, FD_CLOEXEC) == -1) {
	  fprintf(output_stream,"libperfex: cannot open '%s' for writing\n",p);
	} else {
#ifdef _BDB
	  fprintf(output_stream, "libperfex: output sent to %s\n",p);
#endif
	  output_stream = fp;
	}
      }
    }
    perfy_options.perfy = atoi(getenv("__PERFEX_YPERFY"));
    perfy_options.cpu_species_mix = atoi(getenv("__PERFEX_CPUSPECIES"));
    perfy_options.reuse = atoi(getenv("__PERFEX_YREUSE"));

    /* initialize cost table */
    if(p=getenv("PERFEX_COST_FILE")) {
#ifdef _BDB
      fprintf(output_stream,"BDB> loaded table %s in pid %d\n",p,getpid());
      fflush(output_stream);
#endif
      load_table(NULL,p);
    }

#ifdef _BDB
	fprintf(output_stream,"BDB> Parent starting...\n"); 
#endif
}
































