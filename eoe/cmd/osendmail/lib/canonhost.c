#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define PATTERN "canonhost: Error \"%s\" - %s\n"

void
error(type, hostname)
	int type;
	char *hostname;
{
	switch(h_errno) {
	case HOST_NOT_FOUND:
		fprintf(stderr, PATTERN, hostname, "Host not found");
		break;
	case TRY_AGAIN:
		fprintf(stderr, PATTERN, hostname,
			"Temporary nameserver failure");
		break;
	case NO_RECOVERY:
		fprintf(stderr, PATTERN, hostname,
			"Non-recoverable nameserver failure");
		break;
	case NO_DATA:
		fprintf(stderr, PATTERN, hostname,
			"No data");
		break;
	}
}

main(argc, argv)
  int argc;
	char **argv;
{
	register int cnt;
	struct hostent *hostp;
	extern int h_errno;
	int rtn = 0;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s host [host...]\n", argv[0]);
		exit(-1);
	}
	for (cnt = 1; cnt < argc; cnt++) {
		if ((hostp = gethostbyname(argv[cnt])) == NULL) {
			rtn = -1;
			error(h_errno, argv[cnt]);
			continue;
		}
		printf("%s\n", hostp->h_name);
	}
	exit(rtn);
}
