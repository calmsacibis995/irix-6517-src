/*
 *  libtserialio.c - timestamped, non-STREAMS, low-overhead serial port
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
#include <sys/param.h>
#include <assert.h>
#include <signal.h>

#include <fcntl.h>

#include <ulocks.h>

#include <errno.h>

#include <stropts.h>

#include "tserialio.h"
#include <sys/tserialio_pvt.h>

#define max(a,b) ( ((a)>(b)) ? (a) : (b))
#define min(a,b) ( ((a)<(b)) ? (a) : (b))

/* TS process-global error callback ------------------------------------ */

static void tsDefaultErrorHandler(TSstatus status, const char *message)
{
  /* totally non-thread-safe ... works only because fprintf is locked */
  fprintf(stderr, "TS Error %d: %s\n", status, message);
}

extern TSerrfunc       tsErrorHandler  = tsDefaultErrorHandler;
extern int             tsIncludeFunctionNameInErrorMessage = 1;

TSerrfunc tsSetErrorHandler(TSerrfunc newfunc, int includefuncname)
{
  TSerrfunc oldfunc = tsErrorHandler;
  tsErrorHandler = newfunc;
  tsIncludeFunctionNameInErrorMessage = includefuncname;
  return oldfunc;
}

TSerrfunc tsGetErrorHandler(int *includefuncname /*can be NULL*/)
{
  if (includefuncname)
    *includefuncname = tsIncludeFunctionNameInErrorMessage;
  return tsErrorHandler;
}

void
tserror(char *myname, TSstatus status, const char* fmt, ...)
{
  char stringbuf[1024];
  TSerrfunc handler;
  va_list ap;

  setoserror(status);

  assert(myname);

  va_start(ap,fmt);
  
  if (tsIncludeFunctionNameInErrorMessage)
    {
      strcpy(stringbuf, myname); 
      strcat(stringbuf, ": ");
    }
  else
    stringbuf[0] = 0;
      
  vsprintf(stringbuf+strlen(stringbuf), fmt, ap);

  handler = tsErrorHandler; /* non-thread-safe: must grab it just once */

  if (NULL != handler)
    handler(status, stringbuf); /* may be redefined by user */

  va_end(ap);
}

/* port data struct ---------------------------------------------------- */

typedef struct _TSport
{
  int fd;
  void *mmapaddr;
  urbheader_t *header;          /* pointer to user-mapped header */
  unsigned char *data;          /* pointer to user-mapped rb data */
  stamp_t *stamps;              /* pointer to user-mapped rb timestamps */
  int mmap_totalbytes;
  int nlocs;
  TSconfig config;
} _TSport;

/* config stuff ---------------------------------------------------- */

typedef struct _TSconfig
{
  char *portname;
  char direction;               /* TSIO_TOMIPS or TSIO_FROMMIPS */
  int capacity;
  tcflag_t cflag;
  speed_t ospeed;
  int protocol;
  int extclock_factor;
  int debug;
} _TSconfig;

TSconfig tsNewConfig(void)
{
  static char *myname = "tsNewConfig";
  TSconfig config = malloc(sizeof(_TSconfig));
  if (!config)
    {
      tserror(myname, TS_ERROR_OUT_OF_MEM, "cannot allocate TSconfig");
      return NULL;
    }

  config->portname = NULL;
  config->direction = 0;
  config->capacity = -1;
  config->cflag = B0;
  config->ospeed = 0;
  config->protocol = TS_PROTOCOL_RS232;
  config->extclock_factor = 0;

  return config;
}

TSconfig _tsCopyConfig(char *myname, TSconfig from)
{
  TSconfig config = malloc(sizeof(_TSconfig));
  if (!config)
    {
      tserror(myname, TS_ERROR_OUT_OF_MEM, "cannot allocate TSconfig");
      return NULL;
    }
  
  *config = *from;
  if (config->portname)
    {
      if (NULL == (config->portname = strdup(from->portname)))
        {
          tserror(myname, TS_ERROR_OUT_OF_MEM, "cannot allocate TSconfig");
          free(config);
          return NULL;
        }
    }
  
  return config;
}

