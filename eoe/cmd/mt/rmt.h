#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/mt/RCS/rmt.h,v 1.2 1995/06/21 07:17:36 olson Exp $"

/*
 *  FILE
 *
 *	rmt.h    redefine the system calls for the remote mag tape library
 *
 *  SCCS
 *
 *	@(#)rmt.h	1.3	9/20/85
 *
 *  SYNOPSIS
 *
 *	#ifdef RMT
 *	#include <rmt.h>
 *	#endif
 *
 *  DESCRIPTION
 *
 *	This file makes the use of remote tape drives transparent
 *	to the program that includes it.  A remote tape drive has
 *	the form system[.user]:/dev/???.
 *
 *	Note that the standard system calls are simply remapped to
 *	the remote mag tape library support routines.
 *
 *	Also, note that if <sys/stat.h> is included, it must be AFTER
 *	this file to get the stat structure name remapped.
 *
 */

#ifndef access		/* avoid multiple redefinition */

#define access rmtaccess
#define close rmtclose
#define creat rmtcreat
#define ioctl rmtioctl
#define isatty rmtisatty
#define open rmtopen
#define read rmtread
#define write rmtread

int rmtaccess(char *, int);
int rmtclose(int);
int rmtcreat(char *, int);
int rmtioctl(int, int, ...);
off_t rmtlseek(int, off_t, int);
int rmtopen(char *, int, ...);
int rmtread(int, char *, unsigned int);
int rmtwrite(int, char *, unsigned int);

#endif
