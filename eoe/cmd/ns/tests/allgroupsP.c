#include <stdio.h>
#include <pthread.h>
#include <grp.h>

#include "sub_groups.h"


main(int argc, char **argv)
{
        char buf[4096];
	pthread_t	th1, th2;
	
	pthread_create(&th1,
			NULL,
			(void *)sub_getgrent,
			(void *)NULL);
	pthread_create(&th2,
			NULL,
			(void *)sub_getgrent,
			(void *)NULL);
	pthread_join(th1, NULL);
	pthread_join(th2, NULL);

	printf("Happy ending\n");

}

