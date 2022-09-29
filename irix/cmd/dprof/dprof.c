/*
 *   dprof.c
 *   jlr@sgi.com
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/utsname.h>

static void
Usage(void) 
{
  printf(" dprof [-itimer [-ms n]] \n");
  printf("       [-hwpc [-cntr n] [-ovfl n]]\n");
  printf("       [-pcmin min] [-pcmax max] [-damin min] [-damax max]\n");
  printf("       [-page][-line][-hex][-oct][-double][-int|-word]\n");
  /*
   *  printf("       [-sigusr] [-gui]\n");
   */
  printf("       [-out file] [-pout dplace_file] [-threads_per_mem n]\n");
  printf("       [-verbose]\n");
  printf("         program\n");
}

static char dprof_method[] =		"__DPROF_METHOD_=hwpc\0          ";
static char dprof_counter[] = 		"__DPROF_COUNTER_=0\0             ";
static char dprof_frequency[] = 	"__DPROF_FREQUENCY_=10000\0       ";
static char dprof_granularity[] =	"__DPROF_GRANULARITY_=page\0       ";
static char dprof_pc_min[] =		"__DPROF_PC_MIN_=0\0               ";
static char dprof_pc_max[] = 		"__DPROF_PC_MAX_=18446744073709551615\0        ";
static char dprof_da_min[] =		"__DPROF_DA_MIN_=0\0               ";
static char dprof_da_max[] =		"__DPROF_DA_MAX_=18446744073709551615\0        "; 
static char dprof_outputfile[] = 	"__DPROF_OUTPUTFILE_=stdout\0                                                                                      "; 
static char dprof_placefile[] = 	"__DPROF_PLACEFILE_=/dev/null\0                                                                                       "; 
static char dprof_threads_per_mem[] = 	"__DPROF_THREADS_PER_MEM_=2\0                                                                                       "; 
static char dprof_switch[] = 		"__DPROF_SWITCH_=off\0             ";
static char dprof_gui[] =		"__DPROF_GUI_=off\0                "; 
static char dprof_place[] =		"__DPROF_PLACE_=off\0              "; 
static char dprof_sigusr[] =		"__DPROF_SIGUSR_=off\0             "; 
static char dprof_verbose[] =		"__DPROF_VERBOSE_=off\0             "; 

