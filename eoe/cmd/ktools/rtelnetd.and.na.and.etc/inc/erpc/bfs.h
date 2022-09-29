/*****************************************************************************
 *
 *        Copyright 1989, Xylogics, Inc.  ALL RIGHTS RESERVED.
 *
 * ALL RIGHTS RESERVED. Licensed Material - Property of Xylogics, Inc.
 * This software is made available solely pursuant to the terms of a
 * software license agreement which governs its use.
 * Unauthorized duplication, distribution or sale are strictly prohibited.
 *
 * Include file description:
 *   %$(description)$%
 *
 * Original Author: %$(author)$% Created on: %$(created-on)$%
 *
 * Revision Control Information:
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/inc/erpc/RCS/bfs.h,v 1.2 1996/10/04 12:03:07 cwilson Exp $
 *
 * This file created by RCS from:
 *
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/inc/erpc/RCS/bfs.h,v $
 *
 * Revision History:
 *
 * $Log: bfs.h,v $
 * Revision 1.2  1996/10/04 12:03:07  cwilson
 * latest rev
 *
 * Revision 2.6  1989/04/05  12:21:46  loverso
 * Changed copyright notice
 *
 * Revision 2.5  88/06/29  15:54:23  loverso
 * Default to block size of 512 for all hosts.
 * 
 * Revision 2.4  88/04/26  15:58:36  mattes
 * SL/IP integration
 * 
 * Revision 2.3  87/12/04  12:38:19  harris
 * Added MASSCOMP to EXOS, because they seem to require 512 block size too.
 * 
 * Revision 2.2  87/10/01  15:17:25  mattes
 * Added BFS_REQUIRE -- complain if a file isn't there
 * 
 * Revision 2.1  86/09/03  12:05:19  harris
 * Added Excelan support (EXOS).  Block size of 512 required for Excelan.
 * 
 * Revision 2.0  86/02/20  16:00:08  parker
 * First development revision for Release 2
 * 
 * Revision 1.1  85/11/18  13:56:59  brennan
 * Initial revision
 * 
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *  DATE:    $Date: 1996/10/04 12:03:07 $
 *  REVISION:    $Revision: 1.2 $
 *
 ****************************************************************************/

/*  bfs.h   2.0 12-Apr-84   jmt */

/* BFS block size */
#define BFS_BPBLOCK 512

/* Downline load ERPC remote program definitions. */
#define BFS_PROG    0x1
#define BFS_VER     0

/* BFS remote procedures */
#define BFS_OPEN    0
#define BFS_READ    1
#define BFS_WRITE   2
#define BFS_CLOSE   3
#define BFS_END     4

/* u_short offsets to call arguments to BFS_OPEN */
#define BFS_MINVER  0
#define BFS_OPTIONS 1
#define BFS_NAMLEN  2
#define BFS_NAME    3

/* u_short offsets to call arguments to BFS_READ and BFS_WRITE */
#define BFS_BLOCK   0
#define BFS_DATALEN 1
#define BFS_DATA    2

/* values for the options argument to BFS_OPEN */
#define BFS_RECREATE    0	/* Create or recreate */
#define BFS_NORECREATE  1	/* Don't create and don't complain */
#define BFS_REQUIRE     2	/* Complain if it isn't there */

/* BFS abort error values */
#define BFS_ENOENT      0       /* No such file or directory */
#define BFS_OPENERR     1       /* Error opening file */
#define BFS_DUPLOPEN    2       /* Duplicate open request in session */
#define BFS_READERR     3       /* Read error */
#define BFS_EOF         4       /* End of file */
#define BFS_NOTOPEN     5       /* File not yet open */
#define BFS_ACTIVE      6       /* End session attempted while file open */
#define BFS_WRITERR     7       /* Write error */
#define BFS_TOOLONG     8       /* Write request for longer than one block */
#define BFS_UNSPEC      0xffff  /* Unspecified */
