/*
 * Copyright 1995 Silicon Graphics, Inc.  All rights reserved.
 *
 */
#ident "$Revision: 1.3 $"


/* Ioctl commands */

#define RAWISO1394_SET_CHANNEL		1

typedef struct rawiso1394_bufhdr_s {
    long packet_count;
    long packet_length;
    long cyclecounter;
    long endflag;
}rawiso1394_bufhdr_t;
