#include <stdio.h>
#include <netdb.h>

#include "sub_protos.h"

void
sub_getprotoent()
{
	struct protoent p;
	int i;
	char buf[4096];
	pthread_t	th, th1, th2;

	th = pthread_self();
	while (getprotoent_r(&p, buf, sizeof(buf))) {
		printf("Thread %d : name = %s, number = %d\n", 
			th, p.p_name, p.p_proto);
		for (i = 0; p.p_aliases[i]; i++) {
			printf("\talias[%d]: %s\n", i, p.p_aliases[i]);
		}
		
		pthread_create(&th1,
				NULL,
				(void *)sub_getprotobyname,
				(void *)p.p_name);
		pthread_create(&th2,
				NULL,
				(void *)sub_getprotobynumber,
				(void *)p.p_proto);
				
		pthread_join(th1, NULL);
		pthread_join(th2, NULL);
	}
}

void
sub_getprotobyname(const char *name)
{
	struct protoent retp;
	char buf[4096];
	if (! getprotobyname_r(name, &retp, buf, sizeof(buf))) {
		fprintf(stderr, "failed getprotobyname: %s\n",
		    name);
	}
}

void
sub_getprotobynumber(long number)
{
	struct protoent retp;
	char buf[4096];
	if (! getprotobynumber_r(number, &retp, buf, sizeof(buf))) {
		fprintf(stderr, "failed getprotobynumber: %d\n",
		    number);
	}
}
