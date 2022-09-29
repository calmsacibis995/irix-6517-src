/**************************************************************************
 *                                                                        *
 *       Copyright (C) 1997, Silicon Graphics, Inc.                       *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * Support for accessing the cycle counter used to
 * set time values events (both from the kernel and
 * from user processes).
 */
#include "rtmond.h"
#include "timer.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/syssgi.h>

struct cyclecounter cc;

uint64_t
readcc(void)
{
    return cc.iotimer_size == 64 ? *(cc.iotimer_addr64) : *(cc.iotimer_addr32);
}

void
map_timer(void)
{
    int poffmask, fd;
    __psunsigned_t phys_addr;
    void *map_addr;

    /* map the cycle counter into user space */
    cc.iotimer_size = syssgi(SGI_CYCLECNTR_SIZE);
    poffmask = getpagesize() - 1;
    phys_addr = syssgi(SGI_QUERY_CYCLECNTR, &cc.cycleval);
    if (phys_addr == -1)
	Fatal(NULL, "SGI_QUERY_CYCLECNTR failed: %s", strerror(errno));

    fd = open("/dev/mmem", O_RDONLY);
    if (fd >= 0) {
	map_addr = mmap(0, poffmask, PROT_READ, MAP_PRIVATE,
	    fd, phys_addr & ~poffmask);
	if (map_addr == MAP_FAILED)
	    Fatal(NULL, "Cannot mmap /dev/mmem: %s", strerror(errno));
	close(fd);
    } else
	Fatal(NULL, "Cannot open /dev/mmem: %s", strerror(errno));

    if (cc.iotimer_size == 64)
	cc.iotimer_addr64 = (iotimer64_t *)((__psunsigned_t)map_addr
					    + (phys_addr & poffmask));
    else
	cc.iotimer_addr32 = (iotimer32_t *)((__psunsigned_t)map_addr
					    + (phys_addr & poffmask));

    IFTRACE(DEBUG)(NULL, "Mapped cycle counter size: %d", cc.iotimer_size);
}
