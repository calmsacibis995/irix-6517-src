/*
   testtserialio.c - test library
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

#include "../inc/tserialio.h"
TSstatus tsSetDebug(TSconfig config, int debug);

#include "util.h"

#define NJOBS 4

#define OPENINPUT 0
#define OPENOUTPUT 1
#define INPUT 2
#define OUTPUT 3
#define INPUTSELECT 2
#define OUTPUTSELECT 3

int directions[NJOBS] =
{
  TS_DIRECTION_RECEIVE,
  TS_DIRECTION_TRANSMIT,
  TS_DIRECTION_RECEIVE,
  TS_DIRECTION_TRANSMIT
};

int bailout[NJOBS] =
{
  1,
  1,
  0,
  0,
};

char *jobs[NJOBS] = 
{ 
  "opening input port",
  "opening output port",
  "doing input on input port",
  "doing output on output port"
};

int main(int argc, char **argv)
{
  int c;
  int rc;
  char devname[20];
  int portnum=2;
  int job=OPENINPUT;
  stamp_t output_before = 0;
  TSport port;
  int debug=1;

  while ((c = getopt(argc, argv, "b:p:12IOion")) != EOF) 
    {
      switch(c) 
	{
        case 'p':
          portnum = atoi(optarg);
          break;
        case '1':
          job = OPENINPUT;
          break;
        case '2':
          job = OPENOUTPUT;
          break;
        case 'i':
          job = INPUT;
          break;
        case 'o':
          job = OUTPUT;
          break;
        case 'I':
          job = INPUTSELECT;
          break;
        case 'O':
          job = OUTPUTSELECT;
          break;
        case 'n':
          debug = 0;
          break;
        case 'b':
          output_before = (stamp_t)(1000000000LL * atof(optarg));
          break;
        }
    }

  printf("opening serial port %d, %s\n", portnum, jobs[job]);

  sprintf(devname,"/dev/ttyts%d", portnum);

#define QSIZE 133

  {
    TSconfig config = tsNewConfig();
    tsSetPortName(config, devname);
    tsSetDirection(config, directions[job]);
    tsSetQueueSize(config, QSIZE);
    tsSetCflag(config, CS8);
    tsSetOspeed(config, 9600);
    
    if (debug)
      tsSetDebug(config, TS_INTR_DEBUG|TS_TX_DEBUG|TS_LIB_DEBUG);
    
    if (TS_SUCCESS != (rc=tsOpenPort(config, &port)))
      error_exit("open bailed with %d", rc);

    tsFreeConfig(config);
  }

  printf("port opened.  press a key\n");
  getchar();

  if (bailout[job])
    exit(0);
  
  if (directions[job] == TS_DIRECTION_RECEIVE)
    {
      printf("inputting now\n");
      
      while (1)
        {
          unsigned char data;
          stamp_t stamp;

          tsRead(port, &data, &stamp, 1);

          printf("got [%c] at ust %lld\n", data, stamp);
        }
    }
  else if (directions[job] == TSIO_FROMMIPS)
    {
      int nstuff = QSIZE*2;
      int i;
      stamp_t first;
      
#define SKIP 300000000LL
      
      dmGetUST((unsigned long long *)&first);
      first -= output_before;   /* so we test out late data */
      
      printf("outputting now\n");
      
      for(i=0; i < nstuff; i++)
        {
          unsigned char data;
          stamp_t stamp;
          data = 65 + (i%26);
          stamp = first + i*SKIP;
          printf("outputting character [%c] now\n", data);
          
          tsWrite(port, &data, &stamp, 1);
        }
    }
  
  printf("done with i/o.  hit hey to close\n");
  getchar();
 
  tsClosePort(port);
  printf("closed.\n");

  return 0;
}

