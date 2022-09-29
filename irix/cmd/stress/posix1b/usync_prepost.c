#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <semaphore.h>
#include <sys/usync.h>

sem_t sem;

main(int argc, char *argv[])
{
  	usync_arg_t sarg;
	int ret = 0;

	sem_init(&sem, 0, 0);

	sarg.ua_version = USYNC_VERSION_1;
	sarg.ua_addr = (__uint64_t) &sem;
	sarg.ua_flags = 0;

	ret = usync_cntl(USYNC_UNBLOCK, &sarg);
	sem_getvalue(&sem, &ret);
	printf("value = %d\n", ret);

	ret = usync_cntl(USYNC_UNBLOCK, &sarg);
	sem_getvalue(&sem, &ret);
	printf("value = %d\n", ret);

	ret = usync_cntl(USYNC_UNBLOCK, &sarg);
	sem_getvalue(&sem, &ret);
	printf("value = %d\n", ret);

	ret = usync_cntl(USYNC_UNBLOCK, &sarg);
	sem_getvalue(&sem, &ret);
	printf("value = %d\n", ret);

	ret = usync_cntl(USYNC_UNBLOCK, &sarg);
	sem_getvalue(&sem, &ret);
	printf("value = %d\n", ret);

	sem_post(&sem);
	sem_getvalue(&sem, &ret);
	printf("post value = %d\n", ret);

	
	while (sem_trywait(&sem) == 0) {
	  sem_getvalue(&sem, &ret);
	  printf("loop value = %d ... usem = %d\n", ret, sem.sem_value);
	}

	if (sem_destroy(&sem)) {
	  perror("sem_destroy");
	}

	exit(0);
}
