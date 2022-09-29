#include <sys/EVEREST/sysctlr.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#define MAX(a, b)	((a > b) ? a: b)

int
main(int argc, char *argv[]) {
	int i;
	int sysc;
	char buf[60];
	char num[3];

	if (argc != 2) {
		fprintf(stderr, "Usage: scmsg string_to_display\n");
		exit(1);
	}

	buf[0] = SC_ESCAPE;

	buf[1] = SC_SET;

	buf[2] = SC_MESSAGE;

	for (i = 0; i < MAX(strlen(argv[1]), 40); i++)
		buf[5 + i] = argv[1][i];

	buf[5 + i] = SC_TERM;

	sprintf(num, "%2x", i);

	/* Set the command length */
	buf[3] = num[0];
	buf[4] = num[1];

	sysc = open("/dev/sysctlr", O_RDWR);

	if (sysc < 0) {
		fprintf(stderr, "scmsg: Can't open /dev/sysctlr\n");
		return 1;
	}

	write(sysc, buf, 4 + i);

	close(sysc);

	return (0);
}

