#ifndef __TTYMAP_H__
#define __TTYMAP_H__
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.4 $"
/*
*
* Copyright 1992, Silicon Graphics, Inc.
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
/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * ttymap.h - header for ttymap cmd and ttyname libcfunction
 */


#define MAP	"/etc/ttymap"
#define DEV	"/dev"
#define DEVLEN	4
#define DEVFLG	MATCH_ALL

#define TTYSRCH	"/etc/ttysrch"

struct tty_map {
	size_t dent_tbl;
	size_t dent_sz;
	size_t rent_tbl;
	size_t rent_sz;
	size_t name_tbl;
	size_t name_sz;
	size_t dirmap_tbl;
	size_t dirmap_sz;
	time_t date;
};

struct dent {
	dev_t dev;
	uint nrent;
	size_t rent_offset;
};
struct rent {
	dev_t rdev;
	size_t nm_offset;
	uint nm_size;
};

struct dirmap {
	uint len;
	uint depth;
	uint flags;
	size_t nm_offset;
};

struct path_list {
	struct path_list *nxt;
	char *path;
};
struct rdev_list {
	struct rdev_list *nxt;
	dev_t st_rdev;
	struct path_list *path_list;
};
struct dev_list {
	struct dev_list *nxt;
	dev_t st_dev;
	struct rdev_list *rdev_list;
};

struct dirt_list {
	struct dirt_list *nxt;
	char *path;
};

struct entry {
	char *name;
	int len;
	int flags;
};

#define MAX_DEV_PATH	128
#define MAX_SRCH_DEPTH	4

#define MATCH_MM	1
#define MATCH_FS	2
#define MATCH_INO	4
#define MATCH_ALL	7
#define IGNORE_DIR	8

#define MSG_1	"ERROR: Entry '"
#define MSG_2	"' in /etc/ttysrch ignored.\n"

#ifndef NULL
#define NULL	((char *)0L)
#endif

#ifndef S_ISCHR
#define S_ISCHR(mode)	((mode&S_IFMT) == S_IFCHR)
#endif

#ifndef S_ISDIR
#define S_ISDIR(mode)	((mode&S_IFMT) == S_IFDIR)
#endif

#define CONSOLE	"/dev/console"

#ifdef TTYMAP
static const struct entry def_srch_dirs[] = {	/* default search list */
	{ "/dev/term",	9,	MATCH_ALL	},
	{ "/dev/pts",	8,	MATCH_ALL	},
	{ "/dev/xt",	7,	MATCH_ALL	},
	{ "/dev/dsk",	8,	IGNORE_DIR	},
	{ "/dev/rdsk",	9,	IGNORE_DIR	},
	{ NULL, 	0,	0	 	}
};
#endif
#ifdef __cplusplus
}
#endif
#endif /* !__TTYMAP_H__ */
