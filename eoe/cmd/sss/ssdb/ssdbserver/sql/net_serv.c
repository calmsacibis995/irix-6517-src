/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

/* Write and read of logical packets to/from socket
** Writes are cached into net_buffer_length big packets.
** Read packets are reallocated dynamicly when reading big packets.
** Each logical packet has the following pre-info:
** 3 byte length & 1 byte package-number.
*/

#ifdef _WIN32
#include <winsock.h>
#endif
#include <global.h>
#include <my_sys.h>
#include <m_string.h>
#include "mysql.h"
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#if !defined(__WIN32__) && !defined(MSDOS)
#include <sys/socket.h>
#endif
#if !defined(MSDOS) && !defined(__WIN32__) && !defined(HAVE_BROKEN_NETINET_INCLUDES)
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#if !defined(alpha_linux_port)
#include <netinet/tcp.h>
#endif
#if defined(__EMX__)
#include <sys/ioctl.h>
#define ioctlsocket(A,B,C) ioctl((A),(B),(void *)(C),sizeof(*(C)))
#undef HAVE_FCNTL
#endif
#endif
#ifdef MYSQL_SERVER
#include "my_pthread.h"
#include "thr_alarm.h"
void sql_print_error(const my_string format,...);
#define RETRY_COUNT 10
#else
typedef my_bool thr_alarm_t;
#define thr_alarm_init(A) (*A)=0
#define thr_alarm_in_use(A) (A)
#define thr_end_alarm(A)
#define thr_alarm(A,B) local_thr_alarm((A),(B))
inline int local_thr_alarm(my_bool *A,int B)
{
  *A=1;
  return 0;
}
#define thr_got_alarm(A) 0
#define RETRY_COUNT 1
#endif

#if defined(MSDOS) || defined(__WIN32__)
#define raw_net_read(A,B,C) recv((A),(B),(C),0)
#define raw_net_write(A,B,C) send((A),(B),(C),0)
#ifdef __WIN32__
#undef errno
#undef EINTR
#undef EAGAIN
#define errno WSAGetLastError()
#define EINTR  WSAEINTR
#define EAGAIN WSAEINPROGRESS
#endif
#else /* unix */
#define raw_net_read(A,B,C) read((A),(B),(C))
#define raw_net_write(A,B,C) write((A),(B),(C))
#endif
#ifndef EWOULDBLOCK
#define EWOULDBLOCK EAGAIN
#endif

/*
** Give error if a too big packet is found
** The server can change this with the -O switch, but because the client
** can't normally do this the client should have a bigger max-buffer.
*/

#ifdef MYSQL_SERVER
ulong max_allowed_packet=65536;
#else
ulong max_allowed_packet=24*1024*1024L;
#endif
ulong net_buffer_length=8192;	/* Default length. Enlarged if necessary */

extern uint test_flags;		/* QQ */
#define TEST_BLOCKING		8
static int net_write_buff(NET *net,const char *packet,uint len);


	/* Init with packet info */

int my_net_init(NET *net, enum enum_net_type nettype, Socket fd,
		void *pipe)
{
  if (!(net->buff=(uchar*) my_malloc(net_buffer_length,MYF(MY_WME))))
    return 1;
  if (net_buffer_length > max_allowed_packet)
    max_allowed_packet=net_buffer_length;
  net->buff_end=net->buff+(net->max_packet=net_buffer_length);
  net->nettype = nettype;
#ifdef __WIN32__
  net->hPipe = pipe ? *(HANDLE*) pipe : INVALID_HANDLE_VALUE;
#endif
  net->fd=fd;
  net->error=net->return_errno=0;
  net->timeout=NET_READ_TIMEOUT;			/* Timeout for read */
  net->pkt_nr=0;
  net->write_pos=net->read_pos = net->buff;
  net->last_error[0]=0;
  net->compress=net->more=0;
  net->where_b = net->remain_in_buf=0;

#if defined(MYSQL_SERVER) && !defined(___WIN32__) && !defined(__EMX__) 
  if (fd > 0)
  {
    net->fcntl=fcntl(net->fd, F_GETFL);
#if !defined(NO_FCNTL_NONBLOCK)
    if (!(test_flags & TEST_BLOCKING))
    {
      net->fcntl|=O_NONBLOCK;
      (void) fcntl(net->fd, F_SETFL, net->fcntl);
    }
#endif
  }
#endif
#if defined(IPTOS_THROUGHPUT) && !defined(HAVE_LINUXTHREADS) /* For FreeBSD */
  if (fd > 0)
  {
#ifndef __EMX__
    int tos = IPTOS_THROUGHPUT;
    if (!setsockopt(fd, IPPROTO_IP, IP_TOS, (void*) &tos, sizeof(tos)))
#endif
    {
      int nodelay = 1;
      if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void*) &nodelay,
		     sizeof(nodelay)))
      {
	DBUG_PRINT("warning",("Couldn't set socket option for fast send"));
      }
    }
  }
