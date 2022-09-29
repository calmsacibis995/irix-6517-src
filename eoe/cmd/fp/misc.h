/*
 *=============================================================================
 *			File:		misc.h
 *			Purpose:	This file contains miscellaneous
 * 					definitions that're commonly used.
 *=============================================================================
 */
#ifndef	_MISC_H
#define	_MISC_H

#include <sys/types.h>
#include <sys/bsd_types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <strings.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>

#define	K			(1024)
#define	MB			(K*K)
#define GB			(K*MB)
#define	SECTOR_SIZE		(512)

char	*ground(u_long value);
void    *gmalloc(size_t size);
void	gopen(char *dev_name);
void    gseek(off_t offset);
void    gread(void *buf, unsigned nbyte);
void    gwrite(void *buf, unsigned nbyte);
void	gerror(char *err_msg);

u_int   align_int(u_int in);
u_short align_short(u_short in);

char    *spaceskip(char * strp);
u_int   ceiling(u_int num, u_int den);

#endif
