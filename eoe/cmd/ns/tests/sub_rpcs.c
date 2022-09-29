#include <stdio.h>
#include <netdb.h>
#include <pthread.h>

#include "sub_rpcs.h"

void
sub_getrpcent()
{
	struct rpcent r;
	char buf[4096];
	int i;
	pthread_t th, th2, th3;
	FILE *pf;

	if((pf = fopen("/etc/rpc", "r")) == NULL) {
	    printf("Failed to open file [/etc/rpc] \n");
	    exit(-1);
	}

	th = pthread_self();

	while (fgetrpcent_r(pf, &r, buf, sizeof(buf))) {
		printf("Thread %d :name = %s, number = %d\n", 
			th,r.r_name, r.r_number);
		for (i = 0; r.r_aliases[i]; i++) {
			printf("\talias[%d]: %s\n", i, r.r_aliases[i]);
		}
		pthread_create(&th2,
				NULL,
				(void *)sub_getrpcbyname,
				(void *)r.r_name);
		pthread_create(&th3,
				NULL,
				(void *)sub_getrpcbynumber,
				(void *)r.r_number);

		pthread_join(th2, NULL);
		pthread_join(th3, NULL);
	}
	fclose(pf);

}

void sub_getrpcbyname(const char *name)
{
	struct rpcent r;
	char buf[4096];

	if (! getrpcbyname_r(name, &r, buf, sizeof(buf))) {
		fprintf(stderr, "failed getrpcbyname: %s\n", r.r_name);
	}
}

void sub_getrpcbynumber(long number)
{
	struct rpcent r;
	char buf[4096];

	if (! getrpcbynumber_r(number, &r, buf, sizeof(buf))) {
		fprintf(stderr, "failed getrpcbynumber: %d\n",
		    r.r_number);
	}
}

