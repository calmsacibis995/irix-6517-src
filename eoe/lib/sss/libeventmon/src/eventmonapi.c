/* --------------------------------------------------------------------------- */
/* -                             EVMONAPI.C                                  - */
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
/*                                                                             */
/* EventMon API is a set of functions for connecting to and communicating with */
/* EventMon demon. (EventMon demon is a system demon responsible for           */
/* intercepting all system events messages from syslog demon, filtering and    */
/* buffering them.)                                                            */
/* This API allows different applications to communicate with EventMon demon   */
/* to do the following:                                                        */
/*   1. check installed binary image of EventMon demon in system directory     */
/*      (function emapiIsDaemonInstalled());                                   */
/*   2. check in an instance of EventMon demon is running in system memory     */
/*      (function emapiIsDaemonStarted());                                     */
/*   3. declare "unload" event to EventMon demon (function                     */
/*      emapiDeclareDaemonUnload());                                           */
/*   4. declare "reload filter configuration" event to EventMon demon          */
/*      (function emapiDeclareDaemonReloadConfig());                           */
/*   5. send a particular event message to EventMon demon                      */
/*      (function emapiSendEvent(...));                                        */
/*                                                                             */
/* EventMon API functions use two named pipe, provided by EventMon demon, and  */
/* internal commands for communication with EventMon demon. EventMon API uses  */
/* reduced set of error codes in functions.                                    */
/*                                                                             */
/* --------------------------------------------------------------------------- */
/* $Revision: 1.4 $ */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

#include "eventmonapi.h"

/* ------------------------------------------------------------------------- */
/* EventMon daemon commands definition                                       */
#define EVMCMD_STOP    'Q'  /* Priv: Stop (and unload from memory) command */
#define EVMCMD_STAT    'S'  /* Non Priv: Statistics command */
#define EVMCMD_TEST    'T'  /* Non Priv: Test command */
#define EVMCMD_RFILT   'F'  /* Priv: Reload filter info */
#define EVMCMD_POUT    'O'  /* Priv: Kick off (refresh) output subsystem */
#define EVMCMD_AMTON   'A'  /* Priv: Amtickerd "on" command */
#define EVMCMD_AMTOFF  'N'  /* Priv: Amtickerd "off" command */
#define EVMCMD_SETTICK 'J'  /* Priv: Amtickerd "set tick interval" command */
#define EVMCMD_SETSTAT 'E'  /* Priv: Amtickerd "set exec status interval" command */
#define EVMCMD_SETGRP  'G'  /* Priv: Enable "Group Mode" command */
#define EVMCMD_CLRGRP  'M'  /* Priv: Disable "Group Mode" command */
#define EVMCMD_BMSG    'B'  /* Priv: Message command */
#define EVMCMD_BMSGROK 'C'  /* Response: Message command accepted */

/* --------------------------------------------------------------------------- */
/* EventMon daemon Static data                                                 */
static const char szDaemonLocation[]       = "/usr/etc/eventmond";
static const char szEventMonInfoSockPath[] = "/tmp/.eventmond.info.sock";    /* EventMon Info Socket name */
static const char szEventMonCmdSockPath[]  = "/tmp/.eventmond.cmd.sock";     /* EventMon Info Socket name (secured) */
static const char szInpSockName[]          = "/tmp/.eventmond.events.sock";  /* EventMon Events Socket name */
static const char szBindSockNameTemp[]     = "/tmp/.evmapi%lX_%lX.tmp";     /* EventMon API bind socket name (template) */
static unsigned long seqNumber             = 1;                              /* Packet seq. number */

