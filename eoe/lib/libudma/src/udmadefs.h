#ifndef __UDMADEFS_H__
#define __UDMADEFS_H__

/*
 * libudma/udmalib.h
 *
 *
 * Copyright 1993, Silicon Graphics, Inc.
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

#ident "$Revision: 1.2 $"

#if defined(__STDC__)

struct dmaid_s {
	unsigned char	 d_bus;
	unsigned int	 d_adap;
	void		*d_businfo;
};
typedef struct dmaid_s dmaid_t;


/* VME support prototypes */
extern int vme_open(dmaid_t *dp);
extern int vme_close(dmaid_t *dp);
extern void *vme_allocbuf(dmaid_t *dp, int size);
extern int vme_freebuf(dmaid_t *dp, void *bp);
extern int vme_freeparms(dmaid_t *dp, udmaprm_t *dparms);
extern udmaprm_t *vme_mkparms(dmaid_t *dp, void *dinfo, void *buf, int size);
extern int vme_start(dmaid_t *dp, void *busaddr, udmaprm_t *dparms);

/* Universe VME support prototypes */
extern int universe_vme_open(dmaid_t *dp);
extern int universe_vme_close(dmaid_t *dp);
extern void *universe_vme_allocbuf(dmaid_t *dp, int size);
extern int universe_vme_freebuf(dmaid_t *dp, void *bp);
extern int universe_vme_freeparms(dmaid_t *dp, udmaprm_t *dparms);
extern udmaprm_t *universe_vme_mkparms(dmaid_t *dp, void *dinfo, void *buf, int size);
extern int universe_vme_start(dmaid_t *dp, void *busaddr, udmaprm_t *dparms);

#endif /* __STDC__ */
#endif /* __UDMADEFS_H__ */
