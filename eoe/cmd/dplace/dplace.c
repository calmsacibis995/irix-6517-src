/*
 *   dplace.c
 *   jlr@sgi.com
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/sysinfo.h> 
#include <sys/sysmp.h> 

static void
Usage(void) 
{
  printf(" dplace [-place placement_file]\n");
  printf("        [-data_pagesize n]\n");
  printf("        [-data_lpage_wait [off|on]\n");
  printf("        [-stack_pagesize n]\n");
  printf("        [-stack_lpage_wait [off|on]\n");
  printf("        [-text_pagesize n]\n");
  printf("        [-text_lpage_wait [off|on]\n");
  printf("        [-migration [off|on]]\n");
  printf("        [-migration_level n]\n");
  printf("        [-propagate]\n");
  printf("        [-mustrun]\n");
  printf("        [-mandatory]\n");
  printf("        [-v[erbose]]\n");
  printf("        program [program-args]\n");
}

static void
check_pagesize(int n){
  lpg_stat_info_t lpg_stat_buf; 
  if (sysmp(MP_SAGET, MPSA_LPGSTATS, (char *)&lpg_stat_buf,
	    sizeof(lpg_stat_buf)) == -1) { 
    perror("sysmp"); 
    exit(1); 
  } 

  if ( n ==    16384 && lpg_stat_buf.enabled_16k ) return; 
  if ( n ==    65536 && lpg_stat_buf.enabled_64k ) return; 
  if ( n ==   262144 && lpg_stat_buf.enabled_256k ) return; 
  if ( n ==  1048576 && lpg_stat_buf.enabled_1m ) return; 
  if ( n ==  4194304 && lpg_stat_buf.enabled_4m ) return; 
  if ( n == 16777216 && lpg_stat_buf.enabled_16m ) return; 
  fprintf(stderr,"%d is not a valid page size.\n",n);
  fprintf(stderr,"Enabled page sizes on this system are: %s%s%s%s%s%s.\n",
	  lpg_stat_buf.enabled_16k?"16k ":"",
	  lpg_stat_buf.enabled_64k?"64k ":"",
	  lpg_stat_buf.enabled_256k?"256k ":"",
	  lpg_stat_buf.enabled_1m?"1m ":"",
	  lpg_stat_buf.enabled_4m?"4m ":"",
	  lpg_stat_buf.enabled_16m?"16m ":"");
  exit(1);
}

static char dplace_dpagesize[] =	"__DPLACE_DPAGESIZE_=16384\0       ";
static char dplace_dpagewait[] =	"__DPLACE_DPAGEWAIT_=off\0       ";
static char dplace_spagesize[] =	"__DPLACE_SPAGESIZE_=16384\0       ";
static char dplace_spagewait[] =	"__DPLACE_SPAGEWAIT_=off\0       ";
static char dplace_tpagesize[] =	"__DPLACE_TPAGESIZE_=16384\0       ";
static char dplace_tpagewait[] =	"__DPLACE_TPAGEWAIT_=off\0       ";
static char dplace_migration[] =	"__DPLACE_MIGRATION_=off\0       ";
static char dplace_migration_level[] =	"__DPLACE_MIGRATION_LEVEL_=0\0       ";
static char dplace_placefile[] = 	"__DPLACE_PLACEFILE_=/dev/null\0                                                                       "; 
static char dplace_verbose[] =		"__DPLACE_VERBOSE_=off\0           "; 
static char dplace_inherit[] =		"__DPLACE_INHERIT_=off\0             ";
static char dplace_parent[] =		"__DPLACE_PARENT_=xxxxx\0             "; 

static void
get_page_environ(void){
  char *c;
  int n;

  if( c = getenv("PAGESIZE_DATA") ){
    n = 1024*atol(c);
    check_pagesize(n);
    sprintf(dplace_dpagesize,"__DPLACE_DPAGESIZE_=%d",n);
  }

  if( c = getenv("PAGESIZE_STACK") ){
    n = 1024*atol(c);
    check_pagesize(n);
    sprintf(dplace_spagesize,"__DPLACE_SPAGESIZE_=%d",n);
  }

  if( c = getenv("PAGESIZE_TEXT") ){
    n = 1024*atol(c);
    check_pagesize(n);
    sprintf(dplace_tpagesize,"__DPLACE_TPAGESIZE_=%d",n);
  }
}

main(int argc, char *argv[])
{
  char filename[BUFSIZ],buffer[BUFSIZ],*c;
  int n,status;

  argc--; argv++;
  /* defaults */

  putenv("_DSM_OFF=dplace"); /* turn off libmp interference */
  putenv("MPI_DSM_OFF=dplace"); /* turn off libmpi interference */
  if ( NULL == (c=getenv("_RLD_LIST"))){
    putenv("_RLD_LIST=DEFAULT:libdplace.so");
  } else {
    strcpy(buffer,"_RLD_LIST=");
    strcat(buffer,c);
    strcat(buffer,":libdplace.so");
    putenv(buffer);
  }
  get_page_environ();
  putenv(dplace_dpagesize);
  putenv(dplace_spagesize);
  putenv(dplace_tpagesize);
  putenv(dplace_dpagewait);
  putenv(dplace_spagewait);
  putenv(dplace_tpagewait);
  putenv(dplace_migration);
  putenv(dplace_migration_level);
  putenv(dplace_placefile);
  putenv(dplace_verbose);
  putenv(dplace_inherit);
  sprintf(dplace_parent,"__DPLACE_PARENT_=%d",getpid());
  putenv(dplace_parent);

  while(argc)
    {
      if (argv[0][0] == '-')
	{
	  if (!strcmp(argv[0],"-h"))
	    {
	      Usage();
	      exit(1);
	    }

	  else if (!strcmp(argv[0],"-verbose")) 
            {
	      sprintf(dplace_verbose,"__DPLACE_VERBOSE_=on");
	      putenv(dplace_verbose);
              argc--; argv++;
            }
	  else if (!strcmp(argv[0],"-v")) 
            {
	      sprintf(dplace_verbose,"__DPLACE_VERBOSE_=on");
	      putenv(dplace_verbose);
              argc--; argv++;
            }
	  else if (!strcmp(argv[0],"-mustrun")) 
            {
	      putenv("__DPLACE_MUSTRUN_=");
              argc--; argv++;
            }
	  else if (!strcmp(argv[0],"-mandatory")) 
            {
	      putenv("__DPLACE_MANDATORY_=");
              argc--; argv++;
            }
	  else if (!strcmp(argv[0],"-propagate")) 
            {
	      sprintf(dplace_inherit,"__DPLACE_INHERIT_=on");
	      putenv(dplace_inherit);
              argc--; argv++;
            }

	  else if (!strcmp(argv[0],"-place"))
	    {
	      if (argc == 1 || 1 != sscanf(argv[1],"%s",filename) ) {
		fprintf(stderr,
			"option '%s' needs an filename argument.\n",argv[0]);
		exit(1);
	      } else {
		sprintf(dplace_placefile,"__DPLACE_PLACEFILE_=%s",filename);
		putenv(dplace_placefile);
	      }
	      argc -= 2; argv += 2;
	    }
	  else if (!strcmp(argv[0],"-migration"))
	    {
	      if ( argc == 1 || (0 != strcmp(argv[1],"on") && 
				 0 != strcmp(argv[1],"off") &&
				 1 != sscanf(argv[1],"%d",&n) ) ) {
		fprintf(stderr,
			"option '%s' needs an 'on' or 'off' argument.\n",argv[0]);
		exit(1);
	      } else {
		sprintf(dplace_migration,"__DPLACE_MIGRATION_=%s",argv[1]);
		putenv(dplace_migration);
	      } 
	      argc -= 2; argv += 2;
	    }
	  else if (!strcmp(argv[0],"-migration_level"))
	    {
	      if (argc == 1 || 1 != sscanf(argv[1],"%d",&n)  ) {
		fprintf(stderr,
			"option '%s' needs an integer argument.\n",argv[0]);
		exit(1);
	      } else if ( n >= 0 && n <= 100){
		sprintf(dplace_migration_level,"__DPLACE_MIGRATION_LEVEL_=%d",n);
		putenv(dplace_migration_level);
	      } else {
		fprintf(stderr,
			"option '%s' needs an integer argument in the range [0,100].\n",argv[0]);
		exit(1);
	      }
	      argc -= 2; argv += 2;
	    }
	  else if (!strcmp(argv[0],"-data_pagesize"))
	    {
	      if ( argc == 1 || 1 != sscanf(argv[1],"%d",&n) ) {
		fprintf(stderr,
			"option '%s' needs an integer argument.\n",argv[0]);
		exit(1);
	      } else {/* add k,m stuff here */
		char c;
		if ( 2 == sscanf(argv[1],"%d %c",&n,&c) ){
		  if ( 'k' == c || 'K' == c ) n *= 1024;
		  if ( 'm' == c || 'M' == c ) n *= 1024*1024;
		}
		check_pagesize(n);
		sprintf(dplace_dpagesize,"__DPLACE_DPAGESIZE_=%d",n);
		putenv(dplace_dpagesize);
	      }
	      argc -= 2; argv += 2;
	    }
	  else if (!strcmp(argv[0],"-data_lpage_wait"))
	    {
	      if ( argc == 1 || (0 != strcmp(argv[1],"on") && 
				 0 != strcmp(argv[1],"off") &&
				 1 != sscanf(argv[1],"%d",&n) ) ) {
		fprintf(stderr,
			"option '%s' needs an 'on' or 'off' argument.\n",argv[0]);
		exit(1);
	      } else {
		sprintf(dplace_dpagewait,"__DPLACE_DPAGEWAIT_=%s",argv[1]);
		putenv(dplace_dpagewait);
	      } 
	      argc -= 2; argv += 2;
	    }
	  else if (!strcmp(argv[0],"-stack_pagesize"))
	    {
	      if ( argc == 1 || 1 != sscanf(argv[1],"%d",&n) ) {
		fprintf(stderr,
			"option '%s' needs an integer argument.\n",argv[0]);
		exit(1);
	      } else {
		char c;
		if ( 2 == sscanf(argv[1],"%d %c",&n,&c) ){
		  if ( 'k' == c || 'K' == c ) n *= 1024;
		  if ( 'm' == c || 'M' == c ) n *= 1024*1024;
		}
		check_pagesize(n);
		sprintf(dplace_spagesize,"__DPLACE_SPAGESIZE_=%d",n);
		putenv(dplace_spagesize);
	      }
	      argc -= 2; argv += 2;
	    }
	  else if (!strcmp(argv[0],"-stack_lpage_wait"))
	    {
	      if ( argc == 1 || (0 != strcmp(argv[1],"on") && 
				 0 != strcmp(argv[1],"off") &&
				 1 != sscanf(argv[1],"%d",&n) ) ) {
		fprintf(stderr,
			"option '%s' needs an 'on' or 'off' argument.\n",argv[0]);
		exit(1);
	      } else {
		sprintf(dplace_spagewait,"__DPLACE_SPAGEWAIT_=%s",argv[1]);
		putenv(dplace_spagewait);
	      } 
	      argc -= 2; argv += 2;
	    }
	  else if (!strcmp(argv[0],"-text_pagesize"))
	    {
	      if ( argc == 1 || 1 != sscanf(argv[1],"%d",&n) ) {
		fprintf(stderr,
			"option '%s' needs an integer argument.\n",argv[0]);
		exit(1);
	      } else {
		char c;
		if ( 2 == sscanf(argv[1],"%d %c",&n,&c) ){
		  if ( 'k' == c || 'K' == c ) n *= 1024;
		  if ( 'm' == c || 'M' == c ) n *= 1024*1024;
		}
		check_pagesize(n);
		sprintf(dplace_tpagesize,"__DPLACE_TPAGESIZE_=%d",n);
		putenv(dplace_tpagesize);
	      }
	      argc -= 2; argv += 2;
	    }
	  else if (!strcmp(argv[0],"-text_lpage_wait"))
	    {
	      if ( argc == 1 || (0 != strcmp(argv[1],"on") && 
				 0 != strcmp(argv[1],"off") &&
				 1 != sscanf(argv[1],"%d",&n) ) ) {
		fprintf(stderr,
			"option '%s' needs an 'on' or 'off' argument.\n",argv[0]);
		exit(1);
	      } else {
		sprintf(dplace_tpagewait,"__DPLACE_SPAGEWAIT_=%s",argv[1]);
		putenv(dplace_tpagewait);
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
  /* program should be in argv[0] */
  /* fork child as executable from cmd line */
  sigset(SIGPROF,SIG_IGN); /* for dprof */
  if ( (n=fork()) < 0 ){
    fprintf(stderr,"fork failed.\n");
    exit(1);
  }
  if ( n == 0 ){
    setgid(getgid());
    execvp(*argv, argv);
    fprintf(stderr,"Exec of %s failed\n",*argv);
    exit(1);
  }
  signal(SIGINT, SIG_IGN);
  signal(SIGQUIT, SIG_IGN);
  while(wait(&status) != n);
  if ((status&0377) != 0)
    fprintf(stderr,"Command terminated abnormally.\n");
  signal(SIGINT, SIG_DFL);
  signal(SIGQUIT, SIG_DFL);
  return 0 ;
}