/* ------------------------ evmapi_close ------------------------------------- */
static void evmapi_close(int fd)
{ for(;;)
  { if(!close(fd) || (errno != EINTR)) break;
  }
}
/* ------------------------ evmapi_unlink ------------------------------------ */
static void evmapi_unlink(const char *fname)
{ for(;;)
  { if(!unlink(fname) || (errno != EINTR)) break;
  }
}
/* ------------------------ evmapi_is_priv ----------------------------------- */
static int evmapi_is_priv(void)
{ return (!getuid() && !geteuid()) ? 1 : 0;
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
/* ------------------------ prepare_cmdbuffer -------------------------------- */
static int prepare_cmdbuffer(char *buf,char cmd)
{ buf[0] = 'L'; buf[1] = 'V'; buf[2] = 'A'; buf[3] = cmd; 
  return 4;
}
/* ------------------------- declare_something ------------------------------- */
static int declare_something(int secured,char cmd)
{ struct sockaddr_un su_addr;
  char buf[16];
  int s,l;
  if((s = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) return 0;
  prepare_cmdbuffer(buf,cmd); /* prepare command buffer */
  l = sendto(s,buf,4,0,(struct sockaddr*)&su_addr,make_sockaddr(&su_addr,secured ? szEventMonCmdSockPath : szEventMonInfoSockPath)); close(s);
  return (l == 4);
}
/* --------------------------- evmapi_sleep ---------------------------------- */
static int evmapi_sleep(unsigned long clock_size,unsigned long duration)
{ if(!clock_size) sleep(1);
  else            sginap((long)(clock_size*duration));
  return 1;
}
/* ----------------------- emapiIsDaemonInstalled ---------------------------- */
int EVMONAPI emapiIsDaemonInstalled()
{ struct stat finfo;
  int retcode = 0;
  if(!stat(szDaemonLocation,&finfo))
  { if(S_ISREG(finfo.st_mode) && finfo.st_size) retcode++;
  }  
  return retcode;
}
/* ----------------------- emapiIsDaemonStarted ------------------------------ */
int EVMONAPI emapiIsDaemonStarted()
{ return declare_something(0,EVMCMD_TEST);
}
/*---------------------- emapiDeclareDaemonUnload ---------------------------- */
int EVMONAPI emapiDeclareDaemonUnload()
{ return (!evmapi_is_priv() || !declare_something(0,EVMCMD_TEST)) ? 0 : declare_something(1,EVMCMD_STOP);
}
/* ------------------ emapiDeclareDaemonReloadConfig ------------------------- */
int EVMONAPI emapiDeclareDaemonReloadConfig()
{ return (!evmapi_is_priv() || !declare_something(0,EVMCMD_TEST)) ? 0 : declare_something(1,EVMCMD_RFILT);
}
/* ------------------------ emapiSendEvent------------------------------------ */
int EVMONAPI emapiSendEvent(char *_hostnameFrom,unsigned long time,int etype,int epri,char *eventbuffer)
{ char buf[EVMONAPI_MAXEVENTSIZE+128],rcvbuf[16],tmpSockName[256],hostNameEmpty[2];
  int s,i,msglen,slen,reptcnt,sndmaxloopcnt,glbmaxloopcnt,glbtry_cnt,need_try;
  struct sockaddr_un su_addr;
  struct timeval timeout;
  unsigned long sleeptime;
  char *hostname;
  fd_set fdset;
  int retcode = 0;

  hostNameEmpty[0] = 0;

  if(!eventbuffer || !eventbuffer[0] || !declare_something(0,EVMCMD_TEST)) return 0;
  if((slen = strlen(eventbuffer)) > EVMONAPI_MAXEVENTSIZE) return 0;
  hostname = (_hostnameFrom && _hostnameFrom[0]) ? _hostnameFrom : &hostNameEmpty[0];

  if((sleeptime = sysconf(_SC_CLK_TCK)) != (-1)) sleeptime /= 5;
  else sleeptime = 0;

  /* Adjust max loop counter */
  sndmaxloopcnt = (sleeptime == 0) ? 2 : 10;
  glbmaxloopcnt = (sleeptime == 0) ? 2 : 6;

  /* Prepare message for EventMon */
  msglen = time ? sprintf(buf,"#%0lX<%d><%s>legalov:|$(0x%lX)",time,epri,hostname,(unsigned long)etype) :
                  sprintf(buf,"#<%d><%s>legalov:|$(0x%lX)",epri,hostname,(unsigned long)etype);

  if(!msglen) return 0; /* some sprintf error */

  msglen += slen;
  if(msglen >= sizeof(buf)) return 0; /* incorrect buffer size */

  /* add message body */
  strcat(buf,eventbuffer);

  for(need_try = 1,glbtry_cnt = 0;glbtry_cnt < glbmaxloopcnt && need_try && !retcode;glbtry_cnt++)
  { need_try = 0;

    /* Open UDP socket */
    if((s = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
    { need_try = evmapi_sleep(sleeptime,1); continue;
    }

    /* Create unique file name for UNIX socket */
    (void)snprintf(tmpSockName,sizeof(tmpSockName),szBindSockNameTemp,(unsigned long)getpid(),seqNumber++);

    /* Unlink file name */
    evmapi_unlink(tmpSockName);

    /* Bind socket */
    if(bind(s,(struct sockaddr*)&su_addr,make_sockaddr(&su_addr,tmpSockName)) < 0)
    { evmapi_close(s); need_try = evmapi_sleep(sleeptime,1); continue;
    }

    /* Adjust size of "send" buffer */
    i = sizeof(reptcnt);
    if(getsockopt(s,SOL_SOCKET,SO_SNDBUF, &reptcnt,&i) < 0) reptcnt = 0;
    if(reptcnt < (EVMONAPI_MAXEVENTSIZE+1024))
    { reptcnt = (EVMONAPI_MAXEVENTSIZE+1024); setsockopt(s,SOL_SOCKET,SO_SNDBUF, &reptcnt,sizeof(reptcnt));
    }

    /* Adjust size of "receive" buffer */
    i = sizeof(reptcnt);
    if(getsockopt(s,SOL_SOCKET,SO_RCVBUF, &reptcnt,&i) < 0) reptcnt = 0;
    if(reptcnt < (1024*8))
    { reptcnt = (1024*8); setsockopt(s,SOL_SOCKET,SO_RCVBUF,&reptcnt,sizeof(reptcnt));
    }

    /* Create correct socket address */
    slen = make_sockaddr(&su_addr,szInpSockName);

    /* Try to send message to eventmon */
    for(reptcnt = (retcode = 0);reptcnt < sndmaxloopcnt;reptcnt++)
    { if((i = sendto(s,buf,msglen,0,(struct sockaddr*)&su_addr,slen)) == msglen) { retcode++; break; }
      if(i <= 0 && errno == ENOBUFS) evmapi_sleep(sleeptime,4);
      else break;
    }

    /* Check "send message" retcode */
    if(!retcode)
    { evmapi_close(s); evmapi_unlink(tmpSockName);
      continue;
    }

    /* Prepare data for select */
    FD_ZERO(&fdset); FD_SET(s,&fdset);
    timeout.tv_sec = 3; timeout.tv_usec = 0;

    /* Try to do select */
    for(retcode = (reptcnt = 0);reptcnt < 2;)
    { if((i = select((int)s+1,&fdset,0,0,&timeout)) <= 0)
      { if(!i) { reptcnt++; continue; }
        if(i < 0 && errno == EINTR) continue;
        break;
      }
      if(FD_ISSET(s,&fdset)) { retcode++; break; }
    }

    /* Check select retcode */
    if(!retcode)
    { FD_CLR(s,&fdset); evmapi_close(s); evmapi_unlink(tmpSockName);
      break;
    }

    /* Try to receive response if transfer was ok */
    for(retcode = 0;;)
    { memset(&su_addr,0,sizeof(su_addr)); slen = sizeof(su_addr);
      i = recvfrom(s,rcvbuf,sizeof(rcvbuf),0,(struct sockaddr*)&su_addr,&slen);
      if(i < 0 && errno == EINTR) continue;
      if((i == 4) && (rcvbuf[0] == 'L') && (rcvbuf[1] == 'V') && (rcvbuf[2] == 'A'))
      { if(rcvbuf[3] != 'O') need_try = evmapi_sleep(sleeptime,4); /* I'll be back */
        else retcode++; /* It's Ok! */
      }
      break;
    } /* end for */
    FD_CLR(s,&fdset); evmapi_close(s); evmapi_unlink(tmpSockName);
  }
  return retcode;
}
/* --------------------------- emapiSendMessage ------------------------------ */
int EVMONAPI emapiSendMessage(const char *msgbuf,int msgsize,char *responsebuf,int *respbufsize)
{ char buf[EVMONAPI_MAXEVENTSIZE+128],tmpSockName[256];
  int s,i,slen,reptcnt,sndmaxloopcnt;
  struct sockaddr_un su_addr;
  unsigned long sleeptime;
  struct timeval timeout;
  fd_set fdset;
  int retcode = 0;

  if(!msgbuf || !msgsize || msgsize > EVMONAPI_MAXEVENTSIZE || (responsebuf && !respbufsize)) return 0;

  if((sleeptime = sysconf(_SC_CLK_TCK)) != (-1)) sleeptime /= 5;
  else sleeptime = 0;

  /* Adjust max loop counter */
  sndmaxloopcnt = (sleeptime == 0) ? 2 : 10;

  i = prepare_cmdbuffer(buf,EVMCMD_BMSG);
  memcpy(&buf[i],msgbuf,msgsize);
  msgsize += i;

  /* Open UNIX/UDP socket */
  if((s = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) return 0;

  /* Create unique file name for UNIX socket */
  (void)snprintf(tmpSockName,sizeof(tmpSockName),szBindSockNameTemp,(unsigned long)getpid(),seqNumber++);

  /* Unlink file name */
  evmapi_unlink(tmpSockName);

  /* Bind socket */
  if(bind(s,(struct sockaddr*)&su_addr,make_sockaddr(&su_addr,tmpSockName)) < 0)
  { evmapi_close(s);
    return 0;
  }

  /* Adjust size of "send" buffer */
  i = sizeof(slen);
  if(getsockopt(s,SOL_SOCKET,SO_SNDBUF, &reptcnt,&i) < 0) slen = 0;
  if(slen < (EVMONAPI_MAXEVENTSIZE+1024))
  { slen = (EVMONAPI_MAXEVENTSIZE+1024); setsockopt(s,SOL_SOCKET,SO_SNDBUF, &slen,sizeof(slen));
  }

  /* Create correct socket address */
  slen = make_sockaddr(&su_addr,szEventMonCmdSockPath);

  /* Try to send message to eventmon */
  for(reptcnt = (retcode = 0);reptcnt < sndmaxloopcnt;reptcnt++)
  { if((i = sendto(s,buf,msgsize,0,(struct sockaddr*)&su_addr,slen)) == msgsize) { retcode++; break; }
    if(i <= 0 && errno == ENOBUFS) evmapi_sleep(sleeptime,4);
  }

  /* Check "send message" retcode */
  if(!retcode)
  { evmapi_close(s); evmapi_unlink(tmpSockName);
    return 0;
  }

  /* Prepare data for select */
  FD_ZERO(&fdset); FD_SET(s,&fdset);
  timeout.tv_sec = 3; timeout.tv_usec = 0;

  /* Try to do select */
  for(retcode = (reptcnt = 0);reptcnt < 2;)
  { if((i = select((int)s+1,&fdset,0,0,&timeout)) <= 0)
    { if(!i) { reptcnt++; continue; }
      if(i < 0 && errno == EINTR) continue;
      break;
    }
    if(FD_ISSET(s,&fdset)) { retcode++; break; }
  }

  /* Check select retcode */
  if(retcode)
  { /* Try to receive response if transfer was ok */
    for(retcode = 0;;)
    { memset(&su_addr,0,sizeof(su_addr)); slen = sizeof(su_addr);
      i = recvfrom(s,buf,sizeof(buf),0,(struct sockaddr*)&su_addr,&slen);
      if(i < 0 && errno == EINTR) continue;
      if((i >= 4) && (buf[0] == 'L') && (buf[1] == 'V') &&
         (buf[2] == 'A') && (buf[3] == EVMCMD_BMSGROK))
      { retcode++;
        if(responsebuf && *respbufsize > 0)
	{ if(*respbufsize > (i - 4)) *respbufsize = (i - 4);
	  memcpy(responsebuf,&buf[4],*respbufsize);
	}
      }
      break;
    } /* end for */
  }

  FD_CLR(s,&fdset);
  evmapi_close(s);
  evmapi_unlink(tmpSockName);

  return retcode;
}

#if 0
/* ---------------------------------------------------------------------------- */
/*                           EventMon API Test code                             */
/* ---------------------------------------------------------------------------- */
#include <time.h>
main()
{ int i,j;
  char buf[2048];
  printf("emapiIsDaemonInstalled() = %d\n",emapiIsDaemonInstalled());
  printf("emapiIsDaemonStarted() = %d\n",emapiIsDaemonStarted());
/*  printf("emapiDeclareDaemonUnload() = %d\n",emapiDeclareDaemonUnload()); */
/*  printf("emapiDeclareDaemonReloadConfig() = %d\n",emapiDeclareDaemonReloadConfig()); */
  printf("emapiSendEvent(...) = %d\n",emapiSendEvent(NULL,time(NULL),254,0,"Hello \"Peace of mind\""));
  j = sizeof(buf);
  if((i = emapiSendMessage("Privet Rebyata",14,buf,&j)) != 0)
  { if(j)
    { buf[j] = 0;
      printf("emapiSendMessage(const char *msgbuf,int msgsize,buf,sizeof(buf)) = %d,%d - \"%s\"\n",i,j,buf);
    }
    else printf("emapiSendMessage(const char *msgbuf,int msgsize,buf,sizeof(buf)) = %d,%d,   Response buffer is empty!\n",i,j);
  }
  else printf("emapiSendMessage(const char *msgbuf,int msgsize,buf,sizeof(buf)) - Error\n");
}
/* ------------------------------------------------------------------------------ */
#endif
