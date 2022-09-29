#ifndef __MSGLIB_H_
#define __MSGLIB_H_
#if POSIX_MSGLIB
#include <mqueue.h>        /* message queue stuff */
#include <unistd.h>        /* for getopt() */
#include <errno.h>         /* errno and perror */
#include <fcntl.h>         /* O_RDONLY */
#elif SYSV_MSGLIB
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#elif UNIX_SOCK_MSGLIB || STREAM_SOCK_MSGLIB
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>
#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#endif
#include <stdlib.h>        /* calloc(3) */
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#if POSIX_MSGLIB

#define MSGQ_SENDPRI 0	   /* Default Priority at which to send messages. */

extern int msglib_getmsg(mqd_t mq, char *mb,int len);
extern int msglib_createq(char *path,mqd_t *Pmq,int flags,unsigned int msgsize,
			  unsigned int maxmsgs);
extern int msglib_delq(mqd_t mq,int remove);
extern int msglib_maxmsgq(mqd_t mq);
extern int msglib_maxlenq(mqd_t mq);
extern int msglib_curmsgsq(mqd_t mq);
extern int msglib_sendmsg(mqd_t mq, char *mb,int len,int priority);

#elif SYSV_MSGLIB

extern int msglib_getmsg(int mq, char *mb,int len,int msgtype,int flags);
extern int msglib_createq(int msg_key,int *Pmq,int flags,unsigned int size);
extern int msglib_delq(int mq,int remove);
extern int msglib_maxmsgq(int mq);
extern int msglib_maxlenq(int mq);
extern int msglib_curmsgsq(int mq);
extern int msglib_sendmsg(int mq, char *mb,int len,int flags);

#elif UNIX_SOCK_MSGLIB || STREAM_SOCK_MSGLIB

extern int msglib_createq(char *path,int *Pmq,unsigned int timeout);
extern int msglib_delq(int mq,int remove);
extern int msglib_maxmsgq(int mq);
extern int msglib_maxlenq(int mq);
extern int msglib_curmsgsq(int mq);
#if UNIX_SOCK_MSGLIB
extern int msglib_sendmsg(int mq, char *mb,int len,char *Pchar_to);
extern int msglib_getmsg(int mq, char *mb,int len,int do_not_select);
#elif STREAM_SOCK_MSGLIB
extern int msglib_sendmsg(int mq, char *mb,int len,char *Pchar_to,
			  int doconnect);
extern int msglib_getmsg(int mq, char *mb,int len,int do_not_select,
			 int doaccept);
#endif

#if STREAM_SOCK_MSGLIB
#define ACCEPT_FLAG    1
#define CONNECT_FLAG   2
#define CLOSE_FLAG     4
#define LISTEN_Q_SIZE  50
#endif

#endif


extern int msglib_init(int num_queues);
extern void msglib_fork(void);

/*
 * Check parameters of messages in message queue.
 */
#define NUMMSGS(Q)             msglib_curmsgsq(Q)
#define MAXMSGS(Q)             msglib_maxmsgq(Q)
#define MAXMSGLEN(Q)           msglib_maxlenq(Q)
/*
 * Receive a message. Returns number of bytes received. On error returns <= 0.
 */
#if POSIX_MSGLIB

#define GETMSG(Q,BUF,LENGTH)   msglib_getmsg(Q,BUF,LENGTH)
/*
 * Send a message. Returns number of bytes sent. On error returns <= 0.
 */
#define SENDMSG(Q,BUF,LENGTH,PRI)  msglib_sendmsg(Q,BUF,LENGTH,PRI)
/*
 * Create message queue. >= 0 on success. -1 on fail.
 */
#define CREATEQ(PATH,PQ,FLAGS,SIZE,NUM) \
                               msglib_createq(PATH,PQ,FLAGS,SIZE,NUM)

#elif SYSV_MSGLIB

/*
 * Receive a message. Returns number of bytes received. On error returns <= 0.
 */
#define GETMSG(Q,BUF,LENGTH,TYPE,FLAGS)   \
                                     msglib_getmsg(Q,BUF,LENGTH,TYPE,FLAGS)
/*
 * Send a message. Returns number of bytes sent. On error returns <= 0.
 */
#define SENDMSG(Q,BUF,LENGTH,FLAGS)  msglib_sendmsg(Q,BUF,LENGTH,FLAGS)
/*
 * Create message queue. >= 0 on success. -1 on fail.
 */
#define CREATEQ(KEY,PQ,FLAGS,SIZE)   msglib_createq(KEY,PQ,FLAGS,SIZE)

#elif UNIX_SOCK_MSGLIB
/*
 * Receive a message. Returns number of bytes received. On error returns <= 0.
 */
#define GETMSG(Q,BUF,LENGTH,DO_NOT_SELECT)     \
                             msglib_getmsg(Q,BUF,LENGTH,DO_NOT_SELECT)
/*
 * Send a message. Returns number of bytes sent. On error returns <= 0.
 */
#define SENDMSG(Q,BUF,LENGTH,PCHAR_SENDQ)     \
                             msglib_sendmsg(Q,BUF,LENGTH,PCHAR_SENDQ)
/*
 * Create message queue. >= 0 on success. -1 on fail.
 */
#define CREATEQ(PATH,PQ,TIMEOUT)              \
                             msglib_createq(PATH,PQ,TIMEOUT)
#elif STREAM_SOCK_MSGLIB
/*
 * Receive a message. Returns number of bytes received. On error returns <= 0.
 */
#define GETMSG(Q,BUF,LENGTH,DO_NOT_SELECT,DOACCEPT)     \
                             msglib_getmsg(Q,BUF,LENGTH,DO_NOT_SELECT,DOACCEPT)
/*
 * Send a message. Returns number of bytes sent. On error returns <= 0.
 */
#define SENDMSG(Q,BUF,LENGTH,PCHAR_SENDQ,DOCONNECT)     \
                             msglib_sendmsg(Q,BUF,LENGTH,PCHAR_SENDQ,DOCONNECT)
/*
 * Create message queue. >= 0 on success. -1 on fail.
 */
#define CREATEQ(PATH,PQ,TIMEOUT)              \
                             msglib_createq(PATH,PQ,TIMEOUT)

#endif


/*
 * Free and remove message queue. 0 on success. -1 on fail.
 */
#define DELETEQ(Q)             msglib_delq(Q,1)
/*
 * Free but do not remove message queue.
 */
#define FREEQ(Q)               msglib_delq(Q,0)
/*
 * Initialize the message queue library. Need to call it before creating any 
 * queue. Call with the number of message queue's to create.
 */
#define INITQ(N)               msglib_init(N)

#endif /* __MSGLIB_H_ */
