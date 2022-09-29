#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <grp.h>
#include <sys/socket.h>
#include <alloca.h>

#include "sub_hosts.h"
#include "sub_groups.h"
#include "sub_nets.h"
#include "sub_protos.h"
#include "sub_pws.h"
#include "sub_rpcs.h"
#include "sub_servs.h"

void main_thread(const char *arg);

main(int argc, char **argv)
{
	pthread_t	th[1024];
	int		th_cnt=0;
	int		i;

	for (argc--, argv++; argc > 0; argc--, argv++) {
		pthread_create(&th[th_cnt], 
			NULL,
			(void *) main_thread,
			(void*)*argv);
		th_cnt++;
	}
	for(i = 0 ; i < th_cnt; i++) {
		pthread_join(th[i], NULL);
	}
}

void main_thread(const char *arg)
{
	pthread_t	th1, th2, th3, th4, th5, th6, th7;

	char *en_name;

	en_name = (char *)alloca(strlen(arg)+1);
	strcpy(en_name, arg);

	pthread_create(&th1, 
			NULL,
			(void *) sub_gethostbyname,
			(void*)en_name);
	en_name = (char *)alloca(strlen(arg)+1);
	strcpy(en_name, arg);
	pthread_create(&th2, 
			NULL,
			(void *) sub_getgrnam_r,
			(void*)en_name);
	en_name = (char *)alloca(strlen(arg)+1);
	strcpy(en_name, arg);
	pthread_create(&th3, 
			NULL,
			(void *) sub_getnetbyname,
			(void*)en_name);
	en_name = (char *)alloca(strlen(arg)+1);
	strcpy(en_name, arg);
	pthread_create(&th4, 
			NULL,
			(void *) sub_getprotobyname,
			(void*)en_name);
	en_name = (char *)alloca(strlen(arg)+1);
	strcpy(en_name, arg);
	pthread_create(&th5, 
			NULL,
			(void *) sub_getpwbyname,
			(void*)en_name);
	en_name = (char *)alloca(strlen(arg)+1);
	strcpy(en_name, arg);
	pthread_create(&th6, 
			NULL,
			(void *) sub_getrpcbyname,
			(void*)en_name);
	pthread_join(th1, NULL);
	pthread_join(th2, NULL);
	pthread_join(th3, NULL);
	pthread_join(th4, NULL);
	pthread_join(th5, NULL);
	pthread_join(th6, NULL);
}


