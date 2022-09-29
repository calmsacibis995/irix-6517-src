
#include <stdio.h>
#include <sys/lock.h>

extern _fdata;

#define	ONEPAGE 4096

main(int argc, char **argv)
{
	char *cp, *cp0 = 0;
	int np, npages = atoi(argv[1]);

	if (geteuid() != 0)
		fprintf(stderr,"hog: WARNING not uid 0, continueing...\n");
		

	for (np = npages; np--; ) {
		if ((cp = (char*)sbrk(ONEPAGE)) == (char*)-1) {
			perror("sbrk(4096)");
			exit(1);
		}
		*cp = '[';	/* touch the page */
		if (cp0 == 0)	/* save the old brk value */
			cp0 = cp;
	}
	printf("hogging %d kbytes\n", ((char*)sbrk(0) - (char*)&_fdata)>>10);

	for (;;)
		for (np = npages, cp = cp0; np--; cp += ONEPAGE) {
			sginap(10);
			*cp = ']';
		}
/*
	switch ( fork() ) {
	case -1:
		perror("fork");
		exit(1);
	case 0:
		if ( plock(DATLOCK) == -1 )
			perror("plock(DATLOCK)");
		pause();
	default:
		exit(0);
	}
*/
}
