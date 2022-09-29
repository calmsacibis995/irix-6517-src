/*
   tsio.c - test driver directly
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

#include <sys/tserialio_pvt.h>

#include "util.h"

#define NJOBS 6

#define OPENINPUT 0
#define OPENOUTPUT 1
#define INPUT 2
#define OUTPUT 3
#define INPUTSELECT 4
#define OUTPUTSELECT 5

int directions[NJOBS] =
{
  TSIO_TOMIPS,
  TSIO_FROMMIPS,
  TSIO_TOMIPS,
  TSIO_FROMMIPS,
  TSIO_TOMIPS,
  TSIO_FROMMIPS
};

int bailout[NJOBS] =
{
  1,
  1,
  0,
  0,
  0,
  0
};

int doselect[NJOBS] =
{
  0,
  0,
  0,
  0,
  1,
  1
};

char *jobs[NJOBS] = 
{ 
  "opening input port",
  "opening output port",
  "doing input on input port",
  "doing output on output port",
  "doing input on input port with select()",
  "doing output on output port with select()"
};

int main(int argc, char **argv)
{
  urbheader_t *header;          /* pointer to user-mapped header */
  unsigned char *data;          /* pointer to user-mapped rb data */
  stamp_t *stamps;              /* pointer to user-mapped rb timestamps */
  int c;
  int fd;
  char devname[20];
  int portnum=2;
  int job=OPENINPUT;
  int debug=1;
  stamp_t output_before = 0;

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
  fd = open(devname, O_RDWR);
  
  if (fd < 0)
    perror_exit("open");
  
#define NLOCS 100 /* includes pad byte */
  
  printf("opened.\n");
  getchar();

  {
    tsio_acquireurb_t a;
    
    a.nlocs = NLOCS;
    a.direction = directions[job];
    a.commparams.ospeed = 9600;
    a.commparams.cflag = CS8;
    a.commparams.flags = debug ? TSIO_FLAGS_INTR_DEBUG : 0;
    a.commparams.extclock_factor = 0;
    
    if (ioctl(fd, TSIO_ACQUIRE_RB, &a) < 0)
      perror_exit("ioctl");
  }

  printf("acquired. hit key to map.\n");
  getchar();

  {
    int dataoff = sizeof(urbheader_t);
    int stampsoff = roundup(dataoff+NLOCS, __builtin_alignof(stamp_t));
    int mmap_totalbytes = stampsoff + sizeof(stamp_t)*NLOCS;
    int *mmapaddr = 
      mmap(0,mmap_totalbytes,PROT_READ|PROT_WRITE,
           MAP_SHARED,fd,0);
    if (mmapaddr == (void *)-1)
      perror_exit("mmap");

    header = (urbheader_t *)mmapaddr;
    data = (unsigned char *)((char *)mmapaddr + dataoff);
    stamps = (stamp_t *)((char *)mmapaddr + stampsoff);
  }

  printf("mapped.  hit key to continue\n");
  getchar();

  if (bailout[job])
    exit(0);
  
  if (directions[job] == TSIO_TOMIPS)
    {
      volatile int *headp=&header->head;
      volatile int *tailp=&header->tail;
      
      printf("inputting now\n");
      
      while (1)
        {
          int i;
          int head = *headp;
          int tail = *tailp;
          int nfilled = tail - head;
          if (nfilled < 0) nfilled += NLOCS;
          
          if (!doselect[job])
            {
              if (nfilled == 0)
                continue;
            }
          else /* block waiting for buffer data */
            {
              int fillpt = NLOCS/2;
              
              if (nfilled < NLOCS/2)
                {
                  fd_set readfds;
                  int rc;
                  struct timeval tv;
                  tv.tv_sec = 0;
                  tv.tv_usec = 500000;
                  printf("sleeping till there's >= %d filled\n", fillpt);
                  while (1)
                    {
                      volatile stamp_t *fillptp=&header->fillpt;
                      volatile int *fillunitsp=&header->fillunits;
                      *fillptp = fillpt;
                      *fillunitsp = TSIO_FILLUNITS_BYTES;
                      FD_ZERO(&readfds);
                      FD_SET(fd, &readfds);
                      if ((rc=select(getdtablehi(), &readfds, 
                                     NULL, NULL, NULL)) < 0)
                        perror_exit("select");
                      
                      head = *headp;
                      tail = *tailp;
                      nfilled = tail - head;
                      if (nfilled < 0) nfilled += NLOCS;
                      if (rc==1)
                        {
                          printf("select fired with %d filled\n", nfilled);
                          break;
                        }
                      else
                        printf("select() timed out with %d filled\n",
                               nfilled);
                    }
                }
            }
          
          for(i=0; i < nfilled; i++)
            {
              printf("got [%c] at ust %lld\n",
                     data[head], stamps[head]);
              head++;
              if (head >= NLOCS) head -= NLOCS;
            }
          
          *headp = head;
        }
    }
  else if (directions[job] == TSIO_FROMMIPS)
    {
      int nstuff = NLOCS*2;
      int i;
      stamp_t first;

#define SKIP 300000000LL
   
      dmGetUST((unsigned long long *)&first);
      first -= output_before;   /* so we test out late data */
      
      printf("outputting now\n");
      
      for(i=0; i < nstuff; i++)
        {
          volatile int *headp=&header->head;
          volatile int *tailp=&header->tail;
          int fillpt = NLOCS/2;
          int head = *headp;
          int tail = *tailp;
          int nfilled = tail - head;
          if (nfilled < 0) nfilled += NLOCS;
          
          if (!doselect[job])
            {
              while (nfilled >= fillpt)
                {
                  head = *headp;
                  tail = *tailp;
                  nfilled = tail - head;
                  if (nfilled < 0) nfilled += NLOCS;
                }
            }
          else /* block waiting for buffer space */
            {
              if (nfilled >= fillpt)
                {
                  fd_set writefds;
                  int rc;
                  struct timeval tv;
                  tv.tv_sec = 0;
                  tv.tv_usec = 500000;
                  printf("sleeping till there's < %d filled\n", fillpt);
                  while (1)
                    {
                      int nfilled;
                      volatile stamp_t *fillptp=&header->fillpt;
                      volatile int *fillunitsp=&header->fillunits;
                      *fillptp = fillpt;
                      *fillunitsp = TSIO_FILLUNITS_BYTES;
                      FD_ZERO(&writefds);
                      FD_SET(fd, &writefds);
                      if ((rc=select(getdtablehi(), 
                                     NULL, &writefds, NULL, &tv)) < 0)
                        perror_exit("select");
                      
                      head = *headp;
                      tail = *tailp;
                      nfilled = tail - head;
                      if (nfilled < 0) nfilled += NLOCS;
                      if (rc==1)
                        {
                          printf("select fired with %d filled\n", nfilled);
                          break;
                        }
                      else
                        printf("select() timed out with %d filled\n",
                               nfilled);
                    }
                }
            }

          c = 65 + (i%26);
          printf("outputting character [%c] now to loc %d\n", c, tail);
              
          data[tail] = c;
          stamps[tail] = first + i*SKIP;
          tail++;
          if (tail >= NLOCS) tail -= NLOCS;
        
          *tailp = tail;
        }
    }

  printf("done with i/o.  hit hey to close\n");
  getchar();
  
  close(fd);

  printf("closed.\n");

  return 0;
}

