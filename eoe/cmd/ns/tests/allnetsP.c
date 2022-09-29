#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>

#include "sub_nets.h"

main(int argc, char **argv)
{
	pthread_t th1, th2, th3;

	pthread_create(&th1,
			NULL,
			(void *)sub_getnetent,
			(void *)NULL);
	pthread_create(&th2,
			NULL,
			(void *)sub_getnetent,
			(void *)NULL);
	pthread_create(&th3,
			NULL,
			(void *)sub_getnetent,
			(void *)NULL);

	pthread_join(th1, NULL);
	pthread_join(th2, NULL);
	pthread_join(th3, NULL);

}

