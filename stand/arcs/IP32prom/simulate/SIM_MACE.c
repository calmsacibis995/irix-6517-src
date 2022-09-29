/*
 * Implement "client" process that acts as ethernet packet source/sink
 * for mace ethernet simulation.
 *
 * Created by a fork/exec from the ethernet interface register simulator.
 * Data "read" from input pipe is a packet to transmit (or a control message).
 * Received packets are written to the output pipel
 * 
 */

#include "SIM.h"
#include <assert.h>
#include <errno.h>
#include <net/raw.h>
#include <netinet/if_ether.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

/* Packet format for TX/RX packets */
typedef struct{
  int code;
  int len;
  char buf[2048];
} Msg_t;

static int run;			/* Server runs */

/* Snoop packet format */

#define ETHERHDRPAD RAW_HDRPAD(sizeof(struct ether_header))

typedef struct _epkt{
  struct snoopheader  snoop;
  char                pad[ETHERHDRPAD];
  struct _x{
    struct ether_header ether;
    char                data[ETHERMTU];
  } x;
} Epkt_t;

#define XLEN(l) (l - offsetof(Epkt_t,x))

/*********
 * dumpBuf
 */
void static
dumpBuf(char* msg, Msg_t* b, int cnt){
  int i, j;
  if (cnt > b->len)
    cnt = b->len;
  fprintf(stderr, " %s [%d of %d]\n", msg, cnt, b->len);
  for (i=0; i<cnt; i+=16){
    fprintf(stderr,"  ");
    for (j=0; j<16 && (i+j)<cnt; j++)
      fprintf(stderr, " %02x", b->buf[i+j]);
    fprintf(stderr, "\n");
  }
  fprintf(stderr,"\n");
}

/***********
 * serviceTX
 */
void static
serviceTX(int s){
  Msg_t t;
  int tmp;

  tmp = read(0,&t.code,sizeof t.code);
  if (tmp==0)
    return;
  assert(tmp == sizeof t.code);
  switch(t.code){
  case 0:
    tmp = read(0,&t.len,sizeof t.len);
    assert(tmp == sizeof t.code);
    tmp = read(0,t.buf,t.len);
    assert(tmp == t.len);
/*    dumpBuf("TX",&t,32); */
    if (write(s,t.buf,t.len) != t.len){
      fprintf(stderr, "  TX: write error: %s\n", strerror(errno));
    }
    break;
  default:
    fprintf(stderr," TX: bad request\n");
  }
}

/************
 * serviceSkt
 */
static void
serviceSkt(int s){
  Msg_t  t;
  Epkt_t epkt;
  int    len;

  len = read(s,&epkt,sizeof epkt);
  if (len==0){
    fprintf(stderr, "  RX: zero length read\n");
    return;
  }
  if (len<0){
    fprintf(stderr, "  RX: read error: %s\n",strerror(errno));
    exit(1);
  }

  t.len = XLEN(len);
  t.code= 0;

  memcpy(t.buf,&epkt.x,t.len);
  /* dumpBuf("RX",&t,32); */
  write(1,&t,t.len + sizeof t.code + sizeof t.len);

}

/**********
 * setupSkt
 */
static int
setupSkt(void){
  int 			s;
  struct sockaddr_raw 	sr;
  struct snoopfilter    sf;
  struct ether_header*  eh;
  int                   on=1;

  /*
   * Open up a raw "snoop" socket that we will used to look at the network.
   */
  if ((s = socket(PF_RAW, SOCK_RAW, RAWPROTO_SNOOP))<0){
    fprintf(stderr, "  SIM-MACE, socket error: %s\n", strerror(errno));
    exit(1);
  }

  sr.sr_family = AF_RAW;
  sr.sr_port = 0;
  strncpy(sr.sr_ifname,"ec0",sizeof sr.sr_ifname);
  if (bind(s,&sr,sizeof sr)<0){
    fprintf(stderr, " SIM-MACE, bind error: %s\n", strerror(errno));
    exit(1);
  }

  /*
   * Set the snoop filter to look at all broadcast packets and
   * all packets sent to our simulated ethernet address
   */
  memset(&sf,0,sizeof(sf));
  eh = RAW_HDR(sf.sf_mask, struct ether_header);
  memset(eh->ether_dhost,0xff,sizeof eh->ether_dhost);
  eh = RAW_HDR(sf.sf_match, struct ether_header);
  memset(eh->ether_dhost,0xff,sizeof eh->ether_dhost);
  if (ioctl(s,SIOCADDSNOOP,&sf)<0){
    fprintf(stderr, "  SIM-MACE, ioctl error: %s\n", strerror(errno));
    exit(1);
  }
  eh->ether_dhost[0] = SERIAL0;
  eh->ether_dhost[1] = SERIAL1;
  eh->ether_dhost[2] = SERIAL2;
  eh->ether_dhost[3] = SERIAL3;
  eh->ether_dhost[4] = SERIAL4;
  eh->ether_dhost[5] = SERIAL5;
  if (ioctl(s,SIOCADDSNOOP,&sf)<0){
    fprintf(stderr, "  SIM-MACE, ioctl error: %s\n", strerror(errno));
    exit(1);
  }

  if (ioctl(s,SIOCSNOOPING,&on)<0){
    fprintf(stderr, "  SIM-MACE, ioctl error: %s\n", strerror(errno));
    exit(1);
  }  
  return s;
}

/************
 * hupHandler
 */
static void
hupHandler(void){
  run++;
}

/******
 * main		Initialize server
 */
int
main(int argc, char** argv){
  int sts;
  int skt;

  signal(SIGHUP,hupHandler);

  fprintf(stderr,"\n  SIM-MACE server [%d:%d,%d:%d]\n",
	  getuid(),geteuid(),getgid(),getegid());


  skt = setupSkt();

  /*
   * IO assigments 0 => tx input from client
   *               1 => rx output to client
   *               2 => stderr
   *             skt => network socket
   */
  while(run==0){
    fd_set rdset;
    FD_ZERO(&rdset);
    FD_SET(0,&rdset);
    FD_SET(skt,&rdset);

    sts=select(skt+1,&rdset,0,0,0);
    if (sts>0){
      if (FD_ISSET(0,&rdset))
	serviceTX(skt);
      if (FD_ISSET(skt,&rdset))
	serviceSkt(skt);
    }
    else if (sts==0)
      break;
    else
      if (errno!=EINTR){
	fprintf(stderr,"SIM-MACE select error: %s\n", sts, strerror(errno));
	break;
      }
  }
  
  fprintf(stderr,"\n  SIM-MACE server exiting\n");
}



