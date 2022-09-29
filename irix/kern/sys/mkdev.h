/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_MKDEV_H
#define _SYS_MKDEV_H

#ifdef __cplusplus
extern "C" {
#endif

/* SVR3 device number constants */

#define ONBITSMAJOR	7	/* # of SVR3 major device bits */
#define ONBITSMINOR	8	/* # of SVR3 minor device bits */
#define OMAXMAJ		0x7f	/* SVR3 max major value */
#define OMAXMIN		0xff	/* SVR3 max minor value */


#define NBITSMAJOR	14	/* # of SVR4 major device bits */
#define NBITSMINOR	18	/* # of SVR4 minor device bits */
#define MAXMAJ		0x1ff	/* XXXdh this may change if we decide to make
				 * lboot size the MAJOR array flexibly. */

#define MAXMIN		0x3ffff	/* MAX minor */

#if !defined(_KERNEL)

/* undefine sysmacros.h device macros */

#undef makedev
#undef major
#undef minor


dev_t makedev(const major_t, const minor_t);
major_t major(const dev_t);
minor_t minor(const dev_t);
dev_t __makedev(const int, const major_t, const minor_t);
major_t __major(const int, const dev_t);
minor_t __minor(const int, const dev_t);

#define OLDDEV 0	/* old device format */
#define NEWDEV 1	/* new device format */

#define MKDEV_VER NEWDEV


#define makedev(maj, min)	__makedev(MKDEV_VER, maj, min)

#define STRING_SPEC_DEV		__makedev(MKDEV_VER, 0, 0)
#define IS_STRING_SPEC_DEV(x)	((dev_t)(x)==__makedev(MKDEV_VER, 0, 0))

#define major(dev)	__major(MKDEV_VER, dev)

#define minor(dev) 	__minor(MKDEV_VER, dev)

#endif	/* !defined(_KERNEL) */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_MKDEV_H */
