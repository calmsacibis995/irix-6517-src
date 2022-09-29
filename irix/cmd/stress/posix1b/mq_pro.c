#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <semaphore.h>
#include <time.h>
#include <sched.h>
#include <ulocks.h>
#include <sys/mman.h>
#include <errno.h>
#include <mqueue.h>
#include <wait.h>

#define CHILD_NAME "./mq_con"
#define MQ_NAME    "/usr/tmp/mq_procon.posix"
#define MSG_SIZE   100

#define MAGIC_NUMBER1 0xd
#define MAGIC_NUMBER2 0xe

main(int argc, char *argv[])
{
  int messages = 10;
  int children = 1;
  
  int parse;
  int pid;
  int x;
  mqd_t mq;
  char cmessages[10];
  char msg_buffer[MSG_SIZE];
  struct mq_attr mq_attr;
  int mqsize = 2;

  while ((parse = getopt(argc, argv, "m:c:s:")) != EOF)
    switch (parse) {
    case 'm':
      messages = atoi(optarg);
      break;
    case 'c':
      children = atoi(optarg);
      break;
    case 's':
      mqsize = atoi(optarg);
      break;
    }

  printf("mq_pro: sending %d messages to each of %d consumers\n",
	 messages, children);

  sprintf(cmessages, "%d", messages);

  mq_unlink(MQ_NAME);

  mq_attr.mq_maxmsg = mqsize;
  mq_attr.mq_msgsize = MSG_SIZE;
  mq_attr.mq_flags = 0;
  
  mq = mq_open(MQ_NAME, O_CREAT|O_RDWR, S_IRWXU, &mq_attr);
  if (mq == (mqd_t) -1) {
    perror("mq_pro: FAILED - mq_open");
    exit(1);
  }
  
  for (x=0; x < children; x++) {
    if ((pid = fork()) == -1) {
      perror("mq_pro: FAILED - child fork failure");
      exit(1);
    }

    if (pid == 0) {
      if (execlp(CHILD_NAME, CHILD_NAME, cmessages, NULL) < 0) {
	perror("mq_pro: FAILED - exec");
	exit(1);
      }
    }
  }

  messages *= children;

  msg_buffer[0] = MAGIC_NUMBER1;
  msg_buffer[MSG_SIZE-1] = MAGIC_NUMBER2;

  while (messages--)
    if (mq_send(mq, msg_buffer, MSG_SIZE, 1) < 0) {
      perror("mq_pro: FAILED - mq_send");
      exit(1);
    }
  
  wait(NULL);
  
  if (mq_unlink(MQ_NAME) < 0) {
    perror("mq_pro: FAILED - mq_unlink");
    exit(1);
  }

  if (mq_close(mq) < 0) {
    perror("mq_pro: FAILED - mq_close");
    exit(1);
  }

  printf ("mq_pro: PASSED\n");

  exit(0);
}
     
