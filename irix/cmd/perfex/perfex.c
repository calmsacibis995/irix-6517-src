/*
 *  Brond Larson:  1-17-96
 *  7-1-96: cost table additions (Jeff Fier and BEL)
 */

#include "counts.h"
#include <signal.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/schedctl.h>
#include <sys/signal.h>
#include <sys/time.h>

char **EventDesc = 0;

extern int      errno;
extern char     **_environ;

static char Executable_Prog[16384];
int EventType0 = 0;    /* default counts cycles on each */
int EventType1 = 16;
long long SavedCount0 = 0;  /* associated counts */
long long SavedCount1 = 0;
pid_t pidc;            /* target pid (if active) */
char* namec = "Summary for execution of ";    /* used for target name, args */
extern int      is_mp=0;
int sampling_itimer = ITIMER_REAL;
int sampling_signal = SIGALRM;
int sampling_period = 0;
int counts_enabled = 1;
int enable_ex_lvl_cnts = 0; /* default user level only */
int enable_ker_lvl_cnts = 0; /* default user level only */
int enable_sup_lvl_cnts = 0; /* default user level only */
int allow_mixed_species = 0; /* default don't allow mixed species */
int count_all = 0;          /* get all events */
int using_signals=0;
int never_got_start_signal = 1;
static char cost_file[256]; /* user-loadable cost table */

hwperf_profevctrarg_t evctr_args; 
hwperf_cntr_t   cnts;
hwperf_cntr_t   saved_cnts;
int gen_start;
int gen_stop;

FILE* output_stream=stderr;

#define NOWAY_S		0x01
#define NOWAY_P		0x02
#define NOWAY_MP	0x04

static void
Usage() 
{
  int i;

  
  printf(" perfex [-e event0 [-e event1]] [-a] [-k] [-mp | -s] [-x] \n");
  printf("        [-y] [-c input file ] [-t] file [options]\n\n");
  printf(" where file is an executable, possibly with options.  The integer event \n");
  printf(" specifiers index this table:\n\n");

  if(perfy_options.cpu_species_mix == CPUSPECIES_MIXED_R10000_R12000) 
    fprintf(output_stream,"(Mixed cpu architecture system: only common events named)\n");

  for(i=0;i<32;i++){
    printf("     %d = %s\n",i,EventDesc[i]); 
  }

  printf("\n 2, 1, or 0 event specifiers may be given, the default events being to\n");
  printf(" count cycles.  Events may also be specified by setting one or both of the\n");
  printf(" environment variables T5_EVENT0 and T5_EVENT1. Command line event specifiers\n"); 
  printf(" if present will override these. The order of events specified is not important.\n");
  printf(" The counts, together with an event description are written to stderr.\n");
  printf(" Two events which *must* be counted on the same hardware counter will\n");
  printf(" cause a conflicting counters error.\n");
  printf("\n Options:\n");
  printf("  -a  Multiplex over all events, projecting totals.  Ignore event specifiers.\n");
  printf("  -k  Count in kernel mode too (must be superuser).\n");
  printf("  -mp Report per-thread counts for mp programs as well as (default) totals\n");
  printf("  -o <file> Send perfex output to <file> (default: stderr).\n");
  printf("      ( With -mp, the thread-specific output is sent to <file>.<pid>.\n" );
  printf("  -s  Start(stop) counting on USR1(USR2) signal receipt by perfex process\n");
  printf("  -p  <interval> Profile/sample the counters at <interval> given in msecs\n");
  printf("  -x  Count at exception level (as well as the default user level).\n");
  printf("  -y  Report statistics and ranges of estimated times per event\n");
  printf("  -t  Dump the table of costs used for the range calculation option (-y)\n");
  printf("  -c <file> Load a new cost table from <file> (-y)\n");
  /*  printf("  -l <n> Use <n> as the load quantum in the cache reuse calculation (-y)\n");
   */
  printf("\n Example: To collect instruction and data scache miss counts on a program \n");
  printf(" normally executed by \n");
  printf("   %% bar < bar.in > bar.out \n");
  printf(" would be accomplished by \n");
  printf("   %% perfex -e 26 -e 10 bar < bar.in > bar.out .\n");
}

