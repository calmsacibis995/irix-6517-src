#include <sys/types.h>
#include <stdio.h>
#include <time.h>

int
main(int argc, char *argv[])
{
	int	secs;
	int	max = 30;

	if (argc > 1)  {
		if (sscanf(argv[1], "%d", &max) != 1)  {
			fprintf(stderr, "bad parameter, using default value\n");
			fprintf(stderr, "\tof 30 seconds.\n");

			max = 30;
		}
	}

	srandom(time(NULL));
	secs = random();

	secs = secs % max;
	fprintf(stderr, "sleeping for %d seconds\n", secs);
	sleep(secs);
}