main(int argc, char *argv[])
{
  char filename[BUFSIZ],buffer[BUFSIZ],*c;
  int has_counters = 0, n,status;
  struct utsname name;
  if (uname(&name) < 0) {
    perror ("Error in uname: ");
    exit (1);
  }
  if ( !strcmp(name.machine, "IP25") || !strcmp(name.machine, "IP27") ){
    sprintf(dprof_method, "__DPROF_METHOD_=%s","hwpc");
    has_counters = 1;
  } else {
    sprintf(dprof_method, "__DPROF_METHOD_=%s","itimer");
  }
  
  argc--; argv++;
  /* defaults */
  if( NULL == (c=getenv("_RLD_LIST"))){
    putenv("_RLD_LIST=DEFAULT:libdprof.so");
  } else {
    strcpy(buffer,"_RLD_LIST=");
    strcat(buffer,c);
    strcat(buffer,":libdprof.so");
    putenv(buffer);
  }
  putenv(dprof_method);
  putenv(dprof_frequency);
  putenv(dprof_counter);
  putenv(dprof_granularity);
  putenv(dprof_pc_min);
  putenv(dprof_pc_max);
  putenv(dprof_da_min);
  putenv(dprof_da_max);
  putenv(dprof_outputfile);
  putenv(dprof_placefile);
  putenv(dprof_switch);
  putenv(dprof_gui);
  putenv(dprof_place);
  putenv(dprof_sigusr);
  putenv(dprof_verbose);
  putenv(dprof_threads_per_mem);

  while(argc)
    {
      if (argv[0][0] == '-')
	{
	  if (!strcmp(argv[0],"-h"))
	    {
	      Usage();
	      exit(1);
	    }
	  else if (!strcmp(argv[0],"-itimer"))
	    {
	      sprintf(dprof_method,"__DPROF_METHOD_=itimer");
	      putenv(dprof_method);
	      argc--; argv++;
	    }
	  else if (!strcmp(argv[0],"-hwpc"))
	    {
	      if(!has_counters){
		fprintf(stderr,"Must have R10k to use %s\n",argv[0]);
		exit(1);
	      }
	      sprintf(dprof_method,"__DPROF_METHOD_=hwpc");
	      putenv(dprof_method);
	      argc--; argv++;
	    }
          else if (!strcmp(argv[0],"-page")) 
            {
	      sprintf(dprof_granularity,"__DPROF_GRANULARITY_=page");
	      putenv(dprof_granularity);
	      argc--; argv++;
            }
          else if (!strcmp(argv[0],"-line")) 
            {
	      sprintf(dprof_granularity,"__DPROF_GRANULARITY_=line");
	      putenv(dprof_granularity);
              argc--; argv++;
            }	
	  else if (!strcmp(argv[0],"-hex")) 
            {
	      sprintf(dprof_granularity,"__DPROF_GRANULARITY_=hex");
	      putenv(dprof_granularity);
              argc--; argv++;
            }
	  else if (!strcmp(argv[0],"-oct")) 
            {
	      sprintf(dprof_granularity,"__DPROF_GRANULARITY_=oct");
	      putenv(dprof_granularity);
              argc--; argv++;
            }
	  else if (!strcmp(argv[0],"-quad")) 
            {
	      sprintf(dprof_granularity,"__DPROF_GRANULARITY_=quad");
	      putenv(dprof_granularity);
              argc--; argv++;
            }
	  else if (!strcmp(argv[0],"-double")) 
            {
	      sprintf(dprof_granularity,"__DPROF_GRANULARITY_=double");
	      putenv(dprof_granularity);
              argc--; argv++;
            }
	  else if (!strcmp(argv[0],"-int")) 
            {
	      sprintf(dprof_granularity,"__DPROF_GRANULARITY_=int");
	      putenv(dprof_granularity);
              argc--; argv++;
            }
	  else if (!strcmp(argv[0],"-word")) 
            {
	      sprintf(dprof_granularity,"__DPROF_GRANULARITY_=word");
	      putenv(dprof_granularity);
              argc--; argv++;
            }
	  else if (!strcmp(argv[0],"-sigusr")) 
            {
	      sprintf(dprof_sigusr,"__DPROF_SIGUSR_=on");
	      putenv(dprof_sigusr);
              argc--; argv++;
            }
	  else if (!strcmp(argv[0],"-verbose")) 
            {
	      sprintf(dprof_verbose,"__DPROF_VERBOSE_=on");
	      putenv(dprof_verbose);
              argc--; argv++;
            }
	  else if (!strcmp(argv[0],"-place")) 
            {
	      sprintf(dprof_place,"__DPROF_PLACE_=on");
	      putenv(dprof_place);
              argc--; argv++;
            }
	  else if (!strcmp(argv[0],"-gui")) 
            {
	      sprintf(dprof_gui,"__DPROF_GUI_=on");
	      putenv(dprof_gui);
              argc--; argv++;
            }
	  else if (!strcmp(argv[0],"-ms"))
	    {
	      if (argc == 1 || 1 != sscanf(argv[1],"%d",&n) ) {
		fprintf(stderr,
			"option '%s' needs an integer argument.\n",argv[0]);
		exit(1);
	      } else {
		n = n*1000 ;/* convert to micro seconds */
		sprintf(dprof_frequency,"__DPROF_FREQUENCY_=%d",n);
		putenv(dprof_frequency);
	      }
	      argc -= 2; argv += 2;
	    }
	  else if (!strcmp(argv[0],"-cntr"))
	    {
	      if(!has_counters){
		fprintf(stderr,"Must have R10k to use %s\n",argv[0]);
		exit(1);
	      }
	      if ( argc == 1 || 1 != sscanf(argv[1],"%d",&n) ) {
		fprintf(stderr,
			"option '%s' needs an integer argument.\n",argv[0]);
		exit(1);
	      } else {
		sprintf(dprof_counter,"__DPROF_COUNTER_=%d",n);
		putenv(dprof_counter);
	      }
	      argc -= 2; argv += 2;
	    }
	  else if (!strcmp(argv[0],"-ovfl"))
	    {
	      if(!has_counters){
		fprintf(stderr,"Must have R10k to use %s\n",argv[0]);
		exit(1);
	      }
	      if ( argc == 1 || 1 != sscanf(argv[1],"%d",&n) ) {
		fprintf(stderr,
			"option '%s' needs an integer argument.\n",argv[0]);
		exit(1);
	      } else {
		sprintf(dprof_frequency,"__DPROF_FREQUENCY_=%d",n);
		putenv(dprof_frequency);
	      }
	      argc -= 2; argv += 2;
	    }
	  else if (!strcmp(argv[0],"-pcmin"))
	    {
	      if (argc == 1 || 1 != sscanf(argv[1],"%d",&n) ) {
		fprintf(stderr,
			"option '%s' needs an integer argument.\n",argv[0]);
		exit(1);
	      } else {
		sprintf(dprof_pc_min,"__DPROF_PC_MIN_=%llu",
			strtoull(argv[1],(char **)NULL,0) );
		putenv(dprof_pc_min);
	      }
	      argc -= 2; argv += 2;  
	    }
	  else if (!strcmp(argv[0],"-pcmax"))
	    {
	      if (argc == 1 || 1 != sscanf(argv[1],"%d",&n) ) {
		fprintf(stderr,
			"option '%s' needs an integer argument.\n",argv[0]);
		exit(1);
	      } else {
		sprintf(dprof_pc_max,"__DPROF_PC_MAX_=%llu",
			strtoull(argv[1],(char **)NULL,0) );
		putenv(dprof_pc_max);
	      } 
	      argc -= 2; argv += 2;
	    }
	  else if (!strcmp(argv[0],"-damin"))
	    {
	      if (argc == 1 || 1 != sscanf(argv[1],"%d",&n) ) {
		fprintf(stderr,"option '%s' needs an argument.",argv[0]);
		exit(1);
	      } else {
		sprintf(dprof_da_min,"__DPROF_DA_MIN_=%llu",
			strtoull(argv[1],(char **)NULL,0) );
		putenv(dprof_da_min);
	      }
	      argc -= 2; argv += 2;
	    }
	  else if (!strcmp(argv[0],"-damax"))
	    {
	      if (argc == 1 || 1 != sscanf(argv[1],"%d",&n) ) {
		fprintf(stderr,"option '%s' needs an argument.",argv[0]);
		exit(1);
	      } else {
		sprintf(dprof_da_max,"__DPROF_DA_MAX_=%llu",
			strtoull(argv[1],(char **)NULL,0) );
		putenv(dprof_da_max);
	      }
	      argc -= 2; argv += 2;
	    }
	  else if (!strcmp(argv[0],"-threads_per_mem"))
	    {
	      if (argc == 1 || 1 != sscanf(argv[1],"%d",&n) ) {
		fprintf(stderr,"option '%s' needs an argument.",argv[0]);
		exit(1);
	      } else {
		sprintf(dprof_threads_per_mem,"__DPROF_THREADS_PER_MEM_=%llu",
			strtoull(argv[1],(char **)NULL,0) );
		putenv(dprof_threads_per_mem);
	      }
	      argc -= 2; argv += 2;
	    }
	  else if (!strcmp(argv[0],"-out"))
	    {
	      if (argc == 1 || 1 != sscanf(argv[1],"%s",filename) ) {
		fprintf(stderr,"option '%s' needs an argument.",argv[0]);
		exit(1);
	      } else {
		sprintf(dprof_outputfile,"__DPROF_OUTPUTFILE_=%s",filename);
		putenv(dprof_outputfile);
	      }
	      argc -= 2; argv += 2;
	    }
	  else if (!strcmp(argv[0],"-pout"))
	    {
	      if (argc == 1 || 1 != sscanf(argv[1],"%s",filename)) {
		fprintf(stderr,"option '%s' needs an argument.",argv[0]);
		exit(1);
	      } else {
		sprintf(dprof_placefile,"__DPROF_PLACEFILE_=%s",filename);
		putenv(dprof_placefile);
	      }
	      argc -= 2; argv += 2;
	    }
	  else
	    {
	      fprintf(stderr,"unknown option '%s'\n",argv[0]);
	      Usage();
	      exit(1);
	    }
	}
      else
	{
	  break;
	}
    }

  if (argc == 0)
    {
      Usage();
      exit(1);
    }
  /* program should be in argv[0] */
  /* fork child as executable from cmd line */
  if( (n=fork()) < 0 ){
    fprintf(stderr,"fork failed.\n");
    exit(1);
  }
  if( n == 0 ){
    setgid(getgid());
    execvp(*argv, argv);
    fprintf(stderr,"Exec of %s failed\n",*argv);
    exit(1);
  }
  signal(SIGINT, SIG_IGN);
  signal(SIGQUIT, SIG_IGN);
  while(wait(&status) != n);
  if((status&0377) != 0)
    fprintf(stderr,"Command terminated abnormally.\n");
  signal(SIGINT, SIG_DFL);
  signal(SIGQUIT, SIG_DFL);
  return 0 ;
}






