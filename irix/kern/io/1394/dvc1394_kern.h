/*
 * Copyright 1995 Silicon Graphics, Inc.  All rights reserved.
 *
 */
#ident "$Revision: 1.5 $"

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
#ifndef _IEEE1394H
  #include "ieee1394.h"
#endif
#include "dvc1394.h"


#define MAX_DVC1394_PORTS 5
#define MAX_DVC1394_QUEUESIZE 30
#define START_PACKET 0
#define INTERIOR_PACKET 1
#define END_PACKET 2
#define NTSC_FRAME 0
#define PAL_FRAME 1

/* Info stored at dvc1394 and each "cloned" vertex */
typedef struct info_s {
    vertex_hdl_t parent_vertex;
    int app_no;
} info_t;

typedef int (*fun_t)(void*); /* fun_t is typed as function pointer */

/* Per channel information */

typedef struct dvc1394_channel_status_s {
    int packet_offset;
    int packet_sequence;
    int frame_type;
    int packet_last_dseq;
    int packet_last_sequence;
    int ntsc;
}dvc1394_channel_status_t;

typedef struct dvc1394_port_s {
    int opened;         /* 1 if opened */
    int mapped;         /* 1 if mmapped */
    int channel;        /* iso channel number */
    int maplen;         /* Amount of mmapped memory */
    int nbufs;          /* number of buffers */
    dvc1394_buf_t *curbufp;
    int head;
    int tail;
    int full;
    int waiting;
    dvc1394_buf_t *bufpool_kernaddr;
    caddr_t        bufpool_vaddr;
    int direction;	/* 0=not set yet, 1=in, 2=out */
    char *compressedbuf;
    char *transmitbuf;
    char *transmitbufp;
    int num;
    int denom;
    int inc;
    int frame_in_progress;
    int still_in_progress;
    struct pollhead pq; /* Poll head (for select, poll) */
}dvc1394_port_t;

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
extern int (*board_driver_get_isoxmt_info)(int *headp);

/* Callback function structure (UpdateInterestTable(), thread_asyrecv() */
typedef struct callback_s {
    int callback_code; /* -1 for DEVICE_DISAPPEARED, tcode otherwise */
    int app_id;
    int source_id;
    int destination_id;
    long long destination_offset;
    int tlabel;
    int data_length;  /* valid only for block payload transactions */
    int extended_tcode;
    int payload[128]; /* valid only for block payload transactions */
} callback_t;

