#include "counts.h"

/* fortran entries */
int 
start_counters_( int* mt0_p, int* mt1_p ) {
    return start_counters( *mt0_p, *mt1_p );
  }
int 
read_counters_( int* mt0_p, long long* mcount0, int* mt1_p, long long* mcount1 ) {
    return read_counters( *mt0_p, mcount0, *mt1_p, mcount1 );
  }
int
print_counters_( int* mt0_p, long long* mcount0, int* mt1_p, long long* mcount1) {
    return print_counters(*mt0_p,*mcount0,*mt1_p,*mcount1);
  }
int
print_costs_( int* mt0_p, long long* mcount0, int* mt1_p, long long* mcount1) {
  return print_costs(*mt0_p,*mcount0,*mt1_p,*mcount1); 
    }
int
load_costs_( char* cost_table_file ) {
  return load_table( NULL, cost_table_file );
}

/* C entries */

int
load_costs( char* cost_table_file ) {
  return load_table( NULL, cost_table_file );
}

int 
start_counters(int event_type0, int event_type1) {
  hwperf_profevctrarg_t evargs;
  int gen_start;
  int i,events=0;    
  int pid = getpid();
  int fd;
  char    pfile[32],*p;
  short mode=0x0;
    
  sprintf(pfile, "/proc/%010d", pid);
  if ((fd = open(pfile, O_RDONLY)) < 0) {
    perror("Can't open /proc/pid");
    return(-1);
  }

/* get event specifier overrides from environment */
  p = getenv("T5_EVENT0");
  if(p) {event_type0 = atoi(p); events ^= 0x1; }
  p = getenv("T5_EVENT1");
  if(p) 
    {
      if( events ) event_type1 = atoi(p); 
      else event_type0 = atoi(p);
      events ^= 0x1; 
    }

/* check for valid event combination */
  if( event_type0 < HWPERF_CNT1BASE && event_type1 < HWPERF_CNT1BASE || \
      event_type0 >=HWPERF_CNT1BASE && event_type1 >=HWPERF_CNT1BASE ) {
        fprintf(stderr,"conflicting events: \n %d and %d \n",event_type0,event_type1);
	close(fd);
	return(-1);
	}
  mode = HWPERF_CNTEN_U;
#ifdef _BDB
  printf("BDB> counting mode is %d\n",mode);
#endif
   for (i = 0; i < HWPERF_EVENTMAX; i++ ) {
    evargs.hwp_ovflw_freq[i] = 0;      /* no frequency interrupts */
    if (i == event_type0 || i == event_type1) {
      evargs.hwp_evctrargs.hwp_evctrl[i].hwperf_creg.hwp_mode = mode;
      evargs.hwp_evctrargs.hwp_evctrl[i].hwperf_creg.hwp_ie = 1;
      evargs.hwp_evctrargs.hwp_evctrl[i].hwperf_creg.hwp_ev = i < HWPERF_CNT1BASE ? i : i - HWPERF_CNT1BASE;
    } else
      evargs.hwp_evctrargs.hwp_evctrl[i].hwperf_spec = 0;
  }
   /* don't send us any signals */
   evargs.hwp_ovflw_sig = 0;

/* start the counters */
  if ((gen_start = ioctl(fd, PIOCENEVCTRS, (void *)&evargs)) < 0) {
    perror("prioctl PIOCENEVCTRS returns error");
    close(fd);
    return(-1);
  }
  close(fd);
  return gen_start;
}

int 
read_counters(int event_type0, long long *mcount0, \
                       int event_type1, long long *mcount1) {
  hwperf_cntr_t   counts;
  int gen_read;
  int fd;
  char *p;
  int events=0;
  char    pfile[32];

/* open the /proc */

  sprintf(pfile, "/proc/%010d", getpid());
  if ((fd = open(pfile, O_RDONLY)) < 0) {
    perror("Can't open /proc/pid");
    return(-1);
  }

/* get event specifier overrides from environment */
  p = getenv("T5_EVENT0");
  if(p) {event_type0 = atoi(p); events ^= 0x1; }
  p = getenv("T5_EVENT1");
  if(p) 
    {
      if( events ) event_type1 = atoi(p); 
      else event_type0 = atoi(p);
      events ^= 0x1; 
    }

/* check for valid event combination */
  if( event_type0 < HWPERF_CNT1BASE && event_type1 < HWPERF_CNT1BASE || \
      event_type0 >=HWPERF_CNT1BASE && event_type1 >=HWPERF_CNT1BASE ) {
        fprintf(stderr,"conflicting events: \n %d and %d \n",event_type0,event_type1);
	close(fd);
	return (-1);
	}

/* retrieve the counts */
  if ((gen_read = ioctl(fd, PIOCGETEVCTRS, (void *)&counts)) < 0) {
    perror("prioctl PIOCGETEVCTRS returns error");
    close(fd);
    return(-1);
  }

/* stop and release the counters */
  if(ioctl(fd, PIOCRELEVCTRS) < 0) { 
    perror("prioctl PIOCRELEVCTRS returns error");
    close(fd);
    return(-1);
  }

  close(fd);
  *mcount0 = (long long) (counts.hwp_evctr[event_type0]);
  *mcount1 = (long long) (counts.hwp_evctr[event_type1]);
  return gen_read;
}

