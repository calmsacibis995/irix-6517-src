/*
 * Copyright 1996-1997, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ifndef __VME_USRVME_PRIVATE_H__
#define __VME_USRVME_PRIVATE_H__

#define	EDGE_LBL_USER "user"

typedef struct usrvme_piomap_list_item_s {
	uthread_t *                        uthread;
	vmeio_piomap_t                     piomap;
	caddr_t                            uvaddr;
	caddr_t                            kvaddr;
	struct usrvme_piomap_list_item_s * next;
	struct usrvme_piomap_list_item_s * prev;
} * usrvme_piomap_list_item_t;

/*
 * Performance considerations: since the most recently allocated PIO map is
 *   likely the first one to be freed, newly allocated PIO map is added at 
 *   the head of the list.
 */
typedef struct usrvme_piomap_list_s {
	usrvme_piomap_list_item_t head;
	lock_t                    lock;  /* Lock on manipulation of the list */
} * usrvme_piomap_list_t;


extern vertex_hdl_t
usrvme_device_register(vertex_hdl_t); /* VMEbus provider */

#endif /* __VME_USRVME_PRIVATE_H__ */