TSconfig tsCopyConfig(TSconfig from)
{
  static char *myname = "tsCopyConfig";
  return _tsCopyConfig(myname, from);
}

TSconfig tsNewConfigFromPort(TSport port)
{
  static char *myname = "tsNewConfigFromPort";
  return _tsCopyConfig(myname, port->config);
}

TSstatus tsFreeConfig(TSconfig config)
{
  /* static char *myname = "tsFreeConfig"; */
  if (config->portname)
    {
      free(config->portname);
      config->portname = NULL;
    }
  free(config);
  return TS_SUCCESS;
}

TSstatus tsSetDebug(TSconfig config, int debug)
{
  /* static char *myname = "tsSetDebug"; */
  config->debug = debug;
  return TS_SUCCESS;
}

TSstatus tsSetPortName(TSconfig config, char *name)
{
  static char *myname = "tsSetPortName";
  char *s;
  if (NULL == (s = strdup(name)))
    {
      tserror(myname, TS_ERROR_OUT_OF_MEM, "cannot allocate TSconfig");
      return TS_ERROR_OUT_OF_MEM;
    }
  if (config->portname)
    free(config->portname);
  config->portname = s;
  return TS_SUCCESS;
}
TSstatus tsSetDirection(TSconfig config, int direction)
{
  static char *myname = "tsSetDirection";
  if (direction != TS_DIRECTION_TRANSMIT &&
      direction != TS_DIRECTION_RECEIVE)
    {
      tserror(myname, TS_ERROR_BAD_LIBRARY_CALL, "invalid direction");     
      return TS_ERROR_BAD_LIBRARY_CALL;
    }
  config->direction = direction;
  return TS_SUCCESS;
}
TSstatus tsSetQueueSize(TSconfig config, int queuesize)
{
  static char *myname = "tsSetQueueSize";
  if (queuesize <= 0)
    {
      tserror(myname, TS_ERROR_BAD_LIBRARY_CALL, "invalid queuesize");
      return TS_ERROR_BAD_LIBRARY_CALL;
    }
  config->capacity = queuesize;
  return TS_SUCCESS;
}
TSstatus tsSetCflag(TSconfig config, tcflag_t cflag)
{
  /* static char *myname = "tsSetCflag"; */
  config->cflag = cflag;
  return TS_SUCCESS;
}
TSstatus tsSetOspeed(TSconfig config, speed_t ospeed)
{
  static char *myname = "tsSetOspeed";
  if (ospeed == 0)
    {
      tserror(myname, TS_ERROR_BAD_LIBRARY_CALL, "ospeed==0 not supported");
      return TS_ERROR_BAD_LIBRARY_CALL;
    }
  config->ospeed = ospeed;
  return TS_SUCCESS;
}
TSstatus tsSetProtocol(TSconfig config, int protocol)
{
  static char *myname = "tsSetProtocol";
  if (protocol != TS_PROTOCOL_RS232 &&
      protocol != TS_PROTOCOL_RS422)
    {
      tserror(myname, TS_ERROR_BAD_LIBRARY_CALL, "invalid protocol");
      return TS_ERROR_BAD_LIBRARY_CALL;
    }
  config->protocol = protocol;
  return TS_SUCCESS;
}
TSstatus tsSetExternalClockFactor(TSconfig config, int extclock_factor)
{
  static char *myname = "tsSetExternalClockFactor";
  if (extclock_factor < 0)
    {
      tserror(myname, TS_ERROR_BAD_LIBRARY_CALL, "invalid extclock_factor");
      return TS_ERROR_BAD_LIBRARY_CALL;
    }
  config->extclock_factor = extclock_factor;
  return TS_SUCCESS;
}


/* port routines ---------------------------------------------------- */

