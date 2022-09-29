/* * grow the stack and catch signal.
 */
# include "sys/types.h"
# include "sys/signal.h"
# define PAGESIZE 1024

int NPAGES = 240;
int ALARMTIME = 1;

main(argc, argv)
    int argc;
    char **argv;
{
    extern void bye();

    signal(SIGALRM, bye);
    alarm(ALARMTIME);
    child();
}

child()
{
    for( ;; )
	burn(NPAGES);
}

burn(n)
    int n;
{
    int buf[PAGESIZE];

    if( --n < 0 )
	return;
    bufzero(buf, PAGESIZE);
    burn(n);
}

bufzero(buf, len)
    register int *buf;
    register int len;
{
    if( len != PAGESIZE )
	goto wrong;

    while( --len >= 0 )
    {
	if( len >= PAGESIZE )
	    goto wrong;
	*buf++ = 0x10203040;
    }
    return;

wrong:
    printf(" len=$%x!\n", len);
    *(int *)0 = 0;
}

void
bye() { printf(" bye\n"); exit(0); }
