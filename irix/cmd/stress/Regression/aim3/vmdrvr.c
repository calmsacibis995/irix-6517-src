/*
 * AIM Suite III v3.1.1
 * (C) Copyright AIM Technology Inc. 1986,87,88,89,90,91,92.
 * All Rights Reserved.
 */

#ifndef	lint
	static char sccs_id[] = { " @(#) vmdrvr.c:3.2 5/30/92 20:19:01" };
#endif

#include <stdio.h>
#include <signal.h>
#include "vmem.h"
#include "vmtest.h"
#include "testerr.h"

/*
 * this program allocates as much memory as it can and then exercises that
 * memory.  It exercises the memory by touching locations with very little
 * locality of reference.  It touches bytes seperated by MINPAGE repeatedly
 * until stopped by a signal.  On reciept of such a signal it reports on how
 * many bytes it was able to allocate by writing to it's standard output.
 */

long nbytes;				/* amount of memory allocatable */
#ifdef DEBUGON
long debug;
#endif

long atol();
void stopper();
void starter();
varray *getmem();

main(argc,argv)
int argc;
char **argv;
{
    register varray *mem;
    register long index;

#ifdef DEBUGON
    if(argc < 2)
	debug = 0;
    else
	debug = atol(argv[1]);
#endif
    if((mem = getmem(&nbytes)) == NULL)  {
	nbytes = -1;
	write(1,&nbytes,sizeof nbytes);
	exit(1);
    }
    VMDEBUG(fprintf(stderr,"vmdrvr: getmem() got %ld bytes\n",nbytes));
    write(1,&nbytes,sizeof nbytes);	/* let parent process proceed */
    signal(SIGHUP,stopper);
    signal(SIGINT,starter);
    for(index = 0;;index = (index + MINPAGE) % nbytes)  {
	*vaddr(mem,index) += 1;		/* dirty the page */
    }
#ifdef LINT
    exit(0);
#endif
}

void stopper(sig)
int sig;
{
    signal(sig,stopper);
    write(1,&nbytes,sizeof nbytes);
    pause();
    return;
}

void starter(sig)
int sig;
{
    signal(sig,starter);
    return;
}


/*
 * getmem gets as much memory as the system will let it have
 */
varray *
getmem(amount)
long *amount;
{
    varray *tmp;
    long num,max = MAXMEM;

    for(num = MAXMEM;num > MINMEM;max = num,num >>= 1)  {
	if((tmp = vcalloc(num,sizeof (char))) != NULL)  {
	    vfree(tmp);
	    VMDEBUG(fprintf(stderr,"vmtest: got %ld bytes\n",num));
	    break;
	}
	else
	    VMDEBUG(fprintf(stderr,"vmtest: failed to get %ld bytes\n",num));
    }
    while(max > (num + 2048))  {
	if((tmp = vcalloc((num + max) / 2,sizeof (char))) != NULL)  {
	    vfree(tmp);
	    num = (num + max) / 2;
	}
	else  {
	    max = (num + max) / 2;
	}
    }
    if((tmp = vcalloc(num,sizeof (char))) == NULL)  {	/* deadly error */
	*amount = -1;
	VMDEBUG(fprintf(stderr,"vmtest: Whoops, we failed\n"));
	return NULL;
    }
    *amount = num;
    return tmp;
}