#endif
  return 0;
}

void net_end(NET *net)
{
  my_free((gptr) net->buff,MYF(MY_ALLOW_ZERO_PTR));
  net->buff=0;
}

/* Realloc the packet buffer */

static my_bool net_realloc(NET *net, ulong length)
{
  uchar *buff;
  ulong pkt_length;
  if (length >= max_allowed_packet)
  {
    DBUG_PRINT("error",("Packet too large (%ld)", length));
#ifdef MYSQL_SERVER
    sql_print_error("Packet too large (%ld)\n", length);
#else
    fprintf(stderr,"Packet too large (%ld)\n", length);
#endif
    return 1;
  }
  pkt_length = (length+IO_SIZE) & ~(IO_SIZE-1);
  if (!(buff=(uchar*) my_realloc((char*) net->buff, pkt_length, MYF(MY_WME))))
    return 1;
  net->buff=net->write_pos=buff;
  net->buff_end=buff+(net->max_packet=pkt_length);
  return 0;
}

	/* Remove unwanted characters from connection */

#ifndef MYSQL_SERVER
void net_clear(NET *net)
{
#if !defined(MSDOS) && !defined(__WIN32__) && !defined(__EMX__)
#if !defined(NO_FCNTL_NONBLOCK)
  int count;
  if (!(net->fcntl & O_NONBLOCK))
    (void) fcntl(net->fd, F_SETFL, net->fcntl | O_NONBLOCK);
  while ((count=(int) raw_net_read(net->fd,net->buff,net->max_packet)) > 0)
  {
    DBUG_PRINT("info",("skipped %d bytes from file: %d",count,net->fd));
  }
  if (!(net->fcntl & O_NONBLOCK))
    (void) fcntl(net->fd, F_SETFL, net->fcntl);
#endif
#else
#ifndef __EMX__
  if (net->nettype != NET_TYPE_NAMEDPIPE)
#endif
  {
    ulong arg;
    arg=1; ioctlsocket(net->fd,FIONBIO,&arg);
    while ((int) raw_net_read(net->fd,net->buff,net->max_packet) > 0) ;
    arg=0; ioctlsocket(net->fd,FIONBIO,&arg);
  }
#endif
  net->pkt_nr=0;				/* Ready for new command */
  net->write_pos=net->buff;
}
#endif

	/* Flush write_buffer if not empty. */

int net_flush(NET *net)
{
  int error=0;
  if (net->buff != net->write_pos)
  {
    error=net_real_write(net,(char*) net->buff,
			 (uint) (net->write_pos - net->buff));
    net->write_pos=net->buff;
  }
  return error;
}


/*****************************************************************************
** Write something to server/clinet buffer
*****************************************************************************/


/*
** Write a logical packet with packet header
** Format: Packet length (3 bytes), packet number(1 byte)
**         When compression is used a 3 byte compression length is added
** NOTE: If compression is used the original package is destroyed!
*/

int
my_net_write(NET *net,const char *packet,ulong len)
{
  uchar buff[NET_HEADER_SIZE];
  int3store(buff,len);
  buff[3]= (net->compress) ? 0 : (uchar) (net->pkt_nr++);
  if (net_write_buff(net,(char*) buff,NET_HEADER_SIZE))
    return 1;
  return net_write_buff(net,packet,len);
}

