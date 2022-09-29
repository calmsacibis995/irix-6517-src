
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>

#define SIGRESTART 34

void start_child(int, int);
int count_tree(int, int);

#define	WAITTIME	20

#define	GLOBALSZ	2000*1024
#define	STACKSZ		1000*1024
#define	BUFSZ		500*1024
#define	NBUFS		30

int global[GLOBALSZ];

void
scratch_mem(int *g, int *s)
{
	int i, j;
	char *buf;

	for (i = 0; i < GLOBALSZ; i++) {
		*(g++) = 1;
#ifdef COMP
		*(g++) = rand();
#endif
	}
	for (i = 0; i < STACKSZ; i++) {
		*(s++) = rand();
	}
	for (i = 0; i < NBUFS; i++) {
		buf = malloc(BUFSZ);
		for (j = 0; j < BUFSZ; j++) {
			*(buf++) = 1;
#ifdef COMP
			*(buf++) = (char)rand();
#endif
		}
	}
}

main(int argc, char *argv[])
{
	int depth = atoi(argv[1]);
	int span = atoi(argv[2]);
	pid_t root = getpid();
	pid_t ctest_pid = getppid();
	int stack[STACKSZ];

	scratch_mem(global, stack);

	signal(SIGRESTART, 0);

	printf("\n\tlarge: Total Number of Processes: %d\n\n", count_tree(depth, span));
	printf("\tlarge: PID %4d (depth, span)=(%d,%d) ROOT\n", root, depth, span);

	if (depth > 0 && span > 0 && depth < 200 && span < 200)
		start_child(--depth, span);

	sleep(WAITTIME);
	kill(ctest_pid, SIGUSR1);
	printf("\n\tlarge: Done\n");
}

void
start_child(int depth, int span)
{
	pid_t child;
	int stat, i;

	for (i = 0; i < span; i++) {
		if ((child = fork()) == 0) {
			if (depth > 0)
				start_child(--depth, span);

			sleep(WAITTIME);

			exit(0);
		}
		if (child < 0) {
			perror("fork");
			return;
		}
		printf("PID %4d (depth, span)=(%d,%d)\n", child, depth, i);
	}
	wait(&stat);
}

int
count_tree(int d, int s)
{
	int count = 0, i, j, mul;

	for (i = 0; i <= d; i++) {
		mul = 1;
		for (j = 0; j < i; j++)
			mul *= s;
		count += mul;
	}
	return (count);
}
