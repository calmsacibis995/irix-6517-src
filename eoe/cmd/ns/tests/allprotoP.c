#include <stdio.h>
#include <netdb.h>


#include "sub_protos.h"

main(int argc, char **argv)
{
	struct protoent p;
	int i;
	char buf[4096];

	pthread_t th1, th2;

	pthread_create(&th1,
			NULL,
			(void *)sub_getprotoent,
			(void *)NULL);
	pthread_create(&th2,
			NULL,
			(void *)sub_getprotoent,
			(void *)NULL);

	pthread_join(th1, NULL);
	pthread_join(th2, NULL);
}

