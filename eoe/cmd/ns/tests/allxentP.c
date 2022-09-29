#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <grp.h>
#include <sys/socket.h>

#include "sub_hosts.h"
#include "sub_groups.h"
#include "sub_nets.h"
#include "sub_protos.h"
#include "sub_pws.h"
#include "sub_rpcs.h"
#include "sub_servs.h"

main(int argc, char **argv)
{
	pthread_t	th1, th2, th3, th4, th5, th6, th7;

	pthread_create(&th1, 
			NULL,
			(void *) sub_gethostent,
			(void*)NULL);
	pthread_create(&th2, 
			NULL,
			(void *) sub_getgrent,
			(void*)NULL);
	pthread_create(&th3, 
			NULL,
			(void *) sub_getnetent,
			(void*)NULL);
	pthread_create(&th4, 
			NULL,
			(void *) sub_getprotoent,
			(void*)NULL);
	pthread_create(&th5, 
			NULL,
			(void *) sub_getpwent,
			(void*)NULL);
	pthread_create(&th6, 
			NULL,
			(void *) sub_getrpcent,
			(void*)NULL);
	pthread_create(&th7, 
			NULL,
			(void *) sub_getservent,
			(void*)NULL);
	pthread_join(th1, NULL);
	pthread_join(th2, NULL);
	pthread_join(th3, NULL);
	pthread_join(th4, NULL);
	pthread_join(th5, NULL);
	pthread_join(th6, NULL);
	pthread_join(th7, NULL);
}


