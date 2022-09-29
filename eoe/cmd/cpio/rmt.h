/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1986,1988, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident	"$Revision: 1.2 $"

#ifndef RMT
#define rmtclose close
#define rmtfstat fstat
#define rmtlseek lseek
#define rmtopen open
#else
int rmtaccess(char *, int);
int rmtclose(int);
int rmtcreat(char *, int);
int rmtfstat(int, struct stat *);
int rmtioctl(int, int, ...);
off_t rmtlseek(int, off_t, int);
int rmtopen(char *, int, ...);
int rmtread(int, char *, unsigned int);
int rmtwrite(int, char *, unsigned int);
#endif
