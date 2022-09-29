/*
   testdeck.c
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

void giveup(void)
{
  printf("GIVING UP\n");
  sleep(2); /* to keep serial port alive for tracing on scope */
  exit(2);
}

/* simple sony RS-422 protocol parsing ----------------------------------- */

void send1(TSport wp,
           unsigned char cmd1, unsigned char cmd2,
           unsigned char data1)
{
  unsigned char c;
  stamp_t stamp=0;
  
  tsWrite(wp, &cmd1,  &stamp, 1);
  tsWrite(wp, &cmd2,  &stamp, 1);
  tsWrite(wp, &data1, &stamp, 1);
  
  c = (unsigned char)(cmd1+cmd2+data1); /* checksum */
  tsWrite(wp, &c, &stamp, 1);
}

#define packem(hi, lo)  (((unsigned char)(hi) << 4) | ((unsigned char)(lo)))

void sendtc(TSport wp,
           unsigned char cmd1, unsigned char cmd2,
           int h, int m, int s, int f)
{
  unsigned char hh = packem(h/10, h%10);
  unsigned char mm = packem(m/10, m%10);
  unsigned char ss = packem(s/10, s%10);
  unsigned char ff = packem(f/10, f%10);
  unsigned char c;
  stamp_t stamp=0;
  
  tsWrite(wp, &cmd1,  &stamp, 1);
  tsWrite(wp, &cmd2,  &stamp, 1);
  tsWrite(wp, &ff, &stamp, 1);
  tsWrite(wp, &ss, &stamp, 1);
  tsWrite(wp, &mm, &stamp, 1);
  tsWrite(wp, &hh, &stamp, 1);
  
  c = (unsigned char)(cmd1+cmd2+ff+ss+mm+hh); /* checksum */
  tsWrite(wp, &c, &stamp, 1);
}

int readit_verbose = 1;

unsigned char readit(TSport rp, stamp_t *stamp,
                     stamp_t timeout, int *timedout)
{
  unsigned char c;
  int nfilled = tsGetFilledBytes(rp);

  if (nfilled == 0)
    {
      if (timeout == 0)
        {
          *timedout = 1;
          *stamp = -1;
          return 0xff;
        }
      
      /* wait for timeout */
      {
        int nfds;
        fd_set readset;
        struct timeval tv; /* 1/2 sec means deck is dead */
        tv.tv_sec = timeout / 1000000000LL;
        tv.tv_usec = (timeout - 1000000000LL*tv.tv_sec) / 1000;
        
        FD_ZERO(&readset);
        FD_SET(tsGetFD(rp), &readset);
        tsSetFillPointBytes(rp, 1);
        if ((nfds=select(getdtablehi(), &readset, NULL, NULL, &tv)) < 0)
          perror_exit("read select");
        if (nfds == 0) /* timed out */
          {
            *timedout = 1;
            *stamp = -1;
            return 0xff;
          }
      }
    }
  assert(tsGetFilledBytes(rp) > 0);
  
  tsRead(rp, &c, stamp, 1);
  *timedout = 0;
  if (readit_verbose)
    printf("got 0x%02x at %lld\n", c, *stamp);
  return c;
}

void print_tc(unsigned char tc[4])
{
  printf("%02xh:%02xm:%02xs:%02xf%s", 
         tc[3], tc[2], tc[1]&0x7f, tc[0], (tc[1]&0x80)?"*":"");
}

void print_ub(unsigned char ub[4])
{
  printf("ub%02x:%02x:%02x:%02x", ub[3], ub[2], ub[1], ub[0]);
}

#define CHECK_BAIL() { if (timedout) return 0; }

