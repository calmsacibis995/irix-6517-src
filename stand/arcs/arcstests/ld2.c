/*
 * ld2.c - test Execute sequences. Memory descriptors are currently
 *	freed after Invoke, so we should never run out of memory in
 *	this test. Have requested a new command to Invoke and Free,
 *	so that programs have more control of freeing memory descriptors.
 */

#include <arcs/io.h>

main(int argc, char **argv, char **envp)
{
	LONG errno;
	int i;

	if (argc < 1) {
		printf ("Usage: net()<host>:ld2 net()<host>:<ldtest> \n");
		return;
	}
	for (i=0; i<20; i++) {
		if (errno = Execute ((CHAR *)argv[1], (ULONG)argc, (CHAR **)argv, (CHAR **)envp)) {
			printf ("ld2: Error executing %s, loop %d\n", argv[1], i);
			return;
		}
	}
	printf ("ld2: successfully executed %s 20 times\n", argv[1]);
	return;
}
