/*
 * If we use SYSV queues.. we will have to include common.h and change all
 * the memory allocation routines to the mem_alloc variety.
 */
#include "common.h"
#include "msglib.h"
#include "msglib_private.h"

/*
 * Library will provide a higher level view of the communication interface 
 * used -- one of POSIX (mqueue) message, SYSV message, Unix datagram sockets
 *         or Unix stream sockets.
 *
 * Look in msglib.h for the exported macros to use. User of this library will
 * need to keep track of only the mq descriptor.
 *
 * Pthread locking mechanisms are used to grab and release a read-write lock
 * on the msglib interface. This is done to make the msglib thread-safe.
 */

/*
 * Called at fork time.
 */
void msglib_fork(void) 
{
  int i;

  for(i=0;i<num_msgs;i++)
    {
      if(msg_array[i].valid == MSGQ_VALID)
	{
	  msglib_qfree(msg_array[i].msg_q,0);
	}
    }
}

/*
 * Called at exit time..
 */
static void msglib_exit()
{
  int i;

  for(i=0;i<num_msgs;i++)
    {
      if(msg_array[i].valid == MSGQ_VALID)
	{
	  msglib_qfree(msg_array[i].msg_q,1);
	}
    }
  sem_mem_free(msg_array);
  sss_pthreads_msglib_exit();
}

/*
 * Initialize the library.
 */
int msglib_init(int num_queues)
{
  int i;

  /*
   * Check arguments.
   */
  if (num_queues == NULL)
    {
#if (MSGLIB_DEBUG == 1)
      fprintf(stderr,"NUM_QUEUES = 0 in msglib_init.\n");
#endif    
      return 0;
    }
  if(num_msgs)
    {
#if (MSGLIB_DEBUG == 1)
      fprintf(stderr,"msglib is already initialized.\n");
#endif
      return 0;
    }

  /*
   * Initialize our global (static) variables.
   */
  sss_pthreads_msglib_init();
  wrlock_msglib();

  num_msgs=num_queues;
  msg_array=sem_mem_alloc_perm(num_queues*sizeof(struct msg_local));
  if(!msg_array)
    {
      unlock_msglib();
      return -1;
    }
  atexit(msglib_exit);
#if 0
  pthread_atfork(NULL,NULL,msglib_fork);
#endif
  for(i=0;i<num_queues;i++)
    {
      msg_array[i].valid=MSGQ_INVALID;
    }
  unlock_msglib();
  return (msg_array == NULL);
}

/*
 * Free up memory allocated for each queue.
 * We should already hold the write lock when we get here.
 */
#if POSIX_MSGLIB
static void msglib_qfree(mqd_t mq,int debug)
#elif SYSV_MSGLIB || UNIX_SOCK_MSGLIB || STREAM_SOCK_MSGLIB
static void msglib_qfree(int mq,int debug)
#endif
{
  int i;
  struct msg_local *ml=NULL;
  int ret;

  i=msglib_qfind(mq);
  
  if(i<0)
    return;
#if (MSGLIB_DEBUG > 1)
  if(debug)
    fprintf(stderr,
#if POSIX_MSGLIB || UNIX_SOCK_MSGLIB || STREAM_SOCK_MSGLIB
	    "Q=%s NUM. SENT=%d, NUM. RECV=%d\n", msg_array[i].msg_path,

#elif SYSV_MSGLIB 
	    "Q=%d NUM. SENT=%d, NUM. RECV=%d\n", msg_array[i].msg_key,
#endif
	    msg_array[i].msg_sent, msg_array[i].msg_rcvd);
#endif    

#if POSIX_MSGLIB
  sem_mem_free(msg_array[i].msg_buf);
  sem_mem_free(msg_array[i].msg_path);
  msg_array[i].last_msg_prio=0;
  sem_mem_free(msg_array[i].msg_attr);
#elif SYSV_MSGLIB
  sem_mem_free(msg_array[i].msg_attr);
#elif UNIX_SOCK_MSGLIB || STREAM_SOCK_MSGLIB
  sem_mem_free(msg_array[i].saddr);
  if(msg_array[i].msg_path)
    sem_mem_free(msg_array[i].msg_path);
#endif
  msg_array[i].msg_sent=
    msg_array[i].msg_rcvd=0;
  msg_array[i].valid=MSGQ_INVALID;
}
  
