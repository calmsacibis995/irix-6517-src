#define MSGQ_VALID   1
#define MSGQ_INVALID 0
#define MSGLIB_DEBUG 1
#if POSIX_MSGLIB
#define MSGQ_DEF_MSGSIZE 4096
#define MSGQ_DEF_MAXMSG  512
#elif SYSV_MSGLIB
#define MSGQ_DEF_MSGSIZE (4*1024*1024)
#elif UNIX_SOCK_MSGLIB || STREAM_SOCK_MSGLIB
#define MSGQ_DEF_MSGSIZE MAX_MESSAGE_LENGTH
#endif
/*
 * each message has these parameters... in addition to the attributes...which
 * may be useful oneday.
 */
struct msg_local {
  int valid;
#if POSIX_MSGLIB
  mqd_t msg_q;
  struct mq_attr *msg_attr;
  char *msg_buf;
  unsigned int last_msg_prio;
  char *msg_path;
#elif SYSV_MSGLIB
  int msg_q;
  int msg_key;
  struct msqid_ds *msg_attr;
#elif UNIX_SOCK_MSGLIB || STREAM_SOCK_MSGLIB
  int msg_q;			/* socket. */
  struct sockaddr_un *saddr;	/* sock addr structure contains the path too.*/
  time_t timeout;		/* timeout for select. */
  int size;			/* size of send and receive buffer. */
  char *msg_path;
#endif
#if STREAM_SOCK_MSGLIB
  int newsock;			/* temporary socket. */
#endif

  unsigned int msg_sent;
  unsigned int msg_rcvd;
} msg_local;

static struct msg_local *msg_array;
static int num_msgs;

#if POSIX_MSGLIB
static void msglib_qfree(mqd_t mq,int debug);
static int msglib_qfind(mqd_t mq);
#elif SYSV_MSGLIB || UNIX_SOCK_MSGLIB || STREAM_SOCK_MSGLIB
static void msglib_qfree(int mq,int debug);
static int msglib_qfind(int mq);
#endif
