/*
 * flushmem - cause most things to page out
 *
 * This program just mallocs a lot of stuff and then writes to it
 * to force the VM system to allocate memory for it.  Typically,
 * you need to run two or three of these simultaneously to trigger
 * all of the flush routines in the kernel.
 *
 * $Revision: 1.1 $
 */
main()
{
	char *buf;
	int idx;

	for (idx = 0; idx < 10 * 1024; idx++) {
		if ((buf = (char *)malloc (4*1024)) == (char *)0) {
			perror ("malloc");
			exit (2);
		}
		buf[27] = idx;
	}
	exit (0);
}