/*
 * Find the memory allocated for each queue.
 * >=0 on success.
 * -1 on fail.
 */
#if POSIX_MSGLIB
static int msglib_qfind(mqd_t mq)
#elif SYSV_MSGLIB || UNIX_SOCK_MSGLIB || STREAM_SOCK_MSGLIB
static int msglib_qfind(int mq)
#endif
{
  int i;
  struct msg_local *ml=NULL;
  int ret;
#if POSIX_MSGLIB
  struct mq_attr mqstat;
#elif SYSV_MSGLIB
  struct msqid_ds mqstat;
#endif

  /*
   * This search may need to be improved later.. if there are too many message
   * queues.
   */
  for(i=0;i<num_msgs;i++)
    {
      if((msg_array[i].valid == MSGQ_VALID) && 
	 (msg_array[i].msg_q == mq))
	return i;
      if(msg_array[i].valid == MSGQ_INVALID)
	{
	  ml=&msg_array[i];
	  ret=i;
	}
    }

  /*
   * Couldn't find the memory.. allocate some only if it is a real queue.
   */
#if POSIX_MSGLIB
  if(ml && ml->valid == MSGQ_INVALID && !mq_getattr(mq,&mqstat))
    {
      if(!(ml->msg_attr=sem_mem_alloc_temp(sizeof(struct mq_attr))))
	return -1;
      memcpy(ml->msg_attr,&mqstat,sizeof(struct mq_attr));
      if(!(ml->msg_buf=sem_mem_alloc_temp(ml->msg_attr->mq_msgsize)))
	{
	  sem_mem_free(ml->msg_attr);
	  return -1;
	}
      ml->last_msg_prio=0;
#elif SYSV_MSGLIB
  if(ml && ml->valid == MSGQ_INVALID && !msgctl(mq,IPC_STAT,&mqstat))
    {
      if(!(ml->msg_attr=(struct msqid_ds *)
	     sem_mem_alloc_temp(sizeof(struct msqid_ds))))
	return -1;
      memcpy(ml->msg_attr,&mqstat,sizeof(struct msqid_ds));
#elif UNIX_SOCK_MSGLIB || STREAM_SOCK_MSGLIB
  i=sizeof(ml->size);
  if(ml && ml->valid == MSGQ_INVALID && !getsockopt(mq,SOL_SOCKET,SO_RCVBUF,
						    &ml->size,&i))
    {
      if(!(ml->saddr=(struct sockaddr_un *)
	     sem_mem_alloc_temp(sizeof(struct sockaddr_un))))
	return -1;
#endif
      ml->valid=MSGQ_VALID;
      ml->msg_q=mq;
      ml->msg_sent=0;
      ml->msg_rcvd=0;
      return ret;
    }
  
  return -1;
}


/*
 * Actually get a message. Do the IPC.
 * returns
 * >0 on success
 * <= 0 on fail
 */
#if POSIX_MSGLIB
int msglib_getmsg(mqd_t mq, char *mb,int len)
#elif SYSV_MSGLIB
int msglib_getmsg(int mq, char *mb,int len,int msgtype,int flags)
#elif UNIX_SOCK_MSGLIB
int msglib_getmsg(int mq, char *mb,int len,int do_not_select)
#elif STREAM_SOCK_MSGLIB
int msglib_getmsg(int mq, char *mb,int len,int do_not_select,int doaccept)
#endif
{
  int i,j;
#if UNIX_SOCK_MSGLIB || STREAM_SOCK_MSGLIB
  fd_set readfds;
  struct timeval timeout;
  struct sockaddr_un *from=NULL;
  int fromlen=sizeof(struct sockaddr_un);

  wrlock_msglib();

  i=msglib_qfind(mq);
  if(i<0)
    {
      unlock_msglib();
      return -1;
    }
#if UNIX_SOCK_MSGLIB || STREAM_SOCK_MSGLIB
  else
    from=msg_array[i].saddr;
#endif
  
#if POSIX_MSGLIB
  if((mq == (mqd_t)0) 
#elif SYSV_MSGLIB || UNIX_SOCK_MSGLIB || STREAM_SOCK_MSGLIB
  if((mq < 0) 
#endif
     || (mb == NULL) || (len == 0))
    {
      /*
       * Check arguments.
       */

#if (MSGLIB_DEBUG == 1) && !STREAM_SOCK_MSGLIB
#if POSIX_MSGLIB
      if(mq==(mqd_t)0)
#elif SYSV_MSGLIB || UNIX_SOCK_MSGLIB
      if(mq < 0)
#endif
	fprintf(stderr,"mq descriptor is NULL in msglib_getmsg.\n");
      if(mb==NULL)
	fprintf(stderr,"mb buffer is NULL in msglib_getmsg.\n");
      if(len == 0)
	fprintf(stderr,"Length = 0 in msglib_getmsg.\n");
      {
	if(doaccept & CLOSE_FLAG)
	  {
	    close(msg_array[i].newsock);
	  }
      }
#endif   
 
#if STREAM_SOCK_MSGLIB
      if(doaccept & CLOSE_FLAG)
	{
	  close(msg_array[i].newsock);
	  unlock_msglib();
	  return 0;
	}
#endif
      unlock_msglib();
      return -1;
    }
  

#if POSIX_MSGLIB  
  if((j=mq_receive(mq,msg_array[i].msg_buf,msg_array[i].msg_attr->mq_msgsize,
		   &msg_array[i].last_msg_prio)) > 0)
    {
      msg_array[i].msg_rcvd++;

      if(j > len)
	{
	  memcpy(mb,msg_array[i].msg_buf,
		(len > msg_array[i].msg_attr->mq_msgsize) ? 
		msg_array[i].msg_attr->mq_msgsize : len);
	  unlock_msglib();
	  return len;
	}
      else
	{
	  memcpy(mb,msg_array[i].msg_buf,
		(j > msg_array[i].msg_attr->mq_msgsize) ? 
		msg_array[i].msg_attr->mq_msgsize : j);
	  unlock_msglib();
	  return j;
	}
    }
#elif SYSV_MSGLIB
  if((j=msgrcv(mq,mb,len,msgtype,flags)) > 0)
    {
      msg_array[i].msg_rcvd++;
      unlock_msglib();
      return j;
    }
#elif UNIX_SOCK_MSGLIB 
  timeout.tv_sec=msg_array[i].timeout;
  timeout.tv_usec=0;
  bzero(&readfds,sizeof(fd_set));
  FD_SET(mq,&readfds);
  if((do_not_select || (select((mq+1),&readfds,0,0,&timeout) > 0)) && 
     (j=recvfrom(mq,mb,len,0,from,&fromlen)))
#elif STREAM_SOCK_MSGLIB
  j=-1;
  if(doaccept & ACCEPT_FLAG)
  {
#if (MSGLIB_DEBUG == 2)
    printf("Doing the accept\n");
#endif
    timeout.tv_sec=msg_array[i].timeout;
    timeout.tv_usec=0;
    bzero(&readfds,sizeof(fd_set));
    FD_SET(mq,&readfds);

    unlock_msglib();

    if(do_not_select || (select((mq+1),&readfds,0,0,&timeout) > 0))
      {
	if((msg_array[i].newsock=
	    accept(mq,(struct sockaddr *)from,&fromlen)) < 0)
	  {
	    return -1;
	  }
      }
    else
      {
	return -1;
      }

    wrlock_msglib();

#if (MSGLIB_DEBUG == 2)
    printf("Finished the accept, newsock=%d\n",msg_array[i].newsock);
#endif
  }
  timeout.tv_sec=msg_array[i].timeout;
  timeout.tv_usec=0;
  bzero(&readfds,sizeof(fd_set));
  FD_SET(msg_array[i].newsock,&readfds);

  unlock_msglib();

  if((select((msg_array[i].newsock+1),&readfds,0,0,&timeout) > 0) && 
     (j=read(msg_array[i].newsock,mb,len)))
#endif
    {
      wrlock_msglib();
#if (MSGLIB_DEBUG == 2)
      printf("Received = %d, len=%d on socket = %d\n",j,len,
	     msg_array[i].newsock);
#endif
#if STREAM_SOCK_MSGLIB
      if(doaccept & CLOSE_FLAG)
	close(msg_array[i].newsock);
#endif
      msg_array[i].msg_rcvd++;
      unlock_msglib();
      return j;
    }
#endif
#if (MSGLIB_DEBUG == 1)
  else
    {
      perror("msglib_receive:");
    }
#endif

  wrlock_msglib();

#if STREAM_SOCK_MSGLIB
  if(doaccept & CLOSE_FLAG)
    close(msg_array[i].newsock);
#endif
  unlock_msglib();
  return -1;
}

/*
 * Actually send a message. Do the IPC.
 * returns
 * >0 on success
 * <= 0 on fail
 */
#if POSIX_MSGLIB
int msglib_sendmsg(mqd_t mq, char *mb,int len,int priority)
#elif SYSV_MSGLIB
int msglib_sendmsg(int mq, char *mb,int len,int flags)
#elif UNIX_SOCK_MSGLIB
int msglib_sendmsg(int mq, char *mb,int len,char *Pchar_to)
#elif STREAM_SOCK_MSGLIB
int msglib_sendmsg(int mq, char *mb,int len,char *Pchar_to,int doconnect)
#endif
{
  int i,j;
#if UNIX_SOCK_MSGLIB || STREAM_SOCK_MSGLIB
  struct sockaddr_un *to;
  int tolen;
#endif

  wrlock_msglib();

  i=msglib_qfind(mq);
  if(i<0)
    {
      unlock_msglib();
      return -1;
    }
#if UNIX_SOCK_MSGLIB || STREAM_SOCK_MSGLIB
  else
    {
      to=msg_array[i].saddr;
      tolen=strlen(Pchar_to)+sizeof(to->sun_family);
      
      strcpy(to->sun_path,Pchar_to);
      to->sun_family=AF_UNIX;
    }
#endif
  
#if POSIX_MSGLIB
  if((mq == (mqd_t)0) 
#elif SYSV_MSGLIB || UNIX_SOCK_MSGLIB || STREAM_SOCK_MSGLIB
  if((mq < 0) 
#endif
     || (mb == NULL) || (len == 0))
    {
      /*
       * Check arguments.
       */

#if (MSGLIB_DEBUG == 1)
#if POSIX_MSGLIB
      if(mq==(mqd_t)0)
#elif SYSV_MSGLIB || UNIX_SOCK_MSGLIB || STREAM_SOCK_MSGLIB
      if(mq < 0) 
#endif
	fprintf(stderr,"mq descriptor is NULL in msglib_getmsg.\n");
      if(mb==NULL)
	fprintf(stderr,"mb buffer is NULL in msglib_getmsg.\n");
      if(len == 0)
	fprintf(stderr,"Length = 0 in msglib_getmsg.\n");
#endif   
      unlock_msglib();
      return -1;
    }
  
#if POSIX_MSGLIB
  if(len > msg_array[i].msg_attr->mq_msgsize)
  {
    unlock_msglib();
    return -1;
  }

  unlock_msglib();

  if(!mq_send(mq,mb,len,priority))
#elif SYSV_MSGLIB

  unlock_msglib();

  if(!msgsnd(mq,mb,len,flags))
#elif UNIX_SOCK_MSGLIB 

  unlock_msglib();

  if(sendto(mq,mb,len,0,to,tolen)==len)
#elif STREAM_SOCK_MSGLIB

  unlock_msglib();

  if(doconnect & CONNECT_FLAG)
  {
#if (MSGLIB_DEBUG == 2)
    printf("Doing the connect\n");
#endif
    if(connect(mq,(struct sockaddr *)to,tolen) < 0)
      {
	return -1;
      }
#if (MSGLIB_DEBUG == 2)
    printf("Finished the connect\n");
#endif
  }
  if(write(mq,mb,len) == len)
#endif
    {
      wrlock_msglib();
      msg_array[i].msg_sent++;
      unlock_msglib();
      return len;
    }
#if (MSGLIB_DEBUG == 1)
  else
    {
      perror("msglib_send:");
      fprintf(stderr,"Messages = %d\n",NUMMSGS(mq));
    }
#endif
  return -1;
}

/*
 * Check the number of messages sitting in mq. If we already have the message 
 * queue in msg_array, then update the msg_attr in msg_array.
 */
#if POSIX_MSGLIB
int msglib_curmsgsq(mqd_t mq)
#elif SYSV_MSGLIB || UNIX_SOCK_MSGLIB || STREAM_SOCK_MSGLIB
int msglib_curmsgsq(int mq)
#endif
{
  int i;
#if POSIX_MSGLIB
  struct mq_attr qattr;
#elif SYSV_MSGLIB
  struct msqid_ds mqstat;  
#elif UNIX_SOCK_MSGLIB || STREAM_SOCK_MSGLIB
#endif
  
  rdlock_msglib();

  i=msglib_qfind(mq);
  
#if POSIX_MSGLIB
  if(i>=0 && mq_getattr(mq,msg_array[i].msg_attr))
    {
      unlock_msglib();
      return -1;
    }
#elif SYSV_MSGLIB
  if(i>=0 && msgctl(mq,IPC_STAT,msg_array[i].msg_attr))
    {
      unlock_msglib();
      return -1;
    }
#endif

#if POSIX_MSGLIB
  if(i>=0)
    {
      unlock_msglib();
      return msg_array[i].msg_attr->mq_curmsgs;
    }
  else if(!mq_getattr(mq,&qattr))
    {
      unlock_msglib();
      return qattr.mq_curmsgs;
    }
#elif SYSV_MSGLIB
  if(i>=0)
    {
      unlock_msglib();
      return msg_array[i].msg_attr->msg_qnum;
    }
  else if(!msgctl(mq,IPC_STAT,&mqstat))
    {
      unlock_msglib();
      return mqstat.msg_qnum;
    }
#elif UNIX_SOCK_MSGLIB || STREAM_SOCK_MSGLIB
  if(i>=0)
    {
      unlock_msglib();
      return 0;
    }
#endif
  
  unlock_msglib();
  return -1;
}

/*
 * Return the maximum number of messages which can sit on this queue.
 */
#if POSIX_MSGLIB
int msglib_maxmsgq(mqd_t mq)
#elif SYSV_MSGLIB || UNIX_SOCK_MSGLIB || STREAM_SOCK_MSGLIB
int msglib_maxmsgq(int mq)
#endif
{
  int i;
  
  rdlock_msglib();

  i=msglib_qfind(mq);
    
  if(i >= 0 && i < num_msgs)
    {
#if POSIX_MSGLIB
      i=msg_array[i].msg_attr->mq_maxmsg;
      unlock_msglib();
      return i;
#elif SYSV_MSGLIB
      i=msg_array[i].msg_attr->msg_qbytes;
      unlock_msglib();
      return i;
#elif UNIX_SOCK_MSGLIB || STREAM_SOCK_MSGLIB
      unlock_msglib();
      return 0;
#endif
    }
  else
    {
      unlock_msglib();
      return -1;
    }
}

/*
 * Return the maximum length of a message in the queue.
 */
#if POSIX_MSGLIB
int msglib_maxlenq(mqd_t mq)
{
  int i;
  
  rdlock_msglib();
  
  i=msglib_qfind(mq);
  
  if(i >= 0 && i < num_msgs)
    {
      i=msg_array[i].msg_attr->mq_msgsize;
      unlock_msglib();
      return i;
    }
  else
    {
      unlock_msglib();
      return -1;
    }
}
#elif SYSV_MSGLIB
int msglib_maxlenq(int mq)
{
  return msglib_maxmsgq(mq);
}
#elif UNIX_SOCK_MSGLIB || STREAM_SOCK_MSGLIB
int msglib_maxlenq(int mq)
{
  int i;
  
  rdlock_msglib();

  i=msglib_qfind(mq);
    
  if(i >= 0 && i < num_msgs)
    {
      i=msg_array[i].size;
      unlock_msglib();
      return i;
    }
  else
    {
      unlock_msglib();
      return -1;
    }
}
#endif


/*
 * Create message queue.
 * 0 on success. -1 on fail
 */
#if POSIX_MSGLIB
int msglib_createq(char *path,mqd_t *Pmq,int flags,
		   unsigned int size,unsigned int maxmsgs)
#elif SYSV_MSGLIB
int msglib_createq(int msg_key,int *Pmq,int flags,unsigned int size)
#elif UNIX_SOCK_MSGLIB || STREAM_SOCK_MSGLIB
int msglib_createq(char *path,int *Pmq,unsigned int timeout)
#endif
{
#if POSIX_MSGLIB
  struct mq_attr buf;
  int ret;

  flags |= O_CREAT;
  if(size)
    buf.mq_msgsize = size;
  else
    buf.mq_msgsize = MSGQ_DEF_MSGSIZE;
  if(maxmsgs)
    buf.mq_maxmsg  = maxmsgs;
  else
    buf.mq_maxmsg  = MSGQ_DEF_MAXMSG;
  if((*Pmq=mq_open(path,flags,0600,&buf)) < 0)
    return -1;
#elif SYSV_MSGLIB
  struct msqid_ds buf;
  int ret;

  flags |= IPC_CREAT|0600;
  assert((*Pmq=msgget(msg_key,flags)) != -1);
  msgctl(*Pmq,IPC_STAT,&buf);
  if(size)
    {
      buf.msg_qbytes=size;
    }
  else
    buf.msg_qbytes = MSGQ_DEF_MSGSIZE;
  msgctl(*Pmq,IPC_SET,&buf);
#elif UNIX_SOCK_MSGLIB 
  int ret;
  int saddrlen;

  if((*Pmq=socket(AF_UNIX,SOCK_DGRAM,0)) < 0)
    return -1;
#elif STREAM_SOCK_MSGLIB
  int ret;
  int saddrlen;

  if((*Pmq=socket(AF_UNIX,SOCK_STREAM,0)) < 0)
    return -1;
#endif

  wrlock_msglib();

  ret = msglib_qfind(*Pmq);

  if(ret >= 0 && ret < num_msgs)
    {
#if POSIX_MSGLIB
      msg_array[ret].msg_path=(char *)sem_mem_alloc_temp(strlen(path)+1);
      if(!msg_array[ret].msg_path)
	{
	  msglib_delq(*Pmq,0);
	  unlock_msglib();
	  return -1;
	}
      strcpy(msg_array[ret].msg_path,path);
#elif SYSV_MSGLIB
      msg_array[ret].msg_key=msg_key;
#elif UNIX_SOCK_MSGLIB || STREAM_SOCK_MSGLIB
      if(path)
	{
	  unlink(path);
	  msg_array[ret].msg_path=(char *)sem_mem_alloc_temp(strlen(path)+1);
	  if(!msg_array[ret].msg_path)
	    {
	      msglib_delq(*Pmq,0);
	      unlock_msglib();
	      return -1;
	    }
	  strcpy(msg_array[ret].msg_path,path);
	  strcpy(msg_array[ret].saddr->sun_path,path);
	  saddrlen=strlen(path)+sizeof(msg_array[ret].saddr->sun_family);
	  strcpy(msg_array[ret].saddr->sun_path,path);
	  msg_array[ret].saddr->sun_family=AF_UNIX;
	  if(bind(*Pmq,(struct sockaddr*)msg_array[ret].saddr,saddrlen) < 0)
	    {
	      msglib_delq(*Pmq,0);
	      unlock_msglib();
	      return -1;
	    }
#if STREAM_SOCK_MSGLIB
	  listen(*Pmq,LISTEN_Q_SIZE);
#endif
	}
      else
	msg_array[ret].msg_path=NULL;
      msg_array[ret].timeout=timeout;
      fcntl(*Pmq,F_SETFD,FD_CLOEXEC);	/* close on exec... just to be sure. */
#endif
    }
  unlock_msglib();
  return ret;
}

/*
 * Delete message queue.
 * 0 on success.
 */
#if POSIX_MSGLIB
int msglib_delq(mqd_t mq,int remove)
#elif SYSV_MSGLIB || UNIX_SOCK_MSGLIB || STREAM_SOCK_MSGLIB
int msglib_delq(int mq,int remove)
#endif
{
  int i=msglib_qfind(mq);

  if(i>=0)
    {
#if POSIX_MSGLIB
      if(remove)
	mq_unlink(msg_array[i].msg_path);
#elif UNIX_SOCK_MSGLIB || STREAM_SOCK_MSGLIB
      if(remove)
	unlink(msg_array[i].msg_path);
#endif
      msglib_qfree(mq,remove);
    }

#if POSIX_MSGLIB
  if(mq_close(mq))
#elif SYSV_MSGLIB
  if(remove && msgctl(mq,IPC_RMID,NULL))
#elif UNIX_SOCK_MSGLIB || STREAM_SOCK_MSGLIB
  if(close(mq))
#endif
    {
      return -1;
    }
  return 0;
}



  

