#include "common.h"
#include "events.h"
#include "events_private.h"
#include "seh.h"
#include "seh_archive.h"
#include "seh_errors.h"
#include "msglib.h"

#if POSIX_MSGLIB
static mqd_t mq_snd,mq_rcv;	/* message queue. */
#elif SYSV_MSGLIB || UNIX_SOCK_MSGLIB || STREAM_SOCK_MSGLIB
static int mq_snd,mq_rcv;	/* sockets */
#endif

/*
 * setup the interfaces to the archive port. This port only sends and 
 * receives the archive header. If there is an error the data is always
 * a string.
 */
__uint64_t seh_archive_setup_interfaces(int daemon)
{
    __uint64_t err=SEH_ERROR(SEH_MAJ_INIT_ERR,SEH_MIN_INIT_ARCHIVE);
    char archiveq[LARGE_BUFFER_SIZE];
    char sehq[LARGE_BUFFER_SIZE];
    int i=0;

    snprintf(sehq,LARGE_BUFFER_SIZE,SSS_WORKING_DIRECTORY "/%s.%d",
	     SEH_TO_ARCHIVE_MSGQ,
	     getpid());
    for(i=0;i<ARCHIVE_IDS;i++)
	snprintf(sehq,LARGE_BUFFER_SIZE,"%s.%lld",sehq,(__uint64_t)0);


    snprintf(archiveq,LARGE_BUFFER_SIZE,SSS_WORKING_DIRECTORY "/%s",
	     ARCHIVE_TO_SEH_MSGQ);

#if POSIX_MSGLIB
    if((CREATEQ(archiveq,&mq_rcv,O_RDONLY,0,0) < 0) ||
       (CREATEQ(sehq,&mq_snd,O_WRONLY,0,0) < 0))
      return err;
#elif SYSV_MSGLIB 
    if((CREATEQ(sehq,&mq_snd,0,0) < 0)  ||
       (CREATEQ(archiveq,&mq_rcv,0,0) < 0))
      return err;
#elif UNIX_SOCK_MSGLIB || STREAM_SOCK_MSGLIB
    if(daemon)
      {
	if((CREATEQ(archiveq,&mq_rcv,SEM_TIMEOUT_IO_Q) < 0))
	  return err;
      }
    else
      {
	if((CREATEQ(sehq,&mq_rcv,5) < 0))
	  return err;
      }
#endif
	
  return 0;
}

/*
 * Function returns the result of the event registration back to the task.
 * The task after sending (registering) the event to the SEH is expected to 
 * wait for the result of registering the event.
 */
__uint64_t seh_send_archive_port(int daemon,char *buffer,int length,int pid,
				 __uint64_t *aullArchive_id)
{
  int c;
  char *sendmsg;
  char sendq[ARCHIVE_BUFFER_SIZE*LARGE_BUFFER_SIZE];
  Archive_t *Parchive;
  int i;
  __uint64_t taullArchive_id[ARCHIVE_IDS];
  
  if(!aullArchive_id)
  {
      for(i=0;i<ARCHIVE_IDS;i++)
	  taullArchive_id[i]=0;
      aullArchive_id=taullArchive_id;
  }

  if(CREATEQ(NULL,&mq_snd,SEM_TIMEOUT_IO_Q) < 0)
    return SEH_ERROR(SEH_MAJ_INIT_ERR,SEH_MIN_INIT_ARCHIVE);

  if(daemon)
    {
      snprintf(sendq,ARCHIVE_BUFFER_SIZE*LARGE_BUFFER_SIZE,
	       SSS_WORKING_DIRECTORY "/%s.%d",SEH_TO_ARCHIVE_MSGQ,
	       pid);
      for(i=0;i<ARCHIVE_IDS;i++)
	  snprintf(sendq,LARGE_BUFFER_SIZE,"%s.%lld",sendq,aullArchive_id[i]);
    }
  else
    {
      snprintf(sendq,ARCHIVE_BUFFER_SIZE*LARGE_BUFFER_SIZE,
	       SSS_WORKING_DIRECTORY "/%s",ARCHIVE_TO_SEH_MSGQ);
    }
  
  sendmsg=sem_mem_alloc_temp(ARCHIVE_BUFFER_SIZE*LARGE_BUFFER_SIZE+length);
  if(!sendmsg)
    return SEH_ERROR(SEH_MAJ_ARCHIVE_ERR,SEH_MIN_ALLOC_MEM);

  Parchive=(Archive_t *)sendmsg;
  Parchive->LmsgKey=pid;
  for(i=0;i<ARCHIVE_IDS;i++)
      Parchive->aullArchive_id[i]=aullArchive_id[i];
  Parchive->LMagicNumber=ARCHIVE_MAGIC_NUMBER;
  Parchive->iVersion    =ARCHIVE_VERSION_1_0;
  Parchive->iDataLength =length;
  if(length && buffer)
  {
      memcpy((char *)&(Parchive->ptrData),buffer,length);
  }
  
#if POSIX_MSGLIB  
  c=SENDMSG(mq_snd,sendmsg,ARCHIVE_BUFFER_SIZE*LARGE_BUFFER_SIZE,MSGQ_SENDPRI);
#elif SYSV_MSGLIB
  c=SENDMSG(mq_snd,sendmsg,ARCHIVE_BUFFER_SIZE*LARGE_BUFFER_SIZE,0);
#elif UNIX_SOCK_MSGLIB
  c=SENDMSG(mq_snd,sendmsg,ARCHIVE_BUFFER_SIZE*LARGE_BUFFER_SIZE,sendq);
#elif STREAM_SOCK_MSGLIB
  c=SENDMSG(mq_snd,sendmsg,ARCHIVE_BUFFER_SIZE*LARGE_BUFFER_SIZE+length,sendq,
	    CONNECT_FLAG);
#endif
  
  FREEQ(mq_snd);
  sem_mem_free(sendmsg);

  if(c<=0)
    {
      return SEH_ERROR(SEH_MAJ_ARCHIVE_ERR,SEH_MIN_ARCHIVE_XMIT);
    }

  return 0;
}

