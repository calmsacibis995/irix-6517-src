/*
 * ts.c - uses only tserialio
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
  int maplen;
  int limit;
  int echo;
  int bytecount=0;

  /* get user args */

  limit = 10;
  echo = 0;

  while ((c = getopt(argc, argv, "p:l:e")) != EOF) 
    {
      switch(c) 
	{
        case 'p':
          portnum = atoi(optarg);
          break;
        case 'l':
          limit = atoi(optarg);
          break;
        case 'e':
          echo = 1;
          break;
        }
    }

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

  if (echo)
    {
      stamp_t start, now, lastsec;
      dmGetUST((unsigned long long *)&start);
      lastsec = start;
      while (1)
        {
          unsigned char c;
          stamp_t s;
          while (tsGetFilledBytes(rp) > 0)
            {
              tsRead(rp, &c, &s, 1);
              tsWrite(wp, &c, &s, 1);
              bytecount++;
            }
          dmGetUST((unsigned long long *)&now);
          if (limit >= 0 && now-start >= 1000000000LL * limit)
            break;
          if (now > lastsec + 1000000000LL)
            {
              printf("%.2lfsec %d\n", (now-start)/1.E9, bytecount);
              lastsec = now;
            }
          /* wait for 1 second or more chars */
          {
            fd_set readset;
            struct timeval tv;
            tv.tv_sec = 1;
            tv.tv_usec = 0;
            FD_ZERO(&readset);
            tsSetFillPointBytes(rp, 1);
            FD_SET(tsGetFD(rp), &readset);
            select(getdtablehi(), &readset, NULL, 0, &tv);
#if 0
            printf("%.2lfsec\n", now/1.E9);
#endif
          }
        }
    }
  else /* no echo */
    {
      int i;
      for(i=0; limit < 0 || i < limit; i++)
        {
          printf("%d\n", i);
          sleep(1);
        }
    }

  /* then close tsio */
  
  tsClosePort(rp);
  tsClosePort(wp);

  return 0;
}


