/* #include <sys/syssgi.h> */
#define _KMEMUSER
#include <sys/types.h>
#include <sys/prctl.h>
#undef _KMEMUSER
#include <assert.h>
#include <ulocks.h>

extern void single_context(unsigned int threadid);
unsigned int *tid;

void thread_start(void)
{
   unsigned int myid;
   myid = __fetch_and_add(tid, 1);
   printf("%d\n", myid);
   single_context(myid);
}

int
main(int argc, char *argv[])
{
   int parallelism, i;
   if ((argc != 2) || ((parallelism = atoi(argv[1])) <= 0)) {
      printf("%s: parallelism\n", argv[0]);
      return -1;
   }
   tid = (unsigned int *)malloc(sizeof(unsigned int));
   *tid = 2;
   usconfig(CONF_INITUSERS, 1024);
   for (i=1; i<parallelism; i++) {
      sproc(thread_start, PR_SALL, NULL);
   }
   single_context(1);
}

void
failtest(short nid, int64_t i, int64_t j, int64_t iteration)
{
   if (j < -1) {
      printf("fail_test iter %d: (PRDA->t_sys.t_nid=%d) register a2=%d does not make sense\n", iteration, PRDA->t_sys.t_nid, j);
   } else if (j == -1) {
      printf("fail_test iter %d: (PRDA->t_sys.t_nid=%d) nid ?= a0(%d) a1(%d)\n", iteration, PRDA->t_sys.t_nid, nid, i);
   } else if (j < 32) {
      printf("fail_test iter %d: nid %d difference %d register %d\n", iteration, nid, i, j);
   } else if (j < 64) {
      printf("fail_test iter %d: nid %d difference %d register fp%d\n", iteration, nid, i,j-32);
   } else {
      printf("fail_test iter %d: (PRDA->t_sys.t_nid=%d) register a2=%d does not make sense\n", iteration, PRDA->t_sys.t_nid, j);
   }
   exit(-1);
}
