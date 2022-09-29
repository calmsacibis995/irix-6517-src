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
 *	There are no declarations of the remapped functions (rmt<name>)
 *	because the original declarations elsewhere will be remapped also.
 *	("extern int access()" will become "extern int rmtaccess()")
 *
 */

#ifndef access		/* avoid multiple redefinition */
#  define access rmtaccess
#  define close rmtclose
#  define creat rmtcreat
#  define fstat rmtfstat
#  define fstat64 rmtfstat64
#  define ioctl rmtioctl
#  define isatty rmtisatty
#  define lseek rmtlseek
#  define lseek64 rmtlseek
#  define open rmtopen
#  define read rmtread
#  define stat rmtstat
#  define stat64 rmtstat64
#  define write rmtwrite
#endif
