#ifndef __NL_TYPES_H__
#define __NL_TYPES_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 1.19 $"

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
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#include <standards.h>
#include <sgidefs.h>

/*
 * allow for limits.h
 */
#ifndef NL_SETMAX
#define NL_SETMAX	1024
#define NL_MSGMAX	32767
#define NL_TEXTMAX	2048
#endif

#define NL_MAXPATHLEN	1024
#define NL_PATH		"NLSPATH"
#define NL_LANG		"LANG"
#define NL_DEF_LANG	"english"
#define NL_SETD		1
#define NL_MAX_OPENED	10

#define NL_CAT_LOCALE   1

#if _NO_XOPEN4
/*
 * gencat internal structures
 */
struct cat_msg {
  int msg_nr;			/* The message number */
  int msg_len;			/* The actual message len */
  long msg_off;			/* The message offset in the temporary file */
  char *msg_ptr;		/* A pointer to the actual message */
  struct cat_msg *msg_next;     /* Next element in list */
};

struct cat_set {
  int set_nr;			/* The set number */
  int set_msg_nr;		/* The number of messages in the set */
  struct cat_msg *set_msg;	/* The associated message list */
  struct cat_set *set_next;	/* Next set in list */
};
#endif	/* _NO_XOPEN4 */


/*
 * mkmsgs format set
 * information
 */
struct _m_cat_set {
#ifdef first_msg
  int __first_msg;		/* The first message number */
#else
  int first_msg;		/* The first message number */
#endif
#ifdef last_msg
  int __last_msg;		/* The last message in the set */
#else
  int last_msg;			/* The last message in the set */
#endif
};
typedef struct _m_cat_set m_cat_set_t;

/*
 * structure in file
 */
struct _set_info {
#ifdef no_sets
	int __no_sets;
#else
	int no_sets;
#endif
#ifdef sn
	struct _m_cat_set __sn[1];
#else
	struct _m_cat_set sn[1];
#endif
};
typedef struct _set_info set_info_t;

#if _NO_XOPEN4
#define CMD_SET		"set"
#define CMD_SET_LEN	3
#define CMD_DELSET	"delset"
#define CMD_DELSET_LEN	6
#define CMD_QUOTE	"quote"
#define CMD_QUOTE_LEN	5

#define XOPEN_DIRECTORY "/usr/lib/locale/Xopen/LC_MESSAGES"
#define DFLT_MSG	"\01"   
#define M_EXTENSION	".m"
/*
 * Default search pathname
 */
#define DEF_NLSPATH	"/usr/lib/locale/%L/LC_MESSAGES/%N:/usr/lib/locale/%L/Xopen/LC_MESSAGES/%N:/usr/lib/locale/%L/LC_MESSAGES/%N.cat:/usr/lib/locale/C/LC_MESSAGES/%N:/usr/lib/locale/C/LC_MESSAGES/%N.cat"

/*
 * Default search path for the C locale only. Can still be overridden with NLSPATH.
 */
#define _C_LOCALE_DEF_NLSPATH	"/usr/lib/locale/C/LC_MESSAGES/%N:" \
				"/usr/lib/locale/C/Xopen/LC_MESSAGES/%N:" \
				"/usr/lib/locale/%L/LC_MESSAGES/%N.cat"

/* Default explanation and message set numbers */
#define NL_EXPSET       NL_SETD       /* set number for explanations     */
#define NL_MSGSET       NL_SETD       /* set number for messages         */

#if __NLS_INTERNALS
/* catmsgfmt formating information */
#define MSG_FORMAT	"MSG_FORMAT"
#define D_MSG_FORMAT	"%G-%N %C: %S %P\n  %M\n"

/* Internal catopen errors */
#define NL_ERR_MAXOPEN	-2      /* Too many catalog files are open.
				   See NL_MAX_OPENED */
#define NL_ERR_MAP	-3      /* The mmap(2) system call failed */
#define NL_ERR_MALLOC	-4      /* malloc(3C) failed */
#define NL_ERR_HEADER	-5      /* Message catalog header error */

/* Internal catgets and catgetmsg errors */
#define NL_ERR_ARGERR	-6      /* Bad catd argument */
#define NL_ERR_BADSET	-7      /* The set does not exist in the catalog. */
#define NL_ERR_NOMSG    -8      /* The message was not found. */
#define NL_ERR_BADTYPE  -9      /* The catalog type is unknown */

