#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "sub_hosts.h"


void
sub_gethostent()
{
	struct hostent h, h2;
	char buf[4096], b2[4096], b3[4096];
	int i, herr;
	pthread_t thread, th2[1024];
	int	  th_cnt=0, th_cnt2 = 0;
	myHaddr_t	*laddr;

        pthread_t       th;

        th = pthread_self();
	

	while (gethostent_r(&h, buf, sizeof(buf))) {
		printf("Thread %d: name = %s, addrtype = %d, length = %d\n",
		    th, h.h_name, h.h_addrtype, h.h_length);
		for (i = 0; h.h_aliases[i]; i++) {
			printf("Thread1: \talias[%d]: %s\n", i, h.h_aliases[i]);
		}

				
		th_cnt2 = 0;
		for (i = 0; h.h_addr_list[i]; i++) {
			printf("\taddress[%d]: %s\n", i,
			    inet_ntoa(*(struct in_addr *)h.h_addr_list[i]));
			laddr = (myHaddr_t *)malloc(sizeof(myHaddr_t));
			if(laddr) {
				laddr->addr = h.h_addr_list[i];
				laddr->len = h.h_length;
				laddr->type = h.h_addrtype;
	
				pthread_create(&(th2[th_cnt2]),
					NULL,
					(void *)sub_gethostbyaddr,
					(void *)laddr);
				th_cnt2++;
			}
		}
		/*Consolidate the loop */
		for(i=0; i < th_cnt2; i++) {
		    	pthread_join(th2[th_cnt2], NULL);
		}
		pthread_create(&thread,
				NULL,
				(void *)sub_gethostbyname,
				(void *)h.h_name);
		pthread_join(thread, NULL);
	}
}

void
sub_gethostbyname(const char *name)
{
	struct hostent h2;
	int  herr;
	char buf[4096];
        pthread_t       th;

        th = pthread_self();
	if (! gethostbyname_r(name, &h2, buf, sizeof(buf), &herr)) {
		fprintf(stderr, "Thread gethostbyname: failed gethostbyname: %s\n",
		    name);
	} else {
		printf("Thread %d: name = %s, addrtype = %d, length = %d\n",
		    th, h2.h_name, h2.h_addrtype, h2.h_length);
	}
}


void
sub_gethostbyaddr(myHaddr_t *addrSt)
{
	struct hostent h2;
	int  herr;
	char buf[4096];
        pthread_t       th;

        th = pthread_self();

	if (! gethostbyaddr_r(addrSt->addr, addrSt->len,
	    addrSt->type, &h2, buf, sizeof(buf), &herr)) {
			fprintf(stderr, "Thread gethostbyaddr : failed gethostbyaddr: %s\n",
		    inet_ntoa(*(struct in_addr *)addrSt->addr));
	} else {
		printf("Thread %d: name = %s, addrtype = %d, length = %d\n",
		    th, h2.h_name, h2.h_addrtype, h2.h_length);
	}
}


