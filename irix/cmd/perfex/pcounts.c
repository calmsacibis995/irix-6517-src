#include "counts.h"


int 
PFX_start_counters_pid(int event_type0, int event_type1, pid_t pid) {
  int i,gen_start;    
  char    pfile[32];
  short mode=0x0;
  int fd;

  sprintf(pfile, "/proc/%010d", pid);
  if ((fd = open(pfile, O_RDONLY)) < 0) {
    fprintf(output_stream,"Can't open /proc/%d\n",pid);
    exit(1);
  }

/* check for valid event combination */
  if( event_type0 < HWPERF_CNT1BASE && event_type1 < HWPERF_CNT1BASE || \
      event_type0 >=HWPERF_CNT1BASE && event_type1 >=HWPERF_CNT1BASE ) {
        fprintf(output_stream,"conflicting events: \n %s and %s \n",EventDesc[event_type0],EventDesc[event_type1]);
	exit(1);
	}
  mode = HWPERF_CNTEN_U;
  if(enable_ex_lvl_cnts) mode |= HWPERF_CNTEN_E;
  if(enable_ker_lvl_cnts) mode |= HWPERF_CNTEN_K;
  if(enable_sup_lvl_cnts) mode |= HWPERF_CNTEN_S;
#ifdef _BDB
  printf("BDB> counting mode is %d\n",mode);
#endif
  for (i = 0; i < HWPERF_EVENTMAX; i++ ) {
    evctr_args.hwp_ovflw_freq[i] = 0;   /* no frequency interrupts */
    if (i == event_type0 || i == event_type1) {
      evctr_args.hwp_evctrargs.hwp_evctrl[i].hwperf_creg.hwp_mode = mode;
      evctr_args.hwp_evctrargs.hwp_evctrl[i].hwperf_creg.hwp_ie = 1;
      evctr_args.hwp_evctrargs.hwp_evctrl[i].hwperf_creg.hwp_ev = i < HWPERF_CNT1BASE ? i : i - HWPERF_CNT1BASE;
    } else
      evctr_args.hwp_evctrargs.hwp_evctrl[i].hwperf_spec = 0;
  }
   /* don't send us any signals */
   evctr_args.hwp_ovflw_sig = 0;

/* set up to aggregate counts back to parent at exit */

  if (ioctl(fd, PIOCSAVECCNTRS, pid) < 0) {
    perror("prioctl  PIOCSAVECCNTRS returns error");
    exit(1);
  }

/* start the counters */
  if ((gen_start = ioctl(fd, PIOCENEVCTRS, (void *)&evctr_args)) < 0) {
    perror("prioctl PIOCENEVCTRS returns error");
    exit(1);
  }
  close(fd);
  return gen_start;
}

int 
PFX_read_counters_pid(int event_type0, long long *mcount0, \
                       int event_type1, long long *mcount1, pid_t pid, int rel, int fatal) {
    char pfile[32];
    int gen_read;
    static int fd = -1;

    if (fd == -1) {
      sprintf(pfile, "/proc/%010d", pid);
      if ((fd = open(pfile, O_RDONLY)) < 0) {
        if (!fatal) {
	  return -1;
        }
        fprintf(output_stream,"Can't open /proc/%d\n",pid);
        exit(1);
      }
    }

/* retrieve the counts */
  if ((gen_read = ioctl(fd, PIOCGETEVCTRS, (void *)&cnts)) < 0) {
    if (!fatal) {
      return -1;
    }
    perror("prioctl PIOCGETEVCTRS returns error");
    exit(1);
  }
  if(rel) {
/* release the counters */
    if (ioctl(fd, PIOCRELEVCTRS) < 0) {
      if (!fatal) {
        return -1;
      }
      perror("prioctl PIOCRELEVCTRS returns error");
      exit(1);
    }
  }
    *mcount0 = (long long) (cnts.hwp_evctr[event_type0]);
    *mcount1 = (long long) (cnts.hwp_evctr[event_type1]);
    /* close(fd); */
    return gen_read;
  }


int 
PFX_start_counters_pid_all(pid_t pid) {
  int i,gen_start;    
  char    pfile[32];
  short mode=0x0;
  int fd;

  sprintf(pfile, "/proc/%010d", pid);
  if ((fd = open(pfile, O_RDONLY)) < 0) {
    fprintf(output_stream,"Can't open /proc/%d\n",pid);
    exit(1);
  }

  mode = HWPERF_CNTEN_U;
  if(enable_ex_lvl_cnts) mode |= HWPERF_CNTEN_E;
  if(enable_ker_lvl_cnts) mode |= HWPERF_CNTEN_K;
  if(enable_sup_lvl_cnts) mode |= HWPERF_CNTEN_S;
#ifdef _BDB
  printf("BDB> counting mode is %d\n",mode);
#endif
  for (i = 0; i < HWPERF_EVENTMAX; i++ ) {
      evctr_args.hwp_ovflw_freq[i] = 0;   /* no frequency interrupts */   
      evctr_args.hwp_evctrargs.hwp_evctrl[i].hwperf_creg.hwp_mode = mode;
      evctr_args.hwp_evctrargs.hwp_evctrl[i].hwperf_creg.hwp_ie = 1;
      evctr_args.hwp_evctrargs.hwp_evctrl[i].hwperf_creg.hwp_ev = i < HWPERF_CNT1BASE ? i : i - HWPERF_CNT1BASE;
  }
   /* don't send us any signals */
   evctr_args.hwp_ovflw_sig = 0;

/* set up to aggregate counts back to parent at exit */

  if (ioctl(fd, PIOCSAVECCNTRS, pid) < 0) {
    perror("prioctl  PIOCSAVECCNTRS returns error");
    exit(1);
  }

/* start the counters */
  if ((gen_start = ioctl(fd, PIOCENEVCTRS, (void *)&evctr_args)) < 0) {
    perror("prioctl PIOCENEVCTRS returns error");
    exit(1);
  }
  close(fd);
  return gen_start;
}


