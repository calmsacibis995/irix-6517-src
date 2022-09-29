/*
 * periodically throw zombies at init.
 *
 * this program, in combination with heavy paging (?),
 * caused init misbehavior on pmII's.
 */

# include "sys/types.h"
# include "sys/signal.h"
# include "stdio.h"

int errno;

int NPROCS = 12;
int CHILDSLEEP = 5;
int PARENTSLEEP = 5;

main(argc, argv)
    int argc;
    char **argv;
{
    extern void bye();

    register int i;

    argc--; argv++;
    if( argc > 0 )
	NPROCS = atoi(*argv);

    signal(SIGQUIT, bye);
	for( i = 0; i < NPROCS; i++ )
	    forkchild(i);
	waitchild();
	sleep(PARENTSLEEP);
}

void
bye() { exit(0); }

forkchild(i)
    int i;
{
    register int pid;

    pid = fork();
    if( pid < 0 )
    {
	fprintf(stderr, " DUCK:%d\n", errno);
	fflush(stderr);
	exit(1);
    }
    if( pid == 0 )
    {
	refork(i);
	exit(0);
    }
}

refork(i)
    int i;
{
    register int pid;

    pid = fork();
    if( pid < 0 )
    {
	fprintf(stderr, ".%d", errno);
	fflush(stderr);
	exit(1);
    }
    if( pid == 0 )
    {
	child(i);
	exit(0);
    }
}

waitchild()
{
    while( wait((int *)0) > 0 )
	;
}

child(i)
    int i;
{
    sleep(CHILDSLEEP);
}
