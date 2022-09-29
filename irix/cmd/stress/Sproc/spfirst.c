#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/wait.h>


extern char **environ;
/* ARGSUSED */
void
slave(void *a)
{
	while (getppid() != 1)
		sginap(1);
	printf("spfirst:environ 0x%x PATH:%s\n", environ, getenv("PATH"));
}

int
main(int argc, char **argv)
{
	(void)sproc(slave, PR_SALL, 0);
	exit(0);
	/* NOTREACHED */
}
