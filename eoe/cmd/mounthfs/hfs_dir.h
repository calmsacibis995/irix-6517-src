/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1993 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __HFS_DIR_H__
#define __HFS_DIR_H__

#ident "$Revision: 1.4 $"

/* Xinet mapping directory names */

#define DOTRESOURCE ".HSResource"
#define DOTANCILLARY ".HSancillary"


/* Used by readdir routines. */

typedef struct dirs{
  entry   *cur,*last;		/* Ptr to current and last entry */
  u_int   free;			/* Bytes remaining */
  hnode_t *hp;			/* Address of directory node. */
  u_int   offset;		/* Directory stream offset. */
} dirs_t;

/* Useful file names */

extern char *dot;
extern char *dotdot;
extern char *dotresource;
extern char *dotancillary;

typedef int (*dir_callback_t)(hnode_t *, frec_t *, void*);

/* Forward directory routines */

int dir_iterate(hnode_t *dhp, u_int *offset, dir_callback_t cbf, void*);
int dir_readdir_init(hnode_t *hp, u_int offset);
int dir_lookup(hnode_t *hp, char *name, frec_t **frp);
int dir_lookup_m(hnode_t *hp, char *name, frec_t **frp);
int dir_init_fr(frec_t *fp, char *name, u_int hfstime, u_char type);
int dir_rename_fr(frec_t *fp, u_int pfid, char *name);
int dir_fr_to_entry(frec_t *fp, dirs_t* dsp, char *defname, int tag);
int dir_mstr_cmp(char *s0, char *s1);
int dir_validate_uname(char *name);
#endif







