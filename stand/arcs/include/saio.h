#ident "include/saio.h: $Revision: 1.46 $" 

#ifndef __SAIO_H__
#define __SAIO_H__

/*
 * saio.h -- Header file for standalone package
 */

#include <arcs/types.h>
#include <arcs/io.h>
#include <arcs/folder.h>
#include <arcs/fs.h>

#define	DEVREAD(fsb)	((fsb)->IO->FunctionCode = FC_READ, \
			(!((fsb)->DeviceStrategy((fsb)->Device,(fsb)->IO)) \
			 ? (fsb)->IO->Count : -1))
#define	DEVWRITE(fsb)	((fsb)->IO->FunctionCode = FC_WRITE, \
			(!((fsb)->DeviceStrategy((fsb)->Device,(fsb)->IO)) \
			 ? (fsb)->IO->Count : -1))

/*
 * File and device flags
 */
#define F_READ		0x0001		/* file opened for reading */
#define F_WRITE		0x0002		/* file opened for writing */
#define	F_NBLOCK	0x0004		/* non-blocking io */
#define	F_SCAN		0x0008		/* device should be scanned */
#define	F_GFX		0x0010		/* device is part of gfx */
#define	F_BLOCK		0x0020		/* block device */
#define	F_FS		0x0040		/* device may have a filesys */
#define F_MS		0x0080		/* device is a mouse (extra scans) */
#define F_CREATE	0x0100		/* (fs) create file on open */
#define F_SUPERSEDE	0x0200		/* (fs) supersede file on open */
#define F_DIR		0x0400		/* (fs) request to operate on dir */
#define F_CONS		0x0800		/* Pathname requested console(0) */
#define F_ECONS		0x1000		/* Pathname requested console(1) */
#ifdef NETDBX
#define F_READANY	0x2000		/* Read any input */
#endif /* NETDBX */

/* 
 * Irix driver compatibility hooks
 */

/*
 * Request codes
 */
#define NO_IO	0
#define	READ	1
#define	WRITE	2

#include <sys/debug.h>	/* for ASSERT */

/*	This is so normal kernel drivers can be used with minimal changes.
	At this point, the driver still has to fake up any fields it
	uses, clear u_error at entrance, and set i_error at exit.
	Obviously saio.h must be included after user.h
	Some day maybe a bit will be set in the library code as to
	which interface to use.
*/
#undef u	/* from user.h */
struct u {
	int u_error;
};

extern struct u u;

#endif