/* signal handlers */
static void                /* die gracefully... */
CleanUp(void) {
#ifdef _BDB
    fprintf(output_stream,"BDB> terminating child...\n");
#endif
    kill(pidc,SIGINT);
  } 

static void
start_counting(void) {
  int re;
#ifdef _BDB
    fprintf(output_stream,"BDB> catching USR1\n");
#endif
  never_got_start_signal = 0;
    if(counts_enabled) {
      fprintf(output_stream,"got start signal, but already counting--ignored\n");
      fflush(output_stream);
    } else {
      fprintf(output_stream,"got start signal; beginning counting...\n");
      if(count_all) {
            gen_start = PFX_start_counters_pid_all(pidc);
      } else {
            gen_start = PFX_start_counters_pid(EventType0,EventType1,pidc);
      }
      counts_enabled = 1; 
#ifdef _BDB
      fprintf(output_stream,"BDB> signal based start for pid %d returns %d\n",pidc,gen_start);
#endif
      fflush(output_stream);
    }
}

static void
halt_counting(void) {
  int re;
#ifdef _BDB
    fprintf(output_stream,"BDB> catching USR2\n");
#endif
    if(!(counts_enabled)) {
      fprintf(output_stream,"got halt signal, but not counting--ignored\n");
      fflush(output_stream);
    } else {
      fprintf(output_stream,"got halt signal; stopping counting...\n");
      {
        long long c0=0, c1=0;
        gen_stop = PFX_read_counters_pid(EventType0,&c0,EventType1,&c1,pidc,1,1);
	if(gen_stop != gen_start) {
	  fprintf(output_stream,"lost counters! Aborting...\n");
	  exit(1);
	}
        SavedCount0 += c0; SavedCount1 += c1;
	if(count_all) {
	    int i;

	    for(i = 0; i < NCOUNTERS; i++) {
		saved_cnts.hwp_evctr[i] += cnts.hwp_evctr[i];
	    }
	}
        counts_enabled = 0; 
#ifdef _BDB
	fprintf(output_stream,"BDB> signal based read for pid %d returns %d\n",pidc,gen_stop);
#endif
	fflush(output_stream);
      }
    }
}

static void
sample_counting(void) {
    int re;
    long long c0=0, c1=0;
    static unsigned long long intr_num = 0ll, sample_num = 00l;

    /* check counter enable */
    if (!counts_enabled) {
	intr_num++;
	return;
    }

    /* read counters */
#ifdef _BDB
    fprintf(output_stream,"BDB> catching %s\n", sampling_signal);
    fflush(output_stream);
#endif
    gen_stop = PFX_read_counters_pid(EventType0,&c0,EventType1,&c1,pidc,0,0);
    if (gen_stop == -1) {
	return;
    }
    if(gen_stop != gen_start) {
	fprintf(output_stream,"lost counters! Aborting...\n");
	exit(1);
    }
#ifdef _BDB
    fprintf(output_stream,"BDB> signal based read for pid %d returns %d\n",pidc,gen_stop);
    fflush(output_stream);
#endif

    /* add saved counters */
    c0 += SavedCount0; c1 += SavedCount1;
    if(count_all) {
        int i;

        for(i = 0; i < NCOUNTERS; i++) {
	    cnts.hwp_evctr[i] += saved_cnts.hwp_evctr[i];
        }
    }

    /* print results */
    fprintf(output_stream,"Sample #%llu, Time %llu msecs\n\n", sample_num++, sampling_period*intr_num++);
    if(count_all) {
      PFX_print_counters_all( &cnts );
    } else {
      PFX_print_counters(EventType0,c0,EventType1,c1);
    }
    fprintf(output_stream,"\n");
    fflush(output_stream);
}

