/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: pipe.h,v $
 * Revision 65.1  1997/10/20 19:19:57  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.62.1  1996/10/02  17:49:09  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:38:52  damon]
 *
 * Revision 1.1.57.2  1994/06/09  14:09:38  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:24:15  annie]
 * 
 * Revision 1.1.57.1  1994/02/04  20:20:10  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:13:49  devsrc]
 * 
 * Revision 1.1.55.1  1993/12/07  17:26:16  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/03  15:30:43  jaffe]
 * 
 * Revision 1.1.2.2  1993/01/21  19:38:27  zeliff
 * 	Embedding copyright notices
 * 	[1993/01/19  19:52:30  zeliff]
 * 
 * Revision 1.1  1992/01/19  02:51:48  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
*/
/*
 * pipe.h
 *
 * common data structure for describing pipes.
 *
 * (C) Copyright 1991 Transarc Corporation.
 * All Rights Reserved.
 *
 */

#ifndef _TRANSARC_PIPE_H
#define _TRANSARC_PIPE_H

typedef struct afsPipe {
    unsigned  refCount;
    unsigned  flags;
    unsigned  pipe_error;
    int (*pipe_read) _TAKES((struct afsPipe *, char *, unsigned));
    int (*pipe_write) _TAKES((struct afsPipe *, char *, unsigned));
    void (*pipe_hold) _TAKES((struct afsPipe *));
    void (*pipe_rele) _TAKES((struct afsPipe *));
    int (*pipe_close) _TAKES((struct afsPipe *, unsigned));
    int (*pipe_stat) _TAKES((struct afsPipe *, unsigned, long *));
    int (*pipe_crash) _TAKES((struct afsPipe *, unsigned));
    char *private_data;
} afsPipe_t;

/* pronounced "pawp" */

#define POP_READ(obj, buf, size) (*((obj)->pipe_read))(obj, buf, size)
#define POP_WRITE(obj, buf, size) (*((obj)->pipe_write))(obj, buf, size)
#define POP_HOLD(obj) (*((obj)->pipe_hold))(obj)
#define POP_RELE(obj) (*((obj)->pipe_rele))(obj)
#define POP_CLOSE(obj, flags) (*((obj)->pipe_close))(obj, flags)
#define POP_STAT(obj, type, code) (*((obj)->pipe_stat))(obj, type, code)
#define POP_CRASH(obj, flags) (*((obj)->pipe_crash))(obj, flags)

/* pronounce pifful */
#define PFL_READ    0x01 /* read-capable pipe */
#define PFL_WRITE   0x02 /* write-capable pipe */
#define PFL_DUPLEX  (PFL_READ|PFL_WRITE)
#define PFL_EOF     0x04 /* eof detected */
#define PFL_CLOSED  0x08 /* this pipe is closed */
#define PFL_NOCLOSE 0x10 /* don't close upon release */
#endif /* _TRANSARC_PIPE_H */