int readresp(TSport rp, stamp_t timeout)
{
  unsigned char cmd1, cmd2;
  unsigned short cmd;
  unsigned char data[16];
  unsigned char csum, ocsum;
  int ndata, i;
  stamp_t stamp;
  int timedout;
  
  cmd1 = readit(rp, &stamp, timeout, &timedout); CHECK_BAIL();
  cmd2 = readit(rp, &stamp, timeout, &timedout); CHECK_BAIL();
  cmd = (cmd1<<8) | cmd2;
  ndata = (cmd1 & 0x0f);
  csum = cmd1 + cmd2;
  for(i=0; i < ndata; i++)
    {
      data[i] = readit(rp, &stamp, timeout, &timedout); CHECK_BAIL();
      csum += data[i];
    }
  ocsum = readit(rp, &stamp, timeout, &timedout); CHECK_BAIL();
  if (csum != ocsum)
    {
      printf("warning: checksum bad\n");
      giveup();
    }

  switch (cmd)
    {
    case 0x1001: 
      printf("ACK\n");
      break;
    case 0x1112:
      printf("NACK %02x\n", data[0]);
      exit(2);
      break;

    case 0x7400: printf("TIMER-1 tc: "); print_tc(data); 
                 printf("\n");  break;
    case 0x7404: printf("LTC tc: "); print_tc(data); 
                 printf("\n");  break;
    case 0x7804: printf("LTC tc: "); print_tc(data); 
                 printf(" LTC ub: "); print_ub(data+4); 
                 printf("\n");  break;
    case 0x7405: printf("LTC ub: "); print_ub(data); 
                 printf("\n");  break;
    case 0x7406: printf("VITC tc: "); print_tc(data); 
                 printf("\n");  break;
    case 0x7806: printf("VITC tc: "); print_tc(data); 
                 printf(" VITC ub: "); print_ub(data+4); 
                 printf("\n");  break;
    case 0x7407: printf("VITC ub: "); print_ub(data); 
                 printf("\n");  break;
    case 0x700d: printf("no time to report, sorry.");
                 printf("\n");  break;
    case 0x7414: printf("LTC (interp) tc: "); print_tc(data); 
                 printf("\n");  break;
    case 0x7814: printf("LTC (interp) tc: "); print_tc(data); 
                 printf(" LTC (interp) ub: "); print_ub(data+4); 
                 printf("\n");  break;
    case 0x7416: printf("VITC (hold) tc: "); print_tc(data); 
                 printf(" VITC (hold) ub: "); print_ub(data+4); 
                 printf("\n");  break;
    case 0x7816: printf("VITC (hold) tc: "); print_tc(data); 
                 printf("\n");  break;
    default:
      printf("Command %04x: ", cmd);
      for(i=0; i < ndata; i++)
        printf("%02x ", data[i]);
      printf("\n");
      break;
    }

  return 1;
}


int main(int argc, char **argv)
{
  TSport rp, wp;
  char devname[20];
  int c, rc;
  int portnum = 2;
  stamp_t timeout = 10000000; /* 10 ms */

  while ((c = getopt(argc, argv, "p:d:r:")) != EOF) 
    {
      switch(c) 
	{
        case 'p':
          portnum = atoi(optarg);
          break;
        case 'd': /* in millisecons */
          timeout = 1.E6 * atof(optarg);
          break;
        case 'r':
          readit_verbose = atoi(optarg);
          break;
        }
    }

  printf("opening serial port %d\n", portnum);

  sprintf(devname,"/dev/ttyts%d", portnum);

#define QSIZE 100

  {
    TSconfig config = tsNewConfig();
    tsSetPortName(config, devname);
    tsSetQueueSize(config, QSIZE);
    tsSetCflag(config, CS8|PARENB|PARODD);
    tsSetOspeed(config, 38400);

    tsSetDebug(config, TS_INTR_DEBUG|TS_LIB_DEBUG);
    
    tsSetDebug(config, 0 /* TS_INTR_DEBUG|TS_TX_DEBUG|TS_LIB_DEBUG */ );
    
    tsSetDirection(config, TS_DIRECTION_RECEIVE);
    
    if (TS_SUCCESS != (rc=tsOpenPort(config, &rp)))
      error_exit("rx open bailed with %d", rc);
    
    tsSetDirection(config, TS_DIRECTION_TRANSMIT);
    
    if (TS_SUCCESS != (rc=tsOpenPort(config, &wp)))
      error_exit("tx open bailed with %d", rc);
    
    tsFreeConfig(config);
  }

 #if 0
  sendtc(wp, 0x24, 0x31, 1, 2, 14, 0);
  readresp(rp, 1000000000);
  printf("cueued up; press a key...\n");
  getchar();
#else
  printf("rippin now\n");
#endif
  
  while (1)
    {
      stamp_t then, now;
      dmGetUST((unsigned long long *)&then);

      /* send current time sense command */
      
      printf("sending\n"); 
      send1(wp,
            0x61, 0x0c, /* current time sense */
            0x03);
      
      /* read response */
      
      if (!readresp(rp, timeout))
        {
#if 0
          printf("response timed out.  NO DRAIN!!\n");
#elif 0
          printf("response timed out.  fast drain\n"
                 "-------------------------------\n");
          while (tsGetFilledBytes(rp) > 0)
            {
              unsigned char c;
              stamp_t stamp;
              tsRead(rp, &c, &stamp, 1);
              printf("draining: 0x%02x at %lld\n", c, stamp);
            }
#else
          printf("response timed out.  draining rx up to 1 sec\n"
                 "--------------------------------------------\n");
          while (1)
            {
              int timedout;
              stamp_t stamp;
              unsigned char c;
              c = readit(rp, &stamp, 1000000000, &timedout);
              printf("draining: 0x%02x\n", c);
              if (timedout) break;
            }
#endif
        }
              
      /* sleep for rest of desired period */
      dmGetUST((unsigned long long *)&now);
      if (now < then+timeout)
          sginap( CLK_TCK * ((then+timeout-now)/1.E9) );
    }

  return 0;
}