int
print_counters(int event_type0, long long count0, int event_type1, long long count1) {
    char *p;
    int i,events=0;
    char pad[MAX_EVENT_DESC_LEN];
    counts_t counts[NCOUNTERS];
    for(i=0;i<MAX_EVENT_DESC_LEN;i++) pad[i] = '.';

/* get event specifier overrides from environment */
  p = getenv("T5_EVENT0");
  if(p) {event_type0 = atoi(p); events ^= 0x1; }
  p = getenv("T5_EVENT1");
  if(p) 
    {
      if( events ) event_type1 = atoi(p); 
      else event_type0 = atoi(p);
      events ^= 0x1; 
    }

/* check for valid event combination */
  if( event_type0 < HWPERF_CNT1BASE && event_type1 < HWPERF_CNT1BASE || \
      event_type0 >=HWPERF_CNT1BASE && event_type1 >=HWPERF_CNT1BASE ) {
        fprintf(stderr,"conflicting events: \n %d and %d \n",event_type0,event_type1);
	return (-1);
	}

  /* this global is filled by the caller.
   * remove any fields from here that are
   * filled in already
   */
  perfy_options.perfy         = FALSE;
  perfy_options.fpout         = _fpout;
  perfy_options.MultiRunFiles = _MultiRunFiles;
  perfy_options.MHz           = _MHz;
  perfy_options.IP            = _IP;
  perfy_options.range         = _range;

  /* This data structure is required by the print_table interface.
   */
  for (i=0; i<NCOUNTERS; i++) {
    counts[i].active = FALSE;
  }
  counts[event_type0].active = TRUE;
  counts[event_type0].count  = count0;
  counts[event_type1].active = TRUE;
  counts[event_type1].count  = count1;

  print_table(&perfy_options, counts, (pid_t) -1, (char*) NULL);

  return(0);
}

/* print counts and cost estimates in 'perfy' style */
int          
print_costs(

int	event_type0,
evcnt_t	count0,
int	event_type1,
evcnt_t	count1)

{
  int		i,events=0;
  char          *p;
  counts_t	counts[NCOUNTERS];


/* get event specifier overrides from environment */
  p = getenv("T5_EVENT0");
  if(p) {event_type0 = atoi(p); events ^= 0x1; }
  p = getenv("T5_EVENT1");
  if(p) 
    {
      if( events ) event_type1 = atoi(p); 
      else event_type0 = atoi(p);
      events ^= 0x1; 
    }

/* check for valid event combination */
  if( event_type0 < HWPERF_CNT1BASE && event_type1 < HWPERF_CNT1BASE || \
      event_type0 >=HWPERF_CNT1BASE && event_type1 >=HWPERF_CNT1BASE ) {
        fprintf(stderr,"conflicting events: \n %d and %d \n",event_type0,event_type1);
	return (-1);
	}


  /* this global is filled by the caller.
   * remove any fields from here that are 
   * filled in already
   */
  perfy_options.perfy         = TRUE;
  perfy_options.fpout         = _fpout;
  perfy_options.MultiRunFiles = _MultiRunFiles;
  perfy_options.MHz           = _MHz;
  perfy_options.IP            = _IP;
  perfy_options.range         = _range;

  /* This data structure is required by the print_table interface.
   */
  for (i=0; i<NCOUNTERS; i++) {
    counts[i].active = FALSE;
  }
  counts[event_type0].active = TRUE;
  counts[event_type0].count  = count0;
  counts[event_type1].active = TRUE;
  counts[event_type1].count  = count1;

  print_table(&perfy_options, counts, (pid_t) -1, (char*) NULL);

  return(0);
}

