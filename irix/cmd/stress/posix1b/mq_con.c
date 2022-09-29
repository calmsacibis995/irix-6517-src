#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <mqueue.h>
#include <sys/msg.h>
#include <errno.h>
#include <unistd.h>

#define MQ_NAME "/usr/tmp/mq_procon.posix"
#define MSG_SIZE 100

#define MAGIC_NUMBER1 0xd
#define MAGIC_NUMBER2 0xe

main(int argc, char *argv[])
{
  int messages;
  mqd_t mq;
  char msg_buffer[MSG_SIZE];

  if (argc < 2) {
    printf("mq_con: FAILED - number of messages was not specified\n");
    exit(1);
  }

  messages = atoi(argv[1]);

  mq = mq_open(MQ_NAME, O_RDWR);
  if (mq == (mqd_t) -1) {
    perror("mq_con: FAILED - mq_open");
    exit(1);
  }

  while (messages--) {

    msg_buffer[0] = 0;
    msg_buffer[MSG_SIZE-1] = 0;

    if (mq_receive(mq, msg_buffer, MSG_SIZE, NULL) != MSG_SIZE) {
      perror("mq_con: FAILED - mq_receive");
      exit(1);
    }

    /*
     * Check message validity
     */
    if ((msg_buffer[0] != MAGIC_NUMBER1) ||
	(msg_buffer[MSG_SIZE-1] != MAGIC_NUMBER2)) {
      printf("mq_con: FAILED - bad message");
      exit(1);
    }
  }
    exit(0);
}
     

