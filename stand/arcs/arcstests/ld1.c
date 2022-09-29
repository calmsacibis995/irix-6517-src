/*
 * ld1.c - test Load/Invoke sequences. Pass 0 to Invoke, let Invoke
 *	set up stack area - not in spec yet, but requested.
 *
 * Example:
 *
 * 	net()<host>:ld1 net()<host>:testr 0x88100000 0x88200000
 */

#include <arcs/io.h>

main(int argc, char **argv, char **envp)
{
	LONG errno;
	ULONG ldaddr1, addr1, ldaddr2, addr2, pc1, pc2;

	if (argc < 3) {
		printf ("Usage: net()<host>:ld1 net()<host>:<ldtest> <addr1> <addr2>\n");
		return;
	}
	atobu (argv[2], &ldaddr1);
	atobu (argv[3], &ldaddr2);

	printf ("test=%s, ldaddr1=0x%x, ldaddr2=0x%x\n", argv[1], ldaddr1,
		ldaddr2);
	if (errno = Load ((CHAR *)argv[1], ldaddr1, &pc1, &addr1)) {
		printf ("ld1: Error loading %s at 0x%x\n", argv[1], ldaddr1);
		return;
	} else 
		printf ("ld1: Loaded %s, pc=0x%x\n", argv[1], pc1);

	if (errno = Load ((CHAR *)argv[1], ldaddr2, &pc2, &addr2)) {
		printf ("ld1: Error loading %s at 0x%x\n", argv[1], ldaddr2);
		return;
	} else
		printf ("ld1: Loaded %s, pc=0x%x\n", argv[1], pc2);

	if (errno = Invoke (pc1, 0, (LONG)argc, (CHAR **)argv, (CHAR **)envp)) {
		printf ("ld1: Error invoking %s at 0x%x\n", argv[1], ldaddr1);
		return;
	} else
		printf ("ld1: Executed %s, load 1, pc=0x%x\n", argv[1], pc1);

	if (errno = Invoke (pc2, 0, (LONG)argc, (CHAR **)argv, (CHAR **)envp)) {
		printf ("ld1: Error invoking %s at 0x%x\n", argv[1], ldaddr2);
		return;
	} else
		printf ("ld1: Executed %s, load 2,  pc=0x%x\n", argv[1], pc2);

	printf ("ld1: successfully loaded and invoked 2 tests\n");
	return;
}