TSstatus tsOpenPort(TSconfig config, TSport *returnPort)
{
  static char *myname = "tsOpenPort";
  tsio_acquireurb_t a;
  TSport port;
  int fcntl_flags;
  
  if (!config)
    {
      tserror(myname, TS_ERROR_BAD_LIBRARY_CALL, "must provide TSconfig");
      return TS_ERROR_BAD_LIBRARY_CALL;
    }

  if (!returnPort)
    {
      tserror(myname, TS_ERROR_BAD_LIBRARY_CALL, "must provide TSport*");     
      return TS_ERROR_BAD_LIBRARY_CALL;
    }
  
  if (NULL == (port = malloc(sizeof(_TSport))))
    {
      tserror(myname, TS_ERROR_OUT_OF_MEM, "cannot allocate TSport");
      return TS_ERROR_OUT_OF_MEM;
    }
  
  if (NULL == (port->config = _tsCopyConfig(myname, config)))
    {
      free(port);
      return TS_ERROR_OUT_OF_MEM;
    }

  /* --- check out config */

#define HURL(msg) \
  { \
    tserror(myname, TS_ERROR_BAD_LIBRARY_CALL, msg); \
    tsFreeConfig(port->config); \
    free(port); \
    return TS_ERROR_BAD_LIBRARY_CALL; \
  }

  if (!config->portname) HURL("must specify portname");
  if (config->direction == 0) HURL("must specify direction");
  if (config->capacity == -1) HURL("must specify queuesize");
  if (config->cflag == B0)  HURL("must specify cflag");
  if (config->ospeed == 0)  HURL("must specify ospeed");

  /* --- open port */
  
  port->fd = open(config->portname,O_RDWR);
  if (port->fd < 0)
    {
      tserror(myname, TS_ERROR_OPENING_PORT, "port open failed: %s (%d)", 
              strerror(oserror()), oserror());
      tsFreeConfig(port->config);
      free(port);
      return TS_ERROR_OPENING_PORT;
    }

  fcntl(port->fd,F_GETFD, &fcntl_flags);
  fcntl_flags |= FD_CLOEXEC;
  fcntl(port->fd,F_SETFD, &fcntl_flags);
  
  port->nlocs = config->capacity + 1;
  
  a.nlocs = port->nlocs;
  a.direction = config->direction;
  a.commparams.ospeed = config->ospeed;
  a.commparams.cflag = config->cflag;
  a.commparams.flags = 
    ((config->protocol==TS_PROTOCOL_RS422) ? TSIO_FLAGS_RS422 : 0);
  if (config->debug & TS_INTR_DEBUG)
    a.commparams.flags |= TSIO_FLAGS_INTR_DEBUG;
  if (config->debug & TS_TX_DEBUG)
    a.commparams.flags |= TSIO_FLAGS_TX_DEBUG;
  a.commparams.extclock_factor = config->extclock_factor;

  if (ioctl(port->fd, TSIO_ACQUIRE_RB, &a) < 0)
    {
      tserror(myname, TS_ERROR_OPENING_PORT, "port acquire failed: %s (%d)", 
              strerror(oserror()), oserror());
      close(port->fd);
      tsFreeConfig(port->config);
      free(port);
      return TS_ERROR_OPENING_PORT;
    }

  {
    int dataoff = sizeof(urbheader_t);
    int stampsoff = 
      roundup(dataoff+port->nlocs, __builtin_alignof(stamp_t));
    port->mmap_totalbytes = stampsoff + sizeof(stamp_t)*port->nlocs;
    
    port->mmapaddr = mmap(0,port->mmap_totalbytes,PROT_READ|PROT_WRITE,
                          MAP_SHARED,port->fd,0);
    if (port->mmapaddr == (void *)-1)
      {
        tserror(myname, TS_ERROR_OPENING_PORT, "port map failed: %s (%d)", 
                strerror(oserror()), oserror());
        close(port->fd);
        tsFreeConfig(port->config);
        free(port);
        return TS_ERROR_OPENING_PORT;
      }
    
    port->header = (urbheader_t *)port->mmapaddr;
    port->data = (unsigned char *)((char *)port->mmapaddr + dataoff);
    port->stamps = (stamp_t *)((char *)port->mmapaddr + stampsoff);
  }
  
  *returnPort = port;
  return TS_SUCCESS;
}

