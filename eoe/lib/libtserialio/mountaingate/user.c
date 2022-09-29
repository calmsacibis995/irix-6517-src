/*
 * user.c - sample code for using driver.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <unistd.h>
#include <bstring.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <sys/statfs.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/prctl.h>
#include <assert.h>
#include <signal.h>
#include <sys/param.h>
#include <fcntl.h>
#include <ulocks.h>
#include <errno.h>
#include <stropts.h>
#include <dmedia/dmedia.h>
#include <tserialio.h>

#include "common.h"

#define QSIZE 100

/* Utility Routines ----------------------------------------------- */

void error_exit(char *format, ...)
{
  va_list ap;
  
  va_start(ap, format);
  vfprintf( stderr, format, ap );
  va_end(ap);

  fprintf( stderr, "\n" );
  exit(2);
}

void error(char *format, ...)
{
  va_list ap;
  
  va_start(ap, format);
  vfprintf( stderr, format, ap );
  va_end(ap);

  fprintf( stderr, "\n" );
}

void perror_exit(char *format, ...)
{
  va_list ap;
  
  va_start(ap, format);
  vfprintf( stderr, format, ap );
  va_end(ap);

  fprintf(stderr, ": ");
  perror("");

  fprintf( stderr, "\n" );
  exit(2);
}

void perror2(char *format, ...)
{
  va_list ap;
  
  va_start(ap, format);
  vfprintf( stderr, format, ap );
  va_end(ap);

  fprintf(stderr, ": ");
  perror("");

  fprintf( stderr, "\n" );
}

/* main --------------------------------------------------------------- */

int main(int argc, char **argv)
{
  TSport rp, wp;
  char devname[20];
  int c, rc;
  int portnum = 2;
  int deckfd;
  mappedstructure *mapmem;
  int maplen;
  int limit;

  /* get user args */

  limit = 10;

  while ((c = getopt(argc, argv, "p:l:")) != EOF) 
    {
      switch(c) 
	{
        case 'p':
          portnum = atoi(optarg);
          break;
        case 'l':
          limit = atoi(optarg);
          break;
        }
    }

#ifdef TS
  /* open input and output TSport on that serial port */

  printf("opening serial port %d\n", portnum);
  sprintf(devname,"/dev/ttyts%d", portnum);

  {
    TSconfig config = tsNewConfig();
    tsSetPortName(config, devname);
    tsSetQueueSize(config, QSIZE);
#if 0 /* dumb-terminal-like */
    tsSetCflag(config, CS8);
    tsSetOspeed(config, 9600);
#else /* deck-control-like */
    tsSetCflag(config, CS8|PARENB|PARODD);
    tsSetOspeed(config, 38400);
#endif

    tsSetDirection(config, TS_DIRECTION_RECEIVE);
    
    if (TS_SUCCESS != (rc=tsOpenPort(config, &rp)))
      error_exit("rx open bailed with %d", rc);
    
    tsSetDirection(config, TS_DIRECTION_TRANSMIT);
    
    if (TS_SUCCESS != (rc=tsOpenPort(config, &wp)))
      error_exit("tx open bailed with %d", rc);
    
    tsFreeConfig(config);
  }
#endif

  /* open deck driver */

  sprintf(devname,"/dev/deck%d", portnum);
  
  if ((deckfd = open(devname, O_RDWR)) < 0)
    perror_exit("can't open deck control device [%s]", devname);

  /* map in shared memory area */

  maplen = sizeof(struct mappedstructure);
    
  mapmem = mmap(0, maplen, PROT_READ|PROT_WRITE, MAP_SHARED, deckfd, 0);
  
  if (mapmem == (void *)-1)
    perror_exit("can't map deck control device len %d\n", maplen);

  /* start accessing shared memory */
  {
    int i;
    
    /* access through volatile ptr so compiler doesn't
     * try to "optimize out" subsequent accesses of these
     * locations
     */
    volatile int *foop = &mapmem->foo;
    volatile int *barp = &mapmem->bar;
    
    for(i=0; limit < 0 || i < limit; i++)
      {
        printf("foo=%d  bar=%d\n", *foop, *barp);
        sleep(1);
      }
  }

  /* unmap deck memory */
  
  munmap(mapmem, maplen);
  
  /* close deck first */
  
  close(deckfd);

#ifdef TS
  /* then close tsio */
  
  tsClosePort(rp);
  tsClosePort(wp);
#endif

  return 0;
}


