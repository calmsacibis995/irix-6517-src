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
#include "mq_local.h"

#define MAX_MQ_NAME 400

int periodic = 0;

main(int argc, char **argv)
{
  mqd_t mq;
  char mq_name[MAX_MQ_NAME] = {{0}};
  int parse;
  posix_mq_t *pmq = NULL;
  int msg_index;
  int iteration = 1;

  extern void _print_pmq(posix_mq_t *pmq);

  while ((parse = getopt(argc, argv, "q:p:")) != EOF)
    switch (parse) {
    case 'q':
      strncpy(mq_name, optarg, MAX_MQ_NAME);
      break;
    case 'p':
      periodic = atoi(optarg);
      break;
    default:
      printf("usage: mq_view -q filename -p period\n");
      exit(1);
    }

  if (mq_name[0] == 0) {
    printf("usage: mq_view -q filename -p period\n");
    exit(1);
  }

  mq = mq_open(mq_name, O_RDWR);
  if (mq == (mqd_t) -1) {
    printf("mq_view: cannot open %s\n", mq_name);
    exit(1);
  }

  printf("MQ: %s\n", mq_name);

  pmq = ((pmqd_t *) mq)->pmqd_mq;


repeat:

  if (periodic)
    printf("[%d] ==================================================\n",
	   iteration++);

  printf("Message max = %d Message size = %d Messages queued = %d\n",
	   pmq->pmq_maxmsg, pmq->pmq_msgsize, pmq->pmq_curmsgs);

  printf("Blocked Receivers = %d Blocked Senders = %d\n",
	   pmq->pmq_receivers, pmq->pmq_senders);

  sem_print(&pmq->pmq_semlock, stdout, "MQ Sem");

#if 0
  printf("free_list\n");
	
  msg_index = pmq->pmq_list[MQ_PRIO_MAX].pmql_first;
  while (msg_index != PMQ_NULL_MSG) {
    printf("\t%3d\n",msg_index);
    msg_index = pmq->pmq_hdr[msg_index].pmqh_next;
  }
#endif

  if (periodic) {
    sleep(periodic);
    goto repeat;
  }

  /* Close isn't called to avoid library level locking
   * mq_close(mq);
   */

  exit(0);
}
     

