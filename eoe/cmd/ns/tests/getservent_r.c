#include <stdio.h>
#include <netdb.h>


main(int argc, char **argv)
{
	struct servent s;
	char buf[4096];
	int i;

	while (getservent_r(&s, buf, sizeof(buf))) {
		printf("name = %s, port = %d, protocol = %s\n",
		    s.s_name, s.s_port, s.s_proto);
		for (i = 0; s.s_aliases[i]; i++) {
			printf("\talias[%d]: %s\n", i, s.s_aliases[i]);
		}
	}
}