TSstatus tsClosePort(TSport port)
{
  /* static char *myname = "tsClosePort"; */
  TSstatus ret = TS_SUCCESS;
  munmap(port->mmapaddr, port->mmap_totalbytes);
  tsFreeConfig(port->config);
  port->config = NULL;
  close(port->fd);
  port->fd = -1;
  free(port);
  return ret;
}

int tsGetQueueSize(TSport port)
{
  /* static char *myname = "tsGetQueueSize"; */
  return port->nlocs - 1;
}

TSstatus tsSetFillPointBytes(TSport port, int fillptval)
{
  static char *myname = "tsSetFillPointBytes";
  volatile stamp_t *fillptp=&port->header->fillpt;
  volatile int *fillunitsp=&port->header->fillunits;
  if (fillptval < 0 || fillptval > port->config->capacity)
    {
      tserror(myname, TS_ERROR_BAD_LIBRARY_CALL, 
              "fillpoint must be between 0 and queuesize");
      return TS_ERROR_BAD_LIBRARY_CALL;
    }
  *fillptp = fillptval;
  *fillunitsp = TSIO_FILLUNITS_BYTES;
  return TS_SUCCESS;
}

int tsGetFillPointBytes(TSport port)
{
  /* static char *myname = "tsGetFillPointBytes"; */
  volatile stamp_t *fillptp=&port->header->fillpt;
  return *fillptp;
}

int tsGetFilledBytes(TSport port)
{
  /* static char *myname = "tsGetFilledBytes"; */
  volatile int *headp=&port->header->head;
  volatile int *tailp=&port->header->tail;
  int head, tail, nfilled;

  head = *headp;
  tail = *tailp;
  
  nfilled = tail-head;
  if(nfilled < 0) nfilled += port->nlocs;

  return nfilled;
}

#ifdef NOTYET
int tsGetFillableBytes(TSport port)
{
  static char *myname = "tsGetFillableBytes";
  volatile int *headp=&port->header->head;
  volatile int *tailp=&port->header->tail;
  int head, tail, nfillable;

  head = *headp;
  tail = *tailp;

  nfillable = head-tail-1;
  if(nfillable < 0) nfillable += port->nlocs;

  return nfillable;
}
#endif

int tsGetFD(TSport port)
{
  /* static char *myname = "tsGetFD"; */
  return port->fd;
}

#define SAFETY 10

TSstatus tsWrite(TSport port, unsigned char *data, stamp_t *stamps, 
                 int nbytes)
{
  static char *myname = "tsWrite";
  volatile int *headp=&port->header->head;
  volatile int *tailp=&port->header->tail;
  int head, tail, nfillable;
  int nwritten = 0;  

  while (nwritten < nbytes)
    {
      int n_to_xfer, i;
#ifdef DEBUG
      int rc;
#endif

      head = *headp;
      tail = *tailp;
      nfillable = head-tail-1;
      if(nfillable < 0) nfillable += port->nlocs;
      while(nfillable == 0) /* if full, block till we reach fillpoint */
        {
          fd_set writefds;
          /* if we only have a few bytes left to enqueue then we only
           * need to wait till there's that many holes in the queue.
           * otherwise (if we still have lots of stuff to enqueue),
           * we let the queue run down just below a SAFETY margin.
           * that margin should give us enough time to enqueue the next
           * batch of data and move the tail before the queue really
           * underflows.
           *
           * we'll unblock when getfilled() < fillpt
           */
          int capacity = port->nlocs - 1;
          int bytes_left_to_enqueue = nbytes - nwritten;
          int fillpt = max(SAFETY, capacity-bytes_left_to_enqueue);
          tsSetFillPointBytes(port, fillpt);
          FD_ZERO(&writefds);
          FD_SET(port->fd, &writefds);
          if ((
#ifdef DEBUG
               rc=
#endif
               select(port->fd+1,0,&writefds,0,0)) < 0 &&
              oserror() != EINTR)
            {
              tserror(myname, TS_ERROR_SELECT_FAILED, 
                      "port select failed: %s (%d)", 
                      strerror(oserror()), oserror());
              return TS_ERROR_SELECT_FAILED;
            }
          head = *headp;
          tail = *tailp;
          nfillable = head-tail-1;
          if(nfillable < 0) nfillable += port->nlocs;
          assert(nfillable > capacity-fillpt);
#ifdef DEBUG
          if (port->config->debug & TS_LIB_DEBUG)
            printf("nfillable=%d rc=%d\n", nfillable, rc);
#endif
          /* if select() failed with EINTR because of a signal, we loop
           * around.  sigh.
           */
        }
      n_to_xfer = min(nfillable, nbytes-nwritten);
      /* we can now write n_to_xfer things into buf starting at tail */
      for(i=0; i < n_to_xfer; i++)
        {
          port->data[tail] = data[nwritten];
          port->stamps[tail] = stamps[nwritten];
          tail++;
          nwritten++;
          if(tail >= port->nlocs) tail -= port->nlocs;
        }
      *tailp=tail; /* update user ring buffer tail value in one shot */
    }
  return TS_SUCCESS;
}