#endif  /* __NLS_INTERNALS */	

struct cat_hdr {
  __int32_t hdr_magic;		/* The magic number */
  int hdr_set_nr;		/* Set nr in file */
  int hdr_mem;			/* The space needed to load the file */
  __int32_t hdr_off_msg_hdr;	/* Position of messages headers in file */
  __int32_t hdr_off_msg;	/* Position of messages in file */
};
#endif	/* _NO_XOPEN4 */

struct _cat_set_hdr {
#ifdef shdr_set_nr
  int __shdr_set_nr;		/* The set number */
#else
  int shdr_set_nr;		/* The set number */
#endif
#ifdef shdr_msg_nr
  int __shdr_msg_nr;		/* Number of messages in set */
#else
  int shdr_msg_nr;		/* Number of messages in set */
#endif
#ifdef shdr_msg
  int __shdr_msg;		/* Start offset of messages in messages list */
#else
  int shdr_msg;			/* Start offset of messages in messages list */
#endif
};
typedef struct _cat_set_hdr cat_set_hdr_t;

struct _cat_msg_hdr{
#ifdef msg_nr
  int __msg_nr;			/* The messge number */
#else
  int msg_nr;			/* The messge number */
#endif
#ifdef msg_len
  int __msg_len;		/* message len */
#else
  int msg_len;			/* message len */
#endif
#ifdef msg_ptr
  int __msg_ptr;		/* message offset in table */
#else
  int msg_ptr;			/* message offset in table */
#endif
};
typedef struct _cat_msg_hdr cat_msg_hdr_t;

#if _NO_XOPEN4
#define CAT_MAGIC	0xFF88FF89
#endif	/* _NO_XOPEN4 */

typedef int nl_item ;

struct _malloc_data {
#ifdef sets
	cat_set_hdr_t *__sets;
#else
	cat_set_hdr_t *sets;
#endif
#ifdef msgs
	cat_msg_hdr_t *__msgs;
#else
	cat_msg_hdr_t *msgs;
#endif
#ifdef data
	char *__data;
#else
	char *data;
#endif
};
typedef struct _malloc_data malloc_data_t;

struct _gettxt_data {
#ifdef sets
	set_info_t *__sets;
#else
	set_info_t *sets;
#endif
#ifdef msgs
	char *__msgs;
#else
	char *msgs;
#endif
#ifdef msg_size
	int __msg_size;
#else
	int msg_size;
#endif
#ifdef set_size
	int __set_size;
#else
	int set_size;
#endif
};
typedef struct _gettxt_data gettxt_data_t;

typedef struct _nl_catd {
#ifdef type
  char __type;
#else
  char type;
#endif
#ifdef set_nr
  int __set_nr;
#else
  int set_nr;
#endif
#if _SGIAPI
#ifdef path_name
  char *__path_name;
#else
  char *path_name;
#endif
#endif
  union {
#ifdef m
    malloc_data_t __m;
#else
    malloc_data_t m;
#endif
#ifdef g
    gettxt_data_t __g;
#else
    gettxt_data_t g;
#endif
#ifdef info
 } __info;
#else
 } info;
#endif
} nl_catd_t;

typedef	void	*nl_catd;

#if _NO_XOPEN4
/*
 * type fields for nl_catd_t
 */
#define MKMSGS		'M'	/* mkmsgs interfaces */
#define MALLOC		'm'	/* old style malloc  */

#define BIN_MKMSGS  "mkmsgs"
#endif	/* _NO_XOPEN4 */

int catclose(nl_catd);
char *catgets(nl_catd, int, int, const char *);
nl_catd catopen(const char *, int);

#if _SGIAPI
char *catgetmsg(nl_catd, int, int, char *, int);
char *catmsgfmt(const char *, const char *, int, const char *,
		const char *, char *, int, char *, char *);

char *gettxt(const char *, const char *);

#if __NLS_INTERNALS
char *_cat_name(char *, char *, int, int);

int __catgets_error_code(void);
int __catgetmsg_error_code(void);
int __catopen_error_code(void);

char *__cat_path_name(nl_catd);
#endif  /* __NLS_INTERNALS */
#endif

#ifdef __cplusplus
}
#endif
#endif /* !__NL_TYPES_H__ */


