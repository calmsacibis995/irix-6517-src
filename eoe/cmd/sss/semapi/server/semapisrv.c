/* --------------------------------------------------------------------------- */
/* -                             SEMAPISRV.C                                 - */
/* --------------------------------------------------------------------------- */
/*                                                                             */
/* Copyright 1992-1999 Silicon Graphics, Inc.                                  */
/* All Rights Reserved.                                                        */
/*                                                                             */
/* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;      */
/* the contents of this file may not be disclosed to third parties, copied or  */
/* duplicated in any form, in whole or in part, without the prior written      */
/* permission of Silicon Graphics, Inc.                                        */
/*                                                                             */
/* RESTRICTED RIGHTS LEGEND:                                                   */
/* Use, duplication or disclosure by the Government is subject to restrictions */
/* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data      */
/* and Computer Software clause at DFARS 252.227-7013, and/or in similar or    */
/* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -     */
/* rights reserved under the Copyright Laws of the United States.              */
/*                                                                             */
/* --------------------------------------------------------------------------- */
/* $Revision: 1.3 $ */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>

#include "semapisrv.h"

/* --------------------------------------------------------------------------- */
/* Internal definitions                                                        */
#define RCVPACK_MAXSIZE (1024*16)
#define INVALID_SOCKET (-1)

static const char szInpSockName[] = "/tmp/.sehdsm.events.sock";  /* SEM API Events Socket name */
static int rcvs = INVALID_SOCKET;

/* ------------------------ semsrv_close ------------------------------------- */
static void semsrv_close(int fd)
{ while(1) { if(!close(fd) || (errno != EINTR)) break; }
}
/* ------------------------ semsrv_unlink ------------------------------------ */
static void semsrv_unlink(const char *fname)
{ while(1) { if(!unlink(fname) || (errno != EINTR)) break; }
}
/* ---------------------- semsrv_close_unlink -------------------------------- */
static void semsrv_close_unlink(int *s,const char *socket_path)
{ semsrv_close(*s); *s = INVALID_SOCKET;
  semsrv_unlink(socket_path);
}
/* ------------------------ make_sockaddr ------------------------------------ */
static int make_sockaddr(struct sockaddr_un *s_saddr,const char *sock_path)
{ int addrlen = 0;
  if(s_saddr)
  { memset((char *)s_saddr,0,sizeof(struct sockaddr_un));
    if(sock_path) strcpy(s_saddr->sun_path,sock_path);
    s_saddr->sun_family = AF_UNIX;
    addrlen = strlen(s_saddr->sun_path) + sizeof(s_saddr->sun_family);
  }
  return addrlen;
}

/* ---------------------- semsrv_opensocket ---------------------------------- */
static int semsrv_opensocket()
{ struct sockaddr_un saddr;
  int i,slen;

  if(rcvs == INVALID_SOCKET)
  { if((rcvs = socket(AF_UNIX,SOCK_DGRAM,0)) >= 0)
    { semsrv_unlink(szInpSockName);
      if(bind(rcvs,(struct sockaddr*)&saddr,make_sockaddr(&saddr,szInpSockName)) < 0)
       semsrv_close_unlink(&rcvs,szInpSockName);
      else
      { i = sizeof(slen); slen = 0;
        if(getsockopt(rcvs,SOL_SOCKET,SO_RCVBUF,&slen,&i) < 0) slen = 0;
        if(slen < (RCVPACK_MAXSIZE+1024))
        { slen = (RCVPACK_MAXSIZE+1024);
          setsockopt(rcvs,SOL_SOCKET,SO_RCVBUF,&slen,sizeof(slen));
        }
      }
    }
    else rcvs = INVALID_SOCKET;
  }
  return rcvs != INVALID_SOCKET;
}

/* -------------------------- semsrvInit ------------------------------------ */
int semsrvInit()
{ rcvs = INVALID_SOCKET;
  return semsrv_opensocket();
}

/* -------------------------- semsrvDone ------------------------------------ */
void semsrvDone()
{ if(rcvs != INVALID_SOCKET) semsrv_close_unlink(&rcvs,szInpSockName);
}

