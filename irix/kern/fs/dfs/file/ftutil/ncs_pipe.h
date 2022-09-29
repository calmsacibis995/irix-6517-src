/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: ncs_pipe.h,v $
 * Revision 65.1  1997/10/20 19:19:57  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.61.1  1996/10/02  17:49:08  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:38:51  damon]
 *
 * $EndLog$
 */
/*
*/
/*
 * ncs_pipe.h
 *
 * (C) Copyright 1990 Transarc Corporation
 * All Rights Reserved.
 *
 */

#ifndef _TRANSARC_NCS_PIPE_H
#define _TRANSARC_NCS_PIPE_H

#include <dcedfs/pipe.h>

/*
 * interface between NCS pipe code
 * and generic pipe code
 */

typedef struct ncsPipeData {
    pipe_t *pipeP;
    char *dataBuf;
    long dataBufOff;
    long dataBufSize;
    long status;
} ncsPipeData_t;

#define NCS_BUFFER_SIZE (4096)

/* declaration(s) */

int CreateNCSPipe _TAKES((pipe_t *, afsPipe_t *, unsigned));

#endif /* _TRANSARC_NCS_PIPE_H */
