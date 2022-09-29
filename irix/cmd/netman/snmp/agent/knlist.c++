/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Kernel name list procedures
 *
 *	$Revision: 1.2 $
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#include <sys/types.h>
#include <syslog.h>
#include <nlist.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <elf.h>
#include "knlist.h"
extern "C" {
#include "exception.h"
};

static int kmem;
short is_elf64 = 0;

// Keep in sync with knlist.h
#ifdef sgi
struct nlist64 nl64[] = {
    { "kernel_magic" },
    { "end" },
    { "ifnet" },
    { "arptab" },
    { "arptab_size" },
    { "ipstat" },
    { "ipforwarding" },
    { "rthost" },
    { "rtnet" },
    { "rthashsize" },
    { "icmpstat" },
    { "tcpstat" },
    { "tcb" },
    { "udpstat" },
    { "udb" },
    "",
};
#endif /* sgi */
struct nlist nl[] = {
    { "kernel_magic" },
    { "end" },
    { "ifnet" },
    { "arptab" },
    { "arptab_size" },
    { "ipstat" },
    { "ipforwarding" },
    { "rthost" },
    { "rtnet" },
    { "rthashsize" },
    { "icmpstat" },
    { "tcpstat" },
    { "tcb" },
    { "udpstat" },
    { "udb" },
    "",
};

void
init_nlist(char *sysname)
{
    long kern_end;
    int g1, g2, g3;

#ifdef sgi
    int fd;
    char hbuf[EI_CLASS+1];

    if ((fd = open(sysname, O_RDONLY)) < 0) {
	    exc_errlog(LOG_ERR, errno, "cannot open /unix");
            exit(1);
    }

    if (read(fd, hbuf, sizeof(hbuf)) != sizeof(hbuf)) {
	    exc_errlog(LOG_ERR, errno, "read error in /unix");
            exit(1);
    }

    if (hbuf[EI_MAG0] == ELFMAG0 && hbuf[EI_MAG1] == ELFMAG1 &&
        hbuf[EI_MAG2] == ELFMAG2 && hbuf[EI_MAG3] == ELFMAG3 &&
        hbuf[EI_CLASS] == ELFCLASS64)
            is_elf64 = 1;
    (void)close(fd);


    if (is_elf64) {
       exc_errlog(LOG_DEBUG, 0,  "init_nlist: using nlist64");
       nlist64(sysname, nl64);
    } else
#endif  /* sgi */
    {
       exc_errlog(LOG_DEBUG, 0,  "init_nlist: using nlist");
       nlist(sysname, nl);
    }

    if (
#ifdef sgi
        (is_elf64 && (nl64[0].n_type == 0)) || 
#endif /* sgi */
        (!is_elf64 && (nl[0].n_type == 0))
       ) {
	exc_errlog(LOG_ERR, errno, "cannot read system namelist");
	exit(-1);
    }
    kmem = open("/dev/kmem", 0);
    if (kmem < 0) {
	exc_errlog(LOG_ERR, errno, "cannot open /dev/kmem");
	exit(-1);
    }
#ifdef sgi
    if (sizeof(long) == 8) {
        if (!is_elf64 ||
            nl64[N_KERNEL_MAGIC].n_type == 0 || nl64[N_END].n_type == 0
            || (g1 =(0 > lseek(kmem, (long)nl64[N_KERNEL_MAGIC].n_value  & 0x7fffffffffffffff, 0)))
            || sizeof(kern_end) !=(g2= read(kmem, &kern_end, sizeof(kern_end)))            || (g3 = (kern_end != nl64[N_END].n_value))) {
            exc_errlog(LOG_ERR, errno, "wrong system namelist: %s", sysname);
            exit(1);
        }
      }
      else
#endif /* sgi */
    if (sizeof(long) == 8) {
	if (nl[N_KERNEL_MAGIC].n_type == 0 || nl[N_END].n_type == 0
	    || 0 > lseek(kmem, (long)nl[N_KERNEL_MAGIC].n_value & 0x7fffffffffffffff, 0)
	    || sizeof(kern_end) != read(kmem, &kern_end, sizeof(kern_end))
	    || kern_end != nl[N_END].n_value) {
	    exc_errlog(LOG_ERR, errno, "wrong system namelist: %s", sysname);
	    exit(1);
	}
    }
    else {
	if (nl[N_KERNEL_MAGIC].n_type == 0 || nl[N_END].n_type == 0
	    || 0 > lseek(kmem, (long)nl[N_KERNEL_MAGIC].n_value & 0x7fffffff, 0)
	    || sizeof(kern_end) != read(kmem, &kern_end, sizeof(kern_end))
	    || kern_end != nl[N_END].n_value) {
	    exc_errlog(LOG_ERR, errno, "wrong system namelist: %s", sysname);
	    exit(1);
	}
   }
}

#ifndef KERNBASE64
#define KERNBASE64 0x8000000000000000
#endif
#ifndef KERNBASE
#define KERNBASE 0x80000000
#endif

/*
 * Seek into the kernel for a value.
 */

off_t
klseek(off_t base, int whence)
{
	base &= (is_elf64 ? ~KERNBASE64 : ~KERNBASE);
	return (lseek(kmem, base, whence));
}

int
kread(void *buf, int size)
{
     return read(kmem, buf, size);
}