/*
 * Function receives the event to be registered from the API.
 */
__uint64_t seh_receive_archive_port(int daemon,char **Pbuffer,int *len,
				    int *pid,__uint64_t *aullArchive_id)
{
  int c;
  char recvmsg[ARCHIVE_BUFFER_SIZE*LARGE_BUFFER_SIZE+1];
  Archive_t *Ptr_archive;
  int i=0;

  if(!Pbuffer || !len)
    return SEH_ERROR(SEH_MAJ_ARCHIVE_ERR,SEH_MIN_INVALID_ARG);

  Ptr_archive=(Archive_t *)recvmsg;

#if POSIX_MSGLIB  
  c=GETMSG(mq_rcv,recvmsg,ARCHIVE_BUFFER_SIZE*LARGE_BUFFER_SIZE);
#elif SYSV_MSGLIB
  c=GETMSG(mq_rcv,recvmsg,ARCHIVE_BUFFER_SIZE*LARGE_BUFFER_SIZE,0,0);
#elif UNIX_SOCK_MSGLIB
  c=GETMSG(mq_rcv,recvmsg,ARCHIVE_BUFFER_SIZE*LARGE_BUFFER_SIZE,
	   DO_NOT_SELECT_BEFORE_RECEIVE);
#elif STREAM_SOCK_MSGLIB
  /* 
   * In case of stream sockets, data sent with the event can be of any size.
   * so read the header first and then read the remaining data.
   */
  if(daemon)
    c=GETMSG(mq_rcv,recvmsg,(sizeof(Archive_t)-sizeof(void*)),
	     DO_NOT_SELECT_BEFORE_RECEIVE,ACCEPT_FLAG);
  else
    c=GETMSG(mq_rcv,recvmsg,(sizeof(Archive_t)-sizeof(void*)),
	     SELECT_BEFORE_RECEIVE,ACCEPT_FLAG);

  /* 
   * Not enough data in the header ?.
   */
  if(c<(sizeof(Archive_t)-sizeof(void *)))
    return SEH_ERROR(SEH_MAJ_ARCHIVE_ERR,SEH_MIN_ARCHIVE_RCV);

  if(Ptr_archive->LMagicNumber!=ARCHIVE_MAGIC_NUMBER)
    return SEH_ERROR(SEH_MAJ_ARCHIVE_ERR,SEH_MIN_ARCHIVE_GRB);

  if(Ptr_archive->iVersion != ARCHIVE_VERSION_1_0)
    return SEH_ERROR(SEH_MAJ_ARCHIVE_ERR,SEH_MIN_ARCHIVE_VER);

  *Pbuffer=sem_mem_alloc_temp((Ptr_archive->iDataLength+1)*sizeof(char));
  if(Ptr_archive->iDataLength > 0)
    {
      if(Ptr_archive->iDataLength > 
	 (ARCHIVE_BUFFER_SIZE*LARGE_BUFFER_SIZE-sizeof(Archive_t)))
	{			
	  /*
	   * In this case assume that we are getting a garbage data length
	   * and return an error code other than ALLOC_MEM. This is the
	   * fix for PV: 650137.
	   */
	  return SEH_ERROR(SEH_MAJ_ARCHIVE_ERR,SEH_MIN_ARCHIVE_RCV);      
	}

      /* 
       * Now get the rest of the  data. 
       */
      c=GETMSG(mq_rcv,*Pbuffer,
		Ptr_archive->iDataLength,SELECT_BEFORE_RECEIVE,CLOSE_FLAG);

      /* 
       * Not enough data in the event data ?.
       */
      if(c!=Ptr_archive->iDataLength || c<0)
	return SEH_ERROR(SEH_MAJ_ARCHIVE_ERR,SEH_MIN_ARCHIVE_RCV);
    }
  else				/* close the temporary socket. */
    GETMSG(mq_rcv,NULL,0,0,CLOSE_FLAG);
#endif


  /*
   * Some basic checking of the received message.
   * Did we get any data at all ?.
   * Did we get garbage ?.
   * Did we get the correct version number ?.
   */
#if !STREAM_SOCK_MSGLIB
  if(c<=0)
    return SEH_ERROR(SEH_MAJ_ARCHIVE_ERR,SEH_MIN_ARCHIVE_RCV);

  if(Ptr_archive->LMagicNumber!=EVENT_MAGIC_NUMBER || c < sizeof(Archive_t))
    return SEH_ERROR(SEH_MAJ_ARCHIVE_ERR,SEH_MIN_ARCHIVE_GRB);

  if(Ptr_archive->iVersion != ARCHIVE_VERSION_1_0)
    return SEH_ERROR(SEH_MAJ_ARCHIVE_ERR,SEH_MIN_ARCHIVE_VER);
#endif

  *pid=Ptr_archive->LmsgKey;
  if(aullArchive_id)
  {
      for(i=0;i<ARCHIVE_IDS;i++)
	  aullArchive_id[i]=Ptr_archive->aullArchive_id[i];
  }
  *len=Ptr_archive->iDataLength;
  return 0;
}

void seh_archive_clean()
{
  DELETEQ(mq_rcv);
}
