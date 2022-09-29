#include <stdio.h>
#include <ns_api.h>

static ns_map_t map = {0};

main(int argc, char **argv)
{
	char buf[1024];

	if (argc < 3) {
		fprintf(stderr, "usage: %s map key\n", argv[0]);
		exit(1);
	}

	if (ns_lookup(&map, 0, argv[1], argv[2], 0, buf, sizeof(buf)) !=
	    NS_SUCCESS) {
		fprintf(stderr, "failed ns_lookup\n");
		exit(1);
	}
	
	puts(buf);
	exit(0);
}