/* ------------------------ semsrvGetEvent ---------------------------------- */
int semsrvGetEvent(EVENTBLK *eb)
{ char buffer[RCVPACK_MAXSIZE+1024];
  int i,l,slen,idx,success_flg;
  char *bp;
  struct sockaddr_un saddr;

  if(!eb || (eb->struct_size != sizeof(EVENTBLK)) || (eb->msgsize <= 0) ||
     !eb->msgbuffer || !semsrv_opensocket()) return 0;

  memset(&saddr,0,sizeof(saddr)); slen = sizeof(saddr);

  l = recvfrom(rcvs,buffer,sizeof(buffer)-1,0,(struct sockaddr*)&saddr,&slen);
  if(l < 0 && errno == EINTR) return 0;
  if(l < 0 || l >= sizeof(buffer))
  { semsrv_close_unlink(&rcvs,szInpSockName); semsrv_opensocket();
    return 0;
  }

  /* check signature */
  if(slen <= sizeof(saddr.sun_family) || !saddr.sun_path[0] || l < 4 ||
     buffer[0] != 'S' || buffer[1] != 'E' || buffer[2] != 'M' || buffer[3] != 'C') return 0;

  buffer[3] = 'E'; buffer[l] = 0;
  if((i = socket(AF_UNIX,SOCK_DGRAM,0)) < 0) return 0;

  /* try find begin of hostname section */
  for(success_flg = 0,idx = 4;buffer[idx] && buffer[idx] != '<';idx++);
  
  /* --------------- extract hostname */
  if(buffer[idx] == '<')
  { for(++idx,eb->hostnamelen = 0;eb->hostnamelen < EVENTBLK_MAXHOSTNAME && buffer[idx] && buffer[idx] != '>';eb->hostnamelen++)
     eb->hostname[eb->hostnamelen] = buffer[idx++];
    eb->hostname[eb->hostnamelen] = 0;
    if(buffer[idx++] != '>')
    { sendto(i,buffer,4,0,(struct sockaddr*)&saddr,slen); semsrv_close(i);
      return 0; /* incorrect end_tag in hostname section */
    }
  }

  /* --------------- extract timestamp */
  if(buffer[idx] == '<')
  { for(++idx,bp = &buffer[idx];buffer[idx] && buffer[idx] != '>';idx++);
    if(buffer[idx] != '>')
    { sendto(i,buffer,4,0,(struct sockaddr*)&saddr,slen); semsrv_close(i);
      return 0; /* incorrect end_tag in timestamp section */
    }
    buffer[idx++] = 0; sscanf(bp,"%lx",&eb->timestamp);
  }

  /* --------------- extract event class */
  if(buffer[idx] == '<')
  { for(++idx,bp = &buffer[idx];buffer[idx] && buffer[idx] != '>';idx++);
    if(buffer[idx] != '>')
    { sendto(i,buffer,4,0,(struct sockaddr*)&saddr,slen); semsrv_close(i);
      return 0; /* incorrect end_tag in event_class section */
    }
    buffer[idx++] = 0; sscanf(bp,"%x",&eb->eclass);
  }

  /* --------------- extract event type */
  if(buffer[idx] == '<')
  { for(++idx,bp = &buffer[idx];buffer[idx] && buffer[idx] != '>';idx++);
    if(buffer[idx] != '>')
    { sendto(i,buffer,4,0,(struct sockaddr*)&saddr,slen); semsrv_close(i);
      return 0; /* incorrect end_tag in event_type section */
    }
    buffer[idx++] = 0; sscanf(bp,"%x",&eb->etype);
  }

  /* --------------- extract event priority/facility */
  if(buffer[idx] == '<')
  { for(++idx,bp = &buffer[idx];buffer[idx] && buffer[idx] != '>';idx++);
    if(buffer[idx] != '>')
    { sendto(i,buffer,4,0,(struct sockaddr*)&saddr,slen); semsrv_close(i);
      return 0; /* incorrect end_tag in priority/facility section */
    }
    buffer[idx++] = 0; sscanf(bp,"%x",&eb->epri);
  }

  /* --------------- extract event counter */
  if(buffer[idx] == '<')
  { for(++idx,bp = &buffer[idx];buffer[idx] && buffer[idx] != '>';idx++);
    if(buffer[idx] != '>')
    { sendto(i,buffer,4,0,(struct sockaddr*)&saddr,slen); semsrv_close(i);
      return 0;
    }
    buffer[idx++] = 0; sscanf(bp,"%x",&eb->msgcnt); success_flg++;
  }

  if(success_flg && (l > idx) && (eb->msgsize > (l - idx)))
  { buffer[3] = 'O';
    eb->msgsize = l - idx;
    memcpy(eb->msgbuffer,&buffer[idx],eb->msgsize);
    eb->msgbuffer[eb->msgsize] = 0;
  }
  sendto(i,buffer,4,0,(struct sockaddr*)&saddr,slen); semsrv_close(i);
  return (buffer[3] == 'O') ? 1 : 0;
}

#if 0
/* --------------------------------------------------------------------------- */
/* Here is a sample code for SEM API Server. */
#include <signal.h>
static int exit_flg = 0;
static int alarm_counter = 0;

/* Signal handler */
static void mykiller(int sign)
{ exit_flg++;
  printf("SEM SERVER:Signal %d accepted!\n",sign);
  signal(sign,mykiller);
}
/* Alarm handler for check signal safety */
static void mytimer(int sign)
{ alarm_counter++;
  signal(SIGALRM,mytimer);
  alarm(1);
}

main()
{ char buffer[1024*20];
  EVENTBLK eb;
  int i,rcnt;

  signal(SIGTERM,mykiller);
  signal(SIGKILL,mykiller);
  signal(SIGTSTP,mykiller);
  signal(SIGSTOP,mykiller);
  signal(SIGINT,mykiller);
  signal(SIGALRM,mytimer);

  if(semsrvInit()) printf("SEM Server init success!\n");
  else
  { printf("Can't init SEM Server\n");
    exit(1);
  }

  alarm(1);

  for(rcnt = 0;!exit_flg;)
  { memset(&eb,0,sizeof(EVENTBLK));
    eb.struct_size = sizeof(EVENTBLK); /* initialize structure size member */
    eb.msgsize     = sizeof(buffer);   /* initialize max input buffer size */
    eb.msgbuffer   = buffer;           /* initialize buffer pointer */

    if(semsrvGetEvent(&eb))
    { printf("--------------------------------- alarms: %d --------------------------------------------\n",alarm_counter);
      printf("%3d: hostlen: %d hostname: \"%s\" timestamp: %lX\n",rcnt++,eb.hostnamelen,eb.hostname,eb.timestamp);
      printf("   Class: 0x%X, Type: 0x%X, Pri: 0x%X, MsgCnt: %d, MsgLen: %d\n\n",eb.eclass,eb.etype,eb.epri,eb.msgcnt,eb.msgsize);
    }
  }
  semsrvDone();
}
#endif