TSstatus tsRead(TSport port, unsigned char *data, stamp_t *stamps, 
                int nbytes)
{
  static char *myname = "tsRead";
  volatile int *headp=&port->header->head;
  volatile int *tailp=&port->header->tail;
  int head, tail, nfilled;
  int nread = 0;

  while (nread < nbytes)
    {
      int n_to_xfer, i;
#ifdef DEBUG
      int rc;
#endif

      head = *headp;
      tail = *tailp;
      nfilled = tail-head;
      if(nfilled < 0) nfilled += port->nlocs;
      while(nfilled == 0) /* if empty, block till we reach fillpoint  */
        {
          fd_set readfds;
          /*
           * if we only have a few bytes left to read, then we only need
           * to wait until those few bytes arrive in the queue.  otherwise
           * (if we still have lots to read), we let the queue fill up
           * to almost full minus a SAFETY margin.  this margin should
           * give us enough time to read out the next batch of data
           * and move the head before the queue really overflows.
           *
           * we'll unblock when getfilled() >= filledwanted
           */
          int capacity = port->nlocs - 1;
          int bytes_left_to_dequeue = nbytes-nread;
          int filledwanted = min(bytes_left_to_dequeue, capacity-SAFETY);
          tsSetFillPointBytes(port, filledwanted);
          FD_ZERO(&readfds);
          FD_SET(port->fd, &readfds);
          if ((
#ifdef DEBUG
               rc=
#endif
               select(port->fd+1,&readfds,0,0,0)) < 0 &&
              oserror() != EINTR)
            {
              tserror(myname, TS_ERROR_SELECT_FAILED, 
                      "port select failed: %s (%d)", 
                      strerror(oserror()), oserror());
              return TS_ERROR_SELECT_FAILED;
            }
          head = *headp;
          tail = *tailp;
          nfilled = tail-head;
          if(nfilled < 0) nfilled += port->nlocs;
          assert(nfilled >= filledwanted);
#ifdef DEBUG
          if (port->config->debug & TS_LIB_DEBUG)
            printf("nfilled=%d rc=%d\n", nfilled, rc);
#endif
          /* if select() failed with EINTR because of a signal, we loop
           * around.  sigh.
           */
        }
      n_to_xfer = min(nfilled, nbytes-nread);
      /* we can now read n_to_xfer from buf starting at head */
      for(i=0; i < n_to_xfer; i++)
        {
          data[nread] = port->data[head];
          if (stamps)
            stamps[nread] = port->stamps[head];
          head++;
          nread++;
          if (head >= port->nlocs)
            head=0;
        }
      *headp = head; /* update user ring buffer head value in one shot */
    }
  return TS_SUCCESS;
}