int
net_write_command(NET *net,uchar command,const char *packet,ulong len)
{
  uchar buff[NET_HEADER_SIZE+1];
  uint length=len+1;				/* 1 extra byte for command */

  int3store(buff,length);
  buff[3]= (net->compress) ? 0 : (uchar) (net->pkt_nr++);
  buff[4]=command;
  if (net_write_buff(net,(char*) buff,5))
    return 1;
  return test(net_write_buff(net,packet,len) || net_flush(net));
}


static int
net_write_buff(NET *net,const char *packet,uint len)
{
  uint left_length=(uint) (net->buff_end - net->write_pos);

  while (len > left_length)
  {
    memcpy((char*) net->write_pos,packet,left_length);
    if (net_real_write(net,(char*) net->buff,net->max_packet))
      return 1;
    net->write_pos=net->buff;
    packet+=left_length;
    len-=left_length;
    left_length=net->max_packet;
  }
  memcpy((char*) net->write_pos,packet,len);
  net->write_pos+=len;
  return 0;
}

/*  Read and write using timeouts */

int
net_real_write(NET *net,const char *packet,ulong len)
{
  int length;
  char *pos,*end;
  thr_alarm_t alarmed;
  uint retry_count=0;
#ifdef HAVE_COMPRESS
  if (net->compress)
  {
    ulong complen;
    uchar *b;
    uint header_length=NET_HEADER_SIZE+COMP_HEADER_SIZE;
    if (!(b=(uchar*) my_malloc(len + NET_HEADER_SIZE + COMP_HEADER_SIZE,
				    MYF(MY_WME))))
      return 1;
    memcpy(b+header_length,packet,len);

    if (my_compress((byte*) b+header_length,&len,&complen))
    {
      DBUG_PRINT("warning",
		 ("Compression error; Continuing without compression"));
      complen=0;
    }
    int3store(&b[NET_HEADER_SIZE],complen);
    int3store(b,len);
    b[3]=(uchar) (net->pkt_nr++);
    len+= header_length;
    packet= (char*) b;
  }
#endif

  /* DBUG_DUMP("net",packet,len); */
#ifdef MYSQL_SERVER
  thr_alarm_init(&alarmed);
#ifdef HAVE_FCNTL
  if (!(net->fcntl & O_NONBLOCK))
#endif
    thr_alarm(&alarmed,NET_WRITE_TIMEOUT);
#else
  alarmed=0;
#endif /* MYSQL_SERVER */

  pos=(char*) packet; end=pos+len;
  while (pos != end)
  {
#ifdef __WIN32__
    if (net->nettype == NET_TYPE_NAMEDPIPE)
    {
      if ( !WriteFile(net->hPipe, pos, end-pos, &length, NULL) )
      {
	net->error = 1;
	goto end;
      }
    }
    else
#endif
    if ((int) (length=raw_net_write(net->fd,pos,(size_t) (end-pos))) <= 0)
    {
#if (!defined(__WIN32__) && !defined(__EMX__)) || !defined(MYSQL_SERVER)
      if ((errno == EAGAIN || errno == EINTR || errno == EWOULDBLOCK ||
	   length == 0) && !thr_alarm_in_use(alarmed))
      {
	if (!thr_alarm(&alarmed,NET_WRITE_TIMEOUT))
	{					/* Always true for client */
#ifdef HAVE_FCNTL
	  if (net->fcntl & O_NONBLOCK)
	  {
	    while (fcntl(net->fd, F_SETFL, net->fcntl & ~ O_NONBLOCK) < 0)
	    {
	      if (errno == EINTR && retry_count++ < RETRY_COUNT)
		continue;
#ifdef EXTRA_DEBUG
	      fprintf(stderr,
		      "%s: my_net_write: fcntl returned error %d, aborting thread\n",
		      my_progname,errno);
#endif
	      net->error=1;			/* Close socket */
	      goto end;
	    }
	  }
#endif /* HAVE_FCNTL */
	  retry_count=0;
	  continue;
	}
      }
      else
#endif
	if (thr_alarm_in_use(alarmed) && !thr_got_alarm(alarmed) &&
	    (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
      {
	if (retry_count++ < RETRY_COUNT)
	    continue;
#ifdef EXTRA_DEBUG
	  fprintf(stderr, "%s: net_real_write looped, aborting thread\n",
		  my_progname);
#endif
      }
#if defined(THREAD_SAFE_CLIENT) && !defined(MYSQL_SERVER)
      if (errno == EINTR)
      {
	DBUG_PRINT("warning",("Interrupted write. Retrying..."));
	continue;
      }
#endif
      net->error=1;				/* Close socket */
      break;
    }
    pos+=length;
  }
 end:
#ifdef HAVE_COMPRESS
  if (net->compress)
    my_free((char*) packet,MYF(0));
#endif
  if (thr_alarm_in_use(alarmed))
  {
    thr_end_alarm(&alarmed);
#ifdef HAVE_FCNTL
    if (net->fcntl & O_NONBLOCK)
      (void) fcntl(net->fd, F_SETFL, net->fcntl);
#endif
  }
  return (int) (pos != end);
}


/*****************************************************************************
** Read something from server/clinet
*****************************************************************************/


static uint
my_real_read(NET *net, ulong *complen)
{
  uchar *pos;
  ulong remain;
  long length;
  uint i,retry_count;
  ulong len;
  thr_alarm_t alarmed;
  retry_count = 0; *complen = 0;
  len=packet_error;
  remain= (net->compress ? NET_HEADER_SIZE+COMP_HEADER_SIZE :
	   NET_HEADER_SIZE);
  thr_alarm_init(&alarmed);
#ifdef MYSQL_SERVER
#ifdef HAVE_FCNTL
    if (!(net->fcntl & O_NONBLOCK))
#endif
      thr_alarm(&alarmed,net->timeout);
#endif

    pos = net->buff + net->where_b;		/* net->packet -4 */
    for (i=0 ; i < 2 ; i++)
    {
      while (remain > 0)
      {
#ifndef __WIN32__
        errno=0;				/* For linux */
	/* First read is done with non blocking mode */
#else
	if ( net->nettype == NET_TYPE_NAMEDPIPE)
	{
	  if ( !ReadFile(net->hPipe, pos, remain, &length, NULL) )
	  {
	    len = packet_error;
	    net->error = 1;			/* Close named pipe */
	    goto end;
	  }
	}
	else
#endif
	if ((int) (length=raw_net_read(net->fd,(char*) pos,remain)) <= 0)
	{
	  DBUG_PRINT("info",("raw_net_read returned %d,  errno: %d",
			     length,errno));
#if (!defined(__WIN32__) && !defined(__EMX__)) || !defined(MYSQL_SERVER)
	  /*
	    We got an error that there was no data on the socket. We now set up
	    an alarm to not 'read forever', change the socket to non blocking
	    mode and try again
	  */
	  if ((errno == EAGAIN || errno == EINTR || errno == EWOULDBLOCK ||
	       length == 0)
	      && !thr_alarm_in_use(alarmed))
	  {
	    if (!thr_alarm(&alarmed,net->timeout)) /* Don't wait too long */
	    {
#ifdef HAVE_FCNTL
	      if (net->fcntl & O_NONBLOCK)
	      {
		while (fcntl(net->fd, F_SETFL, net->fcntl & ~ O_NONBLOCK) < 0)
		{
		  if (errno == EINTR && retry_count++ < RETRY_COUNT)
		    continue;
		  DBUG_PRINT("error",("fcntl returned error %d, aborting thread",
				      errno));
#ifdef EXTRA_DEBUG
		  fprintf(stderr,
			  "%s: my_net_read: fcntl returned error %d, aborting thread\n",
			  my_progname,errno);
#endif
		  len= packet_error;
		  net->error=1;			/* Close socket */
		  goto end;
		}
	      }
#endif /* HAVE_FCNTL */
	      retry_count=0;
	      continue;
	    }
	  }
#endif
	  if (thr_alarm_in_use(alarmed) && !thr_got_alarm(alarmed) &&
	      (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
	  {					/* Probably in MIT threads */
	    if (retry_count++ < RETRY_COUNT)
	      continue;
#ifdef EXTRA_DEBUG
	    fprintf(stderr, "%s: my_net_read: looped with error %d, aborting thread\n",
		    my_progname,errno);
#endif
	  }
#if defined(THREAD_SAFE_CLIENT) && !defined(MYSQL_SERVER)
	  if (errno == EINTR)
	  {
	    DBUG_PRINT("warning",("Interrupted read. Retrying..."));
	    continue;
	  }
#endif
	  DBUG_PRINT("error",("Couldn't read packet: remain: %d  errno: %d  length: %d  alarmed: %d", remain,errno,length,alarmed));
	  len= packet_error;
	  net->error=1;				/* Close socket */
	  goto end;
	}
	remain -= (ulong) length;
	pos+= (ulong) length;
      }
      if (i == 0)
      {					/* First parts is packet length */
	ulong helping;
	if (net->buff[net->where_b + 3] != (uchar) net->pkt_nr)
	{
	  if (net->buff[net->where_b] != (uchar) 255)
	  {
	    DBUG_PRINT("error",
		       ("Packets out of order (Found: %d, expected %d)",
			(int) net->buff[net->where_b + 3],net->pkt_nr));
#ifdef EXTRA_DEBUG
	    fprintf(stderr,"Packets out of order (Found: %d, expected %d)\n",
		    (int) net->buff[net->where_b + 3],net->pkt_nr);
#endif
	  }
	  len= packet_error;
	  goto end;
	}
	net->pkt_nr++;
#ifdef HAVE_COMPRESS
	if (net->compress)
	{
	  /* complen is > 0 if package is really compressed */
	  *complen=uint3korr(&(net->buff[net->where_b + NET_HEADER_SIZE]));
	}
#endif

	len=uint3korr(net->buff+net->where_b);
	helping = max(len,*complen) + net->where_b;
	/* The necessary size of net->buff */
	if (helping >= net->max_packet)
	{
	  if (net_realloc(net,helping))
	  {
	    len= packet_error;		/* Return error */
	    goto end;
	  }
	}
	pos=net->buff + net->where_b;
	remain = len;
      }
    }

end:
  if (thr_alarm_in_use(alarmed))
  {
    thr_end_alarm(&alarmed);
#ifdef HAVE_FCNTL
    (void) fcntl(net->fd, F_SETFL, net->fcntl);
#endif
  }
  return(len);
}


uint
my_net_read(NET *net)
{
  ulong len,complen;

#ifdef HAVE_COMPRESS
  if (!net->compress)
  {
#endif
    len = my_real_read (net,&complen);
    net->read_pos = net->buff + net->where_b;
    if (len != packet_error)
      net->read_pos[len]=0;		/* Safeguard for mysql_use_result */
    return len;
#ifdef HAVE_COMPRESS
  }
  if (net->remain_in_buf)
    net->buff[net->buf_length - net->remain_in_buf]=net->save_char;
  for (;;)
  {
    if (net->remain_in_buf)
    {
      uchar *pos = net->buff + net->buf_length - net->remain_in_buf;
      if (net->remain_in_buf >= 4)
      {
	net->length = uint3korr(pos);
	if (net->length <= net->remain_in_buf - 4)
	{
	  /* We have a full packet */
	  len=net->length;
	  net->remain_in_buf -= net->length + 4;
	  net->read_pos=pos + 4;
	  break;			/* We have a full packet */
	}
      }
      /* Move data down to read next data packet after current one */
      if (net->buf_length != net->remain_in_buf)
      {
	memmove(net->buff,pos,net->remain_in_buf);
	net->buf_length=net->remain_in_buf;
      }
      net->where_b=net->buf_length;
    }
    else
    {
      net->where_b=0;
      net->buf_length=0;
    }

    if ((len = my_real_read(net,&complen)) == packet_error)
      break;
    if (my_uncompress((byte*) net->buff + net->where_b, &len, &complen))
    {
      len= packet_error;
      net->error=1;			/* caller will close socket */
      break;
    }
    net->buf_length+=len;
    net->remain_in_buf+=len;
  }
  if (len != packet_error)
  {
    net->save_char= net->read_pos[len];	/* Must be saved */
    net->read_pos[len]=0;		/* Safeguard for mysql_use_result */
  }
  return len;
#endif
}
