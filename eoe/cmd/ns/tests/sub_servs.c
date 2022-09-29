#include <stdio.h>
#include <netdb.h>
#include <pthread.h>

#include "sub_servs.h"

void
sub_getservent()
{
	struct servent s;
	int i;
	char buf[4086];

	pthread_t	th, th2, th3;

	th = pthread_self();

	while (getservent_r(&s, buf, sizeof(buf))) {
		printf("Thread %d: name = %s, port = %d, protocol = %s\n",
		    th, s.s_name, s.s_port, s.s_proto);
		for (i = 0; s.s_aliases[i]; i++) {
			printf("\talias[%d]: %s\n", i, s.s_aliases[i]);
		}

		pthread_create(&th2,
				NULL,
				(void *)sub_getservbyname,
				(void *)&s);
		pthread_create(&th3,
				NULL,
				(void *)sub_getservbyport,
				(void *)&s);
		pthread_join(th2, NULL);
		pthread_join(th3, NULL);
	}
}

void
sub_getservbyname(struct servent *s)
{
	struct servent rets;
	char buf[4086];
	if (! getservbyname_r(s->s_name, s->s_proto, &rets, buf, sizeof(buf))) {
		fprintf(stderr, "failed getservbyname: %s\n",
		    s->s_name);
	}
}

void
sub_getservbyport(struct servent *s)
{
	struct servent rets;
	char buf[4086];
	if (! getservbyport_r(s->s_port, s->s_proto, &rets, buf, sizeof(buf))) {
		fprintf(stderr, "failed getservbyport: %d\n",
		    s->s_port);
	}
}
