/*
 * grow a large stack, operated on according to flags.
 *
 * this program crashed pmII's when run twice serially
 * (with FLAGS=0 and NPROC=1).
 */
# include "sys/types.h"
# include "sys/signal.h"
# define PAGESIZE 1022
# define DIRMODE	0777

int NPROC = 20;
int NPAGES = 80;
int CHILDSLEEP = 0;
int FLAGS = 017;
int NREPS = 20;
char *pigpen = "pigpen";

void
bye() { exit(0); }

main(argc, argv)
    int argc;
    char **argv;
{
    register char *ap;
    register int i;

    argc--; argv++;
    while( argc > 0 && *(ap = *argv) == '-' )
    {
	argc--; argv++;
	while(*++ap)
	switch(*ap)
	{
	case 'f':
	    if( --argc < 0 )
	    {
		printf("missing -f {FLAGS}\n");
		exit(1);
	    }
	    FLAGS = strtol(*argv++, (char *)0, 0);
	    break;
	default:
	    printf("unknown flag %c", *ap);
	    exit(1);
	    break;
	}
    }

    if( --argc >= 0 )
	NPROC = atoi(*argv++);
	
    if( --argc >= 0 )
	NREPS = atoi(*argv++);

    if( FLAGS&01 )
    {
	mkdir(pigpen, DIRMODE);
	chdir(pigpen);
    }

    signal(SIGQUIT, bye);

    for( i = 0; i < NPROC; i++ )
	forkchild(i);
}

forkchild(i)
    int i;
{
    register int pid;

    if( FLAGS&01 )
	pid = fork();
    else
	pid = 0;
    if( pid < 0 )
    {
	write(2, " !", 2);
	exit(1);
    }
    if( pid == 0 )
    {
	extern char *itoa();
	char buf[20];

	if( FLAGS&01 )
	{
	    strcpy(buf, itoa(getpid()));
	    mkdir(buf, DIRMODE);
	    chdir(buf);
	}
	child(i);
	exit(0);
    }
}

char *
itoa(n)
    register unsigned n;
{
    static char buf[10];
    register char *cp;
# define repeat		do
# define until(x)	while(!(x))
    
    cp = buf + sizeof buf;
    *--cp = '\0';
    repeat
    {
	*--cp = n%10 + '0';
	n /= 10;
    }
    until( n == 0 );
    return cp;
}

child(i)
    int i;
{
    for(i=0; i != NREPS; i++)
    {
	if( FLAGS & 02 )
	    sleep(CHILDSLEEP);
	burn(NPAGES);
	burn(NPAGES);
    }
}

burn(n)
    int n;
{
    register int i;
    int buf[PAGESIZE];

    if( --n < 0 )
	return;

    if( FLAGS & 04 )
    {
	for( i = 0; i < PAGESIZE; i++ )
	    buf[i] = (0xF7<<24) | (long)buf;
    }
    burn(n);
}