static int
Forkit(int remap_stdout,int argc, char **argv) 
{
    char 	*program_input;
    char	*program_output;
    char	**arg_ptr;
    char	**prog_args;
    int         status,re;
    int iter=0;
    prog_args = (char **) malloc((argc + 1) * sizeof(char *));
    
    if (prog_args == NULL) {
      fprintf(output_stream,"Malloc of prog_args failed: %s\n", strerror(errno));
      exit(1);
    }

    program_input = NULL;
    program_output = NULL;

    /* Parse parameter line for child process */
    prog_args[0] = argv[0];	/* point to program name */
    strcat(namec,argv[0]);
    arg_ptr = prog_args + 1;	/* point to first program argument */
    argv++;
    argc--;
    while (argc) {
	char *arg = argv[0];
	if (arg[0] == '<' && arg[1] == '\0') {
	    program_input = argv[1];
	    argv++, argc--;
	} else if (arg[0] == '>' && arg[1] == '\0') {
	    program_output = argv[1];
	    argv++, argc--;
	} else {
	    *arg_ptr++ = arg;
	}
	strcat(namec," ");
	strcat(namec,argv[0]);
	argv++, argc--;
    }
    *arg_ptr++ = NULL;

    /* fork child */
    if (((int)(pidc = fork())) < 0) {
      fprintf(output_stream,"failed to fork child: %s\n", strerror(errno));
      exit(1);
    }

    if (pidc == 0) {		/* exec process as child */
      if (program_input != NULL) {
	int fd;
	if ((fd = open(program_input, O_RDONLY)) < 0) {
	  fprintf(output_stream,"Unable to open %s\n", program_input);
	  exit(1);
	}
      }
      if (program_output != NULL) {
	int fd;
	if ((fd = creat(program_output, 0666)) < 0) {
	  fprintf(output_stream,"unable to create %s\n", program_output);
	  exit(1);
	}
      } else if (remap_stdout) {
	int fd;
	if ((fd = creat("/dev/null", 0666)) < 0) {
	  fprintf(output_stream,"unable to create %s\n", program_output);
	  exit(1);
	}
      }

    /* set up some environment for the grandchildren in mp case*/
    if(is_mp) {
      static char  *envE0,*envE1,*envXLC,*envGEN,*envMP,*envPERFY,*envREUSE;
      static char *envCFILE, *envKLC, *envSLC, *envSPECIES, *buff;
      static char *envPROFILE;
      char *c;
      envE0  = (char *) malloc( 128*sizeof(char *) );
      envE1  = (char *) malloc( 128*sizeof(char *) );
      envXLC = (char *) malloc( 128*sizeof(char *) );
      envKLC = (char *) malloc( 128*sizeof(char *) );
      envSLC = (char *) malloc( 128*sizeof(char *) );
      envGEN = (char *) malloc( 128*sizeof(char *) );
      envMP  = (char *) malloc( 128*sizeof(char *) );
      envPERFY  = (char *) malloc( 128*sizeof(char *) );
      envREUSE  = (char *) malloc( 128*sizeof(char *) );
      envCFILE  = (char *) malloc( 1024*sizeof(char *) );
      envSPECIES = (char *) malloc( 128*sizeof(char *) );
      envPROFILE = (char *) malloc( 128*sizeof(char *) );
      buff  = (char *) malloc( 256*sizeof(char *) );

      sprintf(envE0,"T5_EVENT0=%d",EventType0);
      putenv(envE0);
      sprintf(envE1,"T5_EVENT1=%d",EventType1);
      putenv(envE1);
      sprintf(envXLC,"__PERFEX_XLCNTS=%d",enable_ex_lvl_cnts);
      putenv(envXLC);
      sprintf(envKLC,"__PERFEX_KLCNTS=%d",enable_ker_lvl_cnts);
      putenv(envKLC);
      sprintf(envSLC,"__PERFEX_SLCNTS=%d",enable_sup_lvl_cnts);
      putenv(envSLC);
      sprintf(envGEN,"__PERFEX_GENERATION=%d",gen_start);   
      putenv(envGEN);
      sprintf(envMP,"__PERFEX_MP_INIT=%d",is_mp);
      putenv(envMP);
      sprintf(envPROFILE,"__PERFEX_PROFILE=%d",sampling_period);
      putenv(envPROFILE);
      /* perfy options */
      sprintf(envPERFY,"__PERFEX_YPERFY=%d",perfy_options.perfy);
      putenv(envPERFY);
      sprintf(envREUSE,"__PERFEX_YREUSE=%d",perfy_options.reuse);
      putenv(envREUSE);
      sprintf(envSPECIES, "__PERFEX_CPUSPECIES=%d",perfy_options.cpu_species_mix);
      putenv(envSPECIES);
      
      if(strlen(cost_file) != NULL) {
#ifdef _BDB
	fprintf(output_stream,"BDB> passing %s to child\n",cost_file);
#endif
	sprintf(envCFILE,"PERFEX_COST_FILE=%s",cost_file);
	putenv(envCFILE);
      }

      /* safely modify the dso list to include libperfex.so */
      if( NULL == (c=getenv("_RLD_LIST"))){
	putenv("_RLD_LIST=libperfex.so:DEFAULT");
      } else {
	strcpy(buff,"_RLD_LIST=libperfex.so:");
	strcat(buff,c);
	putenv(buff);
      }

    }

      {
	static char *envPIDC, *envALL;
	envALL = (char *) malloc( 128*sizeof(char *) );
	envPIDC = (char *) malloc(128*sizeof(char *));
	sprintf(envALL,"__PERFEX_COUNT_ALL=%d",count_all);
	putenv(envALL);
	sprintf(envPIDC,"__PERFEX_PIDC=%d",getpid());
	putenv(envPIDC);
      }
    
    /* exec and exit on error */
      execvp(prog_args[0], prog_args); 
      fprintf(output_stream,"failed to exec source %s\n", prog_args[0]);
      exit(1);
    }

    /* (only parent gets to this point) */

    /* set-up sampling itimer */
    if(sampling_period) {
        struct itimerval itime;

        itime.it_interval.tv_sec = sampling_period/1000;
        itime.it_interval.tv_usec = (sampling_period%1000)*1000;
        itime.it_value.tv_sec = sampling_period/1000;
        itime.it_value.tv_usec = (sampling_period%1000)*1000;
        if (setitimer(sampling_itimer, &itime, NULL) < 0) {
            fprintf(output_stream,"Warning: failed to set-up itimer %d\n", sampling_itimer);
            fflush(output_stream);
        }
    }

    do {  /* wait for signals */  } while (wait(&status) != pidc); 

    /* unset sampling itimer */
    if(sampling_period) {
        struct itimerval itime;

        itime.it_interval.tv_sec = 0;
        itime.it_interval.tv_usec = 0;
        itime.it_value.tv_sec = 0;
        itime.it_value.tv_usec = 0;
        if (setitimer(sampling_itimer, &itime, NULL) < 0) {
            fprintf(output_stream,"Warning: failed to unset itimer %d\n", sampling_itimer);
            fflush(output_stream);
        }
    }

    return status;
}

