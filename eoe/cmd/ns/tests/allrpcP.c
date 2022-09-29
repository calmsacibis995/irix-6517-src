#include <stdio.h>
#include <netdb.h>
#include <pthread.h>

#include "sub_rpcs.h"

main(int argc, char **argv)
{
	struct rpcent *r, r2;
	char buf[4096], b2[4096];
	int i;

	pthread_t	th1, th2;

	pthread_create(&th1,
			NULL,
			(void *)sub_getrpcent,
			(void *)NULL);

	pthread_create(&th2,
			NULL,
			(void *)sub_getrpcent,
			(void *)NULL);


	pthread_join(th1, NULL);
	pthread_join(th2, NULL); 
}

