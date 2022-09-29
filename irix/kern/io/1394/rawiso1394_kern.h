/*
 * Copyright 1995 Silicon Graphics, Inc.  All rights reserved.
 *
 */
#ident "$Revision: 1.1 $"

#include "sys/types.h"
#include "sys/param.h"
#include "sys/systm.h"
#include "sys/edt.h"
#include "sys/sysmacros.h"
#include "sys/cmn_err.h"
#include "sys/debug.h"
#include "sys/kmem.h"
#include "sys/mman.h"
#include "ksys/ddmap.h"
#include "sys/errno.h"
#include "sys/immu.h"
#include "sys/invent.h"
#include "sys/kopt.h"
#include "sys/mbuf.h"
#include "sys/sbd.h"
#include "sys/socket.h"
#include "sys/cpu.h"
#include "sys/ddi.h"
#include <sys/cred.h>
#include "sys/ktime.h"
#include "sys/mload.h"
#include "sys/conf.h"
#include <sys/ksynch.h>
#include <sys/pfdat.h>
#include <sys/hwgraph.h>
#include "sys/poll.h"
#include <string.h>
#ifndef _IEEE1394H
#include "ieee1394.h"
#endif
#include "rawiso1394.h"

/* Info stored at rawiso1394 and each "cloned" vertex */
typedef struct info_s {
    vertex_hdl_t parent_vertex;
    int app_no;
} info_t;

typedef int (*fun_t)(void*); /* fun_t is typed as function pointer */

/* Port information */

typedef struct rawiso1394_port_s {
    int opened;		/* 1 if opened */
    int mapped;		/* 1 if mmapped */
    int channel;	/* iso channel number */
    int maplen;		/* Amount of mmapped memory */
    int corrupted;	/* 1 if port is corrupted */
    int packetcount;
    int *headp;
    int *tailp;
    int *dropcountp;
    int *bufstart;
    int bufnquads;
    int tail;
    int            *bufpool_kernaddr;
    caddr_t         bufpool_vaddr;
    struct pollhead pq; /* Poll head (for select, poll) */
}rawiso1394_port_t;

/* OPERATIONS (to get headerlist info & interrupt callback) */
#define SIDRCV 0
#define ISORCV 1
#define ISOXMT 2
#define ASYRCV 3
#define ASYXMT 4
#define ACKRCV 5
#define BUSRESET 6

extern int (*board_driver_get_headerlist_info)(int op, int index, int **hdr,
                int **cyclecounter, int **len, int **datap);
extern int (*board_driver_get_isorcv_info)(int *tailp);

