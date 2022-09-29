/*
 * external interrupts busywait initialization and reset functions.
 */
#ifdef __STDC__
	#pragma weak eicbusywait = _eicbusywait
	#pragma weak eicbusywait_f = _eicbusywait_f
	#pragma weak eicclear = _eicclear
	#pragma weak eicclear_f = _eicclear_f
	#pragma weak eicinit = _eicinit
	#pragma weak eicinit_f = _eicinit_f
#endif

#include "synonyms.h"
#include <sys/types.h>
#include <sys/ei.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <malloc.h>

static volatile int *eiccounter_g;
static int localcounter_g;

#define EIDEV "/hw/external_int/1"

struct ei_private {
    volatile int *eiccounter;
    int localcounter;
};

static int
do_eicinit(int fd, volatile int **eiccounter_p, int *localcounter_p)
{
    caddr_t counterpage;

    /* get the counter page */
    if ((counterpage = (caddr_t)mmap(0, getpagesize(), PROT_READ,
				     MAP_SHARED, fd, 0)) == (caddr_t)MAP_FAILED)
	return(-1);

    *eiccounter_p = (int*)counterpage;
    *localcounter_p = **eiccounter_p;

    /* enable the interrupts */
    if (ioctl(fd, EIIOCENABLE) < 0)
	return(-1);

    return(0);
}

int
eicinit(void)
{
    int fd;

    if ((fd = open(EIDEV, O_RDONLY)) < 0)
	return(-1);

    if (do_eicinit(fd, &eiccounter_g, &localcounter_g) < 0) {
	close(fd);
	return(-1);
    }

    close(fd);
    return(0);
}

void *
eicinit_f(int fd)
{
    struct ei_private *priv;

    if ((priv = (struct ei_private*)malloc(sizeof(struct ei_private))) == 0)
	return(0);

    if (do_eicinit(fd, &priv->eiccounter, &priv->localcounter) < 0) {
	free(priv);
	return(0);
    }

    return((void*)priv);
}

static void
do_eicclear(volatile int **eiccounter_p, int *localcounter_p)
{
    if (*eiccounter_p)
	*localcounter_p = **eiccounter_p;
}

void
eicclear(void)
{
    do_eicclear(&eiccounter_g, &localcounter_g);
}

void
eicclear_f(void *arg)
{
    struct ei_private *priv = (struct ei_private*)arg;

    do_eicclear(&priv->eiccounter, &priv->localcounter);
}

static int
do_eicbusywait(volatile int **eiccounter_p, int *localcounter_p, int spin)
{
    if (! *eiccounter_p)
	return(-1);

    if (spin) {
	while(*localcounter_p == **eiccounter_p);
	(*localcounter_p)++;
	return(1);
    }
    else {
	if (*localcounter_p == **eiccounter_p)
	    return(0);
	else {
	    (*localcounter_p)++;
	    return(1);
	}
    }
}

int
eicbusywait(int spin)
{
    return(do_eicbusywait(&eiccounter_g, &localcounter_g, spin));
}

int
eicbusywait_f(void *arg, int spin)
{
    struct ei_private *priv = (struct ei_private*)arg;
    
    return(do_eicbusywait(&priv->eiccounter, &priv->localcounter, spin));
}
