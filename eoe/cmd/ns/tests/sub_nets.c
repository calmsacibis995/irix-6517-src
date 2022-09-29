#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>

#include "sub_nets.h"


void
sub_getnetent()
{
	struct netent n, *n2;
	struct in_addr a;
	int i;
	char buf[4096];
	pthread_t	thread, thread2;
	char 	*nm;
	long 	nnet;
	pthread_t	th;
	th = pthread_self();
	while (getnetent_r(&n, buf, sizeof(buf))) {
		a = inet_makeaddr(n.n_net, 0);
		printf("Thread %d :name = %s, addrtype = %d, net = %s\n",
		    th, n.n_name, n.n_addrtype, inet_ntoa(a));
		for (i = 0; n.n_aliases[i]; i++) {
			printf("\talias[%d]: %s\n", i, n.n_aliases[i]);
		}

		nm = (char *) malloc(strlen(n.n_name)+1);
		strcpy(nm, n.n_name);
		pthread_create(&thread,
				NULL,
				(void *) sub_getnetbyname,
				(void *)nm);
		nnet = n.n_net;
		pthread_create(&thread2,
				NULL,
				(void *) sub_getnetbyaddr,
				(void *)nnet);
		pthread_join(thread, NULL);
		pthread_join(thread2, NULL);
	}
}

void 
sub_getnetbyname(const char *name)
{
	char buf[4096];
	struct netent retn;
	struct in_addr a;
	if(! getnetbyname_r(name, &retn, buf, sizeof(buf))) {
		fprintf(stderr, "Thread getnetbyname_r :failed getnetbyname_r: %s\n", name);
	}  /* else {
		a = inet_makeaddr(retn.n_net, 0);
		printf("Thread getnetbyname_r : name = %s, addrtype = %d, net = %s\n",
		    retn.n_name, retn.n_addrtype, inet_ntoa(a));
	} */
}

void
sub_getnetbyaddr(long net)
{
	char buf[4096];
	struct in_addr a;
	struct netent retn;

	a = inet_makeaddr(net, 0);

	if ( ! getnetbyaddr_r(net, AF_INET, &retn, buf, sizeof(buf))) {
		fprintf(stderr, "Thread getnetbyaddr_r : failed getnetbyaddr_r: %s\n", inet_ntoa(a));
	} /*else {
		printf("Thread getnetbyaddr_r : name = %s, addrtype = %d, net = %s\n",
		    retn.n_name, retn.n_addrtype, inet_ntoa(a));
	} */
}
