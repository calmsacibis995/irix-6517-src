/*
 * Copyright (C) 1986, 1992, 1993, 1994, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ifndef __SYS_SERIALIO_H__
#define __SYS_SERIALIO_H__

/*
 * ioctl(fd, I_STR, arg)
 * use the SIOC_RS422 and SIOC_EXTCLK combination to support MIDI
 */
#define SIOC		('z' << 8)
#define SIOC_EXTCLK	(SIOC | 1)  /* select/de-select external clock */
#define SIOC_RS422	(SIOC | 2)  /* select/de-select RS422 protocol */
#define SIOC_ITIMER	(SIOC | 3)  /* upstream timer adjustment */
#define SIOC_LOOPBACK   (SIOC | 4)  /* diagnostic loopback test mode */
#define SIOC_MKHWG	(SIOC | 5)  /* create hwgraph nodes */

/* multi-module system controller daemon private ioctls */
#define SIOC_MMSC_GRAB	(SIOC | 6)  /* put console port in MMSC mode */
#define SIOC_MMSC_RLS	(SIOC | 7)  /* release MMSC mode */

/* libusio private ioctls */
#define SIOC_USIO_GET_MAPID	(SIOC | 8)  /* get usio device map type */
#define SIOC_USIO_GET_ARGS	(SIOC | 9)  /* get usio device args */
#define SIOC_USIO_SET_SSCR	(SIOC | 10)  /* set ioc3 sscr reg. */
#define SIOC_USIO_RESET_SSCR	(SIOC | 11)  /* reset ioc3 sscr reg. */

/* interface types returned by DOWN_GET_MAPID */
#define USIO_MAP_IOC3		0

/* mapping offsets for type USIO_MAP_IOC3 */
#define USIO_MAP_IOC3_DMABUF	0
#define USIO_MAP_IOC3_REG	0x4000

/* id structure for type USIO_MAP_IOC3 */
struct ioc3_mapid {
    int port;
    int size;
};

extern void * usio_init(int);
extern int usio_write(void *, char *, int);
extern int usio_read(void *, char *, int);
extern int usio_get_status(void *);

/* flags for usio_get_status() */
#define USIO_ERR_PARITY		0x1
#define USIO_ERR_FRAMING	0x2
#define USIO_ERR_OVERRUN	0x4
#define USIO_BREAK		0x8

#endif /* __SERIALIO_H__ */