main(int argc,char *argv[])
{
  char *p;
  char *prog_name = argv[0];
  int i,st,re;
  char pad[MAX_EVENT_DESC_LEN];
  char output_file[16384];
  long long mcount0,mcount1,msave0,msave1;
  pid_t pid=getpid();
  int events=0;
  int noway=0;
  FILE* fp;
  size_t name_len;
  char* name;
  Executable_Prog[0] = '\0';

  memset((void *)&cnts, 0, sizeof(hwperf_cntr_t));
  memset((void *)&saved_cnts, 0, sizeof(hwperf_cntr_t));

  for(i=0;i<MAX_EVENT_DESC_LEN;i++) pad[i] = '.';

  argc--; argv++;

  /* get event specifier defaults from environment */
  p = getenv("T5_EVENT0");
  if(p) {
    events = atoi(p);
    if(events < HWPERF_CNT1BASE) {
      EventType0 = events;
    } else {
      EventType1 = events;
    }
  }
  p = getenv("T5_EVENT1");
  if(p) {
      /*      if( events & 0x1 ) EventType1 = atoi(p); 
                 else EventType0 = atoi(p);
      */
    events = atoi(p);
    if(events < HWPERF_CNT1BASE) {
      EventType0 = events;
    } else {
      EventType1 = events;
    }
  }
  

  /* get cpu type information */
  {
    int _cpu_species = 0;
    _cpu_species = system_cpu_mix();
    if(_cpu_species < 0) {
      fprintf(output_stream,"Internal Error: cpu type information not available\n");
      exit(1);
    }
    perfy_options.cpu_species_mix = _cpu_species;
    perfy_options.cpu_majrev = cpu_rev_maj();
#ifdef _BDB
    fprintf(output_stream,"BDB> cpu species: %d\n",perfy_options.cpu_species_mix);
    fprintf(output_stream,"BDB> cpu chip major rev: %d\n",perfy_options.cpu_majrev);
#endif
  }
  /* set up event descriptors */

  if( ! EventDesc ) {
    
    switch ( perfy_options.cpu_species_mix ) {
      
    case CPUSPECIES_PURE_R10000:
      if( perfy_options.cpu_majrev >= 3 ) {
	R10000EventDesc[14] = "ALU/FPU progress cycles";
      }
      EventDesc = &R10000EventDesc[0];
      break;
      
    case CPUSPECIES_PURE_R12000:
      EventDesc = &R12000EventDesc[0];
      break;
      
    case CPUSPECIES_MIXED_R10000_R12000:
      EventDesc = &CommonEventDesc[0];
      break;
      
    default:
      fprintf(output_stream,"No performance counters detected on this system!\n");
      exit(1);
      break;
    }
  } 
  
  /* process arglist */
  while(argc)
    {
      if (argv[0][0] == '-')
	{
	  if (!strcmp(argv[0],"-h"))
	    {
	      Usage();
	      exit(1);
	    }
	  else if (!strcmp(argv[0],"-a"))
	    {
	      count_all = 1;
 fprintf(output_stream,"WARNING: Multiplexing events to project totals--inaccuracy possible\n");
	      argc--; argv++;
	    }
	  else if (!strcmp(argv[0],"-x"))
	    {
	      enable_ex_lvl_cnts = 1;
	      argc--; argv++;
	    }
	  else if (!strcmp(argv[0],"-k"))
	    {
	      enable_ker_lvl_cnts = 1;
	      argc--; argv++;
	    }
	  else if (!strcmp(argv[0],"-T")) 
	    {
	      allow_mixed_species = 1;
	      argc--; argv++;
	    }
	  else if (!strcmp(argv[0],"-v"))
	    {
	      enable_sup_lvl_cnts = 1;
	      argc--; argv++;
	    }
	  else if (!strcmp(argv[0],"-y"))
	    {
	      perfy_options.perfy = 1;
	      argc--; argv++;
	    }
	  else if (!strcmp(argv[0],"-t"))
	    {
	      dump_table(NULL,NULL,output_stream);
	      exit(1);
	      argc--; argv++;
	    }
          else if (!strcmp(argv[0],"-s")) 
            {
	      if(noway & NOWAY_MP) {
		fprintf(output_stream,"Sorry, signals incompatible with per-thread counting\n");
		exit(1);
	      } else {
		counts_enabled = 0;  /* don't start counting until SIGUSR1 */
		using_signals = 1;
		noway |= NOWAY_S;
	      }
              argc--; argv++;
            }
          else if (!strcmp(argv[0],"-mp")) 
            {
	      if(noway & NOWAY_S) {
		fprintf(output_stream,"Sorry, per-thread counting incompatible with signals\n");
		exit(1);
	      } else if(noway & NOWAY_P) {
		fprintf(output_stream,"Sorry, per-thread counting incompatible with sampling\n");
		exit(1);
	      } else {
		is_mp = 1;
		noway |= NOWAY_MP;
	      }
              argc--; argv++;
            }
	  else if (!strcmp(argv[0],"-e"))
	    {
	      if (argc == 1) {
		fprintf(output_stream,"option '%s' needs an argument\n",argv[0]);
		exit(1);
	      } else {
		events = atoi(argv[1]);
		if(events < HWPERF_CNT1BASE) {
		  EventType0 = events;
		} else {
		  EventType1 = events;
		}
		
		/*		if(events & 0x1) EventType1 = atoi(argv[1]); 
				else EventType0 = atoi(argv[1]);
				events++;
		*/
	      }
	      argc -= 2; argv += 2;
	    }
	  else if (!strcmp(argv[0],"-c"))
	    {
	      if (argc == 1) {
		fprintf(output_stream,"option '%s' needs an argument\n",argv[0]);
		exit(1);
	      } else {
		strcpy(cost_file,argv[1]);
		if(load_table(NULL,cost_file) < 0) {
		  fprintf(output_stream,"Couldn't load table from %s--using defaults\n",argv[1]);
                }
	      }
	      argc -= 2; argv += 2;
	    }
	  else if (!strcmp(argv[0],"-o"))
	    {
	      if (argc == 1) {
		fprintf(output_stream,"option '%s' needs an argument\n",argv[0]);
		exit(1);
	      } else {
		fp = fopen(argv[1],"w");
		if( !fp ) {
		  fprintf(output_stream,"couldn't open %s for output\n",argv[1]);
		  exit(1);
		} else {
		  if (fcntl(fileno(fp) , F_SETFL, FAPPEND) == -1) {
		    fprintf(output_stream,"libperfex: cannot set '%s' for append\n",p);
		  } else {
		    if( fcntl(fileno(fp), F_SETFD, FD_CLOEXEC) == -1) {
		    } else {
		      fprintf(output_stream, "perfex output sent to %s\n",argv[1]);
		      output_stream = fp;
		      /* add to environment */ 
		      name_len = strlen(argv[1]);
		      name = ( char* ) malloc((name_len+128)*sizeof(char));
		      sprintf(name,"__PERFEX_OUTPUT_FILE=%s",argv[1]);
		      putenv(name);
		    }
		  }
		}
	      }
	      argc -= 2; argv += 2;
	    }
	  else if (!strcmp(argv[0],"-l"))
	    {
	      if (argc == 1) {
		fprintf(output_stream,"option '%s' needs an argument\n",argv[0]);
		exit(1);
	      } else {
		int q;
		perfy_options.reuse = q = atoi(argv[1]);
		if( q < 0 ) {
		  fprintf(output_stream,"load reuse quantum should be positive\n");
		  exit(1);
		}
	      }
	      argc -= 2; argv += 2;
	    }
	  else if (!strcmp(argv[0],"-p"))
	    {
	      if(noway & NOWAY_MP) {
		fprintf(output_stream,"Sorry, per-thread counting incompatible with sampling\n");
		exit(1);
	      } else {
	        if (argc == 1) {
		  fprintf(output_stream,"option '%s' needs an argument\n",argv[0]);
		  exit(1);
	        } else {
		  int period = atoi(argv[1]);

		  if (period < 1) {
		    fprintf(output_stream,"invalid sampling period\n");
		    exit(1);
		  } else if (period < 10) {
		    if (schedctl(GETNDPRI, 0) == 0) {
		      fprintf(output_stream,"sampling period requires fast clock; check NDPRI for this process\n");
		      exit(1);
		    }
		  } else {
		    sampling_period = period;
		  }
	        }
	      }
	      noway |= NOWAY_P;
	      argc -= 2; argv += 2;
	    }
	  else
	    {
	      fprintf(output_stream,"unknown option '%s'\n",argv[0]);
	      Usage();
	      exit(1);
	    }
	}
      else
	{
	  break;
	}
    }

/* argument bounds check 
 */

  if( (EventType0 >= HWPERF_EVENTMAX || EventType0 < 0) || 
      (EventType1 >= HWPERF_EVENTMAX || EventType1 < 0)  ) { 
    fprintf(output_stream,"Event specifier out of 0-HWPERF_EVENTMAX range\n");
    Usage();
    exit(1);
  }

  /* handle mixed-cpu system complexity */
  if ( perfy_options.cpu_species_mix == CPUSPECIES_MIXED_R10000_R12000 ) {
    if( ! allow_mixed_species ) {
      fprintf(output_stream,"Mixed R10k-R12k cpu systems only allowed under -T (\"trust me\") option\n");
      exit(1);
    }
  }

  /* Only for pure R10000 systems.
   *
   * try to remap if either event 
   * is cycles(0,16) or graduated instructions(15,17) 
   */

  if(perfy_options.cpu_species_mix == CPUSPECIES_PURE_R10000) {
      
    if(EventType0 < HWPERF_CNT1BASE && EventType1 < HWPERF_CNT1BASE) {
      if(EventType0 == 0) EventType0 = 16;
      else if(EventType0 == 15) EventType0=17;
      else if(EventType1 == 0 ) EventType1=16;
      else if(EventType1 == 15 ) EventType1=17;
    }
    if(EventType0 >= HWPERF_CNT1BASE && EventType1 >= HWPERF_CNT1BASE) {
      if(EventType0 == 16 ) EventType0=0;
      else if(EventType0 == 17 ) EventType0=15;
      else if(EventType1 == 16 ) EventType1=0;
      else if(EventType1 == 17 ) EventType1=15;
    }
  }
  

/* set up signal handlers */

  if(sigset(SIGINT,CleanUp) == SIG_ERR) {
    fprintf(output_stream,"Failed to set-up signal handling of SIGINT\n");
    exit(1);
  } 

  if(using_signals) {
    if(sigset(SIGUSR1,start_counting) == SIG_ERR) {
      fprintf(output_stream,"Warning: failed to set-up signal handling of SIGUSR1\n");
      fflush(output_stream);
    }
    if (sigset(SIGUSR2,halt_counting) == SIG_ERR) {
      fprintf(output_stream,"Warning: failed to set-up signal handling of SIGUSR2\n");
      fflush(output_stream);
    }
  }

  /* set sampling signal handler */
  if(sampling_period) {
    if(sigset(sampling_signal,sample_counting) == SIG_ERR) {
      fprintf(output_stream,"Warning: failed to set-up signal handling of %d\n", sampling_signal);
      fflush(output_stream);
    }
  }

  if (argc == 0)
    {
      Usage();
      exit(1);
    }
  { 
    int i;
    strcpy(Executable_Prog,argv[0]);
    i = strlen(argv[0]);
    do {
      if (Executable_Prog[i] == '/') {
	break;
      } else {
	Executable_Prog[i] = '\0';
      }
      i--;
    } while(i != -1);
  }

  /* check and set up counters */
#ifdef _BDB
   fprintf(output_stream,"BDB>%d and %d \n",EventType0,EventType1);
   fprintf(output_stream,"BDB>%s and %s \n", EventDesc[EventType0],EventDesc[EventType1]);
#endif
    if(counts_enabled) {
      if(count_all) {
	gen_start = PFX_start_counters_pid_all(pid);
#ifdef _BDB
     fprintf(output_stream,"BDB> start_counters_pid_all returned %d\n",gen_start);
     fflush(output_stream);
#endif
      } else {
	gen_start = PFX_start_counters_pid( EventType0, EventType1, pid);
#ifdef _BDB
     fprintf(output_stream,"BDB> start_counters_pid returned %d\n",gen_start);
     fflush(output_stream);
#endif

      }
    }
  /* fork child as executable from cmd line */
  Forkit(1,argc,argv);
  if(counts_enabled) {
    if(using_signals) {
      gen_stop = PFX_read_counters_pid( EventType0, &mcount0, EventType1, &mcount1, pidc, 1, 1);
    } else {
      gen_stop = PFX_read_counters_pid( EventType0, &mcount0, EventType1, &mcount1, pid, 1, 1);
#ifdef _BDB
      { int i;
	for(i=0;i<31;i++) {
	  fprintf(output_stream,"BDB> %d: %llu\n",i,(unsigned long long) cnts.hwp_evctr[i]);
	}
      }
#endif

    }
    if(gen_stop != gen_start) {
      fprintf(output_stream,"lost counters! Aborting...\n");
      exit(1);
    }

    SavedCount0 += mcount0; SavedCount1 += mcount1;
    if(count_all) {
	int i;

	for(i = 0; i < NCOUNTERS; i++) {
	    saved_cnts.hwp_evctr[i] += cnts.hwp_evctr[i];
	}
    }
#ifdef _BDB
    fprintf(output_stream,"BDB> read_counters returned %d\n",gen_stop);
    fflush(output_stream);
#endif 
  }
  if(never_got_start_signal && using_signals) {
    fprintf(output_stream,"WARNING: Never got start (USR1) signal\n");
  } else {
  /* print results */
    fprintf(output_stream,"%s\n\n",namec);
    if(count_all) {
      PFX_print_counters_all( &saved_cnts );
    } else {
      PFX_print_counters(EventType0,SavedCount0,EventType1,SavedCount1);
    }
  }
}





