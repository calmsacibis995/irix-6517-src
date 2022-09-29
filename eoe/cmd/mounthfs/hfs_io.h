/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1993 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __HFS_IO_H__
#define __HFS_IO_H__

#ident "$Revision: 1.4 $"

/* Well known global io_ routines */

int io_open(char *pathname, hfs_t *);
int io_close(hfs_t *);
int io_capacity(hfs_t *, u_int *capacity);

int io_pread(hfs_t *, u_int blk, u_int count, void *buf);
int io_lread(hfs_t *, u_int blk, u_int count, void *buf);
int io_aread(hfs_t *, u_int blk, u_int count, void *buf);

int io_pwrite(hfs_t *, u_int blk, u_int count, void *buf);
int io_lwrite(hfs_t *, u_int blk, u_int count, void *buf);
int io_awrite(hfs_t *, u_int blk, u_int count, void *buf);
int io_awrite_partial(hfs_t *, u_int alb_num, u_int alb_off,
                      int count, char *data);

int io_isfd(hfs_t *, int fd);

u_int pack(void *src, void *dst, u_short *mv);
void unpack(void *src, void *dst, u_short *mv);

#endif







